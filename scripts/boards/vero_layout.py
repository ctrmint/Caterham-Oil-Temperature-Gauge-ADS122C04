"""Stripboard layouts for options A and B on 0.1 in stripboard. Stdlib only.

Holes are (column, strip): columns 1..W run along the strips, left to right, and strips 1..H top to bottom, seen
from the component side. The board is cut along the hole lines just outside it (column 0 and W + 1, strip 0 and
H + 1), so it measures (W + 1) x (H + 1) pitches. The terminal blocks sit in the end columns.

Strip plan (both options):
- the module stands component side up, turned so its pin rows cross the strips: VIN GND SCL SDA DRDY !RST down
  column 6 and Ref+ A0 A1 A2 A3 Ref- down column 11, on strips 3-8, with the six cuts between the rows in column 8;
- J1 (display lead) at the left end lands directly on the module's VIN, GND, SCL and SDA strips;
- right of the module, A0, A1 and A2 run along strips 4-6 to Rref, R1 and R2, each lying along its strip over a
  cut; strip 4 beyond Rref joins strip 5 beyond R1 (SENDER) by a link next to J2, and strip 6 beyond R2 is SENSE;
- ground reaches J2 pin 4 along strip 9, linked from the module's ground strip at column 3 and up to J2's strip
  at column 26. J2 pin 3 is empty, so no ground strip lies beside SENSE, SENDER, A1 or A2 anywhere;
- option B adds C1 across strips 4-5 and C2 across strips 5-6, next to the module. Option A leaves their holes free.
"""
import circuit as C

W, H = 28, 10
MODULE_AT = {'col': 6, 'strip': 3}   # the system row's column and the first pin's strip
ROW_SPACING = 5                      # holes between the module's rows (12.7 mm)

# Terminal blocks: (column, strip of pin 1, the side the wires come in from). Pins run down the strips.
BLOCKS = {
    'J1': (1, 3, 'left'),
    'J2': (28, 5, 'right'),
}

# Two-lead parts: ref -> (hole of pin 1, hole of pin 2)
LEADS_A = {
    'Rref': ((16, 4), (20, 4)),
    'R1': ((21, 5), (25, 5)),
    'R2': ((16, 6), (20, 6)),
}
LEADS_B = dict(LEADS_A, C1=((13, 4), (13, 5)), C2=((15, 5), (15, 6)))

CUTS = [
    (8, 3), (8, 4), (8, 5), (8, 6), (8, 7), (8, 8),   # between the module's two rows
    (12, 7),                                          # A3 apart from J2 pin 3 (both unconnected)
    (12, 8),                                          # Ref- apart from the ground strip to J2 pin 4
    (18, 4), (23, 5), (18, 6),                        # under Rref, R1 and R2
]

LINKS = [
    ((3, 4), (3, 9)),      # ground: the module's ground strip down to the ground strip
    ((26, 4), (26, 5)),    # SENDER: Rref's strip to R1's, next to J2
    ((26, 8), (26, 9)),    # ground: the ground strip up to J2 pin 4
]

# J1: the display lead, in the module's own order. J2: the sender harness (circuit.HARNESS).
J1_PINS = {'1': '+3V3', '2': 'GND', '3': 'SCL', '4': 'SDA'}
J1_WIRES = {'1': 'Display 3.3V', '2': 'Display G', '3': 'Display 18 (SCL)', '4': 'Display 17 (SDA)'}


def parts(option):
    """Every part on the stripboard: ref -> (kind, value, {pin: net})."""
    out = {}
    for ref, (kind, value, pins) in C.front_end(option).items():
        out[ref] = ({'R': 'R0207', 'C': 'RAD2'}.get(kind, kind), value, pins)
    out['J1'] = ('MPT4', 'Display lead', dict(J1_PINS))
    out['J2'] = ('MPT4', 'Sender harness', dict(C.HARNESS))
    return out


def leads(option):
    return dict(LEADS_B if option == 'b' else LEADS_A)
