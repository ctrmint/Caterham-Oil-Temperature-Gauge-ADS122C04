"""Write the project's own footprint and symbol libraries (run inside KiCad 10.0.8 by build.sh).

    pcb_footprints.py <pcb_dir>

footprints.pretty: copies of KiCad's stock footprints (so the project opens the same anywhere), plus the Adafruit
ADS122C04 module's footprint, drawn from its pin layout. gauge.kicad_sym: the module's symbol.
"""
import os
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import circuit as C  # noqa: E402
import pcb_design as D  # noqa: E402

STOCK = '/usr/share/kicad/footprints'
PAD, DRILL = 1.8, 1.0        # module pads, for 0.64 mm square header pins
HOLE_D = 2.7                 # module standoff holes: clearance for an M2.5 nylon screw


def mm(v):
    return pcbnew.FromMM(v)


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
    t.SetTextThickness(mm(0.12))
    fp.Add(t)


def stock_copy(name, full):
    lib, stock_name = full.split(':')
    fp = pcbnew.FootprintLoad(f'{STOCK}/{lib}.pretty', stock_name)
    fp.SetFPID(pcbnew.LIB_ID(D.LIB, name))
    fp.SetLibDescription(f'Copy of {full}. ' + fp.GetLibDescription())
    return fp


def pin_positions():
    """Pad -> (x, y) in the footprint, origin at the module's centre, legend upright: pads 1-6 the analogue row
    along the top, 7-12 the system row along the bottom, each left to right."""
    p, rows = C.MODULE['pitch'], C.MODULE['row_spacing'] / 2
    out = {}
    for i in range(6):
        x = round((i - 2.5) * p, 3)
        out[str(i + 1)] = (x, -rows)
        out[str(i + 7)] = (x, rows)
    return out


def module_footprint():
    fp = pcbnew.FOOTPRINT(None)
    fp.SetFPID(pcbnew.LIB_ID(D.LIB, D.MODULE_FP))
    fp.SetLibDescription('Adafruit ADS122C04 module (product 6432): two 6-pin rows 12.7 mm apart at 2.54 mm pitch; '
                         'M2.5 standoff holes under its four mounting holes. Solder the header pins; never socket it.')
    fp.SetAttributes(pcbnew.FP_THROUGH_HOLE)
    for num, (x, y) in pin_positions().items():
        p = pcbnew.PAD(fp)
        p.SetNumber(num)
        p.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        p.SetLayerSet(pcbnew.PAD.PTHMask())
        shape = pcbnew.PAD_SHAPE_RECTANGLE if num in ('1', '7') else pcbnew.PAD_SHAPE_CIRCLE
        for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
            p.SetShape(layer, shape)
            p.SetSize(layer, pcbnew.VECTOR2I(mm(PAD), mm(PAD)))
        p.SetDrillSize(pcbnew.VECTOR2I(mm(DRILL), mm(DRILL)))
        p.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        fp.Add(p)
    for (hx, hy) in C.MODULE['holes']:
        p = pcbnew.PAD(fp)
        p.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
        p.SetLayerSet(pcbnew.LSET.AllCuMask())
        p.SetShape(pcbnew.F_Cu, pcbnew.PAD_SHAPE_CIRCLE)
        p.SetSize(pcbnew.F_Cu, pcbnew.VECTOR2I(mm(HOLE_D), mm(HOLE_D)))
        p.SetDrillSize(pcbnew.VECTOR2I(mm(HOLE_D), mm(HOLE_D)))
        p.SetPosition(pcbnew.VECTOR2I(mm(hx), mm(hy)))
        fp.Add(p)
    hw, hh = C.MODULE['length'] / 2, C.MODULE['width'] / 2
    add_rect(fp, pcbnew.F_Fab, -hw, -hh, hw, hh, 0.1)
    add_rect(fp, pcbnew.F_SilkS, -hw - 0.15, -hh - 0.15, hw + 0.15, hh + 0.15, 0.15)
    add_rect(fp, pcbnew.F_CrtYd, -hw - 0.5, -hh - 0.5, hw + 0.5, hh + 0.5, 0.05)
    add_text(fp, pcbnew.F_Fab, 'ADS122C04 module', 0, 0, 1.0)
    for num, (x, y) in pin_positions().items():   # pin names on the fab layer (they're under the module)
        add_text(fp, pcbnew.F_Fab, C.MODULE_PINS[num], x, y + (1.6 if y < 0 else -1.6), 0.6)
    fp.Reference().SetLayer(pcbnew.F_SilkS)
    fp.Reference().SetPosition(pcbnew.VECTOR2I(0, mm(-hh - 1.2)))
    fp.Value().SetLayer(pcbnew.F_Fab)
    fp.Value().SetPosition(pcbnew.VECTOR2I(0, mm(hh + 1.2)))
    return fp


def symbol_lib():
    """gauge.kicad_sym with the module's symbol: system pins left, analogue pins right."""
    pins = []
    for num, (name, side) in D.MODULE_SYMBOL_PINS.items():
        order = (['VIN', 'GND', 'SCL', 'SDA', 'DRDY', '!RST'] if side == 'L'
                 else ['Ref+', 'A0', 'A1', 'A2', 'A3', 'Ref-'])
        y = 6.35 - 2.54 * order.index(name)
        x, ang = (-10.16, 0) if side == 'L' else (10.16, 180)
        pins.append(f'(pin passive line (at {x} {y:.2f} {ang}) (length 2.54) '
                    f'(name "{name}" (effects (font (size 1.27 1.27)))) (number "{num}" (effects (font (size 1.27 1.27)))))')
    body = '\n'.join(pins)
    s = D.MODULE_SYM
    return f'''(kicad_symbol_lib (version 20251024) (generator "scripts/boards/pcb_footprints.py")
(symbol "{s}" (pin_names (offset 1.016)) (exclude_from_sim no) (in_bom yes) (on_board yes)
(property "Reference" "U" (at 0 10.16 0) (effects (font (size 1.27 1.27))))
(property "Value" "{s}" (at 0 -10.16 0) (effects (font (size 1.27 1.27))))
(property "Footprint" "{D.LIB}:{D.MODULE_FP}" (at 0 -12.7 0) (effects (font (size 1.27 1.27)) (hide yes)))
(property "Datasheet" "https://www.adafruit.com/product/6432" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
(property "Description" "Adafruit ADS122C04 24-bit I2C ADC module, two 6-pin rows" (at 0 0 0) (effects (font (size 1.27 1.27)) (hide yes)))
(symbol "{s}_0_1" (rectangle (start -7.62 8.89) (end 7.62 -8.89) (stroke (width 0.254) (type default)) (fill (type background))))
(symbol "{s}_1_1"
{body}
)
(embedded_fonts no)
)
)
'''


ENVELOPES = ('MicroFit_1x04_RA', 'MicroFit_2x03_RA', D.MODULE_FP)   # KiCad 10.0.8 has no 3D model for these


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
    used = {p[2].split(':')[1] for p in D.PARTS.values()}
    for name, full in D.FOOTPRINTS.items():
        if name not in used:
            continue
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
