"""Single source of the interface board's parts and nets.

gen_sch.py draws the schematic from this, and gen_pcb.py builds the board from it,
so the two can't disagree. kicad-cli's DRC schematic-parity check confirms it.
"""
PROJECT = 'surface-mount-v1'
TITLE = 'Oil-temperature gauge interface: surface-mount-v1'
REV = 'v1'
DATE = '2026-11-15'

NC = None  # no connection

# ref: (lib_id, value, footprint, {pin: net}, sch placement (x, y, rot, mirror), fields)
# Sch coordinates are mm on A4 landscape; keep symbol origins on a 2.54 mm grid.
PARTS = {
    'J1': ('Connector_Generic:Conn_01x08', 'Display header',
           'Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical',
           {'1': 'SCL', '2': 'SDA', '3': NC, '4': NC, '5': NC, '6': '+3V3', '7': 'GND', '8': '5V_DISP'},
           (38.1, 63.5, 0, 'y'), {}),
    'J2': ('Connector_Generic:Conn_01x04', 'Harness',
           'Connector_Molex:Molex_Micro-Fit_3.0_43650-0415_1x04_P3.00mm_Vertical',
           {'1': 'SENDER', '2': 'GND', '3': '5V_IN', '4': 'SENSE'},
           (256.54, 104.14, 0, None), {}),
    'U1': ('Analog_ADC:ADS1115IDGS', 'ADS1115IDGSR', 'Package_SO:TSSOP-10_3x3mm_P0.5mm',
           {'1': 'GND', '2': NC, '3': 'GND', '4': 'AIN0', '5': 'AIN1', '6': NC, '7': 'AIN3',
            '8': '+3V3', '9': 'SDA', '10': 'SCL'},
           (121.92, 66.04, 0, None), {}),
    # Rp: the measurement reference.
    'R1': ('Device:R', '470R 0.1% 25ppm', 'Resistor_SMD:R_1206_3216Metric',
           {'1': '+3V3', '2': 'SENDER'}, (180.34, 50.8, 0, None), {}),
    'R2': ('Device:R', '1k', 'Resistor_SMD:R_0603_1608Metric',
           {'1': '+3V3', '2': 'AIN0'}, (180.34, 76.2, 90, None), {}),
    'R3': ('Device:R', '1k 1206', 'Resistor_SMD:R_1206_3216Metric',
           {'1': 'SENDER', '2': 'TAP_S'}, (213.36, 104.14, 270, None), {}),
    'R4': ('Device:R', '1k', 'Resistor_SMD:R_0603_1608Metric',
           {'1': 'TAP_S', '2': 'AIN3'}, (180.34, 104.14, 90, None), {}),
    'R5': ('Device:R', '1k 1206', 'Resistor_SMD:R_1206_3216Metric',
           {'1': 'SENSE', '2': 'TAP_G'}, (213.36, 142.24, 270, None), {}),
    'R6': ('Device:R', '1k', 'Resistor_SMD:R_0603_1608Metric',
           {'1': 'TAP_G', '2': 'AIN1'}, (180.34, 142.24, 90, None), {}),
    'R7': ('Device:R', '100k', 'Resistor_SMD:R_0603_1608Metric',
           {'1': '+3V3', '2': 'SENSE'}, (233.68, 129.54, 0, None), {}),
    'R8': ('Device:R', '10k', 'Resistor_SMD:R_0603_1608Metric',
           {'1': '+3V3', '2': 'SDA'}, (101.6, 124.46, 0, None), {}),
    'R9': ('Device:R', '10k', 'Resistor_SMD:R_0603_1608Metric',
           {'1': '+3V3', '2': 'SCL'}, (116.84, 124.46, 0, None), {}),
    'C1': ('Device:C', '100n', 'Capacitor_SMD:C_0603_1608Metric',
           {'1': 'AIN0', '2': 'GND'}, (162.56, 81.28, 0, None), {}),
    'C2': ('Device:C', '100n', 'Capacitor_SMD:C_0603_1608Metric',
           {'1': 'AIN1', '2': 'GND'}, (162.56, 147.32, 0, None), {}),
    'C3': ('Device:C', '100n', 'Capacitor_SMD:C_0603_1608Metric',
           {'1': 'AIN3', '2': 'GND'}, (162.56, 109.22, 0, None), {}),
    'C4': ('Device:C', '100n', 'Capacitor_SMD:C_0603_1608Metric',
           {'1': '+3V3', '2': 'GND'}, (139.7, 124.46, 0, None), {}),
    'D1': ('Diode:BAV99', 'BAV99', 'Package_TO_SOT_SMD:SOT-23',
           {'1': 'GND', '2': '+3V3', '3': 'TAP_S'}, (198.12, 116.84, 0, None), {}),
    'D2': ('Diode:BAV99', 'BAV99', 'Package_TO_SOT_SMD:SOT-23',
           {'1': 'GND', '2': '+3V3', '3': 'TAP_G'}, (198.12, 154.94, 0, None), {}),
    'D3': ('Device:D_Zener', 'SMBJ15A', 'Diode_SMD:D_SMB',
           {'1': 'SENDER', '2': 'GND'}, (233.68, 88.9, 270, None), {}),
    'D4': ('Device:D_Schottky', 'SS34', 'Diode_SMD:D_SMA',
           {'1': '5V_DISP', '2': '5V_IN'}, (40.64, 132.08, 0, None), {}),
    'H1': ('Mechanical:MountingHole', 'M2', 'MountingHole:MountingHole_2.2mm_M2', {}, (139.7, 152.4, 0, None), {}),
    'H2': ('Mechanical:MountingHole', 'M2', 'MountingHole:MountingHole_2.2mm_M2', {}, (149.86, 152.4, 0, None), {}),
}

