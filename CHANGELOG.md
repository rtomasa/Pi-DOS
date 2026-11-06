# Changelog

## Unreleased

- **Apply settings:** Use **F4** in DOS `PIDOSCFG` to apply changes without
  rebooting the Raspberry Pi. Some hardware changes require restarting DOS.
- **Turbo mode:** Choose a faster speed for your CPU and toggle it with
  **Ctrl+Alt+T**. A brief notification appears on both CRT and LCD screens.
  Turbo settings are also available in the Windows configuration applet.
- **Clearer setup:** Rename **Cancel** to **Exit** and clarify unsaved changes.
- **Storage feedback:** Show disk-expansion progress and improve recovery
  diagnostics when the DOS drive is unavailable.

## 1.1 — 5 November 2026

Changes since 1.0:

- **Windows 3.1/3.11 setup:** Add a unified installer and Pi-DOS Configuration
  applet, with video and sound driver setup, display options, and MIDI settings.
  Support upgrades, reinstall and uninstall.
- **Better volume controls:** Add a live Windows mixer and separate controls for
  CD images and USB CD audio. Keep volume settings when changing discs.
- **CPU recommendations:** Select sensible defaults for each Raspberry Pi family
  and highlight the recommended profile in DOS setup. Keep saved preferences.
- **LCD monitoring:** Add an optional CPU-load and temperature overlay.
- **DOS compatibility:** Read supported `CONFIG.SYS` settings and preserve user
  edits. Leave more room in the default `PATH` for installed programs.
- **Faster CD access:** Improve physical CD reading with caching.
- **More reliable input and captures:** Fix mouse stalls and capture after video
  mode changes. Improve screenshot shortcuts, saving and numbering.
- **File and setup fixes:** Correct wildcard deletion, filename handling and disk
  write error reporting. Fix DOS setup startup and Windows 3.11 floppy formatting.
- **Ready-to-use MIDI:** Include the required SoundFonts and MT-32/CM-32L ROMs
  in fresh builds.
- **Easier releases:** Package the SD image, Windows drivers, PDF user guide and
  checksums together. Expand the user guide and installation documentation.
