"""Placement and routing of the option A and B PCBs (two copper layers, no vias, no ground pour).

mm from the board centre, viewed from the component side, +x right, +y up. Rotations are KiCad degrees
(counter-clockwise). Both boards are the same apart from option B's C1 and C2.

Left to right:
- J1 (display lead) at the left edge, its mating face at the edge; its row B lines up with the module's VIN, SCL
  and SDA, and its grounds (row A) reach the module's GND on the top copper, over the signal tracks;
- the module, turned so its system row faces J1 and its analogue row faces the resistors;
- option B's C1 and C2, standing across the A0-A1 and A1-A2 tracks next to the module;
- Rref, R1 and R2 lying flat at 10.16 mm, one per analogue pin;
- J2 (sender harness) at the right edge: 1 SENDER, 2 SENSE, 3 empty, 4 return (GND).
Ground runs from J1 round the bottom edge to J2 pin 4, so it never comes near A1, A2, SENDER or SENSE (circuit.SENSITIVE
keeps them at least SENSITIVE_CLEARANCE from it, a DRC rule). R1 meets SENDER at J2 pin 1 by its own track.
"""
import pcb_design as D

WIDTH, HEIGHT, CORNER = 71.0, 36.0, 3.0
HOLES = [(-32.0, 14.5), (32.0, 14.5), (-32.0, -14.5), (32.0, -14.5)]   # M3, for the box's bosses
HOLE_KEEPOUT = 3.6         # standoff / screw-head clearance radius
SENSITIVE_CLEARANCE = 1.0  # mm from A1, A2, SENDER and SENSE copper to ground copper (DRC rule)

EDGE_IN = 9.5              # connector pins this far in from the edge put the mating face at it
J1_X = -WIDTH / 2 + EDGE_IN    # J1 row A (the grounds); row B 3 mm further in
J1_Y = -1.27
J2_X = WIDTH / 2 - EDGE_IN
J2_Y = 4.5                 # pin 1; pins 2-4 at 3 mm steps down
U1_X, U1_Y = -11.5, 0.0    # module centre: system row at x - 6.35, analogue row at x + 6.35
R_X = 9.5                  # resistors' pin 1 (the module side); pin 2 at + 10.16
RREF_Y, R1_Y, R2_Y = 5.2, 1.4, -2.4
C1_X, C2_X = 0.5, 5.5      # option B: C1 from A0 (pin 1, top) to A1; C2 from A1 to A2
GND_RAIL_Y = -15.5

PLACE = {
    # ref: (x, y, rot, side). For two-lead and connector footprints (x, y) is pad 1; for U1 and H* the centre.
    'J1': (J1_X, J1_Y, 90, 'F'),
    'J2': (J2_X, J2_Y, 270, 'F'),
    'U1': (U1_X, U1_Y, 270, 'F'),
    'Rref': (R_X, RREF_Y, 0, 'F'),
    'R1': (R_X, R1_Y, 0, 'F'),
    'R2': (R_X, R2_Y, 0, 'F'),
    'H1': (HOLES[0][0], HOLES[0][1], 0, 'F'),
    'H2': (HOLES[1][0], HOLES[1][1], 0, 'F'),
    'H3': (HOLES[2][0], HOLES[2][1], 0, 'F'),
    'H4': (HOLES[3][0], HOLES[3][1], 0, 'F'),
}
if D.OPTION == 'b':
    PLACE['C1'] = (C1_X, 3.81, 270, 'F')
    PLACE['C2'] = (C2_X, 1.27, 270, 'F')

# Pads that must land where the box and the cables need them.
EXPECT = {('J1', '1'): (J1_X, J1_Y), ('J1', '6'): (J1_X + 3.0, J1_Y + 6.0),
          ('J2', '1'): (J2_X, J2_Y), ('J2', '4'): (J2_X, J2_Y - 9.0),
          ('U1', '7'): (U1_X - 6.35, U1_Y + 6.35), ('U1', '2'): (U1_X + 6.35, U1_Y + 3.81),
          ('U1', '4'): (U1_X + 6.35, U1_Y - 1.27)}

MODEL_OVERRIDE = {}

RULES = {
    'm_MinClearance': 0.3, 'm_TrackMinWidth': 0.4, 'm_ViasMinSize': 0.6, 'm_ViasMinAnnularWidth': 0.15,
    'm_MinThroughDrill': 0.3, 'm_HoleToHoleMin': 0.5, 'm_HoleClearance': 0.3, 'm_CopperEdgeClearance': 0.5,
    'm_SilkClearance': 0.1, 'm_MinSilkTextHeight': 0.8, 'm_MinSilkTextThickness': 0.15,
}

W_SIG, W_3V3, W_GND = 0.5, 0.6, 0.8


