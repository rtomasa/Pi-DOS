#!/usr/bin/env python3
"""Run live mixer checks in the ARM64 DOS runtime with both internal MIDI engines."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdl-library-path', type=Path, required=True)
    args = parser.parse_args()
    runtime = next((ROOT / 'output/buildroot-arm64/build').glob('pi-dos-runtime-*')) / 'src/dosbox-x'
    watcom = Path(os.environ.get('WATCOM', ROOT / 'output/tools/openwatcom'))
    env = dict(os.environ, WATCOM=str(watcom), INCLUDE=str(watcom / 'h'),
               PATH=str(watcom / 'binl64') + os.pathsep + os.environ['PATH'],
               SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    for key in ('PI_DOS_SUPERVISED', 'PI_DOS_SYSTEM_STORAGE', 'PI_DOS_USB_FLOPPY',
                'PI_DOS_USB_CDROM', 'PI_DOS_USB_STORAGE', 'LD_PRELOAD'):
        env.pop(key, None)
    with tempfile.TemporaryDirectory(prefix='pidos-volume-') as temporary:
        work = Path(temporary)
        (work / 'AUDIO.BIN').write_bytes(bytes(2352 * 75))
        (work / 'AUDIO.CUE').write_text('FILE "AUDIO.BIN" BINARY\n  TRACK 01 AUDIO\n    INDEX 01 00:00:00\n')
        sources = ['tools/windows/volume-check.c', 'src/winvideo/volume_state.c',
                   'src/doscfg/config.c', 'src/doscfg/forms.c', 'src/doscfg/live.c', 'src/common/cpu_profiles.c',
                   'src/common/video_profiles.c', 'src/common/midi_profiles.c']
        subprocess.run([str(watcom / 'binl64/wcl'), '-q', '-bt=dos', '-l=dos', '-0', '-ml',
                        '-os', '-s', '-wx', '-i=' + str(ROOT / 'src/common'),
                        '-i=' + str(ROOT / 'src/doscfg'), '-i=' + str(ROOT / 'src/winvideo'),
                        '-fe=' + str(work / 'VOLUME.EXE'), *[str(ROOT / s) for s in sources]],
                       cwd=work, env=env, check=True)
        for engine, channel in (('synth', 'SYNTH'), ('mt32', 'MT32')):
            original = b'; user settings\r\n[dosbox]\r\nmemsize=24\r\n[pidos]\r\nmixer_fm=60:70\r\n'
            (work / 'PIDOS.CFG').write_bytes(original)
            (work / 'CHECK.BAT').write_text(
                '@ECHO OFF\nZ:\\SYSTEM\\PICDROM.COM\n'
                'Z:\\SYSTEM\\MIXER.COM /NOSHOW CDAUDIO 55:65 USBCD 45:55\n'
                'IMGMOUNT E C:\\AUDIO.CUE -T ISO\n'
                'Z:\\SYSTEM\\MIXER.COM /NOSHOW MASTER 70:80 FM 60:70 ' + channel + ' 75:85\n'
                'VOLUME.EXE >RESULT.TXT\nZ:\\SYSTEM\\MIXER.COM >LEVELS.TXT\nEXIT\n', encoding='ascii')
            env['PI_DOS_USB_CDROM'] = '1'
            command = [str(ROOT / 'output/tools/qemu-user/usr/bin/qemu-aarch64'), '-L',
                       str(ROOT / 'output/buildroot-arm64/target'), '-E',
                       'LD_LIBRARY_PATH=' + str(args.sdl_library_path.resolve()), str(runtime),
                       '-defaultconf', '-defaultmapper', '-nopromptfolder', '-defaultdir', str(work),
                       '-fastlaunch', '-nomenu', '-nogui', '-time-limit', '15']
            for setting in ('sdl output=surface', 'sdl videodriver=dummy', 'sdl waitonerror=false',
                            'cpu core=normal', 'cpu cycles=fixed 20000', 'cpu integration device=true',
                            'sblaster sbtype=sbpro2', 'sblaster oplmode=opl3',
                            'midi mididevice=' + engine,
                            'midi midiconfig=' + str(ROOT / 'assets/midi/SC55.SF2'),
                            'midi mt32.romdir=' + str(ROOT / 'assets/midi/MT32')):
                command += ['-set', setting]
            for dos in ('mount c "' + str(work) + '"', 'c:', 'CHECK.BAT'):
                command += ['-c', dos]
            with (work / 'runtime.log').open('w') as log:
                result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=40)
            output = (work / 'RESULT.TXT').read_text(errors='replace') if (work / 'RESULT.TXT').exists() else ''
            assert result.returncode == 0 and 'PASS: live master/FM/MIDI' in output, output + (work / 'runtime.log').read_text()[-6000:]
            saved = (work / 'PIDOS.CFG').read_bytes()
            assert original in saved and b'mixer_master=55:55' in saved and b'mixer_midi=45:45' in saved
            assert b'mixer_cd=20:20' in saved and b'mixer_usbcd=30:30' in saved
            assert (work / 'PIDOS.BAK').read_bytes() == original
            (work / 'RESULT.TXT').unlink()
            print('PASS:', engine, 'live volume including image/USB CD, mute, stereo Cancel, saved settings and 100% maximum')


if __name__ == '__main__':
    main()
