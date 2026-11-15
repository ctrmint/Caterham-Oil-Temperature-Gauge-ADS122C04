"""Single source of through-hole-usb-v1's parts and nets.

gen_sch.py draws the schematic from this and gen_pcb.py builds the board from it, so the two can't
disagree; kicad-cli's DRC schematic-parity check confirms it.

Every footprint comes from the board's own library (`through-hole-v1:`): single-sided copies of KiCad's
stock footprints (pads on B.Cu only, enlarged for a board without plated holes), written by
gen_footprints.py, plus the Adafruit ADS1115 module.

The board is through-hole-v1 without its 12 V supply: the display runs from its own USB-C socket, and the
board takes the display's 3.3 V over the cable. Nothing then ties the board's ground to the car, so the
harness brings a dedicated ground return back from the tank (J2 pin 3), beside the ground-sense wire.

Every resistor lies flat at 10.16 mm: Vishay's MBB/SMA 0207 datasheet (doc 28766) gives a 10.0 mm minimum
pitch for horizontal mounting, and a different (L0) version for 7.5 to 10 mm.
"""
PROJECT = 'through-hole-usb-v1'
TITLE = 'Oil-temperature gauge interface: through-hole-usb-v1'
REV = 'v1'
DATE = '2026-11-15'
LIB = PROJECT          # the board's own footprint and symbol library nickname

NC = None  # no connection

# Single-sided footprints: name in the board's library -> the stock KiCad footprint it's made from.
FOOTPRINTS = {
    'R_P10.16mm': 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal',
    'C_P5.00mm': 'Capacitor_THT:C_Rect_L7.0mm_W3.5mm_P5.00mm',
    'D_DO-35_P7.62mm': 'Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal',
    'D_DO-35_P12.70mm': 'Diode_THT:D_DO-35_SOD27_P12.70mm_Horizontal',
    'D_DO-15_P12.70mm': 'Diode_THT:D_DO-15_P12.70mm_Horizontal',
    'MicroFit_1x04_RA': 'Connector_Molex:Molex_Micro-Fit_3.0_43650-0400_1x04_P3.00mm_Horizontal',
    'MicroFit_2x03_RA': 'Connector_Molex:Molex_Micro-Fit_3.0_43045-0600_2x03_P3.00mm_Horizontal',
    'TestPoint': 'TestPoint:TestPoint_Keystone_5000-5004_Miniature',
    'MountingHole_M3': 'MountingHole:MountingHole_3.2mm_M3',
}
MODULE_FP = 'Adafruit_ADS1115_STEMMA_QT'   # drawn by gen_footprints.py, not copied


def fp(name):
    return f'{LIB}:{name}'


