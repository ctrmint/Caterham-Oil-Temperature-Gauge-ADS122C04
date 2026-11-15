"""Simplified 3D envelopes for the renders of parts KiCad has no model for (not the makers' models):
the two Micro-Fit 3.0 right-angle headers and the Adafruit ADS1115 module.

Each model is in its footprint's own frame (KiCad: x right, y down; VRML: y up), units 0.1 in (2.54 mm).
Sizes: Micro-Fit body widths and the 8.92 mm mating-face offset from the KiCad footprints, heights as
through-hole-v1's headers of the same families (4.37 mm single row, 7.37 mm dual row); module 25.4 x 17.78 x 1.6 mm board on 2.54 mm header spacers.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import module as M  # noqa: E402

U = 2.54
BLACK, DARK, GOLD, BLUE, WHITE = (0.1, 0.1, 0.11), (0.02, 0.02, 0.02), (0.85, 0.7, 0.3), (0.05, 0.12, 0.35), (0.9, 0.9, 0.86)


def box(x0, x1, y0, y1, z0, z1, rgb):
    """Footprint coordinates (y down) in mm."""
    cx, cy, cz = (x0 + x1) / 2, -(y0 + y1) / 2, (z0 + z1) / 2
    return (f'Transform {{ translation {cx / U:.4f} {cy / U:.4f} {cz / U:.4f} children [ Shape {{ appearance Appearance '
            f'{{ material Material {{ diffuseColor {rgb[0]} {rgb[1]} {rgb[2]} specularColor 0.2 0.2 0.2 shininess 0.3 }} }} '
            f'geometry Box {{ size {abs(x1 - x0) / U:.4f} {abs(y1 - y0) / U:.4f} {abs(z1 - z0) / U:.4f} }} }} ] }}')


def microfit_ra(pins_per_row, rows, height):
    """Pin 1 at the origin, pins along +x at 3 mm, rows 3 mm apart in +y; mating face 8.92 mm towards -y."""
    span = 3.0 * (pins_per_row - 1)
    width = span + 6.65
    x0, x1 = span / 2 - width / 2, span / 2 + width / 2
    back = 3.0 * (rows - 1) + 1.5
    shapes = [box(x0, x1, -8.92, back, 0, height, BLACK)]
    for i in range(pins_per_row):          # cavities on the mating face
        for r in range(rows):
            z = 0.8 + r * 3.0
            shapes.append(box(i * 3.0 - 1.2, i * 3.0 + 1.2, -8.95, -8.9, z, z + 2.4, DARK))
    return shapes


def module():
    hw, hh = M.WIDTH / 2, M.HEIGHT / 2
    s = [box(-hw, hw, -hh, hh, 2.54, 2.54 + 1.6, BLUE)]
    for y in (-M.ROW_SPACING / 2, M.ROW_SPACING / 2):                     # header spacers and pins
        s.append(box(-7.62, 7.62, y - 1.27, y + 1.27, 0, 2.54, BLACK))
        for i in range(6):
            x = (i - 2.5) * M.PITCH
            s.append(box(x - 0.32, x + 0.32, y - 0.32, y + 0.32, 2.54, 4.6, GOLD))
    s.append(box(-1.5, 1.5, -1.5, 1.5, 4.14, 5.14, DARK))                  # the ADS1115
    for x in (-hw + 0.6, hw - 4.85):                                      # STEMMA QT sockets
        s.append(box(x, x + 4.25, -2.5, 2.5, 4.14, 5.04, WHITE))
    return s


def write(path, doc, shapes):
    with open(path, 'w', encoding='utf-8') as f:
        f.write('#VRML V2.0 utf8\n# ' + doc + '\n# Simplified envelope for renders only, not the maker\'s model.\n')
        f.write('\n'.join(shapes) + '\n')


MODELS = {
    'MicroFit_1x04_RA.wrl': ('Molex Micro-Fit 3.0 43650-0400, 1x4 right-angle', lambda: microfit_ra(4, 1, 4.37)),
    'MicroFit_2x03_RA.wrl': ('Molex Micro-Fit 3.0 43045-0600, 2x3 right-angle', lambda: microfit_ra(3, 2, 7.37)),
    'Adafruit_ADS1115_STEMMA_QT.wrl': ('Adafruit ADS1115 breakout (1085) on its headers', module),
}

if __name__ == '__main__':
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for name, (doc, make) in MODELS.items():
        write(os.path.join(out, name), doc, make())
