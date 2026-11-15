"""Single source of through-hole-v1's parts and nets.

gen_sch.py draws the schematic from this and gen_pcb.py builds the board from it, so the two can't
disagree; kicad-cli's DRC schematic-parity check confirms it.

Every footprint comes from the board's own library (`through-hole-v1:`): single-sided copies of KiCad's
stock footprints (pads on B.Cu only, enlarged for a board without plated holes), written by
gen_footprints.py, plus the Adafruit ADS1115 module.
"""
PROJECT = 'through-hole-v1'
TITLE = 'Oil-temperature gauge interface: through-hole-v1'
REV = 'v1'
DATE = '2026-11-15'
LIB = PROJECT          # the board's own footprint and symbol library nickname

NC = None  # no connection

# Single-sided footprints: name in the board's library -> the stock KiCad footprint it's made from.
FOOTPRINTS = {
    'R_P10.16mm': 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal',
    'R_P7.62mm': 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal',
    'C_P5.00mm': 'Capacitor_THT:C_Rect_L7.0mm_W3.5mm_P5.00mm',
    'CP_D10_P5.00mm': 'Capacitor_THT:CP_Radial_D10.0mm_P5.00mm',
    'CP_D5_P2.00mm': 'Capacitor_THT:CP_Radial_D5.0mm_P2.00mm',
    'D_DO-35_P7.62mm': 'Diode_THT:D_DO-35_SOD27_P7.62mm_Horizontal',
    'D_DO-35_P12.70mm': 'Diode_THT:D_DO-35_SOD27_P12.70mm_Horizontal',
    'D_DO-41_P10.16mm': 'Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal',
    'D_DO-15_P12.70mm': 'Diode_THT:D_DO-15_P12.70mm_Horizontal',
    'D_DO-201_P15.24mm': 'Diode_THT:D_DO-201_P15.24mm_Horizontal',
    'D_DO-201AD_P12.70mm': 'Diode_THT:D_DO-201AD_P12.70mm_Horizontal',
    'MicroFit_1x03_RA': 'Connector_Molex:Molex_Micro-Fit_3.0_43650-0300_1x03_P3.00mm_Horizontal',
    'MicroFit_2x04_RA': 'Connector_Molex:Molex_Micro-Fit_3.0_43045-0800_2x04_P3.00mm_Horizontal',
    'Buck_SIP3': 'Converter_DCDC:Converter_DCDC_TRACO_TSR-1_THT',
    'PTC_P5.00mm': 'Capacitor_THT:C_Disc_D10.5mm_W5.0mm_P5.00mm',
    'TestPoint': 'TestPoint:TestPoint_Keystone_5000-5004_Miniature',
    'MountingHole_M3': 'MountingHole:MountingHole_3.2mm_M3',
}
MODULE_FP = 'Adafruit_ADS1115_STEMMA_QT'   # drawn by gen_footprints.py, not copied


def fp(name):
    return f'{LIB}:{name}'


