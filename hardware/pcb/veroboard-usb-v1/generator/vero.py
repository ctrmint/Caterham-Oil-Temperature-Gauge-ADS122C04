"""Model and checks for veroboard-usb-v1. Stdlib only.

    python3 vero.py        print the board's size and every problem; exit 1 if there are any

analyse() is what build.sh and tests/test_veroboard_usb_v1.py use. It checks that:
- the strips, cuts and links join exactly design.PARTS's nets: every net is one copper group, no group holds two
  nets, and each unconnected module pin is alone on its strip segment;
- R2 meets Rp's lead on the Kelvin strip from the far side (design.KELVIN);
- every part's leads are at least its datasheet's minimum spacing apart (a capacitor's exactly its lead spacing),
  along a strip or straight across them;
- no two bodies overlap or touch (CLEAR between them), no lead or link sits under another part's body, no lead
  or link passes over a hole another part uses, links don't cross each other or any body except the module's
  (links go in before it), and the terminal blocks' wire entries face the board edge with nothing in the way
  (their faces may overhang the end by up to OVERHANG);
- everything is on the board and clear of the long edges' slots (EDGE_KEEP), no hole holds two leads, and no
  lead sits in a cut.
Coordinates are mm from the board's top-left corner, component side up: hole (c, s) is at (c, s) x PITCH.
"""
import itertools
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import design as D  # noqa: E402
import layout as L  # noqa: E402

P = D.PITCH
CLEAR = 0.2          # mm between bodies, and between a body and another part's lead
LEAD_R = 0.4         # mm, a lead or link wire's half-width where it enters its hole
OVERHANG = 1.0       # mm a terminal block's wire-entry face may stand past the board's end
EDGE_KEEP = 1.0      # mm kept clear along the two long edges, which sit in slots in the box


def mm(hole):
    return hole[0] * P, hole[1] * P


def module_pins():
    c, s = L.MODULE_AT['col'], L.MODULE_AT['strip']
    out = {}
    for i in range(6):
        out[str(7 + i)] = (c, s + i)                                # VIN GND SCL SDA ADDR ALRT
        out[str(1 + i)] = (c + D.MODULE['row_spacing'], s + i)      # A+ A3 A2 A1 A0 A-
    return out


def block_pins(ref):
    col, first, _side = L.BLOCKS[ref]
    n = len(D.PARTS[ref][2])
    return {str(i + 1): (col, first + i) for i in range(n)}


def pins():
    """(ref, pin) -> hole, for every part."""
    out = {}
    for ref, (kind, _v, _p) in D.PARTS.items():
        if kind == 'module':
            got = module_pins()
        elif kind.startswith('MPT'):
            got = block_pins(ref)
        else:
            a, b = L.LEADS[ref]
            got = {'1': a, '2': b}
        for pin, hole in got.items():
            out[(ref, pin)] = hole
    return out


def body(ref):
    """The part's body as a rectangle (x0, y0, x1, y1) in mm."""
    kind = D.PARTS[ref][0]
    if kind == 'module':
        c, s = L.MODULE_AT['col'], L.MODULE_AT['strip']
        cx, cy = (c + D.MODULE['row_spacing'] / 2) * P, (s + 2.5) * P
        return (cx - D.MODULE['width'] / 2, cy - D.MODULE['length'] / 2,
                cx + D.MODULE['width'] / 2, cy + D.MODULE['length'] / 2)
    if kind.startswith('MPT'):
        hp = block_pins(ref)
        xs = [mm(h)[0] for h in hp.values()]
        ys = [mm(h)[1] for h in hp.values()]
        return (xs[0] - D.MPT['depth'] / 2, min(ys) - D.MPT['end'], xs[0] + D.MPT['depth'] / 2, max(ys) + D.MPT['end'])
    b = D.BODIES[kind]
    (x0, y0), (x1, y1) = mm(L.LEADS[ref][0]), mm(L.LEADS[ref][1])
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    along, across = (b['L'], b['D']) if b['type'] == 'axial' else (b['L'], b['T'])
    if y0 == y1:      # along a strip
        return (cx - along / 2, cy - across / 2, cx + along / 2, cy + across / 2)
    return (cx - across / 2, cy - along / 2, cx + across / 2, cy + along / 2)


def segment_of(hole, cuts):
    c, s = hole
    left = max([x for (x, y) in cuts if y == s and x < c], default=0)
    return (s, left)


def _overlap(a, b, gap=0.0):
    return a[0] < b[2] + gap and b[0] < a[2] + gap and a[1] < b[3] + gap and b[1] < a[3] + gap


def _inside(pt, r, gap=0.0):
    return r[0] - gap < pt[0] < r[2] + gap and r[1] - gap < pt[1] < r[3] + gap