# ref: (lib_id, value, footprint, {pin: net}, sch placement (x, y, rot, mirror), fields)
PARTS = {
    # Connectors. A ground beside every display-cable signal; on the harness, a dedicated ground return to the
    # tank (pin 3) beside the ground-sense wire, and a second ground for the screen or a second return (pin 4).
    'J2': ('Connector_Generic:Conn_01x04', 'Sender harness', fp('MicroFit_1x04_RA'),
           {'1': 'SENDER', '2': 'SENSE', '3': 'GND', '4': 'GND'}, (264.16, 101.6, 0, None), {}),
    'J3': ('Connector_Generic:Conn_02x03_Top_Bottom', 'Display cable', fp('MicroFit_2x03_RA'),
           {'1': 'GND', '2': 'GND', '3': 'GND', '4': '+3V3', '5': 'SCL', '6': 'SDA'},
           (35.56, 152.4, 0, None), {}),
    # ADC: the Adafruit module, soldered in by its pins (never socketed: #39).
    'U1': (f'{LIB}:ADS1115_Module', 'Adafruit ADS1115', fp(MODULE_FP),
           {'1': NC, '2': 'AIN3', '3': NC, '4': 'AIN1', '5': 'AIN0', '6': NC,
            '7': '+3V3', '8': 'GND', '9': 'SCL', '10': 'SDA', '11': NC, '12': NC},
           (127.0, 101.6, 0, None), {}),
    # Signal wire: Rp, TVS, the A3 tap (1k, clamp, 1k)
    'R1': ('Device:R', '470R 0.1% 25ppm', fp('R_P10.16mm'),
           {'1': '+3V3', '2': 'SENDER'}, (213.36, 76.2, 0, None), {}),
    'D5': ('Device:D_Zener', 'P6KE18A', fp('D_DO-15_P12.70mm'),
           {'1': 'SENDER', '2': 'GND'}, (246.38, 91.44, 270, None), {}),
    'R3': ('Device:R', '1k 0.6W', fp('R_P10.16mm'),
           {'1': 'SENDER', '2': 'TAP_S'}, (213.36, 101.6, 270, None), {}),
    'D1': ('Device:D', '1N4148', fp('D_DO-35_P7.62mm'),
           {'1': '+3V3', '2': 'TAP_S'}, (195.58, 111.76, 90, None), {}),
    'D2': ('Device:D', '1N4148', fp('D_DO-35_P7.62mm'),
           {'1': 'TAP_S', '2': 'GND'}, (205.74, 121.92, 90, None), {}),
    'R4': ('Device:R', '1k', fp('R_P10.16mm'),
           {'1': 'TAP_S', '2': 'AIN3'}, (177.8, 101.6, 90, None), {}),
    'C3': ('Device:C', '100n', fp('C_P5.00mm'),
           {'1': 'AIN3', '2': 'GND'}, (162.56, 109.22, 0, None), {}),
    # A0: Kelvin from Rp's top pad
    'R2': ('Device:R', '1k', fp('R_P10.16mm'),
           {'1': '+3V3', '2': 'AIN0'}, (177.8, 76.2, 90, None), {}),
    'C1': ('Device:C', '100n', fp('C_P5.00mm'),
           {'1': 'AIN0', '2': 'GND'}, (162.56, 83.82, 0, None), {}),
    # Sense wire: bias on the harness side, the A1 tap (1k, clamp, 1k)
    'R7': ('Device:R', '100k', fp('R_P10.16mm'),
           {'1': '+3V3', '2': 'SENSE'}, (236.22, 132.08, 0, None), {}),
    'R5': ('Device:R', '1k 0.6W', fp('R_P10.16mm'),
           {'1': 'SENSE', '2': 'TAP_G'}, (213.36, 142.24, 270, None), {}),
    'D3': ('Device:D', '1N4148', fp('D_DO-35_P12.70mm'),
           {'1': '+3V3', '2': 'TAP_G'}, (195.58, 152.4, 90, None), {}),
    'D4': ('Device:D', '1N4148', fp('D_DO-35_P7.62mm'),
           {'1': 'TAP_G', '2': 'GND'}, (205.74, 162.56, 90, None), {}),
    'R6': ('Device:R', '1k', fp('R_P10.16mm'),
           {'1': 'TAP_G', '2': 'AIN1'}, (177.8, 142.24, 90, None), {}),
    'C2': ('Device:C', '100n', fp('C_P5.00mm'),
           {'1': 'AIN1', '2': 'GND'}, (162.56, 149.86, 0, None), {}),
    # I2C pull-ups (stronger, for the cable; the module adds 10k) and 3.3 V decoupling at the module
    'R8': ('Device:R', '3k3', fp('R_P10.16mm'),
           {'1': '+3V3', '2': 'SDA'}, (88.9, 132.08, 0, None), {}),
    'R9': ('Device:R', '3k3', fp('R_P10.16mm'),
           {'1': '+3V3', '2': 'SCL'}, (99.06, 132.08, 0, None), {}),
    'C4': ('Device:C', '100n', fp('C_P5.00mm'),
           {'1': '+3V3', '2': 'GND'}, (111.76, 132.08, 0, None), {}),
    # Test points: two grounds for the bench (TP2 beside the harness return), plus the 3.3 V rail
    'TP1': ('Connector:TestPoint', 'GND', fp('TestPoint'), {'1': 'GND'}, (63.5, 172.72, 0, None), {}),
    'TP2': ('Connector:TestPoint', 'GND', fp('TestPoint'), {'1': 'GND'}, (73.66, 172.72, 0, None), {}),
    'TP3': ('Connector:TestPoint', '+3V3', fp('TestPoint'), {'1': '+3V3'}, (83.82, 172.72, 0, None), {}),
    'H1': ('Mechanical:MountingHole', 'M3', fp('MountingHole_M3'), {}, (114.3, 172.72, 0, None), {}),
    'H2': ('Mechanical:MountingHole', 'M3', fp('MountingHole_M3'), {}, (121.92, 172.72, 0, None), {}),
    'H3': ('Mechanical:MountingHole', 'M3', fp('MountingHole_M3'), {}, (129.54, 172.72, 0, None), {}),
    'H4': ('Mechanical:MountingHole', 'M3', fp('MountingHole_M3'), {}, (137.16, 172.72, 0, None), {}),
}

