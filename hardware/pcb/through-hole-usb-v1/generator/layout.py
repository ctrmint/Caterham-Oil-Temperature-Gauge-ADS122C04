"""Placement and routing for through-hole-usb-v1 (single-sided: every track on B.Cu, no vias).

mm from the board centre, viewed from the component side, +x right, +y up. Rotations are KiCad degrees
(counter-clockwise). The two connectors face down, out through the box wall along the bottom edge.

The analogue section is through-hole-v1's, moved 11.5 mm left and 1 mm up; its 12 V section is gone.
"""

WIDTH, HEIGHT, CORNER = 65.0, 62.0, 3.0
HOLES = [(-28.0, 26.5), (28.0, 26.5), (-28.0, -26.5), (28.0, -26.5)]   # M3, for the box's bosses
HOLE_KEEPOUT = 3.6     # standoff / screw-head clearance radius
ZONE_CLEARANCE = 0.4

EDGE_ROW = -HEIGHT / 2 + 9.5        # connector pins: 9.5 mm in from the edge puts the mating face at it
PLACE = {
    # ref: (x, y, rot, side). For two-lead and connector footprints (x, y) is pad 1; for U1, H* and TP* the centre.
    'J3': (-12.9, EDGE_ROW, 180, 'F'),     # row A (edge): 1-3 GND; row B: 4 +3V3, 5 SCL, 6 SDA (each above a GND)
    'J2': (20.0, EDGE_ROW, 180, 'F'),      # pads: 1 SENDER 20, 2 SENSE 17, 3 GND (return) 14, 4 GND 11
    'U1': (-15.5, 7.0, 270, 'F'),          # left column VIN GND SCL SDA (x -21.85), right column A3 A1 A0 (x -9.15)
    'H1': (HOLES[0][0], HOLES[0][1], 0, 'F'),
    'H2': (HOLES[1][0], HOLES[1][1], 0, 'F'),
    'H3': (HOLES[2][0], HOLES[2][1], 0, 'F'),
    'H4': (HOLES[3][0], HOLES[3][1], 0, 'F'),
    # display side: pull-ups across the I2C lanes, decoupling at the module
    'R8': (-9.64, -11.5, 180, 'F'),
    'R9': (-7.34, -8.5, 180, 'F'),
    'C4': (-19.3, 22.5, 0, 'F'),
    'TP1': (-28.5, 1.0, 0, 'F'),
    'TP2': (-1.5, EDGE_ROW, 0, 'F'),       # on the ground track from the harness return to the display
    'TP3': (-11.5, 27.5, 0, 'F'),
    # Analogue: caps next to the module's right column, series resistors 10.16 mm out, the taps beyond.
    # Every resistor is at 10.16 mm: Vishay's minimum pitch for a flat MBB0207.
    # band A (y 17.5): the signal wire's A3 tap
    'C3': (-4.0, 13.6, 0, 'F'),
    'R4': (6.16, 17.5, 180, 'F'),
    'D2': (8.64, 17.5, 90, 'F'),
    'D1': (11.14, 25.12, 270, 'F'),
    'R3': (23.8, 17.5, 180, 'F'),
    # band B (y 10): the sense wire's A1 tap
    'R6': (6.16, 10.0, 180, 'F'),
    'C2': (-4.0, 6.3, 0, 'F'),
    'R5': (20.08, 10.0, 180, 'F'),
    'D3': (9.04, -5.7, 90, 'F'),
    'D4': (12.04, 7.0, 270, 'F'),
    # band C (y 0.5 .. -11): A0's filter, the Kelvin tap, Rp, the sense bias, the signal TVS
    'C1': (-4.0, 0.5, 0, 'F'),
    'R2': (6.16, -3.1, 180, 'F'),
    'R1': (13.64, -6.5, 0, 'F'),
    'R7': (9.92, -11.0, 0, 'F'),
    'D5': (27.5, 6.0, 270, 'F'),
}

EXPECT = {('U1', '7'): (-21.85, 13.35), ('U1', '2'): (-9.15, 10.81), ('J3', '6'): (-18.9, EDGE_ROW + 3),
          ('J3', '1'): (-12.9, EDGE_ROW), ('J2', '1'): (20.0, EDGE_ROW), ('J2', '4'): (11.0, EDGE_ROW)}

MODEL_OVERRIDE = {}

RULES = {
    'm_MinClearance': 0.3, 'm_TrackMinWidth': 0.4, 'm_ViasMinSize': 0.6, 'm_ViasMinAnnularWidth': 0.15,
    'm_MinThroughDrill': 0.3, 'm_HoleToHoleMin': 0.5, 'm_HoleClearance': 0.3, 'm_CopperEdgeClearance': 0.5,
    'm_SilkClearance': 0.1, 'm_MinSilkTextHeight': 0.8, 'm_MinSilkTextThickness': 0.15,
}

W_SIG, W_SENDER, W_3V3, W_GND = 0.5, 0.6, 0.6, 0.8


