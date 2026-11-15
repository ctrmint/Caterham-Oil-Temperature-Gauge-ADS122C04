"""Single source of veroboard-usb-v1's parts, nets and part sizes. Stdlib only.

The circuit is through-hole-usb-v1's (hardware/pcb/through-hole-usb-v1), built on 0.1 in stripboard: the
Adafruit ADS1115 module, the WIRING.md front end with its harness protection, and the I2C pull-ups. The
display runs from its own USB-C socket and the board takes the display's 3.3 V over a cable. Both cables
land in 2.54 mm screw terminal blocks. A USB supply needn't share the car's ground, so the harness brings a
dedicated ground return back from the tank (J2 pin 2) beside the ground-sense wire.

Two-lead parts use pin '1' and '2'; for diodes '1' is the cathode (the band), as in KiCad's Device:D.
"""
PROJECT = 'veroboard-usb-v1'
TITLE = 'Oil-temperature gauge interface: veroboard-usb-v1'
DATE = '2026-11-15'
PITCH = 2.54

# Body sizes from the makers' datasheets (mm), checked 2026-11-15.
#   axial: L along the leads, D diameter; min_span is the closest lead spacing, in holes, the datasheet allows.
#   radial: L across the leads (the lead-spacing direction), T thickness; span is the lead spacing, in holes.
BODIES = {
    # Vishay MBB/SMA 0207 (doc 28766): L 6.5 max, D 2.5 max, minimum pitch 10.0 mm when mounted flat.
    'R0207': {'type': 'axial', 'L': 6.5, 'D': 2.5, 'min_span': 4, 'height': 2.5},
    # Vishay 1N4148 (doc 81857), DO-35: body 3.4 max, diameter 1.75. Bent 2 mm or more from the body at 3 holes.
    'DO35': {'type': 'axial', 'L': 3.4, 'D': 1.75, 'min_span': 3, 'height': 1.75},
    # Vishay P6KE (doc 88369), DO-15: body 6.55 max, diameter 3.5 max, plus up to 0.38 of mould flash a side.
    'DO15': {'type': 'axial', 'L': 6.55 + 2 * 0.38, 'D': 3.5 + 2 * 0.38, 'min_span': 4, 'height': 4.3},
    # TDK FG26 (FG26C0G1H104JNT06): L 5.5, T 3.5, seated height 6.0 max, lead spacing 5.0 (taped +0.8/-0.2).
    'FG26': {'type': 'radial', 'L': 5.5, 'T': 3.5, 'span': 2, 'height': 6.0},
}
# Phoenix Contact MPT 0,5 (1725672, 1725698): 2.54 mm pitch, the body 1.5 mm beyond each end pin, 6.2 deep with
# the pins in the middle, 8.5 tall; the wire enters from one long side, parallel to the board.
MPT = {'end': 1.5, 'depth': 6.2, 'height': 8.5}
# Adafruit ADS1115 (1085): 25.4 x 17.78 mm board, two rows of 6 pins 12.7 mm apart, each row 2.54 mm from its
# long edge, centred along the length. Its pins: one row VIN GND SCL SDA ADDR ALRT, the other A+ A3 A2 A1 A0 A-.
MODULE = {'length': 25.4, 'width': 17.78, 'row_spacing': 5, 'height': 7.5}
MODULE_PINS = {'1': 'A+', '2': 'A3', '3': 'A2', '4': 'A1', '5': 'A0', '6': 'A-',
               '7': 'VIN', '8': 'GND', '9': 'SCL', '10': 'SDA', '11': 'ADDR', '12': 'ALRT'}

