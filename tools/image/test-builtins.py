#!/usr/bin/env python3
"""Run actual ARM64 DOS boot programs headlessly; no Pi or device access needed."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--qemu', type=Path, default=ROOT/'output/tools/qemu-user/usr/bin/qemu-aarch64')
parser.add_argument('--sdl-library-path', type=Path, required=True,
                    help='ARM64 SDL2 with dummy video/audio; production KMS-only SDL cannot run headlessly')
parser.add_argument('--setup-exe', type=Path,
                    default=ROOT/'br2-external/board/raspberrypi/dos/PIDOSCFG.EXE')
args = parser.parse_args()
if not args.qemu.is_file() or not (args.sdl_library_path/'libSDL2-2.0.so.0').is_file():
    parser.error('Provide an ARM64 QEMU executable and an ARM64 SDL2 build with dummy backends.')
runtime = ROOT/'output/buildroot-arm64/target/pi-dos/pi-dos'
(ROOT/'output/smoke').mkdir(exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='boot-programs.', dir=ROOT/'output/smoke'))
shutil.copyfile(args.setup_exe, work/'PIDOSCFG.EXE')
# Match the actual mixer calls generated after the welcome message.
channels = re.findall(r'\{"mixer_[a-z]+", "([A-Z0-9]+)"\}', (ROOT/'src/init/mixer.c').read_text())
assert len(channels) == 13
commands = ['mount c "'+str(work)+'"', 'c:', '@echo off', 'echo Welcome to Pi-DOS >BOOT.OUT']
commands += ['SET PATH=C:\\PROGRAMS\\TOOLS;C:\\SYSTEM;Z:\\;Z:\\SYSTEM;Z:\\DOS;Z:\\BIN;Z:\\TEXTUTIL',
             'SET TEMP=C:\\TEMP', 'SET TMP=C:\\TEMP', 'SET BLASTER=A220 I7 D1 T5']
# Exercise startup with a populated DOS environment rather than a minimal shell.
commands += ['SET PIDOS_TEST_ENV%d=' % i + 'x' * 16 for i in range(64)]
for channel in channels:
    level = '70:80' if channel == 'MASTER' else '100:100'
    commands.append('Z:\\SYSTEM\\MIXER.COM /NOSHOW '+channel+' '+level+' >>BOOT.OUT')
commands += ['Z:\\SYSTEM\\MIXER.COM >MIXER.OUT',
             'Z:\\SYSTEM\\CONFIG.COM -get joystick joysticktype >JOY.OUT',
             'PIDOSCFG.EXE /LIST >SETUP.OUT',
             'PIDOSCFG.EXE /PROFILE 386-33 /CONFIG C:\\STARTUP.CFG >SAVE.OUT',
             'echo PI_DOS_BOOT_PROGRAMS_OK >>BOOT.OUT', 'exit']
cmd = [str(args.qemu), '-L', str(ROOT/'output/buildroot-arm64/target'),
       '-E', 'LD_LIBRARY_PATH='+str(args.sdl_library_path.resolve()), str(runtime),
       '-defaultconf', '-defaultmapper', '-nopromptfolder', '-defaultdir', str(work),
       '-fastlaunch', '-nomenu', '-nogui', '-time-limit', '20',
       '-set', 'sdl output=surface', '-set', 'sdl videodriver=dummy',
       '-set', 'sdl waitonerror=false', '-set', 'cpu core=normal',
       '-set', 'cpu cycles=fixed 20000',
       '-set', 'midi mpu401=none', '-set', 'joystick joysticktype=2axis']
# DOSBox-X accepts at most twelve -c switches. Run the full sequence as a BAT.
(work/'BOOT.BAT').write_bytes(('\r\n'.join(commands[2:])+'\r\n').encode('ascii'))
for command in commands[:2]+['BOOT.BAT']:
    cmd += ['-c', command]
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
# This test uses temporary host mounts; appliance drive ownership is not enabled.
for key in ('PI_DOS_SUPERVISED', 'PI_DOS_SYSTEM_STORAGE', 'PI_DOS_USB_FLOPPY',
            'PI_DOS_USB_CDROM', 'PI_DOS_USB_STORAGE'):
    env.pop(key, None)
with (work/'run.log').open('w') as log:
    result = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
assert result.returncode == 0, work/'run.log'
boot = (work/'BOOT.OUT').read_bytes()
assert b'PI_DOS_BOOT_PROGRAMS_OK' in boot and b'Unable to run program' not in boot, boot
assert re.search(rb'MASTER\s+70:80', (work/'MIXER.OUT').read_bytes())
assert b'2axis' in (work/'JOY.OUT').read_bytes()
assert b'Automatic' in (work/'SETUP.OUT').read_bytes()
assert b'Saved' in (work/'SAVE.OUT').read_bytes()
assert b'cputype=386' in (work/'STARTUP.CFG').read_bytes()
(work/'result.json').write_text(json.dumps({
    'passed': True, 'mixer_commands': len(channels),
    'runtime_sha256': hashlib.sha256(runtime.read_bytes()).hexdigest(),
    'scope': 'Actual ARM64 runtime: MOUNT, boot MIXER calls, MIXER query, CONFIG query, PIDOSCFG startup and file saving with a populated DOS environment; dummy SDL/audio, no hardware boot',
}, indent=2)+'\n')
print('PASS: ARM64 boot programs, mixer commands, PIDOSCFG startup and file saving with a populated DOS environment: '+str(work))
