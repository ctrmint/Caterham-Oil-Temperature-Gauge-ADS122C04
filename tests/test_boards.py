"""Checks on the option A and B boards (hardware/option-a, hardware/option-b; generators in scripts/boards).

- The generated files are current (each option's outputs.sha256).
- Both stripboard layouts pass their checker, and the checker really catches broken layouts.
- Every board carries exactly the documented circuit: option A's three resistors, option B's two extra capacitors
  across pin pairs only, nothing from an analogue input to ground, and the harness with its empty pin 3.
- On the PCBs, recomputed from the board files independently of KiCad: two copper layers, no pour, no vias, and
  AIN1, AIN2, SENDER and SENSE copper at least 1 mm from ground copper.
- The parts lists give part numbers only for the parts that were checked.
"""
import hashlib
import math
import re
import sys
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'scripts/boards'))
import circuit as C  # noqa: E402
import outputs as O  # noqa: E402
import vero as V  # noqa: E402
import vero_layout as VL  # noqa: E402

OPTIONS = {o: ROOT / f'hardware/option-{o}' for o in C.OPTIONS}
CHECKED_MPNS = {'6432', '1725672', '43650-0400', '43045-0600', '43645-0400', '43025-0600', '43030-0007', '63819-0000',
                'TSW-108-07-G-S', 'ESP32-S3-Touch-AMOLED-1.75'}


class TestOutputsCurrent(unittest.TestCase):
    def manifest(self, d):
        lines = (d / 'outputs.sha256').read_text(encoding='utf-8').splitlines()
        return dict(reversed(line.split('  ', 1)) for line in lines if line and not line.startswith('#'))

    def test_every_listed_file_matches(self):
        for o, d in OPTIONS.items():
            with self.subTest(option=o):
                stale = [rel for rel, digest in self.manifest(d).items()
                         if hashlib.sha256((d / rel).read_bytes()).hexdigest() != digest]
                self.assertEqual(stale, [], 'changed without a rebuild: run scripts/boards/build.sh')

    def test_no_unlisted_files(self):
        for o, d in OPTIONS.items():
            with self.subTest(option=o):
                self.assertEqual(sorted(self.manifest(d)), O.manifest_files(str(d)),
                                 'the option folder has files the last build did not write: run scripts/boards/build.sh')

    def test_each_option_has_its_readme_bom_and_both_boards(self):
        for o, d in OPTIONS.items():
            with self.subTest(option=o):
                for rel in ('README.md', 'BOM.md', 'veroboard/layout.svg', 'veroboard/ASSEMBLY.md',
                            f'pcb/option-{o}.kicad_pro', f'pcb/option-{o}.kicad_sch', f'pcb/option-{o}.kicad_pcb',
                            f'pcb/option-{o}.kicad_dru', f'pcb/fab/option-{o}-gerbers.zip'):
                    self.assertTrue((d / rel).is_file(), rel)


class TestStripboardLayouts(unittest.TestCase):
    def test_layouts_have_no_problems(self):
        for o in C.OPTIONS:
            with self.subTest(option=o):
                self.assertEqual(V.analyse(V.Board(o))['problems'], [])

    def test_same_board_for_both_options(self):
        a, b = V.Board('a'), V.Board('b')
        self.assertEqual((a.W, a.H, a.cuts, a.links), (b.W, b.H, b.cuts, b.links))
        self.assertEqual(set(b.parts) - set(a.parts), {'C1', 'C2'})

    def test_option_a_leaves_option_bs_holes_free(self):
        used = set(V.pins(V.Board('a')).values())
        for ref in ('C1', 'C2'):
            for hole in VL.LEADS_B[ref]:
                self.assertNotIn(hole, used)

    def test_resistors_flat_at_10_16_mm(self):
        for ref, (p, q) in VL.LEADS_A.items():
            self.assertEqual(abs(p[0] - q[0]) + abs(p[1] - q[1]), 4, ref)

    def test_capacitors_across_neighbouring_analogue_strips(self):
        a = V.analyse(V.Board('b'))
        for ref, nets in (('C1', {'AIN0', 'AIN1'}), ('C2', {'AIN1', 'AIN2'})):
            p, q = VL.LEADS_B[ref]
            self.assertEqual(p[0], q[0], ref)
            self.assertEqual(abs(p[1] - q[1]), 1, ref)
            got = {a['segment_net'][V.segment_of(h, a['cuts'])] for h in (p, q)}
            self.assertEqual(got, nets, ref)


