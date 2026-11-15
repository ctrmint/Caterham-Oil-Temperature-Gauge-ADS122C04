"""Placement and routing for through-hole-v1 (single-sided: every track on B.Cu, no vias).

mm from the board centre, viewed from the component side, +x right, +y up. Rotations are KiCad degrees
(counter-clockwise). The three connectors face down, out through the box wall along the bottom edge.
"""

WIDTH, HEIGHT, CORNER = 92.0, 64.0, 3.0
HOLES = [(-41.5, 27.5), (41.5, 27.5), (-41.5, -27.5), (41.5, -27.5)]   # M3, for the box's bosses
HOLE_KEEPOUT = 3.6     # standoff / screw-head clearance radius
ZONE_CLEARANCE = 0.4

EDGE_ROW = -HEIGHT / 2 + 9.5        # connector pins: 9.5 mm in from the edge puts the mating face at it
PLACE = {
    # ref: (x, y, rot, side). For two-lead and connector footprints (x, y) is pad 1; for U1, H* and TP* the centre.
    'J1': (-26.0, EDGE_ROW, 180, 'F'),     # pads: 1 GND -26, 2 GND -29, 3 12V_IN -32
    'J3': (-0.5, EDGE_ROW, 180, 'F'),      # row A (edge): 1-3 GND, 4 5V; row B: 5 +3V3, 6 GND, 7 SCL, 8 SDA
    'J2': (29.5, EDGE_ROW, 180, 'F'),      # pads: 1 SENDER 29.5, 2 SENSE 26.5, 3 GND 23.5
    'U1': (-4.0, 6.0, 270, 'F'),           # left column VIN GND SCL SDA (x -10.35), right column A3 A1 A0 (x 2.35)
    'H1': (HOLES[0][0], HOLES[0][1], 0, 'F'),
    'H2': (HOLES[1][0], HOLES[1][1], 0, 'F'),
    'H3': (HOLES[2][0], HOLES[2][1], 0, 'F'),
    'H4': (HOLES[3][0], HOLES[3][1], 0, 'F'),
    # 12 V in -> 5 V (left)
    'F1': (-36.0, -15.0, 90, 'F'),
    'D6': (-42.0, -6.0, 270, 'F'),
    'D7': (-36.0, 7.7, 270, 'F'),
    'U2': (-27.5, 9.0, 0, 'F'),
    'C5': (-39.0, 15.0, 0, 'F'),
    'C7': (-27.5, 3.0, 0, 'F'),
    'C6': (-18.5, 21.0, 90, 'F'),
    'D8': (-16.5, -4.0, 90, 'F'),
    # display side: pull-ups across the I2C lanes, decoupling at the module
    'R8': (-0.68, -12.5, 180, 'F'),
    'R9': (1.62, -9.5, 180, 'F'),
    'C4': (-7.8, 21.5, 0, 'F'),
    'TP1': (-16.5, -28.5, 0, 'F'),
    'TP2': (15.5, -28.5, 0, 'F'),
    'TP3': (0.0, 26.5, 0, 'F'),
    'TP4': (-16.5, -14.0, 0, 'F'),
    # Analogue: caps next to the module's right column (xc 6.5), series resistors 7.62 mm out, the taps beyond.
    # band A (y 15): the signal wire's A3 tap
    'C3': (7.5, 12.6, 0, 'F'),
    'R4': (15.12, 16.5, 180, 'F'),
    'D2': (17.6, 16.5, 90, 'F'),
    'D1': (20.1, 24.12, 270, 'F'),
    'R3': (32.76, 16.5, 180, 'F'),
    # band B (y 9.2): the sense wire's A1 tap
    'R6': (15.12, 9.0, 180, 'F'),
    'C2': (7.5, 5.3, 0, 'F'),
    'R5': (26.5, 9.0, 180, 'F'),
    'D3': (18.0, -6.7, 90, 'F'),
    'D4': (21.0, 6.0, 270, 'F'),
    # band C (y -0.5 .. -12): A0's filter, the Kelvin tap, Rp, the sense bias, the signal TVS
    'C1': (7.5, -0.5, 0, 'F'),
    'R2': (15.12, -4.1, 180, 'F'),
    'R1': (22.6, -7.5, 0, 'F'),
    'R7': (18.88, -12.0, 0, 'F'),
    'D5': (36.0, 5.0, 270, 'F'),
}

EXPECT = {('U1', '7'): (-10.35, 12.35), ('U1', '2'): (2.35, 9.81), ('J3', '8'): (-9.5, EDGE_ROW + 3),
          ('J1', '3'): (-32.0, EDGE_ROW), ('J2', '1'): (29.5, EDGE_ROW)}

MODEL_OVERRIDE = {}

