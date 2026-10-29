"""Convert the GPU layer's raw 32-bit frame dumps (out/dump/NAME_IDX_WxH.raw) to PNG.
usage: python tools/raw2png.py <dump.raw> [--bgra]   (writes <dump>.png beside it)"""
import re
import struct
import sys
import zlib
from pathlib import Path

path = Path(sys.argv[1])
width, height = map(int, re.search(r'_(\d+)x(\d+)\.raw$', path.name).groups())
data = path.read_bytes()[:width * height * 4]
bgra = '--bgra' in sys.argv
rows = []
for y in range(height):
    row = bytearray(data[y * width * 4:(y + 1) * width * 4])
    if bgra:
        row[0::4], row[2::4] = row[2::4], row[0::4]
    rows.append(b'\0' + bytes(row))


def chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff)


png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) +
       chunk(b'IDAT', zlib.compress(b''.join(rows), 6)) + chunk(b'IEND', b''))
out = path.with_suffix('.png')
out.write_bytes(png)
print(out)
