# Pi-DOS

Turn a Raspberry Pi into a dedicated DOS computer.

Pi-DOS boots straight to a DOS-compatible command prompt, ready for games,
applications, programming tools and demoscene productions. It combines the
DOSBox-X emulation engine with a small Linux system built specifically for the
Raspberry Pi. You work in DOS from startup to shutdown.

**Current system version: 1.1.**

**Development status:** basic boot and input have been confirmed on Pi 3, Pi 4
and Pi 5. The shared image targets more boards, but those still need hardware
testing. Peripheral and application compatibility varies; see
[what has been verified](docs/pi-dos-user-guide/pi-dos-user-guide.md#appendix-e-compatibility-and-verification-notes).

## Connect real hardware

Use a physical CRT monitor, insert a floppy disk or install a DOS program from
an actual CD. Pi-DOS connects these peripherals to the emulated DOS computer:

| Hardware | What you can do |
| --- | --- |
| **VGA CRT through an HDMI-to-VGA DAC** | Connect the Pi's HDMI output to a real VGA CRT using a suitable active digital-to-analogue adapter. CRT mode follows DOS display modes and refresh rates, including VGA text near 70 Hz. The monitor and DAC must accept the selected timings. |
| **External USB floppy drive** | Access physical disks as A:, copy files, write to supported writable media and format supported disks. Capacity and format support depend on the drive. |
| **External USB CD/DVD drive** | Access data CDs as D:, run DOS installers and change physical discs without manual mounting. The optical integration targets CDs, including when using DVD-drive hardware. |
| **CD audio** | Play audio tracks from physical CDs in compatible USB optical drives and from supported disc images. Audio is extracted digitally into the Pi-DOS mixer and sent to the selected HDMI, analogue or USB audio output. Physical-disc and image audio have separate volume controls. |

The CRT appearance comes from the monitor itself. You can also use an LCD with
a fixed output resolution, aspect controls and optional smoothing. USB sticks,
external disks, mice, gamepads and external MIDI interfaces are supported too.

USB floppy copying and formatting have been confirmed with specific drives.
Physical optical-drive, mixed-mode and CD-audio compatibility still need
validation with individual drives and discs; see the
[media guide](docs/pi-dos-user-guide/pi-dos-user-guide.md#chapter-8-using-disks-and-removable-media).

## The idea

Pi-DOS is for people who want to sit down at a DOS machine: type commands,
install software, tune the hardware settings and run a program. The prompt is
the starting point, and your files and configuration stay on the card.

Linux handles the Raspberry Pi hardware behind the scenes. There is no desktop,
login screen or Linux shell in the image. Configuration happens through a DOS
setup utility and ordinary text files. The aim is a focused, self-contained
computer that remains useful offline.

Compatibility comes from emulating a PC. Pi-DOS supplies its own DOS-compatible
environment through DOSBox-X; it does not require an MS-DOS installation.
CPU speed, memory, video and sound can be adjusted to suit different eras of
software. Those settings describe the emulated computer, and performance depends
on the Pi and the program.

## What you can do

- **Run DOS software.** Use familiar commands, COM and EXE programs, BAT scripts,
  real- and protected-mode applications, and DOS extenders. Windows 3.x can also
  run with your own installation media and the bundled driver package. An optional native
  Windows configuration applet manages display modes, installs supported sound
  drivers and provides live volume controls.
- **Choose the PC hardware.** Select CPU profiles from 286 through Pentium,
  configure DOS memory, and choose video adapters from CGA and Hercules through
  VGA and SVGA/VESA. An ARM64 dynamic recompiler speeds up x86 execution.
- **Use a CRT or LCD.** CRT mode follows DOS resolutions and refresh rates for
  suitable displays and adapters. LCD mode uses a fixed resolution with 4:3 or
  original-pixel aspect, optional smoothing and an optional CPU/temperature
  overlay. CRT mode is intended for a physical CRT; it does not add scanline effects.
- **Set up classic sound and music.** PC speaker, AdLib, Sound Blaster and Gravis
  UltraSound are available. MIDI options include SoundFont synthesis, Munt-based
  MT-32/CM-32L emulation and external USB MIDI. SC-55 and Roland GM SoundFonts
  and MT-32/CM-32L ROMs are included; some patches must be supplied separately.
  Audio can use HDMI, a supported Pi's analogue jack or a USB DAC.
- **Keep and move your files.** C: is persistent FAT32 storage, accessible from
  another computer. Pi-DOS also manages USB data volumes, floppy and optical
  drives. Mount disk/CD images or run from a ZIP with saves kept in a separate
  folder beside the archive. Supported CD images can also provide CD audio.
- **Configure everything from DOS.** `PIDOSCFG` covers CPU, memory, video, sound,
  mixer, input, networking and system settings. USB controllers, screenshots and
  shortcuts for switching multi-disc image sets are supported. `EDIT` opens the
  editor; `DZ` opens Doszip Commander. `DEMOS` and `BENCH` launch the included
  demoscene and benchmark menus.
- **Connect older software.** Optional networking provides IPX-over-UDP
  multiplayer and an emulated NE2000 adapter with NAT or direct wired LAN
  access. mTCP tools are included. Networking starts disabled; Windows for
  Workgroups file sharing requires additional software and compatible peers.

These features are implemented to different levels of hardware validation.
The [user guide](docs/pi-dos-user-guide/pi-dos-user-guide.md) covers their setup
and limitations.

## System requirements

You need a Raspberry Pi with a **64-bit Arm processor**, a suitable power supply,
a system card, an HDMI display or suitable adapter, and a wired USB keyboard.
A USB mouse is useful for applications that support one.

| Board family | Targets of the shared ARM64 image |
| --- | --- |
| Pi 2 / Pi 3 | Pi 2B revision 1.2; Pi 3B, 3B+ and 3A+ |
| Zero | Zero 2 W |
| Pi 4 | Pi 4B and Pi 400 |
| Pi 5 | Pi 5, Pi 500 and Pi 500+ |
| Compute Modules | CM0, CM3/3+, CM4/4S and CM5/Lite, with suitable carrier and boot hardware |

Pi 1, the original Zero/Zero W and Pi 2B revision 1.1 cannot run this image.
Zero 2 W and Compute Modules need suitable USB host adapters or carrier hardware.
The board list describes build targets; confirmed basic boot/input currently
covers Pi 3, Pi 4 with corrected firmware, and Pi 5.

Choose a card larger than the generated image, with enough extra space for your
software and disk images. The `PI-DOS` data partition is designed to expand into
remaining card space on first boot. There is no separately established minimum
physical RAM figure; the default **16 MiB of emulated DOS memory** is separate
from the Pi's own RAM.

For a VGA CRT, use a suitable HDMI-to-VGA DAC and a monitor that accepts the
configured timings. External USB floppy and CD/DVD drives are optional; their
power needs may require a powered USB hub.

Wired Ethernet is optional, for networking and automatic time synchronization.
Wi-Fi and Bluetooth are not configured. This image uses HDMI output; DSI and
composite displays are outside its current scope. CRT operation requires a
monitor and adapter that accept the selected timings.

## Build the SD-card image

### 1. Prepare a Linux build host

Build as an ordinary user, from the repository root. You need **Python 3.12 or
newer**, a C/C++ compiler, GNU Make, Bash and the usual Buildroot host tools.
`bsdtar` is also required to extract the bundled benchmark assets.

On Debian/Ubuntu, the host packages include:

```sh
sudo apt-get update
sudo apt-get install build-essential bash bc bison bzip2 ca-certificates \
  cpio file flex git libarchive-tools libncurses-dev libssl-dev patch perl \
  pkg-config python3 python3-venv rsync unzip wget xz-utils
python3 --version
```

Check that the reported Python version is at least 3.12; older distributions may
need a newer Python installation available as `python3`. Buildroot checks its
remaining host dependencies when the build starts. Internet access is needed
for the initial source, toolchain, benchmark and demo downloads.

### 2. Verify the MIDI assets

The SC-55 and Roland GM SoundFonts and all four MT-32/CM-32L ROMs are included
in Git under `assets/midi/`. No separate MIDI asset import is needed for a
fresh clone. Verify all six files against their pinned sizes and hashes with:

```sh
python3 tools/dos/midi-assets.py
```

These inputs are required even if you intend to disable MIDI at runtime.
See [the MIDI asset guide](assets/midi/README.md) for the file layout and
optional reimport instructions.

### 3. Configure and build

From the repository root:

```sh
make image-config
JOBS=4 make image
```

`image-config` selects the shared Raspberry Pi ARM64 configuration. The build
fetches the pinned cross-toolchain and sources, builds the kernel and runtime,
packages the DOS tools and assets, and checks the resulting image. You do not
need to install an ARM cross-compiler or a DOS compiler separately.

The SD-card image is written to:

```text
output/buildroot-arm64/images/pi-dos-arm64-sdcard.img
```

The main build log is `output/logs/buildroot-arm64.log`; the image check report is
`output/buildroot-arm64/images/pi-dos-image-validation.json`.

To resume a build after fixing a dependency or download problem, rerun
`JOBS=4 make image`. Use a lower value, such as `JOBS=2`, if the host is short on
memory. Reapplying `make image-config` resets the configuration to the tracked
defaults, so it is not needed for an ordinary incremental build.

For boot troubleshooting, build the diagnostic variant:

```sh
JOBS=4 make image-diagnostic
```

It produces `pi-dos-arm64-diagnostic-sdcard.img` in the same image directory,
with boot, runtime, network and video logs enabled under `C:\SYSTEM`.
See the [image build guide](docs/buildroot-pi5.md) for configuration details,
component builds and diagnostics.

### 4. Write the card and boot

1. Back up the destination card. Use an image-writing utility to write the
   generated `.img` file to the **whole card**. Writing it replaces existing data;
   running `make image` itself does not touch a physical card.
2. Safely eject the card, insert it into the Pi, and connect the display and USB
   keyboard. Use HDMI0 on dual-HDMI boards when relying on default HDMI audio.
3. Apply power and allow the first-boot storage preparation to finish.
4. In the welcome screen, select your keyboard, country, timezone and CRT/LCD
   profile. Press **F2** to save and reboot, then complete the keyboard and clock
   review to reach the DOS prompt.

Fresh images default to a Spanish keyboard, Spain, `Europe/Madrid` and CRT
output. Choose LCD for a flat panel and select a compatible resolution. If you
skip setup, the saved defaults remain active. You can reopen it later with
`PIDOSCFG /FIRSTBOOT`.

## Everyday use

After a clean `POWEROFF`, move the card to another computer and copy programs
into the FAT32 partition labelled `PI-DOS`. Its root becomes `C:\`:

| DOS path | Purpose |
| --- | --- |
| `C:\GAMES` | Games |
| `C:\PROGRAMS` | Applications and tools |
| `C:\DEMOS` | Demoscene pack |
| `C:\DOCS` | Manuals and your documents |
| `C:\SYSTEM` | Pi-DOS configuration and system files |
| `C:\TEMP` | Temporary files |

Keep the partition's `PI-DOS` label: the system uses it to find your files.
Z: contains the built-in utilities. USB floppy drives use A:, optical drives
use D:, and supported USB data volumes receive free letters starting at E:.
`IMGMOUNT` handles disk/CD images; `ZIPMOUNT` handles ZIP archives.

For CD audio, choose a physical audio output in PIDOSCFG's System page.
Use the Mixer page's separate CD-image and USB-CD levels to adjust playback;
an optical drive's analogue audio cable is unnecessary.

Type `PIDOSCFG` to change hardware and system settings, save, then use `REBOOT`
to apply them. Advanced settings live in `C:\SYSTEM\PIDOS.CFG`. Put your own
startup commands in `C:\AUTOEXEC.BAT`; supported DOS directives and compatible
drivers can use `C:\CONFIG.SYS`. Use `POWEROFF` for a clean shutdown before
removing power or the card.

## How the pieces fit together

| Building block | Its job |
| --- | --- |
| **Buildroot** | Builds and packages the small Linux system, libraries, firmware and bootable image. Board-specific recipes live in `br2-external/`. |
| **Linux kernel and initramfs** | Supply the Pi's hardware drivers and an in-memory system filesystem. The kernel contains the initramfs; there is no separate Linux root partition. |
| **Pi-DOS supervisor** | The C program in `src/init/` starts as PID 1, discovers storage and devices, reads configuration, launches the DOS runtime and synchronizes storage during reboot/shutdown. |
| **DOSBox-X-derived runtime** | Emulates the x86 CPU, PC hardware, BIOS and DOS services. Pi-DOS maintains its integration changes as a patch series in `patches/dosbox-x/`. |
| **SDL2 and hardware libraries** | Connect the runtime to Linux input, direct DRM/KMS video with OpenGL ES presentation, and ALSA audio. X11, Wayland and an audio server are unnecessary. |
| **Persistent DOS storage** | A separate FAT32 partition holds C:, configuration and user files, apart from the system loaded into memory. |

At boot, the Pi firmware loads the kernel from the `BOOT` partition. The kernel
starts the supervisor, which mounts the `PI-DOS` data partition and launches the
emulator with the saved settings. DOS programs see emulated PC devices; Linux
and SDL2 translate their display, sound and input into real Pi hardware access.
The supervisor remains in charge of the runtime and clean shutdown.

## Documentation and development

- [User guide](docs/pi-dos-user-guide/pi-dos-user-guide.md) — installation,
  commands, configuration, storage and troubleshooting.
- [Image build guide](docs/buildroot-pi5.md) — image layout, diagnostics and
  hardware integration; the filename predates the shared ARM64 image.
- [Local release procedure](docs/releases.md) — build a fresh compressed SD
  image, PDF guide and Windows driver ZIP, and update one GitHub release.
- [Setup utility](src/doscfg/README.md) — settings, CPU profiles and rebuilding
  `PIDOSCFG` with Open Watcom.
- [Networking](docs/networking.txt), [CONFIG.SYS](docs/config-sys.md) and
  [Windows 3.1](docs/windows31.md) — detailed setup and compatibility notes.
- [Technical reference](docs/technical-reference.md) — deeper architecture,
  implementation details and recorded validation.

For runtime development on Linux, `make phase0` builds a native SDL2/DOSBox-X
baseline without creating an SD image. Its host dependencies and checks are
covered in the [baseline findings](docs/phase0.md). `make help` lists the available
build targets.

## Licensing

A project-wide license for original Pi-DOS code has not yet been selected.
Upstream components and bundled software retain their own licenses and notices.
Generated images are development artifacts; public distribution still requires
completion of the component and asset license review. See
[source provenance and licensing](LICENSES/README.md).
