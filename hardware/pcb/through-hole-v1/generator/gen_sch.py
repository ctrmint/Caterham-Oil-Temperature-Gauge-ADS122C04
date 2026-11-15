"""Write the interface board's KiCad schematic from design.py.

Every pin gets a short stub ending in a net label, a power symbol or a no-connect flag,
so the drawing's connectivity is exactly design.PARTS. ERC checks the result.
Symbols are copied from KiCad's stock libraries (KICAD_SYMBOL_DIR) into the schematic.
"""
import math
import os
import re
import sys
import uuid

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import design as D  # noqa: E402

LIB = os.environ.get('KICAD_SYMBOL_DIR', '/usr/share/kicad/symbols')
NS = uuid.UUID('6f1c1d52-5d0e-4a8e-9a43-8c2b7e1f0a11')


def uid(*parts):
    return str(uuid.uuid5(NS, '/'.join(map(str, parts))))


ROOT = uid('root')


def block(text, name):
    i = text.index(f'(symbol "{name}"')
    d = 0
    for j in range(i, len(text)):
        if text[j] == '(':
            d += 1
        elif text[j] == ')':
            d -= 1
            if d == 0:
                return text[i:j + 1]
    raise ValueError(name)


_libs = {}


BOARD_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def lib_text(lib):
    if lib not in _libs:
        path = os.path.join(BOARD_DIR, f'{lib}.kicad_sym') if lib == D.LIB else f'{LIB}/{lib}.kicad_sym'
        _libs[lib] = open(path, encoding='utf-8').read()
    return _libs[lib]


def lib_symbol(lib_id):
    """The library symbol as embedded in a schematic, with any `extends` flattened."""
    lib, name = lib_id.split(':')
    t = lib_text(lib)
    b = block(t, name)
    m = re.search(r'\(extends "([^"]+)"\)', b)
    if m:
        parent = m.group(1)
        pb = block(t, parent)
        # Take the derived symbol's own properties over the parent's.
        for prop in ('Value', 'Footprint', 'Datasheet', 'Description', 'ki_keywords', 'ki_fp_filters'):
            dm = re.search(r'\(property "%s" "([^"]*)"' % prop, b)
            if dm:
                pb = re.sub(r'(\(property "%s" ")[^"]*(")' % prop,
                            lambda mm: mm.group(1) + dm.group(1).replace('\\', '\\\\') + mm.group(2), pb, count=1)
        pb = pb.replace(f'(symbol "{parent}_', f'(symbol "{name}_')
        b = pb.replace(f'(symbol "{parent}"', f'(symbol "{name}"', 1)
    return b.replace(f'(symbol "{name}"', f'(symbol "{lib_id}"', 1)


def lib_pins(lib_id):
    b = lib_symbol(lib_id)
    pins = {}
    for m in re.finditer(r'\(pin \w+ \w+\s+\(at ([-\d.]+) ([-\d.]+) ([-\d.]+)\)', b):
        num = re.search(r'\(number "([^"]*)"', b[m.end():]).group(1)
        pins[num] = (float(m.group(1)), float(m.group(2)), int(float(m.group(3))))
    return pins


def place_point(x, y, rot, mirror, lx, ly):
    """Library coordinates (Y up) to sheet coordinates (Y down)."""
    if mirror == 'y':
        lx = -lx
    if rot == 0:
        sx, sy = lx, -ly
    elif rot == 90:
        sx, sy = -ly, -lx
    elif rot == 180:
        sx, sy = -lx, ly
    elif rot == 270:
        sx, sy = ly, lx
    else:
        raise ValueError(rot)
    return round(x + sx, 2), round(y + sy, 2)


def pin_geometry(ref):
    lib_id, _v, _f, _n, (x, y, rot, mirror), _fl = D.PARTS[ref]
    out = {}
    for num, (lx, ly, a) in lib_pins(lib_id).items():
        px, py = place_point(x, y, rot, mirror, lx, ly)
        if mirror == 'y':
            a = 180 - a
        outward = (a + rot + 180) % 360
        out[num] = (px, py, outward)
    return out


def fmt(v):
    s = f'{v:.4f}'.rstrip('0').rstrip('.')
    return '0' if s == '-0' else s


FONT = '(effects (font (size 1.27 1.27)))'


def prop(name, value, x, y, hide=False, angle=0, justify=None):
    j = f' (justify {justify})' if justify else ''
    h = ' (hide yes)' if hide else ''
    return (f'(property "{name}" "{value}" (at {fmt(x)} {fmt(y)} {angle}){h} '
            f'(effects (font (size 1.27 1.27)){j}))')


