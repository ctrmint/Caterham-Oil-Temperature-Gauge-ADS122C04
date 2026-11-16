"""Single source of both front-end options' circuits and parts, for the stripboard and PCB generators.

Stdlib only. hardware/WIRING.md describes the circuit; tests/test_boards.py checks these tables against it.

- Option A: Rref, R1 and R2 on the ADS122C04 module's analogue pins.
- Option B: option A plus C1 across A0-A1 and C2 across A1-A2. There's never a capacitor from a pin to ground.

The boards differ from the no-board build in one way only: the harness's return wire lands on the board's ground,
so the display lead needs no fifth wire.
"""
DATE = '2026-11-16'
OPTIONS = ('a', 'b')
NC = None

# Adafruit ADS122C04 module (product 6432). Pin numbers used on both boards: 1-6 the analogue row
# Ref+ A0 A1 A2 A3 Ref-, 7-12 the system row VIN GND SCL SDA DRDY !RST, each left to right with the module's
# legend upright. From Adafruit's board file (JP1, JP2) and silkscreen.
MODULE_PINS = {'1': 'Ref+', '2': 'A0', '3': 'A1', '4': 'A2', '5': 'A3', '6': 'Ref-',
               '7': 'VIN', '8': 'GND', '9': 'SCL', '10': 'SDA', '11': 'DRDY', '12': '!RST'}
MODULE_NETS = {'1': NC, '2': 'AIN0', '3': 'AIN1', '4': 'AIN2', '5': NC, '6': NC,
               '7': '+3V3', '8': 'GND', '9': 'SCL', '10': 'SDA', '11': NC, '12': NC}
# Geometry (mm), from the same board file: a 25.4 x 17.78 board; two rows of six pins, 12.7 mm apart at 2.54 mm
# pitch, each row 2.54 mm in from a long edge and centred along it; four 2.5 mm plated mounting holes, each
# 2.54 mm in from both edges at a corner. Height above the board it's soldered to: the 2.5 mm header spacer, the
# 1.6 mm board and its parts (the tallest are the STEMMA QT sockets, about 3 mm).
MODULE = {'length': 25.4, 'width': 17.78, 'row_spacing': 12.7, 'pitch': 2.54, 'hole_d': 2.5,
          'holes': [(-10.16, -6.35), (10.16, -6.35), (-10.16, 6.35), (10.16, 6.35)], 'height': 7.5}

# Nets where a short to ground gives a believable wrong reading rather than a fault:
# - AIN1 or AIN2 to ground: the measurement turns single-ended (WIRING.md, "Option B: the input filter");
# - SENSE to ground: the ground sensing is lost, so the tank offset adds to the sender's voltage;
# - SENDER to ground: with the tank a few millivolts below the gauge's ground, Vs is that offset, not zero.
# The layouts keep their copper away from ground copper: never on a neighbouring strip, at least 1 mm on the PCB.
SENSITIVE = ('AIN1', 'AIN2', 'SENDER', 'SENSE')

# The sender harness, the same on both boards. Pin 3 is left empty so the return wire's ground never sits beside
# the sense wire. Twist the three wires together.
HARNESS = {'1': 'SENDER', '2': 'SENSE', '3': NC, '4': 'GND'}
HARNESS_WIRES = {'1': 'Signal wire to the sender spade', '2': 'Ground-sense wire to the tank bond stud',
                 '3': 'Empty: keeps the return wire away from the sense wire',
                 '4': 'Return wire to the tank bond stud (the gauge\'s ground reference at the tank)'}

# The display's 8-way case socket, left to right seen from the back: (label, net on the display lead).
DISPLAY_END = [('18', 'SCL'), ('17', 'SDA'), ('16', None), ('RX', None), ('TX', None),
               ('3.3V', '+3V3'), ('G', 'GND'), ('5V', None)]

NET_TITLES = {'+3V3': '3.3 V from the display', 'GND': 'Ground', 'SCL': 'I2C clock (display 18)',
              'SDA': 'I2C data (display 17)', 'AIN0': 'A0: current source out, top of Rref',
              'AIN1': 'A1: sender high side', 'AIN2': 'A2: sender low side',
              'SENDER': 'Signal wire to the sender', 'SENSE': 'Ground-sense wire'}


