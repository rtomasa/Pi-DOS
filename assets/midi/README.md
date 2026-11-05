# MIDI build inputs

This directory holds the SoundFonts and ROMs required by the current SD image
build. `SC55.SF2`, `GMROLAND.SF2` and all four MT-32/CM-32L ROMs are bundled
in Git. All these build inputs remain
available when `output/` is cleaned. Their sizes and SHA-256 hashes are pinned
in `configs/midi-assets.json`.

No import is required for a fresh clone. Verify the bundled files with:

```sh
python3 tools/dos/midi-assets.py
```

To reimport the ROMs, use a ScummVM asset directory containing `extra/`:

```sh
python3 tools/dos/midi-assets.py --source /path/to/scummvm
```

The importer uses the bundled SoundFonts unless the source also contains
`soundfonts/Roland_SC-55.sf2` and/or `soundfonts/GM_Roland.sf2`, in which case
those supplied files must match the manifest too. It checks all six assets
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
already has all six MIDI assets required by `make image`.

The bundled SoundFonts were copied unchanged from the maintainer's ScummVM
asset set (`Roland_SC-55.sf2` and `GM_Roland.sf2`). No accompanying license files
were present in that source directory. Their inclusion does not establish
redistribution permission; see [the project licensing notes](../../LICENSES/README.md).