def symbol_instance(lib_id, ref, value, x, y, rot, mirror, footprint='', fields=None, pins=(), hide_ref=False,
                    ref_at=None, val_at=None, in_bom=True, field_angle=0):
    fields = fields or {}
    rx, ry, rj = ref_at or (x, y - 5.08, None)
    vx, vy, vj = val_at or (x, y + 5.08, None)
    m = f' (mirror {mirror})' if mirror else ''
    parts = [f'(symbol (lib_id "{lib_id}") (at {fmt(x)} {fmt(y)} {rot}){m} (unit 1)',
             f'(exclude_from_sim no) (in_bom {"yes" if in_bom else "no"}) (on_board yes) (dnp no)',
             f'(uuid "{uid("sym", ref)}")',
             prop('Reference', ref, rx, ry, hide=hide_ref, justify=rj, angle=field_angle),
             prop('Value', value, vx, vy, justify=vj, angle=field_angle),
             prop('Footprint', footprint, x, y, hide=True),
             prop('Datasheet', '', x, y, hide=True)]
    for k, v in fields.items():
        parts.append(prop(k, v, x, y, hide=True))
    for p in pins:
        parts.append(f'(pin "{p}" (uuid "{uid("pin", ref, p)}"))')
    parts.append(f'(instances (project "{D.PROJECT}" (path "/{ROOT}" (reference "{ref}") (unit 1))))')
    parts.append(')')
    return '\n'.join(parts)


def direction(o):
    return round(math.cos(math.radians(o)), 6), round(-math.sin(math.radians(o)), 6)