def front_end(option):
    """The measurement parts, the same on every build: ref -> (kind, value, {pin: net})."""
    if option not in OPTIONS:
        raise ValueError(option)
    parts = {
        'U1': ('module', 'Adafruit ADS122C04', dict(MODULE_NETS)),
        'Rref': ('R', '2.00k 0.1% 25ppm', {'1': 'AIN0', '2': 'SENDER'}),
        'R1': ('R', '2.2k 1%', {'1': 'AIN1', '2': 'SENDER'}),
        'R2': ('R', '2.2k 1%', {'1': 'AIN2', '2': 'SENSE'}),
    }
    if option == 'b':
        parts['C1'] = ('C', '47n C0G', {'1': 'AIN0', '2': 'AIN1'})
        parts['C2'] = ('C', '47n C0G', {'1': 'AIN1', '2': 'AIN2'})
    return parts


def nets(parts):
    out = {}
    for ref, (_kind, _value, pins) in parts.items():
        for pin, net in pins.items():
            if net:
                out.setdefault(net, set()).add((ref, pin))
    return out


# Purchasing data: (manufacturer, MPN, Digi-Key UK code, description).
# - Part numbers are given only where they were checked against the maker's data and Digi-Key UK's product page
#   (2026-11-15, for the original project's boards). Everything else is a specification: buy any part that meets it.
# - Resistor bodies: 0207 (6.5 mm long, 2.5 mm across at most), laid flat at 10.16 mm, the minimum horizontal pitch
#   in Vishay's MBB/SMA 0207 datasheet (doc 28766).
PARTS_INFO = {
    'module': ('Adafruit', '6432', '', 'ADS122C04 24-bit I2C ADC module (The Pi Hut SKU ADA6432). Solder it in by '
               'its header pins; never socket it'),
    'rref': ('', '', '', 'Rref: 2.00 kOhm, 0.1 %, 25 ppm/C or better, metal film, 0.25 W, 0207. Measure it and put '
             'the value in kReferenceOhms'),
    'r2k2': ('', '', '', '2.2 kOhm, 1 %, metal film, 0.25 W, 0207'),
    'c47n': ('', '', '', 'Option B: 47 nF, 5 %, C0G (NP0) ceramic, 50 V, radial, 2.5 mm (0.1 in) lead spacing, body '
             'no more than 4.6 mm along the line of its leads and 3.8 mm thick. Never X7R, never more than 100 nF'),
    'mpt4': ('Phoenix Contact', '1725672', '277-1275-ND', 'MPT 0,5/ 4-2,54 PCB screw terminal block, 4-way, 2.54 mm; '
             '0.14-0.5 mm2 (26-20 AWG) wire'),
    'mf1x4': ('Molex', '43650-0400', 'WM1862-ND', 'Micro-Fit 3.0 1x4 right-angle header, tin'),
    'mf2x3': ('Molex', '43045-0600', 'WM1815-ND', 'Micro-Fit 3.0 2x3 right-angle header, tin, board lock'),
    'mf1x4_housing': ('Molex', '43645-0400', 'WM1847-ND', 'Micro-Fit 3.0 1x4 receptacle housing (mates with J2)'),
    'mf2x3_housing': ('Molex', '43025-0600', 'WM1785-ND', 'Micro-Fit 3.0 2x3 receptacle housing (mates with J1)'),
    'mf_crimp': ('Molex', '43030-0007', 'WM1837-ND', 'Micro-Fit 3.0 female crimp terminal, tin, 20-24 AWG'),
    'mf_tool': ('Molex', '63819-0000', '', 'Micro-Fit 3.0 hand crimp tool (or a Micro-Fit ratchet crimper)'),
    'display_plug': ('Samtec', 'TSW-108-07-G-S', '', '8-way 2.54 mm gold pin header: the plug for the display case '
                     'socket'),
}
