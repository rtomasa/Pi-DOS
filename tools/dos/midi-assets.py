#!/usr/bin/env python3
"""Verify bundled MIDI assets or reimport them from a ScummVM asset directory."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / 'assets/midi'

def verify(path, item):
    if not path.is_file() or path.stat().st_size != item['bytes']:
        raise RuntimeError(f'Missing or wrong-size MIDI asset: {path}')
    with path.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    if digest != item['sha256']:
        raise RuntimeError(f'MIDI asset checksum mismatch: {path}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, help='ScummVM directory containing extra/ and optional soundfonts/')
    parser.add_argument('--install', type=Path, help='Copy verified assets to this directory')
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'configs/midi-assets.json').read_text())
    for item in manifest['files']:
        asset = ASSETS / item['destination']
        if args.source:
            source = args.source / item['source']
            if not source.exists() and item['source'].startswith('soundfonts/'):
                source = asset  # Use the bundled SoundFont when no replacement is supplied.
            verify(source, item)
            asset.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(dir=asset.parent, delete=False) as stream:
                temporary = Path(stream.name)
            try:
                shutil.copyfile(source, temporary)
                verify(temporary, item)
                temporary.replace(asset)
                asset.chmod(0o644)
            finally:
                temporary.unlink(missing_ok=True)
        verify(asset, item)
        if args.install:
            target = args.install / item['destination']
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(asset, target)
            target.chmod(0o644)
    print('Verified six MIDI assets' + (f'; installed in {args.install}' if args.install else f' in {ASSETS}'))

if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError) as error:
        raise SystemExit(f'{error}\nImport with: python3 tools/dos/midi-assets.py --source /path/to/scummvm')
