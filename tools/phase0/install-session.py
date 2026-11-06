#!/usr/bin/env python3
"""Install shared session-control sources into a DOSBox-X source tree."""
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[2]
target = Path(sys.argv[1])
settings = target / 'src/misc/pidos-config'
settings.mkdir(parents=True, exist_ok=True)
for name in ('config.c', 'config.h'):
    shutil.copyfile(root / 'src/doscfg' / name, settings / name)
for name in ('cpu_profiles.c', 'cpu_profiles.h', 'video_profiles.c', 'video_profiles.h',
             'midi_profiles.c', 'midi_profiles.h', 'input_options.h', 'network_options.h'):
    shutil.copyfile(root / 'src/common' / name, settings / name)
shutil.copyfile(root / 'src/runtime/session_control.h', target / 'src/misc/pidos_session_control.h')
shutil.copyfile(root / 'src/runtime/host_hud.h', target / 'src/output/pidos_hud.h')
shutil.copyfile(root / 'src/runtime/turbo_notice.h', target / 'src/output/pidos_turbo_notice.h')
