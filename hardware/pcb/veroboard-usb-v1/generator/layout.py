"""veroboard-usb-v1's layout on 0.1 in stripboard. Stdlib only.

Holes are (column, strip): columns 1..W run along the strips, left to right, and strips 1..H top to bottom,
seen from the component side. The board is cut along the hole lines just outside it (column 0 and W + 1,
strip 0 and H + 1), so it measures (W + 1) x (H + 1) pitches. The terminal blocks sit in the end columns, their
wire-entry faces 0.56 mm over the ends.

Strip plan:
- strips 4-9 carry the ADS1115 module's pins, with all six cuts in column 9 under it;
- 2 and 11 are full-length 3.3 V rails, 3 and 10 full-length grounds, all four passing under the module;
- 1 is Rp's Kelvin strip: 3.3 V enters at column 25, Rp's lead is at 26 and R2's (A0) at 29, the far end;
- right of the module, the A3 and A1 chains run along strips 5 and 7 (input, 1k, tap, 1k, harness wire), with
  the clamps standing across them, and A0's strip 8 runs on to R2.
"""
W, H = 33, 11

# The module, component side up, turned so its pin rows cross the strips: VIN..ALRT down column 5 and
# A+..A- down column 10, strips 4..9.
MODULE_AT = {'col': 5, 'strip': 4}

# Terminal blocks: (column, strip of pin 1, the side the wires come in from). Pins run down the strips.
BLOCKS = {
    'J3': (1, 3, 'left'),
    'J2': (33, 5, 'right'),
}

# Two-lead parts: ref: (hole of pin 1, hole of pin 2)
LEADS = {
    # left of the module: the I2C pull-ups, standing across the strips
    'R9': ((3, 2), (3, 6)),
    'R8': ((3, 7), (3, 11)),
    # filters beside the module's analogue pins
    'C3': ((12, 5), (12, 3)),
    'C2': ((12, 7), (12, 9)),
    'C1': ((14, 8), (14, 10)),
    # series 1k from each input, along its strip with a cut underneath
    'R4': ((13, 5), (17, 5)),
    'R6': ((13, 7), (17, 7)),
    'C4': ((16, 9), (16, 11)),
    # clamps from each tap to 3.3 V and to ground
    'D1': ((18, 2), (18, 5)),
    'D3': ((18, 11), (18, 7)),
    'D2': ((19, 5), (19, 10)),
    'D4': ((20, 7), (20, 10)),
    # harness-side 1k, along the strips
    'R3': ((21, 5), (25, 5)),
    'R5': ((21, 7), (25, 7)),
    # Rp from the Kelvin strip to the signal wire; R2 from the Kelvin strip's far end to A0
    'R1': ((26, 1), (26, 5)),
    'R2': ((29, 1), (29, 8)),
    'D5': ((27, 5), (27, 10)),
    'R7': ((30, 11), (30, 7)),
}

CUTS = [
    (9, 4), (9, 5), (9, 6), (9, 7), (9, 8), (9, 9),   # between the module's two rows
    (11, 4), (11, 6), (11, 9),                               # isolate A+, A2 and A- (not connected)
    (15, 5), (15, 7),                                        # under R4 and R6
    (23, 5), (23, 7),                                        # under R3 and R5
    (30, 8),                                                 # end of A0's strip
]

LINKS = [
    # under the module, fitted before it
    ((6, 2), (6, 4)),      # 3.3 V: top rail to VIN
    ((7, 4), (7, 11)),     # 3.3 V: VIN to the bottom rail
    ((8, 3), (8, 5)),      # GND: top rail to the module's GND
    ((6, 5), (6, 8)),      # GND: module GND to ADDR and J3 pin 6
    ((8, 8), (8, 10)),     # GND: to the bottom rail
    # right of the module
    ((13, 9), (13, 10)),   # GND: strip 9 to the bottom rail
    ((25, 2), (25, 1)),    # 3.3 V into the Kelvin strip
    ((30, 3), (30, 6)),    # GND: top rail to strip 6 (J2 pin 2)
    ((31, 8), (31, 9)),    # GND: J2 pin 4's strip to strip 9
]
