"""Draw veroboard-usb-v1's layout as SVG. Stdlib only.

    python3 draw.py <out.svg>

Two views to scale: the component side (parts, links, and the cuts underneath shown faintly) and the copper
side, mirrored as you see it when you turn the board over (strips coloured by net, the cuts, every joint).
Columns are numbered along the top and strips down the side, the same numbers in both views.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import design as D  # noqa: E402
import layout as L  # noqa: E402
import vero as V  # noqa: E402

S = 9.0                      # px per mm
P = D.PITCH
FONT = 'DejaVu Sans, Arial, Helvetica, sans-serif'
BW, BH = (L.W + 1) * P, (L.H + 1) * P
MX, MY = 60.0, 130.0         # left margin, top of the first view (px)
GAP = 150.0                  # between the views (px)
NET_COLOUR = {'+3V3': '#d62728', 'GND': '#444444', 'SCL': '#b8860b', 'SDA': '#1f6fd2', 'SENDER': '#e07000',
              'SENSE': '#7b3fb5', 'AIN0': '#138d75', 'AIN1': '#17a2b8', 'AIN3': '#2e8b57', 'TAP_S': '#c2185b',
              'TAP_G': '#8d6e00', None: '#bbbbbb'}

out = []


def add(s):
    out.append(s)


def text(x, y, s, size=11, anchor='middle', weight='normal', fill='#111', halo=False, rotate=0):
    h = ' stroke="#fff" stroke-width="3" paint-order="stroke" stroke-linejoin="round"' if halo else ''
    r = f' transform="rotate({rotate} {x:.1f} {y:.1f})"' if rotate else ''
    add(f'<text x="{x:.1f}" y="{y:.1f}" font-family="{FONT}" font-size="{size}" text-anchor="{anchor}" '
        f'font-weight="{weight}" fill="{fill}"{h}{r}>{s}</text>')


def view(ox, oy, mirror):
    """px position of a point given in board mm."""
    def f(x, y):
        return (ox + ((BW - x) if mirror else x) * S, oy + y * S)
    return f


def hole_px(f, hole):
    return f(hole[0] * P, hole[1] * P)


def rect_px(f, r):
    (x0, y0), (x1, y1) = f(r[0], r[1]), f(r[2], r[3])
    return min(x0, x1), min(y0, y1), abs(x1 - x0), abs(y1 - y0)


def board(f, title, copper):
    x, y, w, h = rect_px(f, (0, 0, BW, BH))
    add(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" fill="{"#e9d8b0" if copper else "#e8dcb8"}" '
        f'stroke="#555" stroke-width="2"/>')
    text(x, y - 40, title, 15, 'start', 'bold')
    for c in range(1, L.W + 1):
        px, _ = hole_px(f, (c, 0))
        if c == 1 or c % 5 == 0 or c == L.W:
            text(px, y - 8, str(c), 10, fill='#555')
    for s in range(1, L.H + 1):
        _, py = hole_px(f, (0, s))
        text(x - 8, py + 4, str(s), 10, 'end', fill='#555')
        text(x + w + 8, py + 4, str(s), 10, 'start', fill='#555')


def strips(f, a, copper):
    for s in range(1, L.H + 1):
        for c in range(1, L.W + 1):
            net = a['segment_net'][V.segment_of((c, s), a['cuts'])]
            colour = NET_COLOUR.get(net, '#bbbbbb') if copper else '#c98f4a'
            x0, y = hole_px(f, (c - 0.5, s))
            x1, _ = hole_px(f, (c + 0.5, s))
            add(f'<line x1="{x0:.1f}" y1="{y:.1f}" x2="{x1:.1f}" y2="{y:.1f}" stroke="{colour}" '
                f'stroke-width="{1.9 * S:.1f}" opacity="{0.55 if copper else 0.18}"/>')
    for s in range(1, L.H + 1):
        for c in range(1, L.W + 1):
            px, py = hole_px(f, (c, s))
            add(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="{0.5 * S:.1f}" fill="{"#fff" if copper else "#f6efe0"}" '
                f'stroke="#9a8a6a" stroke-width="0.6"/>')


def cuts(f, faint):
    for c, s in L.CUTS:
        px, py = hole_px(f, (c, s))
        r = 1.15 * S
        if faint:
            add(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="{r:.1f}" fill="none" stroke="#e53935" stroke-width="1.2" '
                f'stroke-dasharray="3 2" opacity="0.7"/>')
        else:
            add(f'<rect x="{px - r:.1f}" y="{py - 1.0 * S:.1f}" width="{2 * r:.1f}" height="{2.0 * S:.1f}" fill="#fff" '
                f'stroke="#e53935" stroke-width="2"/>')
            add(f'<line x1="{px - r:.1f}" y1="{py - 1.0 * S:.1f}" x2="{px + r:.1f}" y2="{py + 1.0 * S:.1f}" '
                f'stroke="#e53935" stroke-width="2"/>')


def links(f):
    for i, (a, b) in enumerate(L.LINKS):
        (x0, y0), (x1, y1) = hole_px(f, a), hole_px(f, b)
        add(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}" stroke="#8a8a8a" stroke-width="{0.6 * S:.1f}" '
            f'stroke-linecap="round"/>')
        for x, y in ((x0, y0), (x1, y1)):
            add(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.55 * S:.1f}" fill="#8a8a8a"/>')
        mx, my = (x0 + x1) / 2, (y0 + y1) / 2
        under = V._seg_hits_rect(V.mm(a), V.mm(b), V.body('U1'))
        text(mx + (0 if a[1] == b[1] else 5), my + (14 if a[1] == b[1] else 4), f'L{i + 1}', 9,
             'middle' if a[1] == b[1] else 'start', 'bold', '#666' if under else '#333', halo=True)


BODY_FILL = {'R0207': '#d9c79a', 'DO35': '#e8742c', 'DO15': '#222222', 'FG26': '#3d74b8'}


def parts(f, a):
    pins = a['pins']
    for ref, (kind, value, _p) in D.PARTS.items():
        if kind in BODY_FILL:      # leads first, under the body
            (x0, y0), (x1, y1) = hole_px(f, pins[(ref, '1')]), hole_px(f, pins[(ref, '2')])
            add(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}" stroke="#9e9e9e" stroke-width="{0.6 * S:.1f}"/>')
            for x, y in ((x0, y0), (x1, y1)):
                add(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{0.5 * S:.1f}" fill="#9e9e9e"/>')
    for ref, (kind, value, _p) in D.PARTS.items():
        x, y, w, h = rect_px(f, a['bodies'][ref])
        cx, cy = x + w / 2, y + h / 2
        if kind == 'module':
            add(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{2.54 * S:.1f}" fill="#1f3f8f" '
                f'fill-opacity="0.18" stroke="#1f3f8f" stroke-width="2" stroke-dasharray="6 3"/>')
            for pin, hole in V.module_pins().items():
                px, py = hole_px(f, hole)
                add(f'<rect x="{px - 0.9 * S:.1f}" y="{py - 0.9 * S:.1f}" width="{1.8 * S:.1f}" height="{1.8 * S:.1f}" fill="#d4a017"/>')
                left = (int(pin) >= 7) != (f(0, 0)[0] > f(1, 0)[0])
                text(px + (13 if left else -13), py + 4, D.MODULE_PINS[pin], 9, 'start' if left else 'end', 'bold',
                     '#1f3f8f', halo=True)
            text(cx, y + 16, f'{ref} {value}', 11, weight='bold', fill='#1f3f8f', halo=True)
            text(cx, y + h - 8, 'chip side up', 9, fill='#1f3f8f', halo=True)
            continue
        if kind.startswith('MPT'):
            add(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="3" fill="#2e7d32" fill-opacity="0.85" '
                f'stroke="#1b5e20" stroke-width="1.5"/>')
            side = L.BLOCKS[ref][2]
            for pin, hole in V.block_pins(ref).items():
                px, py = hole_px(f, hole)
                add(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="{0.6 * S:.1f}" fill="#c0c0c0" stroke="#333"/>')
                text(px + (12 if side == 'left' else -12), py + 4, pin, 9, 'middle', 'bold', '#fff')
            ax = x - 12 if (side == 'left') != (f(0, 0)[0] > f(1, 0)[0]) else x + w + 12
            text(cx, y - 6, ref, 11, weight='bold', fill='#1b5e20', halo=True)
            text(ax, cy + 4, 'wires', 9, 'end' if ax < x else 'start', fill='#1b5e20')
            continue
        fill = BODY_FILL[kind]
        rx = min(w, h) / 2 if kind != 'FG26' else 3
        add(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx:.1f}" fill="{fill}" stroke="#333" stroke-width="1"/>')
        if kind in ('DO35', 'DO15'):     # cathode band at pin 1's end
            k = hole_px(f, pins[(ref, '1')])
            band = 0.6 * S
            if w > h:
                bx = x + (band if k[0] < cx else w - 2 * band)
                add(f'<rect x="{bx:.1f}" y="{y:.1f}" width="{band:.1f}" height="{h:.1f}" fill="{"#ddd" if kind == "DO15" else "#222"}"/>')
            else:
                by = y + (band if k[1] < cy else h - 2 * band)
                add(f'<rect x="{x:.1f}" y="{by:.1f}" width="{w:.1f}" height="{band:.1f}" fill="{"#ddd" if kind == "DO15" else "#222"}"/>')
        vertical = h > w
        short = value.split()[0]
        label_fill = '#fff' if kind in ('DO15', 'FG26') else '#111'
        if kind == 'DO35':                       # too small to write on: label beside it
            text(x + w + 3, cy + 4, ref, 9, 'start', 'bold', halo=True)
        elif vertical and kind == 'FG26':
            text(cx, cy - 1, ref, 9, weight='bold', fill=label_fill)
            text(cx, cy + 10, short, 8, fill=label_fill)
        elif vertical:
            text(cx + 4, cy, f'{ref} {short}', 9, weight='bold', fill=label_fill, rotate=-90)
        else:
            text(cx, cy + 4, f'{ref} {short}', 9, weight='bold', fill=label_fill)


def joints(f, a):
    for (ref, pin), hole in a['pins'].items():
        px, py = hole_px(f, hole)
        add(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="{0.75 * S:.1f}" fill="#b0b0b0" stroke="#555" stroke-width="1"/>')
    for a_, b_ in L.LINKS:
        for hole in (a_, b_):
            px, py = hole_px(f, hole)
            add(f'<circle cx="{px:.1f}" cy="{py:.1f}" r="{0.75 * S:.1f}" fill="#d0d0d0" stroke="#555" stroke-width="1"/>')


def net_labels(f, a):
    """Name each strip segment's net beside its first hole on the copper side."""
    done = set()
    for s in range(1, L.H + 1):
        for c in range(1, L.W + 1):
            seg = V.segment_of((c, s), a['cuts'])
            net = a['segment_net'][seg]
            if seg in done or not net:
                continue
            done.add(seg)
            ends = [x for x in range(1, L.W + 1) if V.segment_of((x, s), a['cuts']) == seg]
            mid = (ends[0] + ends[-1]) / 2
            px, py = hole_px(f, (mid, s))
            text(px, py + 3.5, net, 8, weight='bold', fill=NET_COLOUR.get(net, '#333'), halo=True)


