"""Simplified envelope of the Molex 43650-0415 (Micro-Fit 3.0, 1x4 vertical) for the 3D render.

KiCad ships no 3D model for this part. The body outline follows the KiCad footprint's
fab layer (15.65 x 4.37 mm plus the latch ramp) and Molex's 9.91 mm height. Not a Molex model.
VRML units are 0.1 in (2.54 mm); footprint y is down, VRML y is up.
"""
import sys
U = 2.54
def box(x0, x1, y0, y1, z0, z1, rgb):
    cx, cy, cz = (x0 + x1) / 2, -(y0 + y1) / 2, (z0 + z1) / 2
    sx, sy, sz = abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)
    r, g, b = rgb
    return (f'Transform {{ translation {cx/U:.4f} {cy/U:.4f} {cz/U:.4f} children [ Shape {{ '
            f'appearance Appearance {{ material Material {{ diffuseColor {r} {g} {b} specularColor 0.2 0.2 0.2 shininess 0.3 }} }} '
            f'geometry Box {{ size {sx/U:.4f} {sy/U:.4f} {sz/U:.4f} }} }} ] }}')
body = (0.12, 0.12, 0.13)
shapes = [box(-3.325, 12.325, -1.07, 3.3, 0, 9.91, body),          # housing
          box(3.0, 6.0, -2.47, -1.07, 0, 7.5, body)]                # latch ramp
for i in range(4):                                                  # contact cavities
    x = 3.0 * i
    shapes.append(box(x - 1.45, x + 1.45, -0.3, 2.6, 9.91, 9.93, (0.02, 0.02, 0.02)))
    shapes.append(box(x - 0.32, x + 0.32, 0.83, 1.47, 3.0, 9.94, (0.85, 0.7, 0.3)))   # pin tip
open(sys.argv[1], 'w').write('#VRML V2.0 utf8\n# ' + __doc__.strip().replace('\n', '\n# ') + '\n' + '\n'.join(shapes) + '\n')