def route(board, P, nets, fps, pad_xy):
    import pcbnew

    def N(name):
        return nets.get('/' + name) or nets[name]

    def pad(ref, num):
        return pad_xy(fps[ref], num)

    def T(net, pts, w=W_SIG, layer='B'):
        """A track through pads and points, on the bottom (B) or top (F) copper."""
        pts = [pad(*p) if isinstance(p[0], str) else p for p in pts]
        for a, b in zip(pts, pts[1:]):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(P(*a))
            t.SetEnd(P(*b))
            t.SetWidth(pcbnew.FromMM(w))
            t.SetLayer(pcbnew.B_Cu if layer == 'B' else pcbnew.F_Cu)
            t.SetNet(N(net))
            board.Add(t)

    b = D.OPTION == 'b'
    jb = J1_X + 3.0                      # J1 row B
    sys_x = U1_X - 6.35                  # the module's system row
    # Display lead: J1 row B straight across to the module's system pins.
    T('SDA', [('J1', '4'), ('U1', '10')])
    T('SCL', [('J1', '5'), (sys_x - 1.6, J1_Y + 3.0), ('U1', '9')])
    T('+3V3', [('J1', '6'), (jb + 3.0, U1_Y + 6.35), ('U1', '7')], W_3V3)
    # Ground: J1's row A joined; up and over the signal tracks on the top copper to the module's GND.
    T('GND', [('J1', '1'), ('J1', '2'), ('J1', '3')], W_GND)
    gx = sys_x - 1.95
    T('GND', [('J1', '3'), (J1_X + 1.5, 8.0), (gx, 8.0), (gx, U1_Y + 3.81), ('U1', '8')], W_GND, 'F')
    # The harness return: J1's grounds round the bottom edge to J2 pin 4, far from every sensitive net.
    T('GND', [('J1', '1'), (J1_X, GND_RAIL_Y), (J2_X, GND_RAIL_Y), ('J2', '4')], W_GND)

    # Analogue: the module's A0, A1 and A2 to Rref, R1 and R2 (through option B's C1 and C2).
    ana_x = U1_X + 6.35
    if b:
        T('AIN0', [('U1', '2'), ('C1', '1'), (C1_X + 1.4, RREF_Y), ('Rref', '1')])
        T('AIN1', [('U1', '3'), ('C1', '2'), ('C2', '1'), ('R1', '1')])
        T('AIN2', [('U1', '4'), ('C2', '2'), (C2_X + 1.5, R2_Y), ('R2', '1')])
    else:
        T('AIN0', [('U1', '2'), (C1_X, U1_Y + 3.81), (C1_X + 1.4, RREF_Y), ('Rref', '1')])
        T('AIN1', [('U1', '3'), (C2_X, U1_Y + 1.27), ('R1', '1')])
        T('AIN2', [('U1', '4'), (C2_X, U1_Y - 1.27), (C2_X + 1.5, R2_Y), ('R2', '1')])
    del ana_x
    # SENDER: Rref and R1 each by their own track to J2 pin 1 (R1 senses at the connector).
    r2 = R_X + 10.16
    T('SENDER', [('Rref', '2'), (J2_X - 2.0, RREF_Y), ('J2', '1')])
    T('SENDER', [('R1', '2'), (r2 + 1.44, R1_Y), (r2 + 1.44, 3.6), (J2_X - 2.0, 3.8), ('J2', '1')])
    # SENSE: R2 to J2 pin 2, clear of the empty pin 3.
    T('SENSE', [('R2', '2'), (r2 + 3.24, R2_Y), (r2 + 3.24, 0.7), (J2_X - 1.5, J2_Y - 3.0), ('J2', '2')])


# Silkscreen text: (text, x, y, height)
SILK = [
    (f'Option {D.OPTION.upper()}' + (': filtered' if D.OPTION == 'b' else ': minimal'), 12.5, 15.4, 1.2),
    ('ADS122C04 oil-temperature front end', 12.5, 13.4, 0.9),
    ('J2: 1 SENDER  2 SENSE  3 empty  4 RETURN', 14.5, 9.6, 0.8),
    ('J1 DISPLAY', -27.0, 10.0, 0.8),
    ('Rref: measure it, set kReferenceOhms', 13.0, 7.3, 0.8),
]
if D.OPTION == 'b':
    SILK.append(('C1 C2: C0G, 100 nF max', 7.5, -6.0, 0.8))
# Where a centred reference would sit on a pad or another part: (x, y).
REF_AT = {'J1': (J1_X - 2.5, J1_Y - 6.0), 'J2': (J2_X - 2.4, J2_Y - 11.0), 'U1': (U1_X, 14.2)}


def pad_xy_mm(p):
    import pcbnew
    v = p.GetPosition()
    return pcbnew.ToMM(v.x) - 100, 100 - pcbnew.ToMM(v.y)


def decorate(board, P, fps):
    import pcbnew

    # Reference designators sit on each part's body (the centre of its pads), small and upright.
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
