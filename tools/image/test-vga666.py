#!/usr/bin/env python3
"""Check VGA666 BOOT transactions, DRM discovery and all board overlay chains."""
import ctypes
import importlib.util
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('vga666', ROOT/'tools/image/vga666.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)
board = ROOT/'br2-external/board/raspberrypi'
kernel = ROOT/'output/buildroot-arm64/build/linux-custom'

with tempfile.TemporaryDirectory(prefix='pi-dos-vga666.') as directory:
    work = Path(directory)
    library = work/'video-boot.so'
    subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
                    str(ROOT/'src/init/video_boot.c'), str(ROOT/'src/init/config.c'),
                    '-I'+str(ROOT/'src/common'), '-o', str(library)], check=True)
    video = ctypes.CDLL(str(library))
    video.pi_dos_video_boot_write.argtypes = [ctypes.c_char_p, ctypes.c_bool]
    video.pi_dos_video_boot_write.restype = ctypes.c_bool
    video.pi_dos_video_card.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_bool]
    video.pi_dos_video_card.restype = ctypes.c_int
    video.pi_dos_vga666_active.argtypes = [ctypes.c_char_p]
    video.pi_dos_vga666_active.restype = ctypes.c_bool
    boot = work/'boot'
    (boot/'overlays').mkdir(parents=True)
    config = (board/'config.txt').read_bytes()
    (boot/'config.txt').write_bytes(config)
    managed = boot/'pidos-video.txt'
    initial = (board/'pidos-video.txt').read_bytes()
    managed.write_bytes(initial)
    assert not video.pi_dos_video_boot_write(str(boot).encode(), True)  # Missing new BOOT assets.
    assert managed.read_bytes() == initial
    (boot/'overlays/vc4-kms-vga666.dtbo').write_bytes(b'overlay fixture')
    (boot/'pidos-vga666.txt').write_text(assets.boot_arguments((board/'cmdline.txt').read_text()))
    assert video.pi_dos_video_boot_write(str(boot).encode(), True)
    enabled = managed.read_bytes()
    assert b'enable_uart=0\n' in enabled and b'dtoverlay=vc4-kms-vga666\n' in enabled
    assert b'cmdline=pidos-vga666.txt\n' in enabled
    assert video.pi_dos_vga666_active(str(boot/'pidos-vga666.txt').encode())
    assert video.pi_dos_video_boot_write(str(boot).encode(), True)  # No-op.
    (boot/'pidos-video.new').write_bytes(b'interrupted write')
    assert not video.pi_dos_video_boot_write(str(boot).encode(), False)
    assert managed.read_bytes() == enabled
    (boot/'pidos-video.new').unlink()
    assert video.pi_dos_video_boot_write(str(boot).encode(), False)
    assert managed.read_bytes() == initial and (boot/'config.txt').read_bytes() == config
    managed.write_bytes(b'# custom user file\n')
    assert not video.pi_dos_video_boot_write(str(boot).encode(), True)
    assert managed.read_bytes() == b'# custom user file\n'

    sysroot, devroot = work/'drm', work/'dri'
    sysroot.mkdir(); devroot.mkdir()
    for card, connector, status in ((0, 'HDMI-A-1', 'connected'), (2, 'VGA-1', 'connected'), (3, 'VGA-1', 'disconnected')):
        path = sysroot/f'card{card}-{connector}'
        path.mkdir(); (path/'status').write_text(status+'\n')
        (devroot/f'card{card}').touch()
    assert video.pi_dos_video_card(str(sysroot).encode(), str(devroot).encode(), False) == 0
    assert video.pi_dos_video_card(str(sysroot).encode(), str(devroot).encode(), True) == 2
    (devroot/'card2').unlink()
    assert video.pi_dos_video_card(str(sysroot).encode(), str(devroot).encode(), True) == -1

    digital = (ROOT/'dac-crt.bin').read_bytes()
    analog = assets.analog_edid(digital)
    assert not analog[20] & 128 and analog[35:126] == digital[35:126]
    assert not any(sum(analog[i:i+128]) % 256 for i in range(0, len(analog), 128))
    for pos in range(128, len(analog), 128):
        old, new = digital[pos:pos+128], analog[pos:pos+128]
        assert new[new[2]:new[2]+127-old[2]] == old[old[2]:127]  # All custom CRT timings survive.
        assert not new[3] & 0x70
    arguments = assets.boot_arguments((board/'cmdline.txt').read_text())
    assert 'video=VGA-1:720x400@70e' in arguments and 'console=serial0,' not in arguments
    assert assets.boot_arguments(arguments) == arguments
    assert 'loglevel=7' in assets.boot_arguments('loglevel=7 ignore_loglevel\n')

    # Keep each model's existing KMS overlay, then add the shared DPI DAC bridge.
    trees = (board/'device-trees.txt').read_text().splitlines()
    for name in trees:
        if not name.endswith('.dtb'):
            continue
        family = '-pi5' if name.startswith('bcm2712') else '-pi4' if name.startswith('bcm2711') else ''
        base = kernel/'arch/arm64/boot/dts/broadcom'/name
        overlays = kernel/'arch/arm/boot/dts/overlays'
        merged = work/name
        subprocess.run([str(kernel/'scripts/dtc/fdtoverlay'), '-i', str(base), '-o', str(merged),
                        str(overlays/f'vc4-kms-v3d{family}.dtbo'), str(overlays/'vc4-kms-vga666.dtbo')], check=True)
        assert b'dumb-vga-dac\0' in merged.read_bytes()
    count = sum(name.endswith('.dtb') for name in trees)
print(f'PASS: VGA666 BOOT updates/recovery, HDMI/RP1 discovery, analog mode preservation and {count} board overlay chains')
