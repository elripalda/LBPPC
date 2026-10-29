"""Add every runtime export's NID to src/import_names.inc, so runtime_symbol knows the names of all functions
the runtime provides, whatever game imports them (the table first held only Bloodborne's imports).
usage: python -I tools/export_names.py"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'scripts'))
from prepare import nid  # noqa: E402

inc = ROOT / 'src/import_names.inc'
text = inc.read_text(encoding='utf-8')
known = set(re.findall(r'\{"([A-Za-z0-9+-]{11})(?:#[^"]*)?","', text))
names = set()
for f in sorted((ROOT / 'src').glob('runtime*.c')):
    for block in re.findall(r'RuntimeExport\s+\w+\[\]\s*=\s*\{(.*?)\};', f.read_text(errors='replace'), re.S):
        names |= set(re.findall(r'\{\s*"(\w+)"\s*,', block))
added = sorted((nid(n), n) for n in names if nid(n) not in known)
if added:
    marker = '/* Runtime exports by NID alone (tools/export_names.py) */\n'
    if marker not in text:
        text = text.rstrip('\n') + '\n' + marker
    text += ''.join(f'{{"{k}","{n}"}},\n' for k, n in added)
    inc.write_text(text, encoding='utf-8')
print(f'{len(names)} exports, {len(added)} added')