# Purchasing data, written into each symbol's fields so the KiCad BOM export carries it.
# Verified 2026-11-15 against manufacturer datasheets and JLCPCB/LCSC/Digi-Key part pages.
BOM = {
    'U1': ('Texas Instruments', 'ADS1115IDGSR', 'C37593', '16-bit I2C ADC, VSSOP-10'),
    'R1': ('Vishay', 'TNPW1206470RBEEA', '', 'Rp: 470R 0.1% 25ppm/K thin film, 1206, 0.27 W (P70, film <=125 C), AEC-Q200'),
    'R2': ('UNI-ROYAL', '0603WAF1001T5E', 'C21190', '1k 1% 0.1 W 0603'),
    'R4': ('UNI-ROYAL', '0603WAF1001T5E', 'C21190', '1k 1% 0.1 W 0603'),
    'R6': ('UNI-ROYAL', '0603WAF1001T5E', 'C21190', '1k 1% 0.1 W 0603'),
    'R3': ('UNI-ROYAL', '1206W4F1001T5E', 'C4410', '1k 1% 0.25 W 1206 (fault dissipation about 0.11 W)'),
    'R5': ('UNI-ROYAL', '1206W4F1001T5E', 'C4410', '1k 1% 0.25 W 1206 (fault dissipation about 0.11 W)'),
    'R7': ('UNI-ROYAL', '0603WAF1003T5E', 'C25803', '100k 1% 0.1 W 0603'),
    'R8': ('UNI-ROYAL', '0603WAF1002T5E', 'C25804', '10k 1% 0.1 W 0603'),
    'R9': ('UNI-ROYAL', '0603WAF1002T5E', 'C25804', '10k 1% 0.1 W 0603'),
    'C1': ('Yageo', 'CC0603KRX7R9BB104', 'C14663', '100n 50 V X7R 0603'),
    'C2': ('Yageo', 'CC0603KRX7R9BB104', 'C14663', '100n 50 V X7R 0603'),
    'C3': ('Yageo', 'CC0603KRX7R9BB104', 'C14663', '100n 50 V X7R 0603'),
    'C4': ('Yageo', 'CC0603KRX7R9BB104', 'C14663', '100n 50 V X7R 0603'),
    'D1': ('Nexperia', 'BAV99,215', 'C2500', 'Series switching-diode pair, SOT-23'),
    'D2': ('Nexperia', 'BAV99,215', 'C2500', 'Series switching-diode pair, SOT-23'),
    'D3': ('Littelfuse', 'SMBJ15A', 'C83846', 'TVS 600 W, 15 V standoff, unidirectional, SMB'),
    'D4': ('MDD', 'SS34', 'C8678', 'Schottky 40 V 3 A, SMA'),
    'J1': ('Samtec', 'TSW-108-07-G-S', 'C3335496', '8-way 2.54 mm pin header, gold, plugs into the display case socket'),
    'J2': ('Molex', '43650-0415', 'C293547', 'Micro-Fit 3.0 1x4 vertical header, tin; mates with housing 43645-0400 and tin 43030 terminals'),
}

# Fitted by hand rather than by the board house: the through-hole connectors (J1 sits on the
# bottom side) and Rp, whose 0.1 % / 25 ppm part isn't stocked by LCSC.
HAND_FIT = ('J1', 'J2', 'R1')

# Parts that aren't on the board but are needed to fit and connect it.
# (item, qty, manufacturer, MPN, LCSC, notes)
EXTRAS = [
    ('Harness plug housing (mates with J2)', '1', 'Molex', '43645-0400', 'C277618',
     'Micro-Fit 3.0 receptacle, 1x4, single row'),
    ('Crimp terminals for the housing', '4 (+spares)', 'Molex', '43030-0007', '',
     'Tin, loose piece, to match J2\'s tin header; 20-24 AWG (0.5-0.2 mm2), insulation OD 1.42-1.85 mm. '
     '43030-0001 (LCSC C259786) is the same terminal on a reel'),
    ('Crimp tool for the terminals', '1', 'Molex', '63819-0000', '',
     'Ratcheting, 20-30 AWG. Molex only guarantees its crimp spec with Molex tooling; '
     'a third-party Micro-Fit ratchet crimper also works'),
    ('Retaining screws', '2', '', 'M2 pan head, A2 stainless', '',
     'Replace the case\'s two bottom screws: same thread, longer by the board (1.6 mm) plus the spacer. '
     'Measure the original screws first'),
    ('Spacers', '2', '', 'M2 nylon spacer, 2.5 mm', '',
     'Between the board and the case back at H1/H2; matches J1\'s 2.54 mm insulator. Confirm at the fit check'),
    ('Harness wire', 'as needed', '', '0.5 mm2 (20 AWG)', '',
     'Signal and ground-sense twisted together (or 2-core screened, screen grounded at the gauge end); '
     '5 V and GND from the converter'),
    ('12 V to 5 V automotive converter', '1', '', 'not specified here', '',
     'Fused ignition feed, cranking/load-dump/reverse-polarity rated (hardware/BOM.md); its 5 V goes to J2 pin 3'),
]

# Longer schematic stubs where power symbols would otherwise crowd neighbouring pins.
SCH_STUB = {('J1', '6'): 7.62, ('J1', '7'): 12.7, ('J2', '2'): 12.7}

# Nets whose pins are drawn with power symbols rather than labels.
POWER_NETS = {'+3V3': 'power:+3V3', 'GND': 'power:GND'}


def nets():
    out = {}
    for ref, p in PARTS.items():
        for pin, net in p[3].items():
            if net:
                out.setdefault(net, []).append((ref, pin))
    return out
