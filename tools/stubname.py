"""Names the imports behind eboot PLT stubs: tools/stubname.py 0xe0f100 [0x...]. Reads out/boot.bin and out/eboot.asm."""
import re, struct, sys
from pathlib import Path

data = Path('out/boot.bin').read_bytes()
size, entry, ns, nr, ni, caps = struct.unpack_from('<QQQQQQ', data, 8)
pos = 56 + ns * 24
names = [data[pos + i * 128:pos + (i + 1) * 128].split(b'\0')[0].decode() for i in range(ni)]
pos += ni * 128
by_target = {}
for i in range(nr):
    target, kind, value, addend = struct.unpack_from('<QQqq', data, pos + i * 32)
    if 0 <= value < len(names):
        by_target[target] = names[value]
known = dict(re.findall(r'\{"([A-Za-z0-9+-]{11})(?:#[^"]*)?","([^"]+)"\}', Path('src/import_names.inc').read_text()))
asm = Path('out/eboot.asm').read_text(errors='replace')
for arg in sys.argv[1:]:
    stub = int(arg, 16)
    m = re.search(r'\n\s*%x:\s+jmpq\s+\*0x[0-9a-f]+\(%%rip\)\s+# 0x([0-9a-f]+)' % stub, asm)
    if not m:
        print(hex(stub), 'is not a stub')
        continue
    name = by_target.get(int(m.group(1), 16), '?')
    print(hex(stub), name, known.get(name.split('#')[0], ''))
