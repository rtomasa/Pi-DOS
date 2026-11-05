#!/usr/bin/env python3
"""Draw the Pi-DOS Control Panel icon using the Windows 16-colour palette."""
import struct
import sys
from pathlib import Path


def draw_icon():
    pixels = [[None] * 32 for _ in range(32)]

    def rect(x0, y0, x1, y1, colour):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                pixels[y][x] = colour

    def line(x0, y0, x1, y1, colour):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        error = dx + dy
        while True:
            pixels[y0][x0] = colour
            if (x0, y0) == (x1, y1):
                break
            twice = 2 * error
            if twice >= dy:
                error += dy
                x0 += sx
            if twice <= dx:
                error += dx
                y0 += sy

    def polygon(points, colour):
        # Pixel centres and an even/odd fill keep diagonal edges crisp.
        for y in range(32):
            for x in range(32):
                inside = False
                previous = points[-1]
                for current in points:
                    ax, ay = previous
                    bx, by = current
                    if (ay > y + 0.5) != (by > y + 0.5):
                        if x + 0.5 < ax + (y + 0.5 - ay) * (bx - ax) / (by - ay):
                            inside = not inside
                    previous = current
                if inside:
                    pixels[y][x] = colour
        for a, b in zip(points, points[1:] + points[:1]):
            line(*a, *b, colour)

    # Bevelled CRT, with a readable DOS prompt on a navy screen.
    rect(2, 2, 26, 22, 0)
    rect(3, 3, 25, 21, 7)
    line(3, 3, 24, 3, 15)
    line(3, 3, 3, 20, 15)
    line(25, 4, 25, 21, 8)
    line(4, 21, 24, 21, 8)
    rect(5, 5, 23, 18, 8)
    rect(6, 6, 23, 18, 15)
    rect(6, 6, 22, 17, 0)
    rect(7, 7, 21, 16, 1)
    line(9, 9, 11, 11, 11)
    line(11, 11, 9, 13, 11)
    line(14, 13, 17, 13, 15)
    rect(11, 23, 18, 25, 0)
    rect(12, 23, 17, 25, 8)
    line(12, 23, 12, 25, 7)
    polygon([(8, 26), (20, 26), (23, 28), (23, 29), (6, 29), (6, 28)], 0)
    line(8, 27, 20, 27, 15)
    line(7, 28, 21, 28, 7)
    rect(6, 20, 8, 20, 8)
    rect(21, 20, 22, 20, 10)

    # Gold spanner: a strong configuration symbol at the native 32px size.
    polygon([(24, 12), (28, 12), (26, 15), (28, 17), (31, 14),
             (31, 19), (28, 22), (26, 22), (19, 29), (16, 30),
             (13, 27), (14, 24), (22, 16), (22, 14)], 0)
    polygon([(24, 13), (26, 13), (24, 15), (28, 19), (30, 17),
             (30, 19), (27, 21), (25, 21), (18, 28), (16, 28),
             (15, 27), (15, 25), (23, 17), (23, 14)], 6)
    line(24, 14, 24, 16, 14)
    line(24, 17, 16, 25, 14)
    line(16, 25, 16, 27, 14)
    line(17, 27, 25, 19, 14)
    line(27, 20, 29, 18, 14)
    pixels[26][16] = 0
    return pixels


def encode(pixels):
    palette = bytes.fromhex(
        '000000 000080 008000 008080 800000 800080 808000 c0c0c0 '
        '808080 0000ff 00ff00 00ffff ff0000 ff00ff ffff00 ffffff')
    palette = b''.join(palette[i:i + 3][::-1] + b'\0' for i in range(0, 48, 3))
    bitmap = bytes(((row[x] or 0) << 4) | (row[x + 1] or 0)
                   for row in reversed(pixels) for x in range(0, 32, 2))
    # Transparent pixels have zero XOR colour and a set bit in the AND mask.
    mask = b''.join(sum((value is None) << (31 - x) for x, value in enumerate(row))
                    .to_bytes(4, 'big') for row in reversed(pixels))
    dib = struct.pack('<IiiHHIIiiII', 40, 32, 64, 1, 4, 0,
                      len(bitmap) + len(mask), 0, 0, 16, 0)
    data = dib + palette + bitmap + mask
    return (struct.pack('<HHH', 0, 1, 1) +
            struct.pack('<BBBBHHII', 32, 32, 16, 0, 1, 4, len(data), 22) + data)


if __name__ == '__main__':
    Path(sys.argv[1]).write_bytes(encode(draw_icon()))
