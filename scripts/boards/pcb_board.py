"""Build an option's PCB (.kicad_pcb) with KiCad's pcbnew API, and its custom DRC rules (.kicad_dru).

    pcb_board.py <netlist.net> <out.kicad_pcb>

Nets come from the schematic's own netlist, footprints and values from pcb_design.py, and placement and routing
from pcb_layout.py. build.sh runs this inside KiCad 10.0.8 with BOARD_OPTION set.
Coordinates in pcb_layout.py are mm from the board's centre, viewed from the component side, +x right, +y up.
Two copper layers, no vias and no copper pour: a ground pour would sit beside the sensitive nets.
"""
import math
import os
import re
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import circuit as C  # noqa: E402
import pcb_design as D  # noqa: E402
import pcb_layout as L  # noqa: E402
import pcb_sch  # noqa: E402

OX, OY = 100.0, 100.0
FPLIB = '/usr/share/kicad/footprints'
BOARD_DIR = '.'   # set from the output path in main()


def P(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(OX + x), pcbnew.FromMM(OY - y))


def mm(v):
    return pcbnew.ToMM(v)


def read_netlist(path):
    t = open(path, encoding='utf-8').read()
    t = t[t.index('(nets'):]
    nets = {}
    for chunk in re.split(r'\(net\s*\(code', t)[1:]:
        name = re.search(r'\(name "([^"]+)"', chunk).group(1)
        nets[name] = re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', chunk)
    return nets


def outline_points(n_arc=12):
    """Board outline (a rectangle with rounded corners) as a closed polygon, for the zones."""
    w, h, r = L.WIDTH / 2, L.HEIGHT / 2, L.CORNER
    pts = []
    for cx, cy, a0 in ((w - r, h - r, 0), (-w + r, h - r, 90), (-w + r, -h + r, 180), (w - r, -h + r, 270)):
        for i in range(n_arc + 1):
            a = math.radians(a0 + 90 * i / n_arc)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def add_edge(board):
    w, h, r = L.WIDTH / 2, L.HEIGHT / 2, L.CORNER

    def seg(a, b):
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(P(*a))
        s.SetEnd(P(*b))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(pcbnew.FromMM(0.1))
        board.Add(s)

    def arc(cx, cy, a0):
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_ARC)
        pts = [(cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))) for a in (a0, a0 + 45, a0 + 90)]
        s.SetArcGeometry(P(*pts[0]), P(*pts[1]), P(*pts[2]))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(pcbnew.FromMM(0.1))
        board.Add(s)

    seg((-w + r, h), (w - r, h))
    seg((w, h - r), (w, -h + r))
    seg((w - r, -h), (-w + r, -h))
    seg((-w, -h + r), (-w, h - r))
    arc(w - r, h - r, 0)
    arc(-w + r, h - r, 90)
    arc(-w + r, -h + r, 180)
    arc(w - r, -h + r, 270)


def load_fp(ref):
    lib_id, value, footprint, _pins, _sch, _f = D.PARTS[ref]
    lib, name = footprint.split(':')
    path = os.path.join(BOARD_DIR, 'footprints.pretty') if lib == D.LIB else f'{FPLIB}/{lib}.pretty'
    fp = pcbnew.FootprintLoad(path, name)
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPath(pcbnew.KIID_PATH('/' + pcb_sch.uid('sym', ref)))
    fp.SetSheetname('/')
    fp.SetSheetfile(f'{D.PROJECT}.kicad_sch')
    if ref in L.MODEL_OVERRIDE:   # KiCad ships no 3D model for this part
        models = fp.Models()
        models.clear()
        m = pcbnew.FP_3DMODEL()
        m.m_Filename = L.MODEL_OVERRIDE[ref]
        models.push_back(m)
    return fp


def place(board, ref, x, y, rot, side='F'):
    fp = load_fp(ref)
    board.Add(fp)
    fp.SetPosition(P(0, 0))
    if side == 'B':
        fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(P(x, y))
    return fp


def pad_xy(fp, num):
    for p in fp.Pads():
        if p.GetNumber() == num:
            v = p.GetPosition()
            return round(mm(v.x) - OX, 3), round(OY - mm(v.y), 3)
    raise KeyError((fp.GetReference(), num))


