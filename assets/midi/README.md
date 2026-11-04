# MIDI build inputs

This directory holds the user-supplied SoundFonts and ROMs required by the
current SD image build. These are build inputs and remain available when
`output/` is cleaned. Binary payloads are ignored by Git; this guide and
`configs/midi-assets.json` are tracked.

Import the files from a ScummVM asset directory containing `soundfonts/` and
`extra/`:

```sh
python3 tools/dos/midi-assets.py --source /path/to/scummvm
```

The importer checks sizes and SHA-256 hashes against `configs/midi-assets.json`
and creates this layout:

```text
assets/midi/
  SC55.SF2
  GMROLAND.SF2
  MT32/MT32_CONTROL.ROM
  MT32/MT32_PCM.ROM
  CM32L/CM32L_CONTROL.ROM
  CM32L/CM32L_PCM.ROM
```

To migrate files previously staged under `output/midi-assets/`, copy them here
and verify them:

```sh
cp -a output/midi-assets/. assets/midi/
python3 tools/dos/midi-assets.py
```

The existing build hooks verify and install these inputs into the initramfs
and the data partition's `SYSTEM/MIDI` directory. A fresh clone or CI runner
must provision the same inputs before running `make image`.
