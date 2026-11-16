"""Model and checks for the option A and B stripboards. Stdlib only.

    python3 vero.py [a|b]      print each board's size and every problem; exit 1 if there are any

analyse(board) is what build.sh and tests/test_boards.py use. It checks that:
- the strips, cuts and links join exactly the circuit's nets: every net is one copper group, no group holds two
  nets, and each unconnected pin is alone on its strip segment;
- no strip carrying a sensitive net (circuit.SENSITIVE) runs beside ground copper, where a solder bridge between
  neighbouring strips would give a believable wrong reading instead of a fault;
- every part's leads are at least its minimum spacing apart (a capacitor's exactly its lead spacing), along a strip
  or straight across them;
- no two bodies overlap or touch (CLEAR between them), no lead or link sits under another part's body, no lead or
  link passes over a hole another part uses, links don't cross each other or any body except the module's (links
  go in before it), and the terminal blocks' wire entries face the board edge with nothing in the way;
- everything is on the board and clear of the long edges (EDGE_KEEP), no hole holds two leads, and no lead sits in
  a cut.
Coordinates are mm from the board's top-left corner, component side up: hole (c, s) is at (c, s) x PITCH.
"""
import itertools
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import circuit as C  # noqa: E402
import vero_layout as L  # noqa: E402

P = 2.54
CLEAR = 0.2          # mm between bodies, and between a body and another part's lead
LEAD_R = 0.4         # mm, a lead or link wire's half-width where it enters its hole
OVERHANG = 1.0       # mm a terminal block's wire-entry face may stand past the board's end
EDGE_KEEP = 1.0      # mm kept clear along the two long edges

# Body sizes (mm). Axial: L along the leads, D across; min_span is the closest lead spacing in holes.
# Radial: L along the line of the leads, T thickness; span is the lead spacing in holes.
BODIES = {
    # 0207 metal film: 6.5 long and 2.5 across at most; laid flat at 10.16 mm (Vishay doc 28766: 10.0 mm minimum).
    'R0207': {'type': 'axial', 'L': 6.5, 'D': 2.5, 'min_span': 4, 'height': 2.5},
    # Option B's C0G capacitors: 2.5 mm lead spacing; the BOM limits the body to 4.6 mm along the line of the leads
    # and 3.8 mm thick (the PCB's C_Rect_L4.6mm_W3.8mm_P2.50mm footprint), 7 mm tall.
    'RAD2': {'type': 'radial', 'L': 4.6, 'T': 3.8, 'span': 1, 'height': 7.0},
}
# Phoenix Contact MPT 0,5 (1725672): 2.54 mm pitch, the body 1.5 mm beyond each end pin, 6.2 deep with the pins in
# the middle, 8.5 tall; the wire enters from one long side, parallel to the board.
MPT = {'end': 1.5, 'depth': 6.2, 'height': 8.5}


class Board:
    """One option's stripboard. Tests pass changed tables to check that the checks catch faults."""

    def __init__(self, option, parts=None, leads=None, cuts=None, links=None, blocks=None):
        self.option = option
        self.parts = parts if parts is not None else L.parts(option)
        self.leads = leads if leads is not None else L.leads(option)
        self.cuts = list(cuts if cuts is not None else L.CUTS)
        self.links = list(links if links is not None else L.LINKS)
        self.blocks = dict(blocks if blocks is not None else L.BLOCKS)
        self.W, self.H = L.W, L.H

    def size_mm(self):
        return (self.W + 1) * P, (self.H + 1) * P


def mm(hole):
    return hole[0] * P, hole[1] * P


def module_pins():
    c, s = L.MODULE_AT['col'], L.MODULE_AT['strip']
    out = {}
    for i in range(6):
        out[str(7 + i)] = (c, s + i)                       # VIN GND SCL SDA DRDY !RST
        out[str(1 + i)] = (c + L.ROW_SPACING, s + i)       # Ref+ A0 A1 A2 A3 Ref-
    return out


def block_pins(b, ref):
    col, first, _side = b.blocks[ref]
    return {str(i + 1): (col, first + i) for i in range(len(b.parts[ref][2]))}


