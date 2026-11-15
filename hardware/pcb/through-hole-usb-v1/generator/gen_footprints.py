"""Write the board's own footprint and symbol libraries (run inside KiCad 10.0.8 by build.sh).

footprints.pretty: copies of KiCad's stock footprints for a single-sided board. Every through-hole pad gets at
least MIN_RING of copper round its hole, because a single-sided board has no plated holes and a small pad lifts
when soldered. (KiCad keeps through-hole pads on every copper layer; the board is single-sided because every
track and the pour are on B.Cu and only B.Cu is sent to the board house.) Mechanical holes are unchanged.
The Adafruit ADS1115 module's footprint is drawn here from its pin layout.

through-hole-usb-v1.kicad_sym: the module's symbol (12 pins, named as on the module's silkscreen).
"""
import os
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import design as D  # noqa: E402
import module as M  # noqa: E402

STOCK = '/usr/share/kicad/footprints'
MIN_RING = 0.6       # copper beyond the hole edge, each side (mm)
# Tighter-pitch footprints where 0.6 mm would close the gap between pads: (ring in x, ring in y) of the stock footprint.
# JLCPCB's minimum for unplated pads is 0.45 mm. None on this board.
RING = {}


def mm(v):
    return pcbnew.FromMM(v)


def single_sided(fp, name=''):
    rx, ry = RING.get(name, (MIN_RING, MIN_RING))
    for p in fp.Pads():
        if p.GetAttribute() != pcbnew.PAD_ATTRIB_PTH:
            continue
        drill = max(pcbnew.ToMM(p.GetDrillSize().x), pcbnew.ToMM(p.GetDrillSize().y))
        size = p.GetSize(pcbnew.B_Cu)
        w, h = pcbnew.ToMM(size.x), pcbnew.ToMM(size.y)
        p.SetSize(pcbnew.B_Cu, pcbnew.VECTOR2I(mm(max(w, drill + 2 * rx)), mm(max(h, drill + 2 * ry))))
    return fp


SILK_GAP = 0.25      # legend kept this far from the (enlarged) pads


def _inside_spans(a, b, box):
    """Liang-Barsky: the [t0, t1] span of segment a->b inside the axis-aligned box, or None."""
    (x0, y0), (x1, y1) = a, b
    dx, dy = x1 - x0, y1 - y0
    t0, t1 = 0.0, 1.0
    for p, q in ((-dx, x0 - box[0]), (dx, box[2] - x0), (-dy, y0 - box[1]), (dy, box[3] - y0)):
        if p == 0:
            if q < 0:
                return None
            continue
        t = q / p
        if p < 0:
            t0 = max(t0, t)
        else:
            t1 = min(t1, t)
    return (t0, t1) if t0 < t1 else None


def _clip(a, b, boxes):
    """The pieces of segment a->b outside every box."""
    cut = sorted(s for s in (_inside_spans(a, b, bx) for bx in boxes) if s)
    pieces, t = [], 0.0
    for s0, s1 in cut:
        if s0 > t:
            pieces.append((t, s0))
        t = max(t, s1)
    if t < 1.0:
        pieces.append((t, 1.0))
    length = ((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5
    return [((a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u), (a[0] + (b[0] - a[0]) * v, a[1] + (b[1] - a[1]) * v))
            for u, v in pieces if (v - u) * length > 0.2]


def clip_silk(fp):
    """Trim the legend where it would now run onto the enlarged pads, so it stays printable."""
    boxes = []
    for p in fp.Pads():
        bb = p.GetBoundingBox()
        boxes.append((pcbnew.ToMM(bb.GetLeft()) - SILK_GAP, pcbnew.ToMM(bb.GetTop()) - SILK_GAP,
                      pcbnew.ToMM(bb.GetRight()) + SILK_GAP, pcbnew.ToMM(bb.GetBottom()) + SILK_GAP))
    for item in list(fp.GraphicalItems()):
        if item.GetLayer() != pcbnew.F_SilkS:
            continue
        if item.GetClass() != 'PCB_SHAPE':
            fp.Remove(item)          # stray legend text such as a "K" beside a diode
            continue
        shape, w = item.GetShape(), pcbnew.ToMM(item.GetWidth())
        if shape == pcbnew.SHAPE_T_RECT:
            s, e = item.GetStart(), item.GetEnd()
            x0, y0, x1, y1 = pcbnew.ToMM(s.x), pcbnew.ToMM(s.y), pcbnew.ToMM(e.x), pcbnew.ToMM(e.y)
            segs = [((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))]
        elif shape == pcbnew.SHAPE_T_SEGMENT:
            s, e = item.GetStart(), item.GetEnd()
            segs = [((pcbnew.ToMM(s.x), pcbnew.ToMM(s.y)), (pcbnew.ToMM(e.x), pcbnew.ToMM(e.y)))]
        else:
            bb = item.GetBoundingBox()
            box = (pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop()), pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom()))
            # arcs, circles and polygons: drop them only if they actually reach a pad
            if any(box[0] < bx[2] and bx[0] < box[2] and box[1] < bx[3] and bx[1] < box[3] for bx in boxes) and \
                    shape != pcbnew.SHAPE_T_CIRCLE:
                fp.Remove(item)
            continue
        fp.Remove(item)
        for a, b in segs:
            for c, d in _clip(a, b, boxes):
                add_line(fp, pcbnew.F_SilkS, c, d, w)


