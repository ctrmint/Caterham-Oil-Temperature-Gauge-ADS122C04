"""Dimensioned drawing for the 3D-printed box (run inside KiCad by build.sh).

Plan view of the board with its outline, M3 mounting holes and the connectors, plus the cable wall:
where each connector's mating face sits along the bottom edge and how high its opening must be.
All dimensions are mm, from the board's bottom-left corner.
"""
import os
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import design as D  # noqa: E402
import layout as L  # noqa: E402

S = 6.0          # px per mm
M = 22.0         # margin, mm
W, H = L.WIDTH, L.HEIGHT


def bl(x, y):
    """Board coordinates (centre origin, y up) to bottom-left-corner coordinates."""
    return x + W / 2, y + H / 2


def px(x, y):
    """Bottom-left coordinates to SVG pixels (plan view)."""
    return (M + x) * S, (M + H - y) * S


def courtyard_box(fp):
    cy = fp.GetCourtyard(pcbnew.F_CrtYd)
    bb = cy.BBox()
    x0, x1 = pcbnew.ToMM(bb.GetLeft()) - 100, pcbnew.ToMM(bb.GetRight()) - 100
    y0, y1 = 100 - pcbnew.ToMM(bb.GetBottom()), 100 - pcbnew.ToMM(bb.GetTop())
    return x0, y0, x1, y1


def main(pcb, out):
    b = pcbnew.LoadBoard(pcb)
    fps = {fp.GetReference(): fp for fp in b.GetFootprints()}
    o = []
    vw, vh = (W + 2 * M + 70) * S, (H + 2 * M + 48) * S
    o.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{vw:.0f}" height="{vh:.0f}" '
             f'viewBox="0 0 {vw:.0f} {vh:.0f}" font-family="DejaVu Sans, Arial, sans-serif">')
    o.append(f'<rect width="{vw:.0f}" height="{vh:.0f}" fill="white"/>')
    o.append(f'<text x="{M * S}" y="{M * S / 2}" font-size="15" font-weight="bold">{D.PROJECT}: box drawing '
             f'(plan view, component side up; mm from the bottom-left corner)</text>')
    x0, y0 = px(0, H)
    o.append(f'<rect x="{x0}" y="{y0}" width="{W * S}" height="{H * S}" rx="{L.CORNER * S}" fill="#f4f8f0" '
             'stroke="black" stroke-width="2"/>')
    # overall dimensions
    o.append(f'<text x="{px(W / 2, 0)[0]}" y="{px(0, -6)[1]}" font-size="13" text-anchor="middle">'
             f'{W:g} wide (corner radius {L.CORNER:g})</text>')
    o.append(f'<text x="{px(W + 4, H / 2)[0]}" y="{px(0, H / 2)[1]}" font-size="13">{H:g} high</text>')
    for i, (hx, hy) in enumerate(L.HOLES):
        x, y = bl(hx, hy)
        cx, cy = px(x, y)
        o.append(f'<circle cx="{cx}" cy="{cy}" r="{1.6 * S}" fill="white" stroke="black" stroke-width="1.5"/>')
        o.append(f'<circle cx="{cx}" cy="{cy}" r="{L.HOLE_KEEPOUT * S}" fill="none" stroke="#888" '
                 'stroke-dasharray="4 3"/>')
        o.append(f'<text x="{cx}" y="{cy - 4.2 * S}" font-size="12" text-anchor="middle">H{i + 1}</text>')
    for ref, fp in sorted(fps.items()):
        if not fp.GetCourtyard(pcbnew.F_CrtYd).OutlineCount() or ref.startswith('H'):
            continue
        a, b0, c, d = courtyard_box(fp)
        (ax, ay), (cx2, cy2) = bl(a, b0), bl(c, d)
        p0, p1 = px(ax, cy2), px(cx2, ay)
        conn = ref in BOX_OPENINGS
        o.append(f'<rect x="{p0[0]}" y="{p0[1]}" width="{p1[0] - p0[0]}" height="{p1[1] - p0[1]}" '
                 f'fill="{"#ffd9b3" if conn else "none"}" stroke="{"#c60" if conn else "#bbb"}" stroke-width="1"/>')
        o.append(f'<text x="{(p0[0] + p1[0]) / 2}" y="{(p0[1] + p1[1]) / 2 + 4}" font-size="{12 if conn else 9}" '
                 f'text-anchor="middle" fill="{"#c60" if conn else "#999"}">{ref}</text>')
    lines = [('b', 'Mounting holes (M3 clearance, 3.2 mm; keep bosses inside the dashed circles):')]
    for i, (hx, hy) in enumerate(L.HOLES):
        x, y = bl(hx, hy)
        lines.append(('', f'H{i + 1} at x {x:g}, y {y:g}; boss up to {2 * L.HOLE_KEEPOUT:g} mm across'))
    lines.append(('b', 'Cable wall (bottom edge): an opening for each plug, x from the board\'s left edge:'))
    for ref, (label, height) in BOX_OPENINGS.items():
        a, _b, c, _d = courtyard_box(fps[ref])
        xa, xc = bl(a, 0)[0], bl(c, 0)[0]
        lines.append(('', f'{ref} {label}: x {xa:.1f} to {xc:.1f} ({xc - xa:.1f} wide), {height} above the board top'))
    lines.append(('', 'Allow 1 mm round each opening and room above it for the plug\'s latch to be pressed.'))
    lines.append(('b', 'Heights:'))
    for t in TALLEST:
        lines.append(('', t))
    wy = (M + H + 12) * S
    for i, (style, text) in enumerate(lines):
        o.append(f'<text x="{M * S}" y="{wy + 19 * i}" font-size="13"'
                 f'{" font-weight=" + chr(34) + "bold" + chr(34) if style == "b" else ""}>{text}</text>')
    o.append('</svg>')
    with open(out, 'w', encoding='utf-8') as f:
        f.write('\n'.join(o) + '\n')


# ref: (what plugs in, opening height above the board's top surface)
BOX_OPENINGS = {
    'J3': ('display cable, Micro-Fit 2x3 plug', 'about 8 mm (header body 7.4 mm)'),
    'J2': ('sender harness, Micro-Fit 1x4 plug', 'about 5 mm (header body 4.4 mm)'),
}
TALLEST = [
    'J3, the 2x3 header, is the tallest part (body 7.4 mm). The ADS1115 module on its 2.54 mm header stands about 5 mm,',
    'and the test loops 4.6 mm (Keystone drawing). Check the C0G capacitors (TDK FG26) on their datasheet; allow 15 mm above the board.',
    'Leave at least 5 mm under the board for the solder side and the M3 screw heads or nuts.',
]

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