def main(netlist, out):
    global BOARD_DIR
    BOARD_DIR = os.path.dirname(os.path.abspath(out))
    board = pcbnew.NewBoard(out)
    pcbnew.KIID.SeedGenerator(48)   # deterministic UUIDs from here on: an unchanged design rebuilds identically
    tb = board.GetTitleBlock()
    tb.SetTitle(D.TITLE)
    tb.SetRevision(D.REV)
    tb.SetDate(D.DATE)
    ds = board.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    # Fab rules: comfortably inside any standard 2-layer, 1.6 mm, 1 oz prototype service
    # (typically 0.127 mm track/space, 0.3 mm drill).
    for attr, v in L.RULES.items():
        setattr(ds, attr, pcbnew.FromMM(v))
    add_edge(board)

    nets = read_netlist(netlist)
    netobj = {}
    for name in nets:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        netobj[name] = n
    pin_net = {}
    for name, nodes in nets.items():
        for ref, pin in nodes:
            pin_net[(ref, pin)] = name

    fps = {}
    for ref, (x, y, rot, side) in L.PLACE.items():
        fps[ref] = place(board, ref, x, y, rot, side)
    missing = set(D.PARTS) - set(fps)
    if missing:
        raise SystemExit(f'unplaced: {sorted(missing)}')
    for ref, fp in fps.items():
        if ref in D.BOM:
            mf, mpn, lcsc, desc = D.BOM[ref]
            for k, v in (('Manufacturer', mf), ('MPN', mpn), ('DigiKey_UK', lcsc), ('Description', desc)):
                fp.SetField(k, v)
                fp.GetField(k).SetVisible(False)
        for p in fp.Pads():
            name = pin_net.get((ref, p.GetNumber()))
            if name:
                p.SetNet(netobj[name])

    # Check the placement-critical pads land where the case needs them.
    for (ref, num), (ex, ey) in L.EXPECT.items():
        gx, gy = pad_xy(fps[ref], num)
        if abs(gx - ex) > 0.01 or abs(gy - ey) > 0.01:
            raise SystemExit(f'{ref}.{num} at ({gx}, {gy}), expected ({ex}, {ey})')

    L.route(board, P, netobj, fps, pad_xy)

    for hx, hy in L.HOLES:
        z = pcbnew.ZONE(board)
        z.SetIsRuleArea(True)
        z.SetLayerSet(pcbnew.LSET.AllCuMask())
        z.SetDoNotAllowTracks(True)
        z.SetDoNotAllowVias(True)
        z.SetDoNotAllowZoneFills(True)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
        ol = z.Outline()
        ol.NewOutline()
        for i in range(48):
            a = 2 * math.pi * i / 48
            ol.Append(P(hx + L.HOLE_KEEPOUT * math.cos(a), hy + L.HOLE_KEEPOUT * math.sin(a)))
        z.SetZoneName('screw head')
        board.Add(z)

    L.decorate(board, P, fps)

    pcbnew.SaveBoard(out, board)
    write_rules(os.path.splitext(out)[0] + '.kicad_dru')


def write_rules(path):
    """Sensitive nets (circuit.SENSITIVE) keep SENSITIVE_CLEARANCE from ground copper: a short there gives a
    believable wrong reading rather than a fault (hardware/WIRING.md)."""
    names = ' || '.join(f"{{side}}.NetName == '/{n}'" for n in C.SENSITIVE)
    cond = f"(A.NetName == 'GND' && ({names.format(side='B')})) || (B.NetName == 'GND' && ({names.format(side='A')}))"
    with open(path, 'w', encoding='utf-8') as f:
        f.write('(version 1)\n'
                '# Written by scripts/boards/pcb_board.py: a short from these nets to ground gives a believable wrong\n'
                '# reading, so their copper keeps well clear of ground copper.\n'
                f'(rule "sensitive nets keep {L.SENSITIVE_CLEARANCE} mm from ground"\n'
                f'  (constraint clearance (min {L.SENSITIVE_CLEARANCE}mm))\n'
                f'  (condition "{cond}"))\n')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