def stock_copy(name, full):
    lib, stock_name = full.split(':')
    fp = pcbnew.FootprintLoad(f'{STOCK}/{lib}.pretty', stock_name)
    fp.SetFPID(pcbnew.LIB_ID(D.LIB, name))
    fp.SetLibDescription(f'Copy of {full} for a single-sided board: at least {MIN_RING} mm of copper round '
                         'each hole. ' + fp.GetLibDescription())
    single_sided(fp, name)
    clip_silk(fp)
    return fp


def add_line(fp, layer, a, b, w):
    s = pcbnew.PCB_SHAPE(fp, pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(pcbnew.VECTOR2I(mm(a[0]), mm(a[1])))
    s.SetEnd(pcbnew.VECTOR2I(mm(b[0]), mm(b[1])))
    s.SetLayer(layer)
    s.SetWidth(mm(w))
    fp.Add(s)


def add_rect(fp, layer, x0, y0, x1, y1, w):
    for a, b in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
        add_line(fp, layer, a, b, w)


def add_text(fp, layer, text, x, y, size):
    t = pcbnew.PCB_TEXT(fp)
    t.SetText(text)
    t.SetLayer(layer)
    t.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(0.15))
    fp.Add(t)


def module_footprint():
    """The Adafruit ADS1115 module, component side up, soldered in by its header pins.
    Origin at the module's centre; pad numbers follow design.MODULE_PINS."""
    fp = pcbnew.FOOTPRINT(None)
    fp.SetFPID(pcbnew.LIB_ID(D.LIB, D.MODULE_FP))
    fp.SetLibDescription('Adafruit ADS1115 breakout (product 1085, STEMMA QT), two 6-pin rows '
                         f'{M.ROW_SPACING} mm apart at 2.54 mm pitch. Solder the header pins; do not socket it.')
    fp.SetAttributes(pcbnew.FP_THROUGH_HOLE)
    layers = pcbnew.PAD.PTHMask()
    for num, (x, y) in M.pin_positions().items():
        p = pcbnew.PAD(fp)
        p.SetNumber(num)
        p.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        p.SetShape(pcbnew.B_Cu, pcbnew.PAD_SHAPE_RECTANGLE if num in ('1', '7') else pcbnew.PAD_SHAPE_CIRCLE)
        p.SetSize(pcbnew.B_Cu, pcbnew.VECTOR2I(mm(M.PAD), mm(M.PAD)))
        p.SetDrillSize(pcbnew.VECTOR2I(mm(M.DRILL), mm(M.DRILL)))
        p.SetLayerSet(layers)
        p.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        fp.Add(p)
    for (x, y) in M.HOLES:   # the module's own mounting holes, for nylon standoffs
        p = pcbnew.PAD(fp)
        p.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
        p.SetShape(pcbnew.F_Cu, pcbnew.PAD_SHAPE_CIRCLE)
        p.SetSize(pcbnew.F_Cu, pcbnew.VECTOR2I(mm(M.HOLE_D), mm(M.HOLE_D)))
        p.SetDrillSize(pcbnew.VECTOR2I(mm(M.HOLE_D), mm(M.HOLE_D)))
        p.SetLayerSet(pcbnew.LSET.AllCuMask())
        p.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        fp.Add(p)
    hw, hh = M.WIDTH / 2, M.HEIGHT / 2
    add_rect(fp, pcbnew.F_Fab, -hw, -hh, hw, hh, 0.1)
    add_rect(fp, pcbnew.F_SilkS, -hw - 0.15, -hh - 0.15, hw + 0.15, hh + 0.15, 0.15)
    add_rect(fp, pcbnew.F_CrtYd, -hw - 0.5, -hh - 0.5, hw + 0.5, hh + 0.5, 0.05)
    add_text(fp, pcbnew.F_Fab, 'ADS1115 module', 0, 0, 1.0)
    for num, (x, y) in M.pin_positions().items():   # pin names on the fab layer (they're under the module)
        add_text(fp, pcbnew.F_Fab, D.MODULE_PINS[num][0], x, y + (1.6 if y < 0 else -1.6), 0.6)
    fp.Reference().SetLayer(pcbnew.F_SilkS)
    fp.Reference().SetPosition(pcbnew.VECTOR2I(0, mm(-hh - 1.2)))
    fp.Value().SetLayer(pcbnew.F_Fab)
    fp.Value().SetPosition(pcbnew.VECTOR2I(0, mm(hh + 1.2)))
    return fp