# ref: (lib_id, value, footprint, {pin: net}, sch placement (x, y, rot, mirror), fields)
PARTS = {
    # Connectors. A ground beside every display-cable signal, two on the power input, a drain on the harness.
    'J1': ('Connector_Generic:Conn_01x03', 'Power 12 V', fp('MicroFit_1x03_RA'),
           {'1': 'GND', '2': 'GND', '3': '12V_IN'}, (25.4, 50.8, 0, 'y'), {}),
    'J2': ('Connector_Generic:Conn_01x03', 'Sender harness', fp('MicroFit_1x03_RA'),
           {'1': 'SENDER', '2': 'SENSE', '3': 'GND'}, (264.16, 101.6, 0, None), {}),
    'J3': ('Connector_Generic:Conn_02x04_Top_Bottom', 'Display cable', fp('MicroFit_2x04_RA'),
           {'1': 'GND', '2': 'GND', '3': 'GND', '4': '5V_OUT', '5': '+3V3', '6': 'GND', '7': 'SCL', '8': 'SDA'},
           (35.56, 152.4, 0, None), {}),
    # ADC: the Adafruit module, soldered in by its pins (never socketed: #39).
    'U1': (f'{LIB}:ADS1115_Module', 'Adafruit ADS1115', fp(MODULE_FP),
           {'1': NC, '2': 'AIN3', '3': NC, '4': 'AIN1', '5': 'AIN0', '6': NC,
            '7': '+3V3', '8': 'GND', '9': 'SCL', '10': 'SDA', '11': NC, '12': NC},
           (127.0, 101.6, 0, None), {}),
    # 12 V -> 5 V
    'U2': ('Converter_DCDC:TSR_1-2450', 'TSR 1-2450', fp('Buck_SIP3'),
           {'1': '12V_P', '2': 'GND', '3': '5V_REG'}, (101.6, 50.8, 0, None), {}),
    'F1': ('Device:Polyfuse', 'MF-R075', fp('PTC_P5.00mm'),
           {'1': '12V_IN', '2': '12V_F'}, (45.72, 50.8, 90, None), {}),
    'D6': ('Device:D_Zener', '1.5KE27A', fp('D_DO-201_P15.24mm'),
           {'1': '12V_F', '2': 'GND'}, (55.88, 60.96, 270, None), {}),
    'D7': ('Device:D_Schottky', '1N5822', fp('D_DO-201AD_P12.70mm'),
           {'1': '12V_P', '2': '12V_F'}, (66.04, 50.8, 180, None), {}),
    'C5': ('Device:C_Polarized', '220u 50V', fp('CP_D10_P5.00mm'),
           {'1': '12V_P', '2': 'GND'}, (78.74, 58.42, 0, None), {}),
    'C7': ('Device:C', '100n', fp('C_P5.00mm'),
           {'1': '12V_P', '2': 'GND'}, (88.9, 58.42, 0, None), {}),
    'C6': ('Device:C_Polarized', '100u 16V', fp('CP_D5_P2.00mm'),
           {'1': '5V_REG', '2': 'GND'}, (127.0, 58.42, 0, None), {}),
    'D8': ('Device:D_Schottky', '1N5817', fp('D_DO-41_P10.16mm'),
           {'1': '5V_OUT', '2': '5V_REG'}, (142.24, 50.8, 180, None), {}),
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
    'R4': ('Device:R', '1k', fp('R_P7.62mm'),
           {'1': 'TAP_S', '2': 'AIN3'}, (177.8, 101.6, 90, None), {}),
    'C3': ('Device:C', '100n', fp('C_P5.00mm'),
           {'1': 'AIN3', '2': 'GND'}, (162.56, 109.22, 0, None), {}),
    # A0: Kelvin from Rp's top pad
    'R2': ('Device:R', '1k', fp('R_P7.62mm'),
           {'1': '+3V3', '2': 'AIN0'}, (177.8, 76.2, 90, None), {}),
    'C1': ('Device:C', '100n', fp('C_P5.00mm'),
           {'1': 'AIN0', '2': 'GND'}, (162.56, 83.82, 0, None), {}),
    # Sense wire: bias on the harness side, the A1 tap (1k, clamp, 1k)
    'R7': ('Device:R', '100k', fp('R_P7.62mm'),
           {'1': '+3V3', '2': 'SENSE'}, (236.22, 132.08, 0, None), {}),
    'R5': ('Device:R', '1k 0.6W', fp('R_P7.62mm'),
           {'1': 'SENSE', '2': 'TAP_G'}, (213.36, 142.24, 270, None), {}),
    'D3': ('Device:D', '1N4148', fp('D_DO-35_P12.70mm'),
           {'1': '+3V3', '2': 'TAP_G'}, (195.58, 152.4, 90, None), {}),
    'D4': ('Device:D', '1N4148', fp('D_DO-35_P7.62mm'),
           {'1': 'TAP_G', '2': 'GND'}, (205.74, 162.56, 90, None), {}),
    'R6': ('Device:R', '1k', fp('R_P7.62mm'),
           {'1': 'TAP_G', '2': 'AIN1'}, (177.8, 142.24, 90, None), {}),
    'C2': ('Device:C', '100n', fp('C_P5.00mm'),
           {'1': 'AIN1', '2': 'GND'}, (162.56, 149.86, 0, None), {}),
    # I2C pull-ups (stronger, for the cable; the module adds 10k) and 3.3 V decoupling at the module
    'R8': ('Device:R', '3k3', fp('R_P7.62mm'),
           {'1': '+3V3', '2': 'SDA'}, (88.9, 132.08, 0, None), {}),
    'R9': ('Device:R', '3k3', fp('R_P7.62mm'),
           {'1': '+3V3', '2': 'SCL'}, (99.06, 132.08, 0, None), {}),
    'C4': ('Device:C', '100n', fp('C_P5.00mm'),
           {'1': '+3V3', '2': 'GND'}, (111.76, 132.08, 0, None), {}),
    # Test points: two grounds for the bench, plus the two rails
    'TP1': ('Connector:TestPoint', 'GND', fp('TestPoint'), {'1': 'GND'}, (63.5, 172.72, 0, None), {}),
    'TP2': ('Connector:TestPoint', 'GND', fp('TestPoint'), {'1': 'GND'}, (73.66, 172.72, 0, None), {}),
    'TP3': ('Connector:TestPoint', '+3V3', fp('TestPoint'), {'1': '+3V3'}, (83.82, 172.72, 0, None), {}),
    'TP4': ('Connector:TestPoint', '5V', fp('TestPoint'), {'1': '5V_OUT'}, (93.98, 172.72, 0, None), {}),
    'H1': ('Mechanical:MountingHole', 'M3', fp('MountingHole_M3'), {}, (114.3, 172.72, 0, None), {}),
    'H2': ('Mechanical:MountingHole', 'M3', fp('MountingHole_M3'), {}, (121.92, 172.72, 0, None), {}),
    'H3': ('Mechanical:MountingHole', 'M3', fp('MountingHole_M3'), {}, (129.54, 172.72, 0, None), {}),
    'H4': ('Mechanical:MountingHole', 'M3', fp('MountingHole_M3'), {}, (137.16, 172.72, 0, None), {}),
}

