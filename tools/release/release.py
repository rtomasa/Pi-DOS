#!/usr/bin/env python3
"""Build a fresh local release bundle; optionally update GitHub's rolling release."""

import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / 'output/release'
TAG = 'rolling'
PAYLOADS = ('pi-dos-arm64-sdcard.img.xz', 'pi-dos-user-guide.pdf', 'PIDOSDRV.ZIP')
ASSETS = (*PAYLOADS, 'build-info.json', 'SHA256SUMS')
ASSET_LIMIT = 2 * 1024**3


def run(*args, cwd=ROOT, **kwargs):
    print('+', ' '.join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), cwd=cwd, check=True, **kwargs)


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()


def clean_commit():
    if git('status', '--porcelain', '--untracked-files=normal'):
        raise RuntimeError('Commit or stash source changes first; releases build committed HEAD only.')
    return git('rev-parse', 'HEAD')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')


def repository(override):
    if override:
        repo = override
    else:
        remote = git('remote', 'get-url', 'origin')
        match = re.fullmatch(r'(?:https://github\.com/|git@github\.com:|ssh://git@github\.com/)(.+?)(?:\.git)?/?', remote)
        if not match:
            raise RuntimeError('Cannot identify GitHub origin; supply --repo OWNER/REPO.')
        repo = match[1]
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repo):
        raise RuntimeError('Repository must be OWNER/REPO on github.com.')
    return repo


def api(repo, endpoint, *, method='GET', data=None, missing_ok=False):
    command = ['gh', 'api', '--hostname', 'github.com', '--method', method,
               f'repos/{repo}/{endpoint}']
    if data is not None:
        command += ['--input', '-']
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True,
                            input=None if data is None else json.dumps(data))
    try:
        response = json.loads(result.stdout) if result.stdout else {}
    except json.JSONDecodeError:
        response = {}
    if result.returncode:
        if missing_ok and isinstance(response, dict) and str(response.get('status')) == '404':
            return None
        raise RuntimeError(result.stderr.strip() or f'GitHub API failed: {endpoint}')
    return response


def upload_preflight(repo, commit):
    if not shutil.which('gh'):
        raise RuntimeError('Install GitHub CLI (gh), then run: gh auth login --hostname github.com')
    # A local commit must already exist on GitHub; this procedure never pushes a branch.
    if api(repo, f'commits/{commit}', missing_ok=True) is None:
        raise RuntimeError(f'Commit {commit} is not on {repo}; push it before uploading.')
    # Listing also finds drafts left by an interrupted first upload. The
    # release-by-tag endpoint is documented for published releases only.
    release = next((item for item in api_items(repo, 'releases') if item['tag_name'] == TAG), None)
    if release and release.get('immutable'):
        raise RuntimeError('The rolling release is immutable and cannot be updated.')
    return release


def api_items(repo, endpoint):
    page = 1
    while True:
        items = api(repo, f'{endpoint}?per_page=100&page={page}')
        if not isinstance(items, list):
            raise RuntimeError(f'Invalid GitHub list response: {endpoint}')
        yield from items
        if len(items) < 100:
            return
        page += 1


def build_preflight(drivers, jobs):
    if sys.version_info < (3, 12):
        raise RuntimeError('Python 3.12 or newer is required.')
    if jobs < 1:
        raise RuntimeError('JOBS must be positive.')
    for adapter in ('S3', 'ET4'):
        if not (drivers / adapter / 'OEMSETUP.INF').is_file():
            raise RuntimeError(f'Missing {drivers / adapter / "OEMSETUP.INF"}')
    for tool in ('bash', 'make', 'xz', 'unzip', 'bsdtar'):
        if not shutil.which(tool):
            raise RuntimeError(f'Missing host tool: {tool}; see README.md build prerequisites.')


