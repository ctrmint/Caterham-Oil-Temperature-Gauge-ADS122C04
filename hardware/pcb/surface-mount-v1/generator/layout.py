"""Placement and routing for the interface board.

mm from the display case centre, viewed from the board's top side (the case back),
+x right, +y up. Rotations are KiCad degrees (counter-clockwise).
"""

# Case geometry, from Waveshare's ESP32-S3-Touch-AMOLED-1.75-B drawing (case 51 mm across; header row
# 18.70 mm below centre at 2.54 mm pitch; bottom screws at (+-13.75, -14.70); SPK/BAT connectors from y = +15.6).
CASE_RADIUS = 25.5
R_EDGE = 24.5          # 1 mm inside the case rim
Y_TOP = -1.5
X_CLIP = 19.5
HOLES = [(-13.75, -14.7), (13.75, -14.7)]   # the case's bottom screws (Waveshare -B drawing)
HOLE_KEEPOUT = 2.6     # screw head clearance radius
ZONE_CLEARANCE = 0.25

J1_Y = -18.70
PLACE = {
    # ref: (x, y, rot, side)
    'J1': (-8.89, J1_Y, 270, 'B'),
    'J2': (-4.5, -4.9, 0, 'F'),
    'H1': (HOLES[0][0], HOLES[0][1], 0, 'F'),
    'H2': (HOLES[1][0], HOLES[1][1], 0, 'F'),
    # Signal wire (left wing): TVS, Rp, the A3 tap's first stage and clamp
    'D3': (-12.5, -4.2, 180, 'F'),
    'R1': (-11.2, -7.9, 0, 'F'),
    'R3': (-11.2, -10.6, 180, 'F'),
    'D1': (-15.9, -10.0, 0, 'F'),
    'R4': (-10.0, -12.6, 0, 'F'),
    # ADC and filters (centre)
    'U1': (-3.0, -13.8, 180, 'F'),
    'C3': (-6.0, -11.0, 0, 'F'),
    'C4': (-7.4, -13.8, 90, 'F'),
    'R8': (-3.6, -17.0, 180, 'F'),
    'R9': (-10.4, -16.9, 270, 'F'),
    'C2': (1.7, -11.4, 180, 'F'),
    'R6': (4.8, -11.4, 180, 'F'),
    'C1': (1.7, -14.2, 0, 'F'),
    'R2': (1.7, -16.0, 180, 'F'),
    # Sense wire (right wing): bias, the A1 tap's first stage and clamp
    'R5': (11.6, -4.4, 0, 'F'),
    'R7': (11.6, -6.9, 180, 'F'),
    'D2': (16.1, -4.4, 180, 'F'),
    # 5 V feed
    'D4': (9.3, -13.5, 90, 'F'),
}

MODEL_OVERRIDE = {'J2': '${KIPRJMOD}/3d/Molex_43650-0415_envelope.wrl'}

EXPECT = {('J1', '1'): (-8.89, J1_Y), ('J1', '8'): (8.89, J1_Y), ('J2', '1'): (-4.5, -4.9)}

RULES = {
    'm_MinClearance': 0.2, 'm_TrackMinWidth': 0.2, 'm_ViasMinSize': 0.6, 'm_ViasMinAnnularWidth': 0.15,
    'm_MinThroughDrill': 0.3, 'm_HoleToHoleMin': 0.5, 'm_HoleClearance': 0.25, 'm_CopperEdgeClearance': 0.5,
    'm_SilkClearance': 0.1, 'm_MinSilkTextHeight': 0.8, 'm_MinSilkTextThickness': 0.15,
}

W_SIG, W_SENDER, W_3V3, W_PWR, W_GND = 0.25, 0.4, 0.3, 0.6, 0.4
VIA_D, VIA_DRILL = 0.6, 0.3