# Purchasing data, checked 2026-11-15 against manufacturer datasheets and Digi-Key UK part pages (stock seen then;
# J2, J3 and their housings by a separate check on the same day).
# ref: (manufacturer, MPN, Digi-Key UK code, description). The code is blank where it wasn't confirmed on the
# page itself: order those by MPN.
_R1K = ('Vishay', 'MBB02070C1001FCT00', 'BC3566CT-ND', '1k 1% 0.6 W metal film, 0207')
_R1K_FAULT = ('Vishay', 'MBB02070C1001FCT00', 'BC3566CT-ND',
              '1k 1% 0.6 W metal film, 0207 (about 0.11 W if a harness wire reaches 14.4 V)')
_C100N = ('TDK', 'FG26C0G1H104JNT06', '445-173396-1-ND', '100n 5% 50 V C0G, 5 mm leads')
_1N4148 = ('Vishay', '1N4148-TAP', '1N4148-TAPCT-ND', 'Small-signal diode, DO-35, 25 nA max leakage at 20 V, 25 C')
_R3K3 = ('Vishay', 'MBB02070C3301FCT00', 'BC3743CT-ND', '3k3 1% 0.6 W; I2C pull-up for the cable (the module adds 10k)')
BOM = {
    'J2': ('Molex', '43650-0400', 'WM1862-ND', 'Micro-Fit 3.0 1x4 right-angle header, tin; sender harness with its ground return'),
    'J3': ('Molex', '43045-0600', 'WM1815-ND', 'Micro-Fit 3.0 2x3 right-angle header, tin, board lock; display cable'),
    'U1': ('Adafruit', '1085', '', 'ADS1115 breakout (STEMMA QT). Solder its header pins into the board, '
           'with 2.5 mm M2.5 nylon standoffs at its four holes'),
    'C1': _C100N, 'C2': _C100N, 'C3': _C100N, 'C4': _C100N,
    'R1': ('Vishay', 'MBB0207VD4700BC100', '56-MBB0207VD4700BC100CT-ND',
           'Rp: 470R 0.1% 25 ppm/K, 0207, 0.4 W; measure it before fitting (kPullUpOhms)'),
    'R2': _R1K, 'R4': _R1K, 'R6': _R1K, 'R3': _R1K_FAULT, 'R5': _R1K_FAULT,
    'R7': ('Vishay', 'MBB02070C1003FCT00', 'BC3571CT-ND', '100k 1% 0.6 W; break-detect bias on the sense wire'),
    'R8': _R3K3, 'R9': _R3K3,
    'D1': _1N4148, 'D2': _1N4148, 'D3': _1N4148, 'D4': _1N4148,
    'D5': ('Vishay', 'P6KE18A-E3/54', 'P6KE18A-E3/54GICT-ND', 'TVS 600 W, 15.3 V standoff, unidirectional, DO-15; '
           'signal wire'),
    'TP1': ('Keystone', '5001', '36-5001-ND', 'Test point loop, black, 1.0 mm hole'),
    'TP2': ('Keystone', '5001', '36-5001-ND', 'Test point loop, black, 1.0 mm hole'),
    'TP3': ('Keystone', '5001', '36-5001-ND', 'Test point loop, black, 1.0 mm hole'),
}

# Parts that aren't on the board but are needed to connect and box it. (item, qty, mfr, MPN, Digi-Key UK, notes)
EXTRAS = [
    ('Harness plug housing (mates with J2)', '1', 'Molex', '43645-0400', 'WM1847-ND',
     'Micro-Fit 3.0 1x4 receptacle housing (Molex calls it a receptacle; its 43640 "plug" is a different part)'),
    ('Display-cable plug housing (mates with J3)', '1', 'Molex', '43025-0600', 'WM1785-ND',
     'Micro-Fit 3.0 2x3 receptacle housing'),
    ('Crimp terminals', '10 (+spares)', 'Molex', '43030-0007', 'WM1837-ND',
     'Tin, loose piece, for both housings; 20-24 AWG (0.5-0.2 mm2), insulation 1.42-1.85 mm'),
    ('Crimp tool', '1', 'Molex', '63819-0000', '', 'Or a Micro-Fit ratchet crimper (as the other boards)'),
    ('Display end of the cable', '1', 'Samtec', 'TSW-108-07-G-S', '',
     '8-way 2.54 mm gold pin header, wired by hand (see README) and pushed into the display case socket'),
    ('Display cable', '30 cm or less', '', '3 twisted pairs, 24 AWG stranded', '',
     'Pairs: 3.3V+GND, SCL+GND, SDA+GND. Keep it short: it carries the I2C bus. No 5 V wire'),
    ('Display power', '1', '', 'USB-C supply, 5 V, 1 A or more', '',
     'In the car, a USB-C charger on an ignition-switched 12 V circuit; on the bench, a USB-C supply or a laptop. '
     'It plugs into the display, not this board'),
    ('Module standoffs', '4', '', 'M2.5 x 2.5 mm nylon spacer, M2.5 nylon screws and nuts', '',
     'Hold the ADS1115 module down in a vibrating car'),
    ('Board mounting', '4', '', 'M3 screws into the box\'s bosses', '', 'See box/through-hole-usb-v1-box.svg'),
    ('Harness wire', 'as needed', '', '0.5 mm2 (20 AWG)', '',
     'Three wires to the tank, twisted together: signal (J2 pin 1), ground sense (pin 2) and ground return (pin 3). '
     'Or a 3-core screened cable, screen to J2 pin 4 at this end only'),
]

