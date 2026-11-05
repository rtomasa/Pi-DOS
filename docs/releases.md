# Local rolling release

The tracked procedure `tools/release/release.py` builds the SD-card image,
user guide and Windows 3.x drivers locally. It can publish them to one GitHub
release with the fixed tag `rolling`. Repeating the upload updates that release
and its assets; it does not create a release per version or execution.

## One-time setup

Use the Linux x86-64 build host and dependencies described in the
[README](../README.md#build-the-sd-card-image), including Python 3.12 or newer,
`bsdtar`, `unzip` and `xz`. Allow enough disk space and time for a complete
Buildroot build, including a new cross-toolchain, on every execution.

Install the guide dependencies in a local environment:

```sh
python3 -m venv output/release-venv
. output/release-venv/bin/activate
python3 -m pip install -r docs/pi-dos-user-guide/requirements.txt
```

On Debian/Ubuntu, install `python3-venv` if environment creation needs it.
Activate this environment again in subsequent shells before running the
procedure. The guide also supports a Typst CLI installation instead of the
Python `typst` package; `markdown-it-py` is still required.

Video driver inputs and all five original audio driver archives are tracked in
`assets/windows-drivers/` and hash-checked during packaging. A fresh clone needs
no external driver folder or driver downloads. See the
[driver input inventory](../assets/windows-drivers/README.md) and
[Windows guide](windows31.md). Windows installation media remain user-supplied.
Open Watcom is bootstrapped to
`output/tools/openwatcom` when missing; `WATCOM=/path/to/openwatcom` selects an
existing Linux x64 installation.

For uploading, install a current [GitHub CLI](https://cli.github.com/) and log in:

```sh
gh auth login --hostname github.com
```

The account needs permission to write repository contents, releases and the
`rolling` tag. A fine-grained token needs **Contents: read and write** on the
target repository. For unattended execution, GitHub CLI also accepts `GH_TOKEN`
in the environment. Keep credentials in GitHub CLI's credential storage or the
process environment.

The repository must permit mutable releases and updates to the `rolling` tag.
Keep **release immutability disabled** for this workflow: immutable releases
prevent replacing assets and moving their tags. The procedure rejects an
existing immutable release and does not change repository protection settings.
See [GitHub CLI's immutability documentation](https://cli.github.com/manual/gh_release_create#immutable-releases).

## Build locally

Commit your intended source changes first. The working tree must be clean,
including untracked source files, because the build uses a snapshot of committed
`HEAD`. Ignored files under `output/` and `dl/` do not make it dirty.

```sh
JOBS=4 make release
```

This command makes no GitHub requests. It:

1. Exports committed sources into a fresh `output/release/work/source` tree.
2. Rebuilds `PIDOSCFG.EXE` and the unified Windows 3.1/3.11 driver installer.
3. Configures the standard ARM64 image from the tracked defconfig and builds it
   from scratch, including the existing image verification.
4. Regenerates the guide from Markdown and compiles its PDF.
5. Compresses the image with XZ, checks the XZ stream and driver ZIP, and writes
   build metadata and SHA-256 checksums.

Downloaded archives in `dl/` and the Open Watcom compiler can be reused. Compiled
Buildroot outputs are deliberately rebuilt. Your normal `output/buildroot-arm64`
tree and tracked DOS binaries/guide Typst source are not modified by these builds.
The release image contains fresh default storage, not data copied from a used SD
card or your development image staging directories.

The completed bundle is in `output/release/current/`:

| Asset | Contents |
| --- | --- |
| `pi-dos-arm64-sdcard.img.xz` | Compressed standard ARM64 SD-card image |
| `pi-dos-user-guide.pdf` | User guide built from the same source revision |
| `PIDOSDRV.ZIP` | Unified Windows 3.1/3.11 driver and configuration installer |
| `build-info.json` | Commit, version, UTC build time, input and artifact hashes |
| `SHA256SUMS` | Checksums for the four assets above |

The previous successful bundle is retained in `output/release/previous/`.
An unsuccessful build leaves `current/` intact. The build snapshot and logs stay
under `output/release/work/`; the next build replaces that work directory.
Only one invocation can run at a time in this checkout. Avoid simultaneous
development builds that download into the shared `dl/` cache.

Inspect the PDF and the image verification report in
`output/release/work/source/output/buildroot-arm64/images/pi-dos-image-validation.json`
as needed. Host checks do not establish successful booting on physical hardware.

Verify downloaded or locally generated assets with:

```sh
cd output/release/current
sha256sum -c SHA256SUMS
```

## Build and upload automatically

Push your source commit normally, then run:

```sh
git push
JOBS=4 make release-upload
```

The procedure checks GitHub access and that `HEAD` is already on the target
repository before starting the build. After building, it checks the bundle and
source revision again, creates `rolling` as a draft if necessary, uploads the
five assets, verifies their remote sizes/hashes when provided by GitHub, advances
the remote `rolling` tag to the source commit, and publishes/updates the same
release as **Latest**. It also replaces the release notes with the build details.

The target repository is inferred from `origin` (currently `rtomasa/Pi-DOS`).
For a fork or another target, pass `RELEASE_REPO=OWNER/REPO` to the Make target,
or `--repo OWNER/REPO` directly to the script. It never pushes a source branch,
commits your changes, updates your local tags or deletes other releases/assets.

After a local build, or to retry an interrupted upload without rebuilding:

```sh
make release-upload-only
```

This uses only `output/release/current/`, verifies its checksums and requires the
same clean `HEAD` that produced it. A build or checksum failure stops before
uploading. [Asset replacement uses `gh release upload --clobber`](https://cli.github.com/manual/gh_release_upload):
GitHub deletes a same-named asset before uploading its replacement, so updating
an existing release is not atomic. An interruption can leave missing or mixed
assets; rerun `release-upload-only` to complete the update. Tag/notes updates
occur after asset verification. Run uploads from only one machine at a time.

Stable download links use the tag, for example:

```text
https://github.com/rtomasa/Pi-DOS/releases/download/rolling/pi-dos-arm64-sdcard.img.xz
https://github.com/rtomasa/Pi-DOS/releases/download/rolling/pi-dos-user-guide.pdf
https://github.com/rtomasa/Pi-DOS/releases/download/rolling/PIDOSDRV.ZIP
```

[GitHub requires each asset to be under 2 GiB](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases#storage-and-bandwidth-quotas).
The procedure enforces that limit before uploading.

## What belongs in Git

Track the procedure, Make targets and documentation. Generated bundles, build
trees, logs and the optional virtual environment stay under `output/`, already
covered by `.gitignore`. The ordinary generated guide PDF remains ignored too.
There is no need to commit release binaries or add broad `*.zip`/`*.pdf` ignore
rules. Required vendor driver inputs are tracked under `assets/windows-drivers/`,
outside the disposable build output. `DRIVERS=/path/to/DRIVERS` is available only
as an optional override for different video inputs.

Public distribution remains subject to the project's existing
[component and asset license review](../LICENSES/README.md); this packaging
procedure does not complete that review or select a project license.