class TestStripboardChecksCatchFaults(unittest.TestCase):
    def problems(self, option='b', **changes):
        return V.analyse(V.Board(option, **changes))['problems']

    def assertCaught(self, problems, words):
        self.assertTrue(any(words in p for p in problems), problems)

    def test_missing_cut_between_the_module_rows(self):
        self.assertCaught(self.problems(cuts=[c for c in VL.CUTS if c != (8, 5)]), 'are joined')

    def test_missing_cut_under_rref(self):
        self.assertCaught(self.problems(cuts=[c for c in VL.CUTS if c != (18, 4)]), "['AIN0', 'SENDER']")

    def test_missing_ground_link(self):
        self.assertCaught(self.problems(links=VL.LINKS[1:]), 'GND is split')

    def test_ground_beside_the_sense_strip(self):
        parts = VL.parts('b')
        parts['J2'] = (parts['J2'][0], parts['J2'][1], {'1': 'SENDER', '2': 'SENSE', '3': 'GND', '4': 'GND'})
        self.assertCaught(self.problems(parts=parts, cuts=[c for c in VL.CUTS if c != (12, 7)] + [(12, 7)]),
                          'runs beside ground')

    def test_short_resistor_spacing(self):
        leads = VL.leads('b')
        leads['R2'] = ((16, 6), (19, 6))
        self.assertCaught(self.problems(leads=leads), 'minimum 4')

    def test_capacitor_bodies_too_close(self):
        leads = VL.leads('b')
        leads['C2'] = ((14, 5), (14, 6))
        self.assertCaught(self.problems(leads=leads), 'bodies overlap')

    def test_lead_in_a_cut(self):
        leads = VL.leads('b')
        leads['R1'] = ((19, 5), (23, 5))
        self.assertCaught(self.problems(leads=leads), 'in a cut')

    def test_terminal_block_away_from_the_edge(self):
        blocks = dict(VL.BLOCKS)
        blocks['J2'] = (26, 5, 'right')
        self.assertCaught(self.problems(blocks=blocks), 'more than one pitch')


# --- PCB files, read without KiCad --------------------------------------------------------------------------

def sexpr_blocks(text, head):
    """Every `(head ...)` block in text, nested or not, balanced. KiCad writes either a space or a newline after
    the head."""
    out = []
    for m in re.finditer(rf'\({head}[\s]', text):
        i = m.start()
        d = 0
        for j in range(i, len(text)):
            if text[j] == '(':
                d += 1
            elif text[j] == ')':
                d -= 1
                if d == 0:
                    out.append(text[i:j + 1])
                    break
    return out