def _between(hole, a, b):
    """Holes strictly between a and b on a straight line along or across the strips."""
    if a[0] == b[0] == hole[0]:
        return min(a[1], b[1]) < hole[1] < max(a[1], b[1])
    if a[1] == b[1] == hole[1]:
        return min(a[0], b[0]) < hole[0] < max(a[0], b[0])
    return False


def _seg_hits_rect(a, b, r):
    """Does the straight segment a-b (mm) pass through the open rectangle r?"""
    (x0, y0), (x1, y1) = a, b
    t0, t1 = 0.0, 1.0
    dx, dy = x1 - x0, y1 - y0
    for p, q in ((-dx, x0 - r[0]), (dx, r[2] - x0), (-dy, y0 - r[1]), (dy, r[3] - y0)):
        if p == 0:
            if q <= 0:
                return False
            continue
        t = q / p
        if p < 0:
            t0 = max(t0, t)
        else:
            t1 = min(t1, t)
    return t0 < t1


def _segs_cross(a, b, c, d):
    def orient(p, q, r):
        v = (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
        return (v > 0) - (v < 0)

    def on(p, q, r):
        return min(p[0], r[0]) <= q[0] <= max(p[0], r[0]) and min(p[1], r[1]) <= q[1] <= max(p[1], r[1])
    o1, o2, o3, o4 = orient(a, b, c), orient(a, b, d), orient(c, d, a), orient(c, d, b)
    if o1 != o2 and o3 != o4:
        return True
    return any(o == 0 and on(p, q, r) for o, p, q, r in ((o1, a, c, b), (o2, a, d, b), (o3, c, a, d), (o4, c, b, d)))


def analyse():
    problems = []
    pin_hole = pins()
    cuts = set(L.CUTS)
    link_ends = [(i, end, hole) for i, link in enumerate(L.LINKS) for end, hole in enumerate(link)]

    # --- holes: on the board, one lead each, never in a cut
    users = {}
    for (ref, pin), hole in pin_hole.items():
        users.setdefault(hole, []).append(f'{ref}.{pin}')
    for i, _end, hole in link_ends:
        users.setdefault(hole, []).append(f'link {i + 1}')
    for hole, who in sorted(users.items()):
        if not (1 <= hole[0] <= L.W and 1 <= hole[1] <= L.H):
            problems.append(f'{who} at {hole} is off the board')
        if len(who) > 1:
            problems.append(f'hole {hole} holds {", ".join(who)}')
        if hole in cuts:
            problems.append(f'{who} at {hole} is in a cut')
    if len(cuts) != len(L.CUTS):
        problems.append('a cut is listed twice')
    for c in cuts:
        if not (1 <= c[0] <= L.W and 1 <= c[1] <= L.H):
            problems.append(f'cut {c} is off the board')

    # --- connectivity: strip segments joined by links
    parent = {}

    def find(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for a, b in L.LINKS:
        parent[find(segment_of(a, cuts))] = find(segment_of(b, cuts))
    groups = {}
    for key, hole in pin_hole.items():
        groups.setdefault(find(segment_of(hole, cuts)), set()).add(key)
    want = {(ref, pin): net for ref, (_k, _v, pmap) in D.PARTS.items() for pin, net in pmap.items()}
    for net, members in D.nets().items():
        roots = {find(segment_of(pin_hole[m], cuts)) for m in members}
        if len(roots) > 1:
            problems.append(f'{net} is split into {len(roots)} copper groups')
    for root, members in groups.items():
        named = {want[m] for m in members if want[m]}
        if len(named) > 1:
            problems.append(f'nets {sorted(named)} are joined: {sorted(members)}')
        unconnected = [m for m in members if want[m] is None]
        if unconnected and len(members) > 1:
            problems.append(f'unconnected pin {unconnected} shares copper with {sorted(set(members) - set(unconnected))}')

    # --- Kelvin: the tap is at the far end of the sense lead's strip segment
    for tap, sense in D.KELVIN:
        th, sh = pin_hole[tap], pin_hole[sense]
        if segment_of(th, cuts) != segment_of(sh, cuts):
            problems.append(f'{tap} is not on the same strip segment as {sense}')
            continue
        seg = segment_of(sh, cuts)
        points = sorted(h[0] for h in users if h[1] == sh[1] and segment_of(h, cuts) == seg)
        order = [p for p in points]
        if not (order[:2] == [th[0], sh[0]] or order[-2:] == [sh[0], th[0]]):
            problems.append(f'{tap} must be the end connection on {sense}\'s strip, next to it (Kelvin)')

    # --- lead spacing
    for ref, (a, b) in L.LEADS.items():
        b_ = D.BODIES[D.PARTS[ref][0]]
        if a[0] != b[0] and a[1] != b[1]:
            problems.append(f'{ref} is not along or across the strips')
            continue
        span = abs(a[0] - b[0]) + abs(a[1] - b[1])
        if b_['type'] == 'axial' and span < b_['min_span']:
            problems.append(f'{ref} leads {span} holes apart, minimum {b_["min_span"]}')
        if b_['type'] == 'radial' and span != b_['span']:
            problems.append(f'{ref} leads {span} holes apart, must be {b_["span"]}')

    # --- bodies
    rects = {ref: body(ref) for ref in D.PARTS}
    outline = (0.0, 0.0, (L.W + 1) * P, (L.H + 1) * P)
    for ref, r in rects.items():
        # A terminal block's wire-entry face may overhang its end of the board by up to OVERHANG.
        side = L.BLOCKS.get(ref, (0, 0, ''))[2]
        lo = outline[0] - (OVERHANG if side == 'left' else 0)
        hi = outline[2] + (OVERHANG if side == 'right' else 0)
        if not (r[0] >= lo and r[1] >= outline[1] and r[2] <= hi and r[3] <= outline[3]):
            problems.append(f'{ref} body {tuple(round(v, 2) for v in r)} is off the board')
        elif r[1] < outline[1] + EDGE_KEEP or r[3] > outline[3] - EDGE_KEEP:
            problems.append(f'{ref} is within {EDGE_KEEP} mm of a long edge (the box slots)')
    for a, b in itertools.combinations(rects, 2):
        if _overlap(rects[a], rects[b], CLEAR):
            problems.append(f'{a} and {b} bodies overlap or are closer than {CLEAR} mm')
    own = {}
    for (ref, pin), hole in pin_hole.items():
        own.setdefault(hole, ref)
    for hole, who in users.items():
        for ref, r in rects.items():
            if own.get(hole) == ref:
                continue
            is_link = who[0].startswith('link')
            if is_link and D.PARTS[ref][0] == 'module':
                continue
            if _inside(mm(hole), r, LEAD_R + CLEAR):
                problems.append(f'{who[0]} at {hole} is under {ref}')

    # --- leads and links passing over holes in use
    spans = [(ref, a, b) for ref, (a, b) in L.LEADS.items()] + [(f'link {i + 1}', a, b) for i, (a, b) in enumerate(L.LINKS)]
    for name, a, b in spans:
        for hole, who in users.items():
            if hole not in (a, b) and _between(hole, a, b):
                problems.append(f'{name} passes over {who[0]} at {hole}')
    for i, (a, b) in enumerate(L.LINKS):
        if a[0] != b[0] and a[1] != b[1]:
            problems.append(f'link {i + 1} is not along or across the strips')
        for ref, r in rects.items():
            if D.PARTS[ref][0] != 'module' and _seg_hits_rect(mm(a), mm(b), r):
                problems.append(f'link {i + 1} runs under {ref}')
    for (i, (a, b)), (j, (c, d)) in itertools.combinations(enumerate(L.LINKS), 2):
        if _segs_cross(mm(a), mm(b), mm(c), mm(d)):
            problems.append(f'links {i + 1} and {j + 1} cross or touch')

    # --- terminal blocks: wires come in from the board edge with nothing in the way
    for ref, (_col, _first, side) in L.BLOCKS.items():
        r = rects[ref]
        lane = (outline[0], r[1], r[0], r[3]) if side == 'left' else (r[2], r[1], outline[2], r[3])
        if (lane[2] - lane[0]) > P:      # (negative when the face overhangs the end: fine)
            problems.append(f'{ref}\'s wire entry is more than one pitch from the {side} edge')
        for other, ro in rects.items():
            if other != ref and _overlap(lane, ro):
                problems.append(f'{other} is in front of {ref}\'s wire entry')

    segment_net = {}
    for root, members in groups.items():
        named = {want[m] for m in members if want[m]}
        segment_net[root] = named.pop() if len(named) == 1 else None
    seg_net = {}
    for s in range(1, L.H + 1):
        for c in range(1, L.W + 1):
            seg = segment_of((c, s), cuts)
            seg_net[seg] = segment_net.get(find(seg))
    return {'problems': problems, 'pins': pin_hole, 'bodies': rects, 'segment_net': seg_net, 'cuts': cuts}


def size_mm():
    return (L.W + 1) * P, (L.H + 1) * P


if __name__ == '__main__':
    a = analyse()
    w, h = size_mm()
    print(f'{D.PROJECT}: {L.W} holes x {L.H} strips, board {w:.1f} x {h:.1f} mm, '
          f'{len(L.CUTS)} cuts, {len(L.LINKS)} links')
    for p in a['problems']:
        print('PROBLEM', p)
    sys.exit(1 if a['problems'] else 0)
