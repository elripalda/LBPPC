"""The game's imports by library, against what the Bloodborne runtime provides (docs/imports.md).
usage: python -I tools/imports.py <game dump> <Bloodborne port checkout> <shadPS4 checkout> <out.json>
Reads the dump's eboot.bin and sce_sys/param.sfo; uses the Bloodborne port's scripts/prepare.py parser."""
import collections, json, re, sys
from pathlib import Path

dump, bb, ref, out = map(Path, sys.argv[1:5])
sys.path.insert(0, str(bb / 'scripts'))
import prepare as P  # our own parser (bb-195/scripts/prepare.py)

ALPHA = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-'


def decode_id(s):
    n = 0
    for ch in s:
        n = n * 64 + ALPHA.index(ch)
    return n


def imports_of(path):
    elf, header, ph, _, _ = P.parse_self(path.read_bytes())
    dp = P.segment(ph, 2, path.name)
    dyn = []
    for pos in range(dp['offset'], dp['offset'] + dp['filesz'], 16):
        tag, value = P.unpack('<QQ', elf, pos)
        if tag == 0:
            break
        dyn.append((tag, value))
    tags = dict(dyn)
    lib = P.segment(ph, 0x61000000, path.name)
    blob = P.span(elf, lib['offset'], lib['filesz'])
    strings = P.span(blob, *P.entries(tags, 0x61000035, 0x61000037, path.name))
    string = lambda off: strings[off:strings.index(0, off)].decode('ascii')
    libs = {}  # DT_SCE_IMPORT_LIB: name offset low 32 bits, id in the top 16
    modules = {}
    for t, v in dyn:
        if t == 0x61000015:
            libs[v >> 48] = string(v & 0xffffffff)
        elif t == 0x6100000f:
            modules[v >> 48] = string(v & 0xffffffff)
    syms = P.span(blob, *P.entries(tags, 0x61000039, 0x6100003f, path.name))
    found = []
    for pos in range(0, len(syms), 24):
        name, info, other, shndx, value, size = P.unpack('<IBBHQQ', syms, pos)
        s = string(name)
        if shndx == 0 and s.count('#') == 2:
            nid, l, m = s.split('#')
            found.append((nid, s, libs.get(decode_id(l), '?' + l), modules.get(decode_id(m), '?' + m)))
    return found, sorted(set(modules.values())), sorted(set(libs.values()))


# NID -> name, and which NIDs the Bloodborne runtime answers.
names = {}
LIBF = re.compile(r'LIB_FUNCTION\(\s*"([A-Za-z0-9+-]{11})"\s*,\s*"([^"]+)"[^;]*?,\s*([A-Za-z_][\w:]*)\s*\)')
for f in (ref / 'src/core/libraries').rglob('*.cpp'):
    for nid, libname, fn in LIBF.findall(f.read_text(errors='replace')):
        names.setdefault(nid, fn.split('::')[-1])
inc = (bb / 'src/import_names.inc').read_text()
for nid, sym in re.findall(r'\{"([A-Za-z0-9+-]{11})#[^"]*","([^"]+)"\}', inc):
    names[nid] = sym
known_nids = set(re.findall(r'\{"([A-Za-z0-9+-]{11})#', inc))  # runtime_symbol only knows these
c_exports = set()
for f in (bb / 'src').glob('runtime*.c'):
    for block in re.findall(r'RuntimeExport\s+\w+\[\]\s*=\s*\{(.*?)\};', f.read_text(errors='replace'), re.S):
        c_exports |= set(re.findall(r'\{\s*"(\w+)"\s*,', block))
gpu_nids = set()
for f in list((bb / 'gpu/shadps4').rglob('*.cpp')) + list((bb / 'gpu/bbnet').rglob('*.cpp')):
    for nid, libname, fn in LIBF.findall(f.read_text(errors='replace')):
        gpu_nids.add(nid)
        names.setdefault(nid, fn.split('::')[-1])
c_exports_nids = {P.nid(n) for n in c_exports}
for f in (bb / 'src').glob('*.c'):  # files that match imports by scoped NID string
    c_exports_nids |= set(re.findall(r'"([A-Za-z0-9+-]{11})#', f.read_text(errors='replace')))

report = {'sfo': {k: v for k, v in P.sfo((dump / 'sce_sys/param.sfo').read_bytes()).items()
                  if k in ('TITLE', 'TITLE_ID', 'APP_VER', 'VERSION', 'CATEGORY', 'SYSTEM_VER')}}
own = set()
for prx in sorted((dump / 'sce_module').glob('*.prx')):
    report.setdefault('bundled_modules', []).append(prx.name)
eboot, modules, libs = imports_of(dump / 'eboot.bin')
report['needed_modules'], report['import_libraries'] = modules, libs
by_lib = collections.defaultdict(lambda: {'total': 0, 'have': [], 'missing': []})
for nid, scoped, libname, mod in eboot:
    e = by_lib[libname]
    e['total'] += 1
    name = names.get(nid, nid)
    have = nid in gpu_nids or (nid in c_exports_nids and nid in known_nids) or nid in c_exports_nids
    (e['have'] if have else e['missing']).append(name)
report['by_library'] = {k: {'total': v['total'], 'have': len(v['have']), 'missing': sorted(v['missing'])}
                        for k, v in sorted(by_lib.items(), key=lambda kv: -len(kv[1]['missing']))}
report['totals'] = dict(imports=len(eboot), have=sum(len(v['have']) for v in by_lib.values()),
                        unnamed=sum(1 for n, *_ in eboot if n not in names),
                        c_exports_not_in_import_names=sorted(n for n in c_exports
                                                             if P.nid(n) in {x[0] for x in eboot}
                                                             and P.nid(n) not in known_nids))
out.write_text(json.dumps(report, indent=1))
print(json.dumps(report['sfo']), report['bundled_modules'])
print('modules:', modules)
print('totals:', {k: (len(v) if isinstance(v, list) else v) for k, v in report['totals'].items()})
for k, v in report['by_library'].items():
    print(f"{k:32} {v['have']:4}/{v['total']:<4} missing {len(v['missing'])}")