def main(path):
    a = V.analyse()
    if a['problems']:
        raise SystemExit('layout problems:\n' + '\n'.join(a['problems']))
    total_w = MX * 2 + BW * S + 40
    total_h = MY + 2 * BH * S + GAP + 300
    add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{total_w:.0f}" height="{total_h:.0f}" '
        f'viewBox="0 0 {total_w:.0f} {total_h:.0f}">')
    add(f'<rect width="{total_w:.0f}" height="{total_h:.0f}" fill="#ffffff"/>')
    w_mm, h_mm = V.size_mm()
    text(MX, 30, f'{D.PROJECT}: {L.W} holes x {L.H} strips, {w_mm:.1f} x {h_mm:.1f} mm, '
                 f'{len(L.CUTS)} track cuts, {len(L.LINKS)} wire links', 17, 'start', 'bold')
    text(MX, 52, 'Strips run left to right. Columns are numbered along the top, strips down the sides; '
                 'the copper side is mirrored, as you see it with the board turned over.', 11, 'start', fill='#444')
    f1 = view(MX, MY, False)
    board(f1, 'Component side', False)
    strips(f1, a, False)
    cuts(f1, True)
    links(f1)
    parts(f1, a)
    y2 = MY + BH * S + GAP
    f2 = view(MX, y2, True)
    board(f2, 'Copper side (board turned over left to right): cut the strips at the red marks', True)
    strips(f2, a, True)
    cuts(f2, False)
    joints(f2, a)
    net_labels(f2, a)
    ly = y2 + BH * S + 40
    text(MX, ly, 'Cuts (column, strip): ' + ', '.join(f'{c},{s}' for c, s in sorted(L.CUTS)), 11, 'start')
    text(MX, ly + 20, 'Links: ' + ', '.join(f'L{i + 1} {a_[0]},{a_[1]}-{b_[0]},{b_[1]}' for i, (a_, b_) in enumerate(L.LINKS)),
         11, 'start')
    text(MX, ly + 40, 'Diodes: the band (cathode) is drawn at the end marked. The module sits chip side up on its '
                      'header pins, soldered, never socketed. L1-L5 go under the module: fit them first.', 11, 'start')
    legend = [(net, col) for net, col in NET_COLOUR.items() if net]
    for i, (net, col) in enumerate(legend):
        x = MX + i * 78
        add(f'<rect x="{x:.1f}" y="{ly + 58:.1f}" width="14" height="10" fill="{col}" opacity="0.7"/>')
        text(x + 18, ly + 67, net, 10, 'start')
    add('</svg>')
    with open(path, 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main(sys.argv[1])