NC = None
# ref: (kind, value, {pin: net})
PARTS = {
    'U1': ('module', 'Adafruit ADS1115', {'1': NC, '2': 'AIN3', '3': NC, '4': 'AIN1', '5': 'AIN0', '6': NC,
                                          '7': '+3V3', '8': 'GND', '9': 'SCL', '10': 'SDA',
                                          '11': 'GND', '12': NC}),   # ADDR to GND: address 0x48 (WIRING.md)
    # Display cable: 3.3 V, SCL and SDA, each beside a ground.
    'J3': ('MPT6', 'Display cable', {'1': 'GND', '2': '+3V3', '3': 'GND', '4': 'SCL', '5': 'SDA', '6': 'GND'}),
    # Harness: the dedicated ground return (pin 2) between the signal and the ground-sense wire, and a second
    # ground (pin 4) for the screen or a second return.
    'J2': ('MPT4', 'Sender harness', {'1': 'SENDER', '2': 'GND', '3': 'SENSE', '4': 'GND'}),
    # Signal wire: Rp, TVS, the A3 tap (1k, clamp, 1k)
    'R1': ('R0207', '470R 0.1% 25ppm', {'1': '+3V3', '2': 'SENDER'}),
    'D5': ('DO15', 'P6KE18A', {'1': 'SENDER', '2': 'GND'}),
    'R3': ('R0207', '1k 0.6W', {'1': 'TAP_S', '2': 'SENDER'}),
    'D1': ('DO35', '1N4148', {'1': '+3V3', '2': 'TAP_S'}),
    'D2': ('DO35', '1N4148', {'1': 'TAP_S', '2': 'GND'}),
    'R4': ('R0207', '1k', {'1': 'AIN3', '2': 'TAP_S'}),
    'C3': ('FG26', '100n', {'1': 'AIN3', '2': 'GND'}),
    # A0: Kelvin from Rp's top lead
    'R2': ('R0207', '1k', {'1': '+3V3', '2': 'AIN0'}),
    'C1': ('FG26', '100n', {'1': 'AIN0', '2': 'GND'}),
    # Sense wire: bias on the harness side, the A1 tap (1k, clamp, 1k)
    'R7': ('R0207', '100k', {'1': '+3V3', '2': 'SENSE'}),
    'R5': ('R0207', '1k 0.6W', {'1': 'TAP_G', '2': 'SENSE'}),
    'D3': ('DO35', '1N4148', {'1': '+3V3', '2': 'TAP_G'}),
    'D4': ('DO35', '1N4148', {'1': 'TAP_G', '2': 'GND'}),
    'R6': ('R0207', '1k', {'1': 'AIN1', '2': 'TAP_G'}),
    'C2': ('FG26', '100n', {'1': 'AIN1', '2': 'GND'}),
    # I2C pull-ups for the cable (the module adds 10k) and 3.3 V decoupling
    'R8': ('R0207', '3k3', {'1': 'SDA', '2': '+3V3'}),
    'R9': ('R0207', '3k3', {'1': '+3V3', '2': 'SCL'}),
    'C4': ('FG26', '100n', {'1': 'GND', '2': '+3V3'}),
}

# Rules the layout must keep (checked by vero.py):
# - Kelvin: R2's lead joins R1's +3V3 lead's strip on the far side from every other connection, so no supply
#   current flows in the copper between them.
KELVIN = [(('R2', '1'), ('R1', '1'))]
# - Nothing in series between Rp and the harness: SENDER is one copper group (checked by the netlist).

# Purchasing data. ref: (manufacturer, MPN, Digi-Key UK code, description). Codes were read on Digi-Key UK
# product pages on 2026-11-15; where blank, order by MPN.
_R1K = ('Vishay', 'MBB02070C1001FCT00', 'BC3566CT-ND', '1k 1% 0.6 W metal film, 0207')
_R1K_FAULT = ('Vishay', 'MBB02070C1001FCT00', 'BC3566CT-ND',
              '1k 1% 0.6 W metal film, 0207 (about 0.11 W if a harness wire reaches 14.4 V)')