# Purchasing data, checked 2026-11-15 against manufacturer datasheets and Digi-Key UK part pages (stock seen then).
# ref: (manufacturer, MPN, Digi-Key UK code, description). The code is blank where it wasn't confirmed on the
# page itself: order those by MPN.
_R1K = ('Vishay', 'MBB02070C1001FCT00', 'BC3566CT-ND', '1k 1% 0.6 W metal film, 0207')
_R1K_FAULT = ('Vishay', 'MBB02070C1001FCT00', 'BC3566CT-ND',
              '1k 1% 0.6 W metal film, 0207 (about 0.11 W if a harness wire reaches 14.4 V)')
_C100N = ('TDK', 'FG26C0G1H104JNT06', '445-173396-1-ND', '100n 5% 50 V C0G, 5 mm leads')
_1N4148 = ('Vishay', '1N4148-TAP', '1N4148-TAPCT-ND', 'Small-signal diode, DO-35, 25 nA max leakage at 20 V, 25 C')
_R3K3 = ('Vishay', 'MBB02070C3301FCT00', 'BC3743CT-ND', '3k3 1% 0.6 W; I2C pull-up for the cable (the module adds 10k)')
BOM = {
    'J1': ('Molex', '43650-0300', 'WM1861-ND', 'Micro-Fit 3.0 1x3 right-angle header, tin, snap-in peg; power'),
    'J2': ('Molex', '43650-0300', 'WM1861-ND', 'Micro-Fit 3.0 1x3 right-angle header, tin, snap-in peg; sender harness'),
    'J3': ('Molex', '43045-0800', 'WM1816-ND', 'Micro-Fit 3.0 2x4 right-angle header, tin, board lock; display cable'),
    'U1': ('Adafruit', '1085', '', 'ADS1115 breakout (STEMMA QT). Solder its header pins into the board, '
           'with 2.5 mm M2.5 nylon standoffs at its four holes'),
    'U2': ('Traco Power', 'TSR 1-2450', '1951-2747-ND', '12 V -> 5 V 1 A switching regulator, SIP-3; 6.5-36 V in'),
    'F1': ('Bourns', 'MF-R075', '', 'Resettable fuse 0.75 A hold (0.47 A at 60 C), 60 V, 5.1 mm leads'),
    'D6': ('Littelfuse', '1.5KE27A', '1.5KE27ALFCT-ND', 'TVS 1500 W, 23.1 V standoff, unidirectional, DO-201; '
           'stays off at a 24 V jump start'),
    'D7': ('Vishay', '1N5822-E3/54', '', 'Schottky 40 V 3 A, DO-201AD; reverse-polarity protection'),
    'D8': ('Vishay', '1N5817-E3/54', '', 'Schottky 20 V 1 A, DO-41; stops USB-C feeding back into U2'),
    'C5': ('Rubycon', '50ZLH220MEFC10X16', '', '220u 50 V 105 C low-ESR electrolytic, 10 x 16 mm'),
    'C6': ('Rubycon', '16ZLH100MEFC5X11', '', '100u 16 V 105 C low-ESR electrolytic, 5 x 11 mm'),
    'C7': _C100N, 'C1': _C100N, 'C2': _C100N, 'C3': _C100N, 'C4': _C100N,
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
    'TP4': ('Keystone', '5001', '36-5001-ND', 'Test point loop, black, 1.0 mm hole'),
}

