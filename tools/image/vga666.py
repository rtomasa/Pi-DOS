#!/usr/bin/env python3
"""Generate VGA666 boot arguments or an analog EDID from the HDMI mode list."""
import argparse
from pathlib import Path


def analog_edid(source):
    if len(source) < 128 or len(source) % 128 or source[:8] != bytes.fromhex('00ffffffffffff00'):
        raise ValueError('Invalid source EDID')
    if any(sum(source[i:i+128]) % 256 for i in range(0, len(source), 128)):
        raise ValueError('Invalid source EDID checksum')
    result = bytearray(source[:128])
    result[20] = 0x0e  # Analog, separate sync; retain all base timings.
    for start in range(128, len(source), 128):
        original = source[start:start+128]
        if original[0] != 2:
            raise ValueError('Unsupported EDID extension')
        offset = original[2]
        if not 4 <= offset <= 127:
            raise ValueError('Invalid CTA data offset')
        blocks = bytearray()
        pos = 4
        while pos < offset:
            length = (original[pos] & 31) + 1
            if pos + length > offset:
                raise ValueError('Invalid CTA data block')
            if original[pos] >> 5 == 2:  # Video only; omit audio/HDMI metadata.
                blocks.extend(original[pos:pos+length])
            pos += length
        cta = bytearray(128)
        cta[:4] = bytes((2, original[1], 4 + len(blocks), original[3] & 15))
        cta[4:4+len(blocks)] = blocks
        timings = original[offset:127]
        cta[cta[2]:cta[2]+len(timings)] = timings
        cta[127] = (-sum(cta[:127])) & 255
        result.extend(cta)
    result[126] = len(result) // 128 - 1
    result[127] = (-sum(result[:127])) & 255
    return bytes(result)


def boot_arguments(source):
    words = [word for word in source.split()
             if not word.startswith(('video=', 'drm.edid_firmware=', 'console=serial0,', 'pi-dos.vga666='))]
    words += ['pi-dos.vga666=1', 'video=HDMI-A-1:d', 'video=HDMI-A-2:d',
              'video=VGA-1:720x400@70e', 'drm.edid_firmware=VGA-1:edid/vga666.bin']
    return ' '.join(words) + '\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=('edid', 'cmdline'))
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    if args.kind == 'edid':
        args.destination.write_bytes(analog_edid(args.source.read_bytes()))
    else:
        args.destination.write_text(boot_arguments(args.source.read_text()))