_C100N = ('TDK', 'FG26C0G1H104JNT06', '445-173396-1-ND', '100n 5% 50 V C0G, 5 mm leads')
_1N4148 = ('Vishay', '1N4148-TAP', '1N4148-TAPCT-ND', 'Small-signal diode, DO-35, 25 nA max leakage at 20 V, 25 C')
_R3K3 = ('Vishay', 'MBB02070C3301FCT00', 'BC3743CT-ND', '3k3 1% 0.6 W; I2C pull-up for the cable (the module adds 10k)')
BOM = {
    'J2': ('Phoenix Contact', '1725672', '277-1275-ND', 'MPT 0,5/ 4-2,54 PCB screw terminal block, 2.54 mm, 4-way; '
           'sender harness. 0.14-0.5 mm2 (26-20 AWG) wire'),
    'J3': ('Phoenix Contact', '1725698', '277-1277-ND', 'MPT 0,5/ 6-2,54 PCB screw terminal block, 2.54 mm, 6-way; '
           'display cable. 0.14-0.5 mm2 (26-20 AWG) wire'),
    'U1': ('Adafruit', '1085', '', 'ADS1115 breakout (STEMMA QT). Solder its header pins into the stripboard; '
           'never socket it (#39)'),
    'R1': ('Vishay', 'MBB0207VD4700BC100', '56-MBB0207VD4700BC100CT-ND',
           'Rp: 470R 0.1% 25 ppm/K, 0207, 0.4 W; measure it before fitting (kPullUpOhms)'),
    'R2': _R1K, 'R4': _R1K, 'R6': _R1K, 'R3': _R1K_FAULT, 'R5': _R1K_FAULT,
    'R7': ('Vishay', 'MBB02070C1003FCT00', 'BC3571CT-ND', '100k 1% 0.6 W; break-detect bias on the sense wire'),
    'R8': _R3K3, 'R9': _R3K3,
    'C1': _C100N, 'C2': _C100N, 'C3': _C100N, 'C4': _C100N,
    'D1': _1N4148, 'D2': _1N4148, 'D3': _1N4148, 'D4': _1N4148,
    'D5': ('Vishay', 'P6KE18A-E3/54', 'P6KE18A-E3/54GICT-ND', 'TVS 600 W, 15.3 V standoff, unidirectional, DO-15; '
           'signal wire'),
}

# Not on the board but needed to build and connect it. (item, qty, mfr, MPN, Digi-Key UK, notes)
EXTRAS = [
    ('Stripboard', '1', '', '0.1 in (2.54 mm) copper stripboard, 1.6 mm', '',
     'Cut to the size in the README: the strips run along its length'),
    ('Tinned copper wire', '20 cm', '', '0.6 mm (22 SWG) tinned copper', '', 'For the wire links'),
    ('Display end of the cable', '1', 'Samtec', 'TSW-108-07-G-S', '',
     '8-way 2.54 mm gold pin header, wired by hand (see README) and pushed into the display case socket'),
    ('Display cable', '30 cm or less', '', '3 twisted pairs, 24 AWG stranded', '',
     'Pairs: 3.3V+GND, SCL+GND, SDA+GND. Keep it short: it carries the I2C bus. No 5 V wire'),
    ('Display power', '1', '', 'USB-C supply, 5 V, 1 A or more', '',
     'In the car, a USB-C charger on an ignition-switched 12 V circuit; on the bench, a USB-C supply or a laptop. '
     'It plugs into the display, not this board'),
    ('Harness wire', 'as needed', '', '0.5 mm2 (20 AWG)', '',
     'Three wires to the tank, twisted together: signal (J2 pin 1), ground return (pin 2) and ground sense (pin 3). '
     'Or a 3-core screened cable, screen to J2 pin 4 at this end only'),
    ('Ferrules (display cable)', '6', '', '0.25 mm2 bootlace ferrules, no collar', '',
     'Phoenix lists 0.25-0.34 mm2 ferrules for MPT 0,5; for 0.5 mm2 harness wire, strip 4.5 mm and fit it bare'),
]

# Terminal-block wiring for BOM.md: (ref, title, {pin: where the wire goes})
PINOUTS = [
    ('J2', 'sender harness (MPT 0,5, 4-way)', {'1': 'Sender spade terminal (signal)',
                                              '2': 'Ground return to the tank bond stud (required: the board\'s ground reference)',
                                              '3': 'Ground-sense wire to the tank bond stud',
                                              '4': 'Harness screen at this end only, or a second return wire to the same stud'}),
    ('J3', 'display cable (MPT 0,5, 6-way)', {'1': 'Display G (paired with 3.3 V)', '2': 'Display 3.3V (supplied by the display)',
                                             '3': 'Display G (paired with SCL)', '4': 'Display 18 (SCL)',
                                             '5': 'Display 17 (SDA)', '6': 'Display G (paired with SDA)'}),
]
# The display's 8-way case socket, left to right seen from the back: (label, net on the cable)
DISPLAY_END = [('18', 'SCL'), ('17', 'SDA'), ('16', None), ('RX', None), ('TX', None),
               ('3.3V', '+3V3'), ('G', 'GND'), ('5V', None)]


def nets():
    out = {}
    for ref, (_k, _v, pins) in PARTS.items():
        for pin, net in pins.items():
            if net:
                out.setdefault(net, []).append((ref, pin))
    return out