def pins(b):
    """(ref, pin) -> hole, for every part."""
    out = {}
    for ref, (kind, _v, _p) in b.parts.items():
        if kind == 'module':
            got = module_pins()
        elif kind.startswith('MPT'):
            got = block_pins(b, ref)
        else:
            a, c = b.leads[ref]
            got = {'1': a, '2': c}
        for pin, hole in got.items():
            out[(ref, pin)] = hole
    return out


def body(b, ref):
    """The part's body as a rectangle (x0, y0, x1, y1) in mm."""
    kind = b.parts[ref][0]
    if kind == 'module':
        c, s = L.MODULE_AT['col'], L.MODULE_AT['strip']
        cx, cy = (c + L.ROW_SPACING / 2) * P, (s + 2.5) * P
        return (cx - C.MODULE['width'] / 2, cy - C.MODULE['length'] / 2,
                cx + C.MODULE['width'] / 2, cy + C.MODULE['length'] / 2)
    if kind.startswith('MPT'):
        hp = block_pins(b, ref)
        xs = [mm(h)[0] for h in hp.values()]
        ys = [mm(h)[1] for h in hp.values()]
        return (xs[0] - MPT['depth'] / 2, min(ys) - MPT['end'], xs[0] + MPT['depth'] / 2, max(ys) + MPT['end'])
    bd = BODIES[kind]
    (x0, y0), (x1, y1) = mm(b.leads[ref][0]), mm(b.leads[ref][1])
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    along, across = (bd['L'], bd['D']) if bd['type'] == 'axial' else (bd['L'], bd['T'])
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