def route(board, P, nets, fps, pad_xy):
    import pcbnew

    def N(name):
        return nets.get('/' + name) or nets[name]

    def pad(ref, num):
        return pad_xy(fps[ref], num)

    def T(net, pts, w=W_SIG):
        """A track on B.Cu (the only copper layer) through pads and points."""
        pts = [pad(*p) if isinstance(p[0], str) else p for p in pts]
        for a, b in zip(pts, pts[1:]):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(P(*a))
            t.SetEnd(P(*b))
            t.SetWidth(pcbnew.FromMM(w))
            t.SetLayer(pcbnew.B_Cu)
            t.SetNet(N(net))
            board.Add(t)

    # The module's system pins to J3's inner row, in order: SDA, SCL, 3.3 V (no crossings)
    T('SDA', [('U1', '10'), (-19.8, 5.73), (-19.8, -16.5), ('J3', '6')])
    T('SCL', [('U1', '9'), (-17.5, 8.27), (-17.5, -16.5), ('J3', '5')])
    T('+3V3', [('U1', '7'), (-12.9, 13.35), (-12.9, -16.5), ('J3', '4')], W_3V3)
    T('+3V3', [(-12.9, -8.5), ('R9', '1')], W_3V3)
    T('+3V3', [(-12.9, -11.5), ('R8', '1')], W_3V3)
    # Grounds. The module's ground runs round the left of the I2C lanes to J3's outer row; the harness return
    # (J2 pins 3 and 4) runs along the bottom, past TP2, to the display's ground pins.
    T('GND', [('U1', '8'), (-25.0, 10.81), (-25.0, EDGE_ROW), ('J3', '3')], W_GND)
    T('GND', [('TP1', '1'), (-25.0, 1.0)], W_GND)
    T('GND', [('J2', '3'), ('J2', '4'), ('TP2', '1'), ('J3', '1')], W_GND)
    # Top rail: module VIN -> C4 -> TP3 -> the A3 tap's clamp
    T('+3V3', [('U1', '7'), (-19.3, 15.0), ('C4', '1'), (-19.3, 27.5), ('TP3', '1'), (8.6, 27.5), ('D1', '1')], W_3V3)
    # Bottom rail: -> sense bias, the A1 tap's clamp, and Rp's top pad
    T('+3V3', [(-12.9, -14.0), (9.92, -14.0), ('R7', '1')], W_3V3)
    T('+3V3', [('R7', '1'), (9.04, -9.5), ('D3', '1')], W_3V3)
    T('+3V3', [('R7', '1'), (13.64, -9.0), ('R1', '1')], W_3V3)
    # A0: Kelvin from Rp's top pad, its own track, clear of D3's +3V3 pad
    T('+3V3', [('R2', '1'), (11.0, -3.1), ('R1', '1')])

    # Signal wire: harness pin -> Rp -> the A3 tap; TVS beside it. Nothing in series.
    T('SENDER', [('J2', '1'), (20.0, -18.5), (23.8, -14.7), ('R1', '2'), ('R3', '1')], W_SENDER)
    T('SENDER', [('D5', '1'), (23.8, 6.0)], W_SENDER)
    T('TAP_S', [('R3', '2'), ('D1', '2'), ('D2', '1'), ('R4', '1')])
    T('AIN3', [('R4', '2'), ('C3', '1'), (-7.0, 12.0), ('U1', '2')])

    # Sense wire: harness pin -> bias -> the A1 tap
    T('SENSE', [('J2', '2'), (17.0, -18.5), (20.08, -15.42), ('R7', '2'), ('R5', '1')])
    T('TAP_G', [('R6', '1'), ('R5', '2'), ('D3', '2'), ('D4', '1')])
    T('AIN1', [('R6', '2'), ('C2', '1'), (-7.0, 6.3), ('U1', '4')])
    T('AIN0', [('R2', '2'), ('C1', '1'), (-7.0, 2.5), ('U1', '5')])


SILK = []
# Where a centred reference would sit on a pad or another part's legend: (x, y).
REF_AT = {'TP1': (-28.5, 3.3), 'TP2': (-1.5, -24.0), 'TP3': (-8.2, 29.3),
          'J2': (15.5, -28.8), 'J3': (-15.9, -28.8)}


def pad_xy_mm(p):
    import pcbnew
    v = p.GetPosition()
    return pcbnew.ToMM(v.x) - 100, 100 - pcbnew.ToMM(v.y)


def decorate(board, P, fps):
    import pcbnew

    # Reference designators sit on each part's body (centre of its pads), small and upright, so a
    # hand assembler can read every one; the module's goes above it.
    for ref, fp in fps.items():
        r = fp.Reference()
        if ref.startswith('H'):
            r.SetVisible(False)
            continue
        if ref in REF_AT:
            x, y = REF_AT[ref]
        else:
            xs = [pad_xy_mm(p)[0] for p in fp.Pads()]
            ys = [pad_xy_mm(p)[1] for p in fp.Pads()]
            x, y = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
        r.SetPosition(P(x, y))
        r.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(1.0), pcbnew.FromMM(1.0)))
        r.SetTextThickness(pcbnew.FromMM(0.15))
        r.SetTextAngleDegrees(0)
        r.SetKeepUpright(True)
    for text, x, y, h in SILK:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(text)
        t.SetPosition(P(x, y))
        t.SetLayer(pcbnew.F_SilkS)
        t.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(h), pcbnew.FromMM(h)))
        t.SetTextThickness(pcbnew.FromMM(0.15))
        board.Add(t)