def pcb_copper(option):
    """Pads and track segments on copper: [(kind, net, layers, shape)], shape a circle (x, y, r) for pads (their
    circumscribed circle: conservative) or a capsule (x0, y0, x1, y1, half-width) for tracks. Board mm, y down."""
    text = (OPTIONS[option] / f'pcb/option-{option}.kicad_pcb').read_text(encoding='utf-8')
    items = []
    for fp in sexpr_blocks(text, 'footprint'):
        at = re.search(r'\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)', fp)
        fx, fy, fa = float(at.group(1)), float(at.group(2)), float(at.group(3) or 0)
        ref = re.search(r'\(property "Reference" "([^"]+)"', fp).group(1)
        for pad in sexpr_blocks(fp, 'pad'):
            if ' np_thru_hole ' in pad:
                continue
            m = re.search(r'\(at ([-\d.]+) ([-\d.]+)', pad)
            px, py = float(m.group(1)), float(m.group(2))
            t = math.radians(fa)
            x = fx + px * math.cos(t) + py * math.sin(t)
            y = fy - px * math.sin(t) + py * math.cos(t)
            w, h = map(float, re.search(r'\(size ([\d.]+) ([\d.]+)\)', pad).groups())
            net = re.search(r'\(net (?:\d+ )?"([^"]*)"\)', pad)
            num = re.search(r'\(pad "([^"]*)"', pad).group(1)
            items.append(('pad', net.group(1) if net else '', ('F.Cu', 'B.Cu'), (x, y, math.hypot(w, h) / 2),
                          f'{ref}.{num}'))
    for seg in sexpr_blocks(text, 'segment'):
        (x0, y0), (x1, y1) = [tuple(map(float, re.search(rf'\({k} ([-\d.]+) ([-\d.]+)\)', seg).groups()))
                              for k in ('start', 'end')]
        w = float(re.search(r'\(width ([\d.]+)\)', seg).group(1))
        layer = re.search(r'\(layer "([^"]+)"\)', seg).group(1)
        net = re.search(r'\(net (?:\d+ )?"([^"]*)"\)', seg).group(1)
        items.append(('track', net, (layer,), (x0, y0, x1, y1, w / 2), 'track'))
    return text, items


def _pt_seg(px, py, x0, y0, x1, y1):
    dx, dy = x1 - x0, y1 - y0
    L2 = dx * dx + dy * dy
    t = 0.0 if L2 == 0 else max(0.0, min(1.0, ((px - x0) * dx + (py - y0) * dy) / L2))
    return math.hypot(px - (x0 + t * dx), py - (y0 + t * dy))


def _seg_seg(a, b):
    return min(_pt_seg(a[0], a[1], *b[:4]), _pt_seg(a[2], a[3], *b[:4]),
               _pt_seg(b[0], b[1], *a[:4]), _pt_seg(b[2], b[3], *a[:4]))


def gap(a, b):
    """Copper-to-copper distance between two items (conservative for pads)."""
    (ka, _, _, sa, _), (kb, _, _, sb, _) = a, b
    if ka == 'pad' and kb == 'pad':
        return math.hypot(sa[0] - sb[0], sa[1] - sb[1]) - sa[2] - sb[2]
    if ka == 'pad':
        return _pt_seg(sa[0], sa[1], *sb[:4]) - sa[2] - sb[4]
    if kb == 'pad':
        return _pt_seg(sb[0], sb[1], *sa[:4]) - sb[2] - sa[4]
    return _seg_seg(sa, sb) - sa[4] - sb[4]


def pad_nets(option):
    return {name: net for kind, net, _l, _s, name in pcb_copper(option)[1] if kind == 'pad' and net}


