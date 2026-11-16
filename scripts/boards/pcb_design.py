"""The PCB's parts and nets for one option, built from circuit.py. Stdlib only.

The option comes from the BOARD_OPTION environment variable ('a' or 'b'); build.sh sets it for each board.
pcb_sch.py draws the schematic from this and pcb_board.py builds the board from it, so the two can't disagree;
kicad-cli's DRC schematic-parity check confirms it.

The board: two copper layers, plated through-hole parts only. The display lead plugs into J1 (Micro-Fit 2x3: each
signal beside a ground) and the sender harness into J2 (Micro-Fit 1x4, pin 3 empty). The different sizes mean
neither cable fits the other's header. The Adafruit module is soldered in by its header pins.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import circuit as C  # noqa: E402

OPTION = os.environ.get('BOARD_OPTION', 'a')
if OPTION not in C.OPTIONS:
    raise SystemExit(f'BOARD_OPTION must be one of {C.OPTIONS}, not {OPTION!r}')
PROJECT = f'option-{OPTION}'
TITLE = f'Oil-temperature gauge front end, option {OPTION.upper()}' + (' (filtered)' if OPTION == 'b' else ' (minimal)')
REV = 'v1'
DATE = C.DATE
LIB = 'gauge'          # the project's own footprint and symbol library nickname

# Footprints: name in the project library -> the stock KiCad footprint it's copied from (kept in the project so
# it opens the same anywhere).
FOOTPRINTS = {
    'R_P10.16mm': 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal',
    'C_P2.50mm': 'Capacitor_THT:C_Rect_L4.6mm_W3.8mm_P2.50mm_MKS02_FKP02',
    'MicroFit_1x04_RA': 'Connector_Molex:Molex_Micro-Fit_3.0_43650-0400_1x04_P3.00mm_Horizontal',
    'MicroFit_2x03_RA': 'Connector_Molex:Molex_Micro-Fit_3.0_43045-0600_2x03_P3.00mm_Horizontal',
    'MountingHole_M3': 'MountingHole:MountingHole_3.2mm_M3',
}
MODULE_FP = 'Adafruit_ADS122C04'   # drawn by pcb_footprints.py
MODULE_SYM = 'ADS122C04_Module'


def fp(name):
    return f'{LIB}:{name}'


# J1, the display lead: each signal pin (row B) beside a ground (row A). Its pins line up with the module's own
# VIN, SCL and SDA, so those tracks don't cross.
J1_PINS = {'1': 'GND', '2': 'GND', '3': 'GND', '4': 'SDA', '5': 'SCL', '6': '+3V3'}
J1_WIRES = {'1': 'Display G (paired with SDA, pin 4)', '2': 'Display G (paired with SCL, pin 5)',
            '3': 'Display G (paired with 3.3 V, pin 6)', '4': 'Display 17 (SDA)', '5': 'Display 18 (SCL)',
            '6': 'Display 3.3V (supplied by the display)'}

_front = C.front_end(OPTION)
_SYMBOL = {'module': f'{LIB}:{MODULE_SYM}', 'R': 'Device:R', 'C': 'Device:C'}
_FOOT = {'module': fp(MODULE_FP), 'R': fp('R_P10.16mm'), 'C': fp('C_P2.50mm')}
# Schematic placement: (x, y, rot, mirror), on an A4 sheet in mm.
_SCH = {'U1': (106.68, 101.6, 0, None), 'Rref': (154.94, 83.82, 90, None), 'R1': (154.94, 101.6, 90, None),
        'R2': (154.94, 119.38, 90, None), 'C1': (132.08, 91.44, 0, None), 'C2': (139.7, 109.22, 0, None)}

# ref: (lib_id, value, footprint, {pin: net}, sch placement (x, y, rot, mirror), fields)
PARTS = {}
for _ref, (_kind, _value, _pins) in _front.items():
    PARTS[_ref] = (_SYMBOL[_kind], _value, _FOOT[_kind], dict(_pins), _SCH[_ref], {})
PARTS['J1'] = ('Connector_Generic:Conn_02x03_Top_Bottom', 'Display lead', fp('MicroFit_2x03_RA'), dict(J1_PINS),
               (50.8, 101.6, 0, None), {})
PARTS['J2'] = ('Connector_Generic:Conn_01x04', 'Sender harness', fp('MicroFit_1x04_RA'), dict(C.HARNESS),
               (200.66, 101.6, 0, 'y'), {})
for _i, _x in enumerate((50.8, 60.96, 71.12, 81.28)):
    PARTS[f'H{_i + 1}'] = ('Mechanical:MountingHole', 'M3', fp('MountingHole_M3'), {}, (_x, 160.02, 0, None), {})

# Purchasing data per part: (manufacturer, MPN, Digi-Key UK code, description), from circuit.PARTS_INFO.
_INFO = {'U1': 'module', 'Rref': 'rref', 'R1': 'r2k2', 'R2': 'r2k2', 'C1': 'c47n', 'C2': 'c47n', 'J1': 'mf2x3',
         'J2': 'mf1x4'}
BOM = {ref: C.PARTS_INFO[k] for ref, k in _INFO.items() if ref in PARTS}

# PWR_FLAGs for ERC: both rails arrive through J1. (net, x, y) on the schematic.
PWR_FLAGS = [('+3V3', 30.48, 160.02), ('GND', 38.1, 160.02)]

SCH_NOTES = [
    (25.4, 20.32, f'{TITLE}. Two copper layers, through-hole parts only. J1 takes the display lead (3.3 V, SDA, SCL,\\n'
                  'each paired with a ground); the display runs from its own USB-C socket. J2 takes the sender harness.'),
    (25.4, 30.48, 'Measurement (hardware/WIRING.md): the ADC drives 250 uA out of A0 through Rref and the sender.\\n'
                  'Vref = A0 - A1 across Rref, Vs = A1 - A2 across the sender, Rs = Rref x Vs / Vref.\\n'
                  'R1 and R2 tap the sender and the ground-sense wire; at least 2 kOhm between every harness wire and the ADC.'),
    (25.4, 43.18, 'Layout rules: AIN1, AIN2, SENDER and SENSE keep at least 1 mm from ground copper, and there is no\\n'
                  'ground pour (a short there gives a believable wrong reading). R1 meets SENDER at J2 pin 1 by its own\\n'
                  'track. J2 pin 3 is empty, between the sense wire and the return.'
                  + ('\\nOption B: C1 and C2 go across the pin pairs only, never to ground; C0G, never more than 100 nF.'
                     if OPTION == 'b' else '')),
]

# Longer schematic stubs where power symbols would otherwise crowd neighbouring pins.
SCH_STUB = {('J1', '1'): 12.7, ('J1', '2'): 7.62, ('U1', '8'): 7.62}

# Nets drawn with power symbols rather than labels.
POWER_NETS = {'+3V3': 'power:+3V3', 'GND': 'power:GND'}

# The module's symbol: system pins left, analogue pins right, as on its two header rows.
MODULE_SYMBOL_PINS = {num: (name, 'L' if int(num) >= 7 else 'R') for num, name in C.MODULE_PINS.items()}


def nets():
    out = {}
    for ref, p in PARTS.items():
        for pin, net in p[3].items():
            if net:
                out.setdefault(net, []).append((ref, pin))
    return out
