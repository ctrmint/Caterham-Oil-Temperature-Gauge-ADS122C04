"""Build the interface board (.kicad_pcb) with KiCad's pcbnew API.

Nets come from the schematic's own netlist, footprints and values from design.py,
and placement and routing from layout.py. build.sh runs this inside KiCad 10.0.8.
Coordinates in layout.py are mm from the display case centre, viewed from the back
(the board's top side), +x right, +y up.
"""
import math
import os
import re
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import design as D  # noqa: E402
import gen_sch  # noqa: E402
import layout as L  # noqa: E402

OX, OY = 100.0, 100.0
FPLIB = '/usr/share/kicad/footprints'


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


def outline_points(n_arc=180):
    """Board outline as a closed polygon (used for zones); the Edge.Cuts uses true arcs."""
    r, yt, xc = L.R_EDGE, L.Y_TOP, L.X_CLIP
    pts = []
    xt = min(xc, math.sqrt(r * r - yt * yt))
    yb = -math.sqrt(r * r - xt * xt)  # where the side meets the arc
    pts.append((-xt, yt))
    pts.append((xt, yt))
    if yb < yt:
        pts.append((xt, yb))
    a0 = math.atan2(yb, xt)
    a1 = math.atan2(yb, -xt) - 2 * math.pi
    for i in range(1, n_arc):
        a = a0 + (a1 - a0) * i / n_arc
        pts.append((r * math.cos(a), r * math.sin(a)))
    if yb < yt:
        pts.append((-xt, yb))
    return pts, xt, yb


def add_edge(board):
    r, yt = L.R_EDGE, L.Y_TOP
    _pts, xt, yb = outline_points()

    def seg(a, b):
        s = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(P(*a))
        s.SetEnd(P(*b))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(pcbnew.FromMM(0.1))
        board.Add(s)

    seg((-xt, yt), (xt, yt))
    if yb < yt - 1e-6:
        seg((xt, yt), (xt, yb))
        seg((-xt, yb), (-xt, yt))
    arc = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_ARC)
    arc.SetArcGeometry(P(xt, yb), P(0, -r), P(-xt, yb))
    arc.SetLayer(pcbnew.Edge_Cuts)
    arc.SetWidth(pcbnew.FromMM(0.1))
    board.Add(arc)


def load_fp(ref):
    lib_id, value, footprint, _pins, _sch, _f = D.PARTS[ref]
    lib, name = footprint.split(':')
    fp = pcbnew.FootprintLoad(f'{FPLIB}/{lib}.pretty', name)
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPath(pcbnew.KIID_PATH('/' + gen_sch.uid('sym', ref)))
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
    board = pcbnew.NewBoard(out)
    pcbnew.KIID.SeedGenerator(48)   # deterministic UUIDs from here on: an unchanged design rebuilds identically
    tb = board.GetTitleBlock()
    tb.SetTitle(D.TITLE)
    tb.SetRevision(D.REV)
    tb.SetDate(D.DATE)
    ds = board.GetDesignSettings()
    ds.SetCopperLayerCount(2)
    # Fab rules: comfortably inside a standard 2-layer, 1.6 mm, 1 oz prototype service
    # (JLCPCB/PCBWay standard: 0.127 mm track/space, 0.3 mm drill).
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
            for k, v in (('Manufacturer', mf), ('MPN', mpn), ('LCSC', lcsc), ('Description', desc)):
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

    pts, _xt, _yb = outline_points()
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(netobj['GND'])
        ol = z.Outline()
        ol.NewOutline()
        for x, y in pts:
            ol.Append(P(x, y))
        z.SetLocalClearance(pcbnew.FromMM(L.ZONE_CLEARANCE))
        z.SetMinThickness(pcbnew.FromMM(0.25))
        z.SetThermalReliefGap(pcbnew.FromMM(0.3))
        z.SetThermalReliefSpokeWidth(pcbnew.FromMM(0.4))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetZoneName(f'GND_{"F" if layer == pcbnew.F_Cu else "B"}')
        board.Add(z)
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

    # Zones are filled by `kicad-cli pcb drc --refill-zones --save-board`, which applies the board rules.
    pcbnew.SaveBoard(out, board)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