class TestPcb(unittest.TestCase):
    def test_two_copper_layers_no_pour_no_vias_through_hole_only(self):
        for o in C.OPTIONS:
            with self.subTest(option=o):
                text, items = pcb_copper(o)
                self.assertIn('(layers\n\t\t(0 "F.Cu" signal)\n\t\t(2 "B.Cu" signal)', text)
                zones = sexpr_blocks(text, 'zone')
                self.assertEqual(len(zones), 4)
                for z in zones:      # only the screw-head keepouts: no copper pour anywhere
                    self.assertIn('(keepout', z)
                    self.assertIn('(copperpour not_allowed)', z)
                self.assertIsNone(re.search(r'\(via\b', text))
                self.assertIsNone(re.search(r'\(attr smd', text))
                layers = {it[2][0] for it in items if it[0] == 'track'}
                self.assertEqual(layers, {'F.Cu', 'B.Cu'})

    def test_pad_positions_read_back_where_the_layout_put_them(self):
        """Validates the transform used by the clearance check: J2 pin 1 and the module's VIN."""
        for o in C.OPTIONS:
            with self.subTest(option=o):
                pads = {it[4]: it[3][:2] for it in pcb_copper(o)[1] if it[0] == 'pad'}
                self.assertAlmostEqual(pads['J2.1'][0], 100 + 26.0, places=2)
                self.assertAlmostEqual(pads['J2.1'][1], 100 - 4.5, places=2)
                self.assertAlmostEqual(pads['J2.4'][1], 100 + 4.5, places=2)
                self.assertAlmostEqual(pads['U1.7'][0], 100 - 11.5 - 6.35, places=2)
                self.assertAlmostEqual(pads['U1.7'][1], 100 - 6.35, places=2)

    def test_sensitive_copper_at_least_1mm_from_ground(self):
        sensitive = {f'/{n}' for n in C.SENSITIVE}
        for o in C.OPTIONS:
            with self.subTest(option=o):
                items = pcb_copper(o)[1]
                gnd = [it for it in items if it[1] == 'GND']
                sens = [it for it in items if it[1] in sensitive]
                for group in (gnd, sens):
                    self.assertTrue([it for it in group if it[0] == 'track'] and [it for it in group if it[0] == 'pad'])
                worst = min((gap(s, g), s[4], g[4]) for s in sens for g in gnd if set(s[2]) & set(g[2]))
                self.assertGreaterEqual(worst[0], 1.0, worst)

    def test_drc_rule_covers_the_sensitive_nets(self):
        for o in C.OPTIONS:
            with self.subTest(option=o):
                rule = (OPTIONS[o] / f'pcb/option-{o}.kicad_dru').read_text(encoding='utf-8')
                self.assertIn('(constraint clearance (min 1.0mm))', rule)
                for n in C.SENSITIVE:
                    self.assertIn(f"'/{n}'", rule)

    def test_board_nets_are_the_circuit(self):
        for o in C.OPTIONS:
            with self.subTest(option=o):
                got = {}
                for name, net in pad_nets(o).items():
                    if not net.startswith('unconnected'):
                        got.setdefault(net.lstrip('/'), set()).add(tuple(name.split('.')))
                want = C.nets(C.front_end(o))
                for net in want:
                    want[net] = set(want[net])
                want.setdefault('+3V3', set()).add(('J1', '6'))
                want.setdefault('SCL', set()).add(('J1', '5'))
                want.setdefault('SDA', set()).add(('J1', '4'))
                want['GND'] |= {('J1', '1'), ('J1', '2'), ('J1', '3'), ('J2', '4')}
                want['SENDER'].add(('J2', '1'))
                want['SENSE'].add(('J2', '2'))
                self.assertEqual(got, want)

    def test_gerbers_hold_both_copper_layers_the_outline_and_drill(self):
        for o in C.OPTIONS:
            with self.subTest(option=o):
                with zipfile.ZipFile(OPTIONS[o] / f'pcb/fab/option-{o}-gerbers.zip') as z:
                    names = ' '.join(z.namelist())
                for part in ('F_Cu', 'B_Cu', 'F_Mask', 'B_Mask', 'F_Silkscreen', 'Edge_Cuts', '.drl'):
                    self.assertIn(part, names)