def route(board, P, nets, fps, pad_xy):
    import pcbnew

    def N(name):
        return nets.get('/' + name) or nets[name]

    def pad(ref, num):
        return pad_xy(fps[ref], num)

    def T(net, pts, w=W_SIG, layer=pcbnew.F_Cu):
        pts = [pad(*p) if isinstance(p[0], str) else p for p in pts]
        for a, b in zip(pts, pts[1:]):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(P(*a))
            t.SetEnd(P(*b))
            t.SetWidth(pcbnew.FromMM(w))
            t.SetLayer(layer)
            t.SetNet(N(net))
            board.Add(t)

    def B(net, pts, w=W_SIG):
        T(net, pts, w, pcbnew.B_Cu)

    def V(net, x, y):
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(P(x, y))
        v.SetWidth(pcbnew.FromMM(VIA_D))
        v.SetDrill(pcbnew.FromMM(VIA_DRILL))
        v.SetNet(N(net))
        board.Add(v)
        return (x, y)

    # SENDER: harness pin -> one bus -> TVS, Rp and the A3 tap resistor. Nothing in series.
    T('SENDER', [('J2', '1'), (-8.7, -4.9), ('D3', '1')], W_SENDER)
    T('SENDER', [(-8.7, -4.9), (-8.7, -10.6), ('R3', '1')], W_SENDER)
    T('SENDER', [(-8.7, -7.9), ('R1', '2')], W_SENDER)

    # A3 tap: R3 -> clamp D1 -> R4 -> C3 -> U1 AIN3
    T('TAP_S', [('R3', '2'), ('D1', '3')])
    T('TAP_S', [('R3', '2'), (-12.66, -11.6), (-12.0, -12.3), ('R4', '1')])
    T('AIN3', [('R4', '2'), (-9.0, -11.95), (-7.3, -11.95), ('C3', '1')])
    T('AIN3', [('C3', '1'), (-6.6, -11.7), (-6.6, -13.3), ('U1', '7')])

    # A1 tap: sense pin -> R5 -> clamp D2 -> R6 -> C2 -> U1 AIN1; R7 biases the harness side.
    T('SENSE', [('J2', '4'), (9.4, -4.9), ('R5', '1')])
    T('SENSE', [('R5', '1'), (10.14, -5.6), ('R7', '2')])
    T('TAP_G', [('R5', '2'), ('D2', '3')])
    T('TAP_G', [('D2', '3'), (14.6, -5.4), (14.6, -8.6), (8.0, -9.6), (6.2, -10.6), ('R6', '1')])
    T('AIN1', [('R6', '2'), ('C2', '1')])
    T('AIN1', [('C2', '1'), (2.3, -12.5), (-0.2, -12.8), ('U1', '5')])

    # A0: Kelvin from Rp's top pad (bottom layer, its own trace) -> R2 -> C1 -> U1 AIN0
    V('+3V3', -12.66, -6.4)
    T('+3V3', [(-12.66, -6.4), ('R1', '1')], W_SIG)
    B('+3V3', [(-12.66, -6.4), (-6.0, -6.4), (-2.0, -11.0), (3.3, -15.2), (3.3, -16.0)], W_SIG)
    V('+3V3', 3.3, -16.0)
    T('+3V3', [(3.3, -16.0), ('R2', '1')], W_SIG)
    T('AIN0', [('R2', '2'), ('C1', '1')])
    T('AIN0', [('C1', '1'), (0.875, -13.6), (0.4, -13.3), ('U1', '4')])

    # I2C: SDA under the ADC body to R8 and J1.2; SCL round the outside to J1.1 and R9.
    T('SDA', [('U1', '9'), (-3.9, -14.3), (-3.9, -16.4), ('R8', '2')])
    T('SDA', [('R8', '2'), (-5.6, -17.6), ('J1', '2')])
    T('SCL', [('U1', '10'), (-6.6, -15.5), (-8.89, -17.0), ('J1', '1')])
    T('SCL', [('R9', '2'), (-9.74, -18.2), ('J1', '1')])

    # +3V3 supply: J1.6 -> bottom-layer feeders -> vias at each load.
    B('+3V3', [('J1', '6'), (2.6, -17.3), (-9.4, -17.3), (-9.4, -15.3)], W_3V3)
    V('+3V3', -9.4, -15.3)
    T('+3V3', [(-9.4, -15.3), ('R9', '1')], W_3V3)
    B('+3V3', [(-9.4, -15.3), (-8.4, -14.6)], W_3V3)
    V('+3V3', -8.4, -14.6)
    T('+3V3', [(-8.4, -14.6), ('C4', '1')], W_3V3)
    T('+3V3', [('C4', '1'), (-6.9, -14.4), (-6.3, -13.8), ('U1', '8')], W_SIG)
    V('+3V3', -1.7, -17.0)
    B('+3V3', [(-1.7, -17.3), (-1.7, -17.0)], W_3V3)
    T('+3V3', [(-1.7, -17.0), ('R8', '1')], W_3V3)
    B('+3V3', [(-9.4, -15.3), (-10.6, -12.0), (-13.9, -8.6)], W_3V3)
    V('+3V3', -13.9, -8.6)
    T('+3V3', [(-13.9, -8.6), ('R1', '1')], W_3V3)
    B('+3V3', [(-13.9, -8.6), (-17.0, -12.2)], W_3V3)
    V('+3V3', -17.0, -12.2)
    T('+3V3', [(-17.0, -12.2), ('D1', '2')], W_3V3)
    B('+3V3', [('J1', '6'), (5.0, -17.5), (10.3, -17.5), (10.3, -9.0), (13.4, -7.6)], W_3V3)
    V('+3V3', 13.4, -7.6)
    T('+3V3', [(13.4, -7.6), ('R7', '1')], W_3V3)
    B('+3V3', [(13.4, -7.6), (16.0, -2.4)], W_3V3)
    V('+3V3', 16.0, -2.4)
    T('+3V3', [(16.0, -2.4), ('D2', '2')], W_3V3)

    # 5 V: harness pin 3 -> (bottom) -> D4 -> J1.8
    B('5V_IN', [('J2', '3'), (1.5, -8.6), (7.5, -11.5)], W_PWR)
    V('5V_IN', 7.5, -11.5)
    T('5V_IN', [(7.5, -11.5), ('D4', '2')], W_PWR)
    B('5V_IN', [(7.5, -11.5), (6.6, -12.0)], W_PWR)   # second via for the supply current
    V('5V_IN', 6.6, -12.0)
    T('5V_IN', [(6.6, -12.0), (7.5, -11.5)], W_PWR)
    T('5V_DISP', [('D4', '1'), ('J1', '8')], W_PWR)

    # GND: pads to stitching vias; both layers are GND pours.
    for ref, num, vx, vy in (('D3', '2', -16.6, -4.2), ('D1', '1', -17.0, -7.8), ('C3', '2', -4.2, -11.0),
                             ('C4', '2', -8.3, -13.4), ('C1', '2', 3.4, -13.4), ('C2', '2', 0.875, -10.2),
                             ('D2', '1', 18.5, -5.5)):
        V('GND', vx, vy)
        T('GND', [(ref, num), (vx, vy)], W_GND)
    for vx, vy in ((-18.6, -2.9), (17.8, -9.6), (16.6, -12.0), (-6.9, -8.9), (-18.6, -13.9), (6.5, -16.0)):
        V('GND', vx, vy)   # stitching: ties the two GND pours together
    V('GND', -2.4, -14.2)
    T('GND', [('U1', '3'), (-2.0, -13.8), (-2.4, -14.2)], W_SIG)
    T('GND', [('U1', '1'), (-2.0, -14.8), (-2.4, -14.2)], W_SIG)