def main(out_path, fields_by_ref=None):
    fields_by_ref = fields_by_ref or {}
    items = []
    used = set()
    pwr = [0]

    def power_symbol(net, x, y, outward, tag):
        lib_id = D.POWER_NETS[net]
        used.add(lib_id)
        pwr[0] += 1
        base = 270 if net == 'GND' else 90
        if outward in (0, 180):
            outward = base  # horizontal stub: +3V3 points up, GND down
        rot = (outward - base) % 360
        ref = f'#PWR{pwr[0]:02d}'
        dx, dy = direction(outward)
        vx, vy = x + dx * 3.6, y + dy * 3.6
        items.append(symbol_instance(lib_id, ref, net, x, y, rot, None, pins=('1',), hide_ref=True,
                                     val_at=(vx, vy, None), in_bom=False).replace(uid('sym', ref), uid('pwr', tag)))

    for ref, (lib_id, value, footprint, pinnets, (x, y, rot, mirror), fl) in D.PARTS.items():
        used.add(lib_id)
        geo = pin_geometry(ref)
        fields = dict(fl)
        if ref in D.BOM:
            mf, mpn, lcsc, desc = D.BOM[ref]
            fields.update({'Manufacturer': mf, 'MPN': mpn, 'DigiKey_UK': lcsc, 'Description': desc})
        fields.update(fields_by_ref.get(ref, {}))
        fangle = 0
        horizontal = len({round(p[0], 2) for p in geo.values()}) > 1 and len({round(p[1], 2) for p in geo.values()}) == 1
        if lib_id.startswith('Device:') and horizontal:
            ref_at, val_at = (x, y - 2.79, None), (x, y + 2.79, None)
        elif lib_id.startswith('Device:'):
            ref_at, val_at = (x + 2.54, y - 1.27, 'left'), (x + 2.54, y + 1.27, 'left')
        else:
            ref_at = val_at = None
        if rot in (90, 270):
            fangle = rot
        if lib_id == 'Diode:BAV99':
            ref_at, val_at = (x, y - 5.08, None), (x, y - 2.54, None)
        if lib_id.startswith('Connector_Generic:Conn_01x08'):
            ref_at, val_at = (x, y - 12.7, None), (x, y + 15.24, None)
        if lib_id == 'Connector_Generic:Conn_01x04':
            ref_at, val_at = (x, y - 5.08, None), (x, y + 8.89, None)
        if rot in (180, 270):   # KiCad mirrors a field's justification with the symbol
            ref_at = ref_at and (ref_at[0], ref_at[1], 'right' if ref_at[2] == 'left' else ref_at[2])
            val_at = val_at and (val_at[0], val_at[1], 'right' if val_at[2] == 'left' else val_at[2])
        if lib_id.startswith('Connector_Generic:Conn_02'):
            ref_at, val_at = (x + 1.27, y - 5.08, None), (x + 1.27, y + 7.62, None)
        if lib_id == 'Analog_ADC:ADS1115IDGS':
            ref_at, val_at = (x + 2.54, y - 15.24, 'left'), (x + 5.08, y + 13.97, 'left')
        if lib_id == 'Mechanical:MountingHole':
            ref_at, val_at = (x, y - 3.81, None), (x, y + 3.81, None)
        items.append(symbol_instance(lib_id, ref, value, x, y, rot, mirror, footprint, fields,
                                     sorted(geo, key=lambda s: (len(s), s)), ref_at=ref_at, val_at=val_at,
                                     field_angle=fangle, in_bom=lib_id != 'Mechanical:MountingHole'))
        for pin, net in pinnets.items():
            px, py, o = geo[pin]
            if net is None:
                items.append(f'(no_connect (at {fmt(px)} {fmt(py)}) (uuid "{uid("nc", ref, pin)}"))')
                continue
            dx, dy = direction(o)
            stub = D.SCH_STUB.get((ref, pin), 2.54)
            qx, qy = round(px + dx * stub, 2), round(py + dy * stub, 2)
            items.append(f'(wire (pts (xy {fmt(px)} {fmt(py)}) (xy {fmt(qx)} {fmt(qy)})) '
                         f'(stroke (width 0) (type default)) (uuid "{uid("stub", ref, pin)}"))')
            if net in D.POWER_NETS:
                power_symbol(net, qx, qy, o, f'{ref}.{pin}')
            else:
                just = 'left bottom' if o in (0, 90) else 'right bottom'
                items.append(f'(label "{net}" (at {fmt(qx)} {fmt(qy)} {o}) '
                             f'(effects (font (size 1.27 1.27)) (justify {just})) (uuid "{uid("lbl", ref, pin)}"))')

    # PWR_FLAGs: these rails come in through connector pins or diodes, so ERC needs to be told they're driven.
    used.add('power:PWR_FLAG')
    for i, (net, x, y) in enumerate(D.PWR_FLAGS):
        if net in D.POWER_NETS:
            flag_rot = 180 if net == '+3V3' else 0
            items.append(symbol_instance('power:PWR_FLAG', f'#FLG0{i + 1}', 'PWR_FLAG', x, y, flag_rot, None,
                                         pins=('1',), hide_ref=True,
                                         val_at=(x, y + (5.08 if net == '+3V3' else -5.08), None), in_bom=False))
            ey = y - 5.08 if net == '+3V3' else y + 5.08
            items.append(f'(wire (pts (xy {fmt(x)} {fmt(y)}) (xy {fmt(x)} {fmt(ey)})) '
                         f'(stroke (width 0) (type default)) (uuid "{uid("flagwire", net)}"))')
            power_symbol(net, x, ey, 90 if net == '+3V3' else 270, f'flag.{net}')
        else:
            items.append(symbol_instance('power:PWR_FLAG', f'#FLG0{i + 1}', 'PWR_FLAG', x, y, 0, None, pins=('1',),
                                         hide_ref=True, val_at=(x, y - 5.08, None), in_bom=False))
            items.append(f'(wire (pts (xy {fmt(x)} {fmt(y)}) (xy {fmt(x + 5.08)} {fmt(y)})) '
                         f'(stroke (width 0) (type default)) (uuid "{uid("flagwire", net)}"))')
            items.append(f'(label "{net}" (at {fmt(x + 5.08)} {fmt(y)} 0) (effects (font (size 1.27 1.27)) '
                         f'(justify left bottom)) (uuid "{uid("flaglbl", net)}"))')

    notes = D.SCH_NOTES
    for i, (x, y, t) in enumerate(notes):
        items.append(f'(text "{t}" (exclude_from_sim no) (at {fmt(x)} {fmt(y)} 0) '
                     f'(effects (font (size 1.27 1.27)) (justify left top)) (uuid "{uid("note", i)}"))')

    libsyms = '\n'.join(lib_symbol(l) for l in sorted(used))
    doc = f'''(kicad_sch (version 20250114) (generator "eeschema") (generator_version "9.0")
(uuid "{ROOT}")
(paper "A4")
(title_block (title "{D.TITLE}") (date "{D.DATE}") (rev "{D.REV}")
 (comment 1 "Caterham dry-sump oil-temperature gauge: ADS1115 front end with harness protection")
 (comment 2 "Generated from the design table; checked by ERC and DRC schematic parity"))
(lib_symbols
{libsyms}
)
{chr(10).join(items)}
(sheet_instances (path "/" (page "1")))
(embedded_fonts no)
)
'''
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(doc)


if __name__ == '__main__':
    main(sys.argv[1])