class TestCircuit(unittest.TestCase):
    def test_option_a_is_three_resistors_and_b_adds_two_capacitors(self):
        a, b = C.front_end('a'), C.front_end('b')
        self.assertEqual(set(a), {'U1', 'Rref', 'R1', 'R2'})
        self.assertEqual(set(b) - set(a), {'C1', 'C2'})
        self.assertEqual(a['Rref'][2], {'1': 'AIN0', '2': 'SENDER'})
        self.assertEqual(a['R1'][2], {'1': 'AIN1', '2': 'SENDER'})
        self.assertEqual(a['R2'][2], {'1': 'AIN2', '2': 'SENSE'})
        self.assertEqual(set(b['C1'][2].values()), {'AIN0', 'AIN1'})
        self.assertEqual(set(b['C2'][2].values()), {'AIN1', 'AIN2'})

    def test_nothing_connects_an_analogue_input_to_ground(self):
        for o in C.OPTIONS:
            for ref, (_k, _v, pins) in C.front_end(o).items():
                if ref == 'U1':
                    continue
                nets = set(pins.values())
                self.assertFalse(nets & {'AIN0', 'AIN1', 'AIN2'} and 'GND' in nets, ref)

    def test_every_harness_wire_reaches_the_adc_through_2k_or_more(self):
        for o in C.OPTIONS:
            parts = C.front_end(o)
            for net in ('SENDER', 'SENSE'):
                for ref, (_k, value, pins) in parts.items():
                    if net in pins.values() and ref != 'U1':
                        self.assertTrue(value.startswith(('2.00k', '2.2k')), (ref, value))

    def test_harness_pin_3_is_empty_between_sense_and_return(self):
        self.assertEqual(C.HARNESS, {'1': 'SENDER', '2': 'SENSE', '3': None, '4': 'GND'})

    def test_values_agree_with_the_firmware(self):
        cfg = (ROOT / 'main/Config.h').read_text(encoding='utf-8')
        ref = float(re.search(r'kReferenceOhms = ([\d.]+)f', cfg).group(1))
        bias = float(re.search(r'kGroundSenseBiasV = ([\d.]+)f', cfg).group(1))
        self.assertLess(abs(ref - 2000.0), 2000.0 * 0.01)                  # a measured 2.00 kOhm 0.1 % part
        self.assertAlmostEqual(bias, 10e-6 * 2200.0, places=6)              # the burn-out source across R2
        self.assertTrue(C.front_end('a')['R2'][1].startswith('2.2k'))

    def test_display_end_matches_the_firmware(self):
        hal = (ROOT / 'main/hal/Ads122c04Source.cpp').read_text(encoding='utf-8')
        self.assertIn('kSda = GPIO_NUM_17', hal)
        self.assertIn('kScl = GPIO_NUM_18', hal)
        end = dict(C.DISPLAY_END)
        self.assertEqual((end['17'], end['18'], end['3.3V'], end['G'], end['5V']), ('SDA', 'SCL', '+3V3', 'GND', None))

    def test_module_pins_follow_adafruits_board(self):
        self.assertEqual([C.MODULE_PINS[str(i)] for i in range(1, 13)],
                         ['Ref+', 'A0', 'A1', 'A2', 'A3', 'Ref-', 'VIN', 'GND', 'SCL', 'SDA', 'DRDY', '!RST'])


class TestBom(unittest.TestCase):
    def test_part_numbers_only_where_checked(self):
        for o, d in OPTIONS.items():
            for csv_path in (d / f'veroboard/option-{o}-veroboard-bom.csv', d / f'pcb/option-{o}-pcb-bom.csv'):
                with self.subTest(file=csv_path.name):
                    rows = csv_path.read_text(encoding='utf-8').splitlines()[1:]
                    mpns = {re.split(r',(?=(?:[^"]*"[^"]*")*[^"]*$)', r)[4] for r in rows}
                    self.assertTrue(mpns - {''} <= CHECKED_MPNS, mpns - {''} - CHECKED_MPNS)

    def test_bom_lists_every_board_part(self):
        for o, d in OPTIONS.items():
            with self.subTest(option=o):
                md = (d / 'BOM.md').read_text(encoding='utf-8')
                for ref in C.front_end(o):
                    self.assertIn(ref, md)
                self.assertEqual('C1, C2' in md, o == 'b')
                for word in ('Build 1: without a board', 'Build 2: stripboard', 'Build 3: PCB', '1725672',
                             '43650-0400', '43045-0600', 'TSW-108-07-G-S'):
                    self.assertIn(word, md)


if __name__ == '__main__':
    unittest.main()