SILK = [
    # (text, x, y, height) on the top silkscreen
    ('SIG', -4.5, -9.4, 0.8), ('GND', -1.5, -9.4, 0.8), ('5V', 1.5, -9.4, 0.8), ('SNS', 4.5, -9.4, 0.8),
    ('18', -8.89, -20.4, 0.8), ('17', -6.35, -20.4, 0.8), ('16', -3.81, -20.4, 0.8), ('RX', -1.27, -20.4, 0.8),
    ('TX', 1.27, -20.4, 0.8), ('3V3', 3.81, -20.4, 0.8), ('G', 6.35, -20.4, 0.8), ('5V', 8.89, -20.4, 0.8),
    ('SURFACE-MOUNT-V1', 0.0, -22.0, 1.0),
]


def decorate(board, P, fps):
    import pcbnew

    # Reference designators live on the fab layer (assembly drawing); the top silkscreen carries
    # only the connector pin functions, J1's case-label order and the board name.
    for ref, fp in fps.items():
        if ref != 'J1':
            fp.Reference().SetVisible(False)
    for text, x, y, h in SILK:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(text)
        t.SetPosition(P(x, y))
        t.SetLayer(pcbnew.F_SilkS)
        t.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(h), pcbnew.FromMM(h)))
        t.SetTextThickness(pcbnew.FromMM(0.15))
        board.Add(t)