def analyse(b):
    problems = []
    pin_hole = pins(b)
    cuts = set(b.cuts)
    link_ends = [(i, end, hole) for i, link in enumerate(b.links) for end, hole in enumerate(link)]

    # --- holes: on the board, one lead each, never in a cut
    users = {}
    for (ref, pin), hole in pin_hole.items():
        users.setdefault(hole, []).append(f'{ref}.{pin}')
    for i, _end, hole in link_ends:
        users.setdefault(hole, []).append(f'link {i + 1}')
    for hole, who in sorted(users.items()):
        if not (1 <= hole[0] <= b.W and 1 <= hole[1] <= b.H):
            problems.append(f'{who} at {hole} is off the board')
        if len(who) > 1:
            problems.append(f'hole {hole} holds {", ".join(who)}')
        if hole in cuts:
            problems.append(f'{who} at {hole} is in a cut')
    if len(cuts) != len(b.cuts):
        problems.append('a cut is listed twice')
    for c in cuts:
        if not (1 <= c[0] <= b.W and 1 <= c[1] <= b.H):
            problems.append(f'cut {c} is off the board')

    # --- connectivity: strip segments joined by links
    parent = {}

    def find(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for a, c in b.links:
        parent[find(segment_of(a, cuts))] = find(segment_of(c, cuts))
    groups = {}
    for key, hole in pin_hole.items():
        groups.setdefault(find(segment_of(hole, cuts)), set()).add(key)
    want = {(ref, pin): net for ref, (_k, _v, pmap) in b.parts.items() for pin, net in pmap.items()}
    for net, members in C.nets(b.parts).items():
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

    segment_net = {}
    for root, members in groups.items():
        named = {want[m] for m in members if want[m]}
        segment_net[root] = named.pop() if len(named) == 1 else None

    def net_at(hole):
        if hole in cuts:
            return None
        return segment_net.get(find(segment_of(hole, cuts)))

    # --- sensitive strips never beside ground copper
    reported = set()
    for s in range(1, b.H):
        for c in range(1, b.W + 1):
            pair = {net_at((c, s)), net_at((c, s + 1))}
            if 'GND' in pair and pair & set(C.SENSITIVE):
                other = (pair - {'GND'}).pop()
                if (s, other) not in reported:
                    reported.add((s, other))
                    problems.append(f'{other} on strip {s if net_at((c, s)) == other else s + 1} runs beside ground '
                                    f'from column {c}: a solder bridge there would give a believable wrong reading')

    # --- lead spacing
    for ref, (a, c) in b.leads.items():
        bd = BODIES[b.parts[ref][0]]
        if a[0] != c[0] and a[1] != c[1]:
            problems.append(f'{ref} is not along or across the strips')
            continue
        span = abs(a[0] - c[0]) + abs(a[1] - c[1])
        if bd['type'] == 'axial' and span < bd['min_span']:
            problems.append(f'{ref} leads {span} holes apart, minimum {bd["min_span"]}')
        if bd['type'] == 'radial' and span != bd['span']:
            problems.append(f'{ref} leads {span} holes apart, must be {bd["span"]}')

    # --- bodies
    rects = {ref: body(b, ref) for ref in b.parts}
    w, h = b.size_mm()
    outline = (0.0, 0.0, w, h)
    for ref, r in rects.items():
        side = b.blocks.get(ref, (0, 0, ''))[2]
        lo = outline[0] - (OVERHANG if side == 'left' else 0)
        hi = outline[2] + (OVERHANG if side == 'right' else 0)
        if not (r[0] >= lo and r[1] >= outline[1] and r[2] <= hi and r[3] <= outline[3]):
            problems.append(f'{ref} body {tuple(round(v, 2) for v in r)} is off the board')
        elif r[1] < outline[1] + EDGE_KEEP or r[3] > outline[3] - EDGE_KEEP:
            problems.append(f'{ref} is within {EDGE_KEEP} mm of a long edge')
    for a, c in itertools.combinations(rects, 2):
        if _overlap(rects[a], rects[c], CLEAR):
            problems.append(f'{a} and {c} bodies overlap or are closer than {CLEAR} mm')
    own = {}
    for (ref, pin), hole in pin_hole.items():
        own.setdefault(hole, ref)
    for hole, who in users.items():
        for ref, r in rects.items():
            if own.get(hole) == ref:
                continue
            if who[0].startswith('link') and b.parts[ref][0] == 'module':
                continue
            if _inside(mm(hole), r, LEAD_R + CLEAR):
                problems.append(f'{who[0]} at {hole} is under {ref}')

    # --- leads and links passing over holes in use
    spans = [(ref, a, c) for ref, (a, c) in b.leads.items()] + \
            [(f'link {i + 1}', a, c) for i, (a, c) in enumerate(b.links)]
    for name, a, c in spans:
        for hole, who in users.items():
            if hole not in (a, c) and _between(hole, a, c):
                problems.append(f'{name} passes over {who[0]} at {hole}')
    for i, (a, c) in enumerate(b.links):
        if a[0] != c[0] and a[1] != c[1]:
            problems.append(f'link {i + 1} is not along or across the strips')
        for ref, r in rects.items():
            if b.parts[ref][0] != 'module' and _seg_hits_rect(mm(a), mm(c), r):
                problems.append(f'link {i + 1} runs under {ref}')
    for (i, (a, c)), (j, (d, e)) in itertools.combinations(enumerate(b.links), 2):
        if _segs_cross(mm(a), mm(c), mm(d), mm(e)):
            problems.append(f'links {i + 1} and {j + 1} cross or touch')

    # --- terminal blocks: wires come in from the board edge with nothing in the way
    for ref, (_col, _first, side) in b.blocks.items():
        r = rects[ref]
        lane = (outline[0], r[1], r[0], r[3]) if side == 'left' else (r[2], r[1], outline[2], r[3])
        if (lane[2] - lane[0]) > P:
            problems.append(f'{ref}\'s wire entry is more than one pitch from the {side} edge')
        for other, ro in rects.items():
            if other != ref and _overlap(lane, ro):
                problems.append(f'{other} is in front of {ref}\'s wire entry')

    seg_net = {}
    for s in range(1, b.H + 1):
        for c in range(1, b.W + 1):
            seg_net[segment_of((c, s), cuts)] = segment_net.get(find(segment_of((c, s), cuts)))
    return {'problems': problems, 'pins': pin_hole, 'bodies': rects, 'segment_net': seg_net, 'cuts': cuts}


if __name__ == '__main__':
    bad = False
    for opt in (sys.argv[1:] or C.OPTIONS):
        b = Board(opt)
        a = analyse(b)
        w, h = b.size_mm()
        print(f'option {opt.upper()} stripboard: {b.W} holes x {b.H} strips, {w:.1f} x {h:.1f} mm, '
              f'{len(b.cuts)} cuts, {len(b.links)} links, {len(b.parts)} parts')
        for p in a['problems']:
            print('PROBLEM', p)
        bad = bad or bool(a['problems'])
    sys.exit(1 if bad else 0)