def prepare_guide_python(source):
    """Install pinned guide tools locally, without changing the system Python."""
    requirements = source / 'docs/pi-dos-user-guide/requirements.txt'
    environment = OUTPUT / 'guide-venv'
    interpreter = environment / 'bin/python'
    if environment.is_symlink():
        raise RuntimeError(f'Remove the unexpected symlink: {environment}')
    if not interpreter.is_file():
        try:
            run(sys.executable, '-m', 'venv', environment)
        except subprocess.CalledProcessError as error:
            raise RuntimeError('Cannot create the local guide environment. On Debian/Ubuntu, '
                               'install python3-venv, then rerun this command.') from error
    probe = '''from importlib.metadata import version
from pathlib import Path
import sys
for line in Path(sys.argv[1]).read_text().splitlines():
    line = line.strip()
    if line and not line.startswith('#'):
        package, expected = line.split('==')
        assert version(package) == expected, package
import markdown_it, typst
'''
    command = [str(interpreter), '-c', probe, str(requirements)]
    status = subprocess.run(command, cwd=source, capture_output=True)
    if status.returncode:
        print('Preparing pinned PDF guide tools in output/release/guide-venv', flush=True)
        run(interpreter, '-m', 'pip', 'install', '--disable-pip-version-check',
            '--cache-dir', OUTPUT / 'pip-cache', '-r', requirements)
        subprocess.run(command, cwd=source, check=True)
    return interpreter


def build_bundle(commit, drivers, jobs):
    work = OUTPUT / 'work'
    # This directory is owned by this procedure; keep it after builds for logs.
    if work.is_symlink():
        raise RuntimeError(f'Remove the unexpected symlink: {work}')
    if work.exists():
        shutil.rmtree(work)
    work.mkdir()
    source = work / 'source'
    source.mkdir()
    archive = work / 'source.tar'
    run('git', 'archive', '--format=tar', f'--output={archive}', commit)
    with tarfile.open(archive) as package:
        package.extractall(source, filter='data')
    archive.unlink()
    # Prepare guide tools before starting the expensive compiler/image builds.
    guide_python = prepare_guide_python(source)
    # Share only downloaded archives and the compiler; all compiled image/driver
    # outputs live in the fresh snapshot, never in the developer's output tree.
    (ROOT / 'dl').mkdir(exist_ok=True)
    (source / 'dl').symlink_to(ROOT / 'dl', target_is_directory=True)
    compiler = Path(os.environ.get('WATCOM', ROOT / 'output/tools/openwatcom')).resolve()
    if 'WATCOM' not in os.environ and not (compiler / 'binl64/wcl').is_file():
        run(sys.executable, 'tools/dos/bootstrap.py')
    environment = dict(os.environ, WATCOM=str(compiler), JOBS=str(jobs))
    # Existing builders invoke python3 by name. Use the interpreter/venv selected
    # for this command, including its guide dependencies.
    bin_dir = work / 'bin'
    bin_dir.mkdir()
    python_command = bin_dir / 'python3'
    python_command.write_text(f'#!/bin/sh\nexec {shlex.quote(sys.executable)} "$@"\n')
    python_command.chmod(0o755)
    environment['PATH'] = str(bin_dir) + os.pathsep + environment.get('PATH', '')

    def build(*command):
        run(*command, cwd=source, env=environment)

    build('bash', 'tools/dos/build.sh')
    # Use the committed copy inside the snapshot, rather than the live checkout.
    bundled_drivers = ROOT / 'assets/windows-drivers/video'
    driver_source = source / 'assets/windows-drivers/video' if drivers == bundled_drivers.resolve() else drivers
    build('bash', 'tools/windows/build-video.sh', driver_source)
    build('bash', 'tools/image/build.sh', 'configure')
    build('bash', 'tools/image/build.sh', 'build')  # Includes image verification.
    build(guide_python, 'docs/pi-dos-user-guide/build-guide.py')

    image = source / 'output/buildroot-arm64/images/pi-dos-arm64-sdcard.img'
    with tempfile.TemporaryDirectory(prefix='.bundle-', dir=OUTPUT) as temporary:
        stage = Path(temporary)
        with (stage / PAYLOADS[0]).open('wb') as compressed:
            # One thread keeps compression memory bounded on smaller build hosts.
            run('xz', '-T1', '-6', '--check=crc64', '--stdout', image, stdout=compressed)
        run('xz', '--test', stage / PAYLOADS[0])
        shutil.copyfile(source / 'docs/pi-dos-user-guide/pi-dos-user-guide.pdf', stage / PAYLOADS[1])
        shutil.copyfile(source / 'output/windows-video/PIDOSDRV.ZIP', stage / PAYLOADS[2])
        if not (stage / PAYLOADS[1]).read_bytes().startswith(b'%PDF-'):
            raise RuntimeError('The guide builder did not produce a PDF.')
        with zipfile.ZipFile(stage / PAYLOADS[2]) as package:
            if package.testzip() or not {'PIDOSDRV/SETUP.EXE', 'PIDOSDRV/PIDOS.CPL',
                                         'PIDOSDRV/PIDINST.DLL'} <= set(package.namelist()):
                raise RuntimeError('Invalid Windows driver ZIP.')
        info = dict(schema=1, commit=commit, version=(source / 'VERSION').read_text().strip(),
                    built_at=datetime.now(timezone.utc).isoformat(),
                    image_sha256=digest(image),
                    buildroot_config_sha256=digest(source / 'output/buildroot-arm64/.config'),
                    sources_lock_sha256=digest(source / 'sources.lock.json'),
                    windows_drivers=json.loads((source / 'output/windows-video/build.json').read_text()),
                    payloads={name: dict(bytes=(stage / name).stat().st_size, sha256=digest(stage / name))
                              for name in PAYLOADS})
        write_json(stage / 'build-info.json', info)
        (stage / 'SHA256SUMS').write_text(''.join(
            f'{digest(stage / name)}  {name}\n' for name in ASSETS if name != 'SHA256SUMS'), encoding='utf-8')
        verify_bundle(stage)
        # Preserve the last successful bundle if any build/package step fails.
        current, previous = OUTPUT / 'current', OUTPUT / 'previous'
        if previous.exists():
            shutil.rmtree(previous)
        if current.exists():
            current.rename(previous)
        stage.rename(current)
    print(f'Bundle ready: {OUTPUT / "current"}', flush=True)
    return info