# Parts that aren't on the board but are needed to connect and box it. (item, qty, mfr, MPN, Digi-Key UK, notes)
EXTRAS = [
    ('Power plug housing (mates with J1)', '1', 'Molex', '43645-0300', 'WM1846-ND', 'Micro-Fit 3.0 1x3 receptacle'),
    ('Harness plug housing (mates with J2)', '1', 'Molex', '43645-0300', 'WM1846-ND', 'Micro-Fit 3.0 1x3 receptacle'),
    ('Display-cable plug housing (mates with J3)', '1', 'Molex', '43025-0800', 'WM1786-ND',
     'Micro-Fit 3.0 2x4 receptacle'),
    ('Crimp terminals', '14 (+spares)', 'Molex', '43030-0007', 'WM1837-ND',
     'Tin, loose piece, for all three housings; 20-24 AWG (0.5-0.2 mm2), insulation 1.42-1.85 mm'),
    ('Crimp tool', '1', 'Molex', '63819-0000', '', 'Or a Micro-Fit ratchet crimper (as surface-mount-v1)'),
    ('Display end of the cable', '1', 'Samtec', 'TSW-108-07-G-S', '',
     '8-way 2.54 mm gold pin header, wired by hand (see README) and pushed into the display case socket'),
    ('Display cable', '30 cm or less', '', '4 twisted pairs, 24 AWG stranded', '',
     'Pairs: 5V+GND, 3.3V+GND, SDA+GND, SCL+GND. Keep it short: it carries the I2C bus'),
    ('Module standoffs', '4', '', 'M2.5 x 2.5 mm nylon spacer, M2.5 nylon screws and nuts', '',
     'Hold the ADS1115 module down in a vibrating car'),
    ('Board mounting', '4', '', 'M3 screws into the box\'s bosses', '', 'See box/through-hole-v1-box.svg'),
    ('Harness and power wire', 'as needed', '', '0.5 mm2 (20 AWG)', '',
     'Signal and ground-sense twisted (or 2-core screened, screen to J2 pin 3 only); fused ignition 12 V to J1'),
]

# PWR_FLAGs for ERC: rails that arrive through connectors or diodes. (net, x, y) on the schematic.
PWR_FLAGS = [('+3V3', 50.8, 182.88), ('GND', 63.5, 182.88), ('12V_P', 25.4, 193.04)]

SCH_NOTES = [
    (25.4, 20.32, 'through-hole-v1: one copper layer, through-hole parts only, in its own box. A cable joins J3 to the display\'s\\n'
                  '8-way case socket (5V, 3.3V, SDA, SCL, each paired with a ground). 12 V comes in on J1; U2 makes the display\'s 5 V.'),
    (160.02, 20.32, 'Ratiometric, differential measurement (hardware/WIRING.md):\\n'
                    'Vp = A0 - A3 across R1 (Rp), Vs = A3 - A1 across the sender, Rs = Rp x Vs / Vp.\\n'
                    'Layout rules: R2 joins R1 pin 1 by its own track (Kelvin); nothing in series between R1 and J2 pin 1;\\n'
                    'R7 (break-detect bias) stays on the harness side of R5. C1-C3 return to GND; the module ties A- to GND itself.'),
    (25.4, 200.66, 'Power: F1 limits a sustained fault, D6 clamps transients, D7 blocks reverse polarity, C5 rides through short dips.\\n'
                   'D8 stops USB-C power feeding back into U2 when both are connected. 3.3 V comes back from the display on J3.'),
    (160.02, 182.88, 'Harness faults (a wire at 14.4 V): R3/R5 limit clamp current to about 10 mA each into +3V3;\\n'
                     'D1-D4 hold the taps inside GND..3.3 V; D5 limits transients on the signal wire and does not conduct at 14.4 V.'),
]

# Connector pinouts for BOM.md: (ref, title, {net: where the wire goes})
PINOUTS = [
    ('J1', 'power (Micro-Fit 3.0, 1x3)', {'12V_IN': 'Fused, ignition-switched 12 V',
                                         'GND': 'Chassis ground near the tank bond; wire both pins for a lower-resistance return'}),
    ('J2', 'sender harness (Micro-Fit 3.0, 1x3)', {'SENDER': 'Sender spade terminal (signal)',
                                                   'SENSE': 'Ground-sense wire to the tank bond stud',
                                                   'GND': 'Harness screen or drain, at this end only'}),
    ('J3', 'display cable (Micro-Fit 3.0, 2x4)', {'5V_OUT': 'Display 5V', '+3V3': 'Display 3.3V (supplied by the display)',
                                                  'SDA': 'Display 17 (SDA)', 'SCL': 'Display 18 (SCL)', 'GND': 'Display G'}),
]
# The display's 8-way case socket, left to right seen from the back: (label, net on the cable)
DISPLAY_END = [('18', 'SCL'), ('17', 'SDA'), ('16', None), ('RX', None), ('TX', None),
               ('3.3V', '+3V3'), ('G', 'GND'), ('5V', '5V_OUT')]

# Longer schematic stubs where power symbols would otherwise crowd neighbouring pins.
SCH_STUB = {('J1', '2'): 7.62, ('J1', '3'): 12.7}

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
