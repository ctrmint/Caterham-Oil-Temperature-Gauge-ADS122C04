"""Check that the schematic's exported netlist (kicadsexpr) is exactly design.PARTS."""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import design as D  # noqa: E402
t = open(sys.argv[1]).read()
nets_txt = t[t.index('(nets'):]
got = {}
for chunk in re.split(r'\(net\s*\(code', nets_txt)[1:]:
    name = re.search(r'\(name "([^"]+)"', chunk).group(1).lstrip('/')
    got[name] = set(re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', chunk))
want = {k: set(v) for k, v in D.nets().items()}
ok = True
for k in sorted(set(got) | set(want)):
    g, w = got.get(k, set()), want.get(k, set())
    if k.startswith('unconnected'):
        print('NC   ', k)
        continue
    if g != w:
        ok = False
        print('MISMATCH', k, 'got', sorted(g), 'want', sorted(w))
print('nets match design.py' if ok else 'NETS DIFFER', f'({len(want)} nets)')
sys.exit(0 if ok else 1)
