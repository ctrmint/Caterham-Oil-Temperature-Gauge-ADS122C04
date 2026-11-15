"""Geometry of the Adafruit ADS1115 breakout (product 1085, STEMMA QT version). Stdlib only.

Component side up, origin at the board centre, x right, y down (KiCad footprint convention).
Pin order from the module's silkscreen (hardware/ads1115-module.jpg): top row A+ A3 A2 A1 A0 A-,
bottom row VIN GND SCL SDA ADDR ALRT, left to right.
"""
PITCH = 2.54
ROW_SPACING = 12.7          # 0.5 in between the two rows
WIDTH, HEIGHT = 25.4, 17.8  # board outline, 1.0 x 0.7 in
HOLES = [(-10.16, -6.35), (10.16, -6.35), (-10.16, 6.35), (10.16, 6.35)]   # M2.5 mounting holes
HOLE_D = 2.7                # clearance for an M2.5 screw through a nylon standoff
PAD, DRILL = 2.2, 1.0       # for 0.64 mm square header pins

TOP = ['A+', 'A3', 'A2', 'A1', 'A0', 'A-']          # pads 1-6
BOTTOM = ['VIN', 'GND', 'SCL', 'SDA', 'ADDR', 'ALRT']  # pads 7-12


def pin_positions():
    out = {}
    for i in range(6):
        x = (i - 2.5) * PITCH
        out[str(i + 1)] = (round(x, 3), -ROW_SPACING / 2)
        out[str(i + 7)] = (round(x, 3), ROW_SPACING / 2)
    return out