def symbol_lib():
    """through-hole-usb-v1.kicad_sym with the module's symbol: system pins left, analogue pins right."""
    pins = []
    for num, (name, side) in D.MODULE_PINS.items():
        order = ['VIN', 'GND', 'SCL', 'SDA', 'ADDR', 'ALRT'] if side == 'L' else ['A+', 'A3', 'A2', 'A1', 'A0', 'A-']
        y = 6.35 - 2.54 * order.index(name)
        x, ang = (-10.16, 0) if side == 'L' else (10.16, 180)
        pins.append(f'(pin passive line (at {x} {y:.2f} {ang}) (length 2.54) '
                    f'(name "{name}" (effects (font (size 1.27 1.27)))) (number "{num}" (effects (font (size 1.27 1.27)))))')
    body = '\n'.join(pins)
    return f'''(kicad_symbol_lib (version 20251024) (generator "through-hole-usb-v1 gen_footprints.py")
(symbol "ADS1115_Module" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)
(property "Reference" "U" (at 0 10.16 0) (effects (font (size 1.27 1.27))))
(property "Value" "ADS1115_Module" (at 0 -10.16 0) (effects (font (size 1.27 1.27))))
(property "Footprint" "{D.LIB}:{D.MODULE_FP}" (at 0 -12.7 0) (effects (font (size 1.27 1.27)) (hide yes)))
(property "Datasheet" "https://www.adafruit.com/product/1085" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
(property "Description" "Adafruit ADS1115 16-bit I2C ADC breakout (STEMMA QT), two 6-pin rows" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
(symbol "ADS1115_Module_0_1" (rectangle (start -7.62 8.89) (end 7.62 -8.89) (stroke (width 0.254) (type default)) (fill (type background))))
(symbol "ADS1115_Module_1_1"
{body}
)
(embedded_fonts no)
)
)
'''


ENVELOPES = ('MicroFit_1x04_RA', 'MicroFit_2x03_RA', D.MODULE_FP)   # KiCad has no 3D model for these


def set_model(fp, name):
    models = fp.Models()
    models.clear()
    m = pcbnew.FP_3DMODEL()
    m.m_Filename = '${KIPRJMOD}/3d/' + name + '.wrl'
    models.push_back(m)


def main(out_dir):
    pcbnew.KIID.SeedGenerator(51)   # deterministic UUIDs: an unchanged design rebuilds identically
    pretty = os.path.abspath(os.path.join(out_dir, 'footprints.pretty'))
    io = pcbnew.PCB_IO_KICAD_SEXPR()
    if not os.path.isdir(pretty):
        io.CreateLibrary(pretty)
    for name, full in D.FOOTPRINTS.items():
        fp = stock_copy(name, full)
        if name in ENVELOPES:
            set_model(fp, name)
        io.FootprintSave(pretty, fp)
    fp = module_footprint()
    set_model(fp, D.MODULE_FP)
    io.FootprintSave(pretty, fp)
    with open(os.path.join(out_dir, f'{D.LIB}.kicad_sym'), 'w', encoding='utf-8') as f:
        f.write(symbol_lib())


if __name__ == '__main__':
    main(sys.argv[1])