# PWR_FLAGs for ERC: rails that arrive through connectors. (net, x, y) on the schematic.
PWR_FLAGS = [('+3V3', 50.8, 182.88), ('GND', 101.6, 182.88)]

SCH_NOTES = [
    (25.4, 20.32, 'through-hole-usb-v1: one copper layer, through-hole parts only, in its own box. A cable joins J3 to the display\'s\\n'
                  '8-way case socket (3.3V, SDA, SCL, each paired with a ground). The display runs from its own USB-C socket.'),
    (160.02, 20.32, 'Ratiometric, differential measurement (hardware/WIRING.md):\\n'
                    'Vp = A0 - A3 across R1 (Rp), Vs = A3 - A1 across the sender, Rs = Rp x Vs / Vp.\\n'
                    'Layout rules: R2 joins R1 pin 1 by its own track (Kelvin); nothing in series between R1 and J2 pin 1;\\n'
                    'R7 (break-detect bias) stays on the harness side of R5. C1-C3 return to GND; the module ties A- to GND itself.'),
    (25.4, 35.56, 'Grounds: a USB supply needn\'t share the car\'s ground, so J2 pin 3 is a dedicated return wire to the tank bond stud.\\n'
                   'It carries the sender current back to the board. Without it no current flows and the tank floats up to 3.3 V,\\n'
                   'so the gauge shows GROUND FAULT, never a reading. J2 pin 4 takes the screen or a second return.'),
    (160.02, 38.1, 'Harness faults (a wire at 14.4 V): R3/R5 limit clamp current to about 10 mA each into +3V3;\\n'
                     'D1-D4 hold the taps inside GND..3.3 V; D5 limits transients on the signal wire and does not conduct at 14.4 V.'),
]

# Connector pinouts for BOM.md: (ref, title, {pin: where the wire goes})
PINOUTS = [
    ('J2', 'sender harness (Micro-Fit 3.0, 1x4)', {'1': 'Sender spade terminal (signal)',
                                                   '2': 'Ground-sense wire to the tank bond stud',
                                                   '3': 'Ground return to the tank bond stud (required: the board\'s ground reference)',
                                                   '4': 'Harness screen at this end only, or a second return wire to the same stud'}),
    ('J3', 'display cable (Micro-Fit 3.0, 2x3)', {'1': 'Display G (paired with 3.3 V, pin 4)', '2': 'Display G (paired with SCL, pin 5)',
                                                  '3': 'Display G (paired with SDA, pin 6)',
                                                  '4': 'Display 3.3V (supplied by the display)',
                                                  '5': 'Display 18 (SCL)', '6': 'Display 17 (SDA)'}),
]
# The display's 8-way case socket, left to right seen from the back: (label, net on the cable)
DISPLAY_END = [('18', 'SCL'), ('17', 'SDA'), ('16', None), ('RX', None), ('TX', None),
               ('3.3V', '+3V3'), ('G', 'GND'), ('5V', None)]

# Longer schematic stubs where power symbols would otherwise crowd neighbouring pins.
SCH_STUB = {('J2', '3'): 7.62, ('J3', '1'): 12.7, ('J3', '2'): 7.62, ('U1', '8'): 12.7}

# Nets whose pins are drawn with power symbols rather than labels.
POWER_NETS = {'+3V3': 'power:+3V3', 'GND': 'power:GND'}

# The module's symbol, kept in the board's own symbol library (through-hole-v1.kicad_sym).
MODULE_PINS = {   # pin: (name, side)
    '7': ('VIN', 'L'), '8': ('GND', 'L'), '9': ('SCL', 'L'), '10': ('SDA', 'L'), '11': ('ADDR', 'L'), '12': ('ALRT', 'L'),
    '1': ('A+', 'R'), '2': ('A3', 'R'), '3': ('A2', 'R'), '4': ('A1', 'R'), '5': ('A0', 'R'), '6': ('A-', 'R'),
}


def nets():
    out = {}
    for ref, p in PARTS.items():
        for pin, net in p[3].items():
            if net:
                out.setdefault(net, []).append((ref, pin))
    return out