RULES = {
    'm_MinClearance': 0.3, 'm_TrackMinWidth': 0.4, 'm_ViasMinSize': 0.6, 'm_ViasMinAnnularWidth': 0.15,
    'm_MinThroughDrill': 0.3, 'm_HoleToHoleMin': 0.5, 'm_HoleClearance': 0.3, 'm_CopperEdgeClearance': 0.5,
    'm_SilkClearance': 0.1, 'm_MinSilkTextHeight': 0.8, 'm_MinSilkTextThickness': 0.15,
}

W_SIG, W_SENDER, W_3V3, W_PWR, W_GND = 0.5, 0.6, 0.6, 1.0, 0.8


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

    # 12 V in -> fuse -> TVS -> reverse-polarity diode -> bulk caps -> TSR 1-2450 -> 5 V
    T('12V_IN', [('J1', '3'), (-34.0, -19.0), ('F1', '1')], W_PWR)
    T('12V_F', [('F1', '2'), ('D7', '2')], W_PWR)
    T('12V_F', [('D6', '1'), (-39.0, -7.5), (-36.0, -7.5)], W_PWR)
    T('12V_P', [('D7', '1'), (-29.0, 7.7), ('U2', '1')], W_PWR)
    T('12V_P', [(-29.0, 7.7), ('C7', '1')], W_PWR)
    T('12V_P', [('D7', '1'), (-39.0, 10.7), ('C5', '1')], W_PWR)
    T('5V_REG', [('U2', '3'), (-20.0, 9.0), (-16.5, 7.5), ('D8', '2')], W_PWR)
    T('5V_REG', [(-20.0, 9.0), (-20.0, 12.0), (-18.5, 19.0), ('C6', '1')], W_PWR)
    T('5V_OUT', [('D8', '1'), ('TP4', '1'), (-16.5, -19.5), (-13.5, -22.5), ('J3', '4')], W_PWR)

    # The module's system pins to J3, in order: SDA, SCL, GND, 3.3 V (no crossings)
    T('SDA', [('U1', '10'), (-8.3, 4.73), (-8.3, -17.5), ('J3', '8')])
    T('SCL', [('U1', '9'), (-6.0, 7.27), (-6.0, -17.5), ('J3', '7')])
    T('GND', [('U1', '8'), (-3.7, 9.81), (-3.7, -17.5), ('J3', '6')], W_3V3)
    T('+3V3', [('U1', '7'), (-1.4, 12.35), (-1.4, -17.5), ('J3', '5')], W_3V3)
    T('+3V3', [(-1.4, -9.5), ('R9', '1')], W_3V3)
    T('+3V3', [(-1.4, -12.5), ('R8', '1')], W_3V3)
    # Top rail: module VIN -> C4 -> TP3 -> the A3 tap's clamp
    T('+3V3', [('U1', '7'), (-7.8, 14.0), ('C4', '1'), (-7.8, 26.5), ('TP3', '1'), (20.1, 26.5), ('D1', '1')], W_3V3)
    # Bottom rail: -> sense bias, the A1 tap's clamp, and Rp's top pad
    T('+3V3', [(-1.4, -15.0), (18.88, -15.0), ('R7', '1')], W_3V3)
    T('+3V3', [('R7', '1'), (18.0, -10.5), ('D3', '1')], W_3V3)
    T('+3V3', [('R7', '1'), (22.6, -10.0), ('R1', '1')], W_3V3)
    # A0: Kelvin from Rp's top pad, its own track
    T('+3V3', [('R2', '1'), (19.0, -4.0), ('R1', '1')])

    # Signal wire: harness pin -> Rp -> the A3 tap; TVS beside it. Nothing in series.
    T('SENDER', [('J2', '1'), (29.5, -19.0), (32.76, -16.0), ('R1', '2'), ('R3', '1')], W_SENDER)
    T('SENDER', [('D5', '1'), (32.76, 5.0)], W_SENDER)
    T('TAP_S', [('R3', '2'), ('D1', '2'), ('D2', '1'), ('R4', '1')])
    T('AIN3', [('R4', '2'), ('C3', '1'), (4.5, 11.0), ('U1', '2')])

    # Sense wire: harness pin -> bias -> the A1 tap
    T('SENSE', [('J2', '2'), ('R7', '2'), ('R5', '1')])
    T('TAP_G', [('R6', '1'), ('R5', '2'), ('D3', '2'), ('D4', '1')])
    T('AIN1', [('R6', '2'), ('C2', '1'), (4.5, 5.3), ('U1', '4')])
    T('AIN0', [('R2', '2'), ('C1', '1'), (4.5, 1.5), ('U1', '5')])


SILK = []
# Where a centred reference would sit on a pad or another part's legend: (x, y).
REF_AT = {'TP1': (-19.8, -28.5), 'TP2': (12.2, -28.5), 'TP3': (3.3, 28.3), 'TP4': (-13.2, -14.0),
          'C5': (-36.5, 21.6), 'C6': (-14.8, 22.0), 'U2': (-24.96, 13.5), 'J3': (-5.0, -29.8)}


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
