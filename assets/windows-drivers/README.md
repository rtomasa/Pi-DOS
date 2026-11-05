# Windows 3.x driver build inputs

These tracked files supply the video and audio inputs for `make windows-drivers`,
`make release` and `make release-upload`. No separate driver folder, extracted
Pi-DOS package or driver download is needed for an ordinary build.

## Video

`video/S3/` contains S3 Trio64V / Trio765 Windows drivers, version 1.70.04
(22 September 1997). `video/ET4/` contains the Tseng ET4000 Windows 3.1 drivers.
These are the payloads used by the existing Pi-DOS installer, together with the
vendor OEMSETUP.INF files and available README/VERSION files. They are not
complete original vendor disk archives.

The files were recovered from the retained `output/windows-video/DISK` staging
tree. All 72 driver/font/support payloads match the input hashes in the previous
build manifest. The original external driver directory and complete download
archives were no longer available. `configs/windows-video.json` records the
retained files and their SHA-256 hashes; packaging checks these hashes before
using the bundled inputs. Git preserves all vendor bytes without line-ending
conversion.

## Audio

`audio/` retains the five complete original, unmodified archives already used
by the audio packager: `SBW31.EXE`, `SBP2W31.EXE`, `SB16WU.ZIP`, `GUS411CD.ZIP`
and `SPEAK.EXE`. Their upstream URLs, versions, SHA-256 hashes and device mappings
remain in `configs/windows-audio.json`. Packaging verifies their pinned hashes.
The optional `--fetch` audio command remains available to recover missing
archives into the ignored download cache.

## Build and provenance

Run `make windows-drivers` with the documented host tools installed. An explicit
`DRIVERS=/path/to/DRIVERS` remains available to select different video inputs.
Generated installer binaries and ZIPs remain under ignored `output/`.

Vendor copyrights and notices remain applicable. Bundling these build inputs
does not select a project license or complete the existing public-distribution
review in [LICENSES/README.md](../../LICENSES/README.md). Windows installation
media, including Windows' built-in AdLib driver, remain user-supplied.