def verify_bundle(bundle):
    for name in ASSETS:
        path = bundle / name
        if path.is_symlink() or not path.is_file() or not 0 < path.stat().st_size < ASSET_LIMIT:
            raise RuntimeError(f'Missing, empty or oversized asset (must be under 2 GiB): {path}')
    expected = ''.join(f'{digest(bundle / name)}  {name}\n' for name in ASSETS if name != 'SHA256SUMS')
    if (bundle / 'SHA256SUMS').read_text() != expected:
        raise RuntimeError('Bundle checksums do not match; rebuild before uploading.')
    info = json.loads((bundle / 'build-info.json').read_text())
    if info.get('schema') != 1 or not re.fullmatch('[0-9a-f]{40}', info.get('commit', '')):
        raise RuntimeError('Invalid build-info.json; rebuild the bundle.')
    for name in PAYLOADS:
        if info['payloads'][name] != dict(bytes=(bundle / name).stat().st_size, sha256=digest(bundle / name)):
            raise RuntimeError(f'Build metadata does not match {name}.')
    return info


def upload_bundle(repo, info):
    bundle = OUTPUT / 'current'
    commit = clean_commit()
    verified = verify_bundle(bundle)
    if verified != info or commit != info['commit']:
        raise RuntimeError('HEAD changed since this bundle was built; rebuild before uploading.')
    release = upload_preflight(repo, commit)
    # Use a draft for a new release so incomplete first uploads are not published.
    if release is None:
        release = api(repo, 'releases', method='POST', data=dict(
            tag_name=TAG, target_commitish=commit, name='Pi-DOS rolling release', draft=True))
    run('gh', 'release', 'upload', TAG, *(bundle / name for name in ASSETS),
        '--repo', f'github.com/{repo}', '--clobber')
    # Confirm GitHub received the full set before advancing the tag/notes.
    assets = api_items(repo, f'releases/{release["id"]}/assets')
    uploaded = {asset['name']: asset for asset in assets}
    for name in ASSETS:
        asset = uploaded.get(name)
        if not asset or asset.get('state') != 'uploaded' or asset['size'] != (bundle / name).stat().st_size:
            raise RuntimeError(f'GitHub upload incomplete: {name}; rerun with --upload-only.')
        if asset.get('digest') and asset['digest'] != 'sha256:' + digest(bundle / name):
            raise RuntimeError(f'GitHub checksum mismatch: {name}; rerun with --upload-only.')
    ref = api(repo, f'git/ref/tags/{TAG}', missing_ok=True)
    if ref is None:
        api(repo, 'git/refs', method='POST', data=dict(ref=f'refs/tags/{TAG}', sha=commit))
    elif ref['object']['sha'] != commit:
        api(repo, f'git/refs/tags/{TAG}', method='PATCH', data=dict(sha=commit, force=True))
    notes = (f'Locally built Pi-DOS {info["version"]}. This rolling release is updated in place.\n\n'
             f'Source commit: {commit}\n\nBuilt at (UTC): {info["built_at"]}\n\n'
             '- `pi-dos-arm64-sdcard.img.xz`: fresh Raspberry Pi ARM64 SD-card image.\n'
             '- `pi-dos-user-guide.pdf`: user guide built from the same sources.\n'
             '- `PIDOSDRV.ZIP`: Windows 3.1/3.11 drivers and configuration installer.\n'
             '- `build-info.json`: source revision and build/input hashes.\n'
             '- `SHA256SUMS`: download checksums; verify with `sha256sum -c SHA256SUMS`.\n\n'
             'Extract the image with `xz -dk pi-dos-arm64-sdcard.img.xz` before writing it, '
             'or use an image writer that accepts XZ.\n')
    updated = api(repo, f'releases/{release["id"]}', method='PATCH', data=dict(
        name=f'Pi-DOS {info["version"]} — rolling release', body=notes,
        target_commitish=commit, draft=False, prerelease=False, make_latest='true'))
    print(f'Updated release: {updated["html_url"]}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--drivers', type=Path, default=ROOT / 'assets/windows-drivers/video',
                        help='optional override for bundled S3/ and ET4/ drivers')
    parser.add_argument('--jobs', type=int, default=int(os.environ.get('JOBS', '4')))
    parser.add_argument('--repo', help='GitHub OWNER/REPO (default: origin)')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--upload', action='store_true', help='build fresh artifacts, then update the rolling release')
    mode.add_argument('--upload-only', action='store_true', help='upload/retry the verified current bundle without rebuilding')
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    with (OUTPUT / 'release.lock').open('w') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError('Another local release procedure is already running.') from None
        commit = clean_commit()
        uploading = args.upload or args.upload_only
        repo = repository(args.repo) if uploading else None
        if uploading:
            upload_preflight(repo, commit)
        if args.upload_only:
            info = verify_bundle(OUTPUT / 'current')
        else:
            drivers = args.drivers.expanduser().resolve()
            build_preflight(drivers, args.jobs)
            info = build_bundle(commit, drivers, args.jobs)
        if uploading:
            upload_bundle(repo, info)


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.CalledProcessError, tarfile.TarError,
            zipfile.BadZipFile) as error:
        sys.exit(f'Release failed: {error}\nBuild logs, if started: {OUTPUT / "work/source/output/logs"}')
