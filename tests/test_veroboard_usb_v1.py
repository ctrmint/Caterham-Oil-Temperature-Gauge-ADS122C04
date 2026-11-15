"""Checks on the veroboard-usb-v1 stripboard layout (hardware/pcb/veroboard-usb-v1).

The layout's checker (generator/vero.py) joins the strips through their cuts and links and compares the result
with the circuit, and checks the parts' sizes and spacings. These tests run it, check that it really catches
broken layouts, check the circuit is through-hole-usb-v1's, and check the generated files are current.
"""
import hashlib
import importlib.util
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOARD = ROOT / 'hardware/pcb/veroboard-usb-v1'
SIBLINGS = ('design', 'module', 'layout', 'outputs', 'vero', 'draw')   # generator module names shared between boards


def load(gen, name, tag):
    """Load a generator module so that its own imports find its own board's siblings."""
    saved = {k: sys.modules.pop(k) for k in SIBLINGS if k in sys.modules}
    spec = importlib.util.spec_from_file_location(f'{tag}_{name}', gen / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    sys.path.insert(0, str(gen))
    try:
        spec.loader.exec_module(module)
    finally:
        sys.path.remove(str(gen))
        for k in SIBLINGS:
            sys.modules.pop(k, None)
        sys.modules.update(saved)
    return module


V = load(BOARD / 'generator', 'vero', 'vbu1')
D, L = V.D, V.L
O = load(BOARD / 'generator', 'outputs', 'vbu1')
PCB = load(ROOT / 'hardware/pcb/through-hole-usb-v1/generator', 'design', 'thuv1v')


class TestOutputsCurrent(unittest.TestCase):
    def manifest(self):
        lines = (BOARD / 'outputs.sha256').read_text(encoding='utf-8').splitlines()
        return dict(reversed(line.split('  ', 1)) for line in lines if line and not line.startswith('#'))

    def test_every_listed_file_matches(self):
        stale = [rel for rel, digest in self.manifest().items()
                 if hashlib.sha256((BOARD / rel).read_bytes()).hexdigest() != digest]
        self.assertEqual(stale, [], 'changed without a rebuild: run hardware/pcb/veroboard-usb-v1/build.sh')

    def test_no_unlisted_files(self):
        self.assertEqual(sorted(self.manifest()), O.manifest_files(str(BOARD)),
                         'the board folder has files the last build did not write: run its build.sh')


class TestLayout(unittest.TestCase):
    def test_layout_has_no_problems(self):
        self.assertEqual(V.analyse()['problems'], [])

    def test_size(self):
        w, h = V.size_mm()
        self.assertEqual((L.W, L.H), (33, 11))
        self.assertAlmostEqual(w, 86.36, places=2)
        self.assertAlmostEqual(h, 30.48, places=2)

    def test_resistors_at_vishays_minimum_pitch(self):
        # Vishay doc 28766: an MBB/SMA 0207 mounted flat needs at least 10.0 mm between its leads.
        self.assertGreaterEqual(D.BODIES['R0207']['min_span'] * D.PITCH, 10.0)
        for ref, (kind, _v, _p) in D.PARTS.items():
            if kind == 'R0207':
                (a, b) = L.LEADS[ref]
                with self.subTest(ref=ref):
                    self.assertGreaterEqual((abs(a[0] - b[0]) + abs(a[1] - b[1])) * D.PITCH, 10.0)

    def test_capacitor_leads_fit_their_spacing(self):
        # TDK FG26, taped: lead spacing 5.0 mm +0.8/-0.2
        self.assertTrue(4.8 <= D.BODIES['FG26']['span'] * D.PITCH <= 5.8)


class TestChecksCatchFaults(unittest.TestCase):
    """Break the layout in one place and expect vero.analyse() to say so."""

    def broken(self, **changes):
        saved = {k: getattr(L, k) for k in changes}
        try:
            for k, v in changes.items():
                setattr(L, k, v)
            return '\n'.join(V.analyse()['problems'])
        finally:
            for k, v in saved.items():
                setattr(L, k, v)

    def test_missing_module_cut_joins_a_pin_to_an_unconnected_one(self):
        self.assertIn('unconnected pin', self.broken(CUTS=[c for c in L.CUTS if c != (9, 6)]))

    def test_missing_cut_under_a_resistor_joins_two_nets(self):
        self.assertIn("'AIN3'", self.broken(CUTS=[c for c in L.CUTS if c != (15, 5)]))

    def test_missing_link_splits_a_net(self):
        self.assertIn('GND is split', self.broken(LINKS=[k for k in L.LINKS if k != ((8, 3), (8, 5))]))

    def test_kelvin_order(self):
        links = [((28, 1), (28, 2)) if k == ((25, 2), (25, 1)) else k for k in L.LINKS]
        self.assertIn('Kelvin', self.broken(LINKS=links))

    def test_short_resistor_spacing(self):
        self.assertIn('R4 leads 3 holes apart, minimum 4', self.broken(LEADS={**L.LEADS, 'R4': ((13, 5), (16, 5))}))

    def test_overlapping_bodies(self):
        self.assertIn('C1 and C4 bodies overlap', self.broken(LEADS={**L.LEADS, 'C4': ((15, 9), (15, 11))}))

    def test_lead_in_a_cut(self):
        self.assertIn('is in a cut', self.broken(CUTS=L.CUTS + [(17, 5)]))

    def test_link_under_a_part(self):
        self.assertIn('runs under R3', self.broken(LINKS=L.LINKS + [((22, 4), (22, 6))]))

    def test_terminal_block_away_from_the_edge(self):
        self.assertIn('wire entry', self.broken(BLOCKS={**L.BLOCKS, 'J2': (31, 5, 'right')}))


class TestCircuit(unittest.TestCase):
    def test_same_circuit_as_through_hole_usb_v1(self):
        for ref, (_kind, value, pins) in D.PARTS.items():
            if ref.startswith('J'):
                continue
            with self.subTest(ref=ref):
                theirs = PCB.PARTS[ref][3]
                self.assertEqual(value, PCB.PARTS[ref][1])
                self.assertEqual(D.BOM[ref][1], PCB.BOM[ref][1])
                if ref[0] in 'RC':          # not polarised: same two nets, either way round
                    self.assertEqual(sorted(pins.values()), sorted(theirs.values()))
                elif ref == 'U1':           # ADDR goes to GND here (address 0x48, as WIRING.md), open on the PCB
                    self.assertEqual({p: n for p, n in pins.items() if p != '11'},
                                     {p: n for p, n in theirs.items() if p != '11'})
                    self.assertEqual(pins['11'], 'GND')
                else:
                    self.assertEqual(pins, theirs)
        self.assertEqual({r for r in D.PARTS if not r.startswith('J')}, {r for r in PCB.PARTS if r[0] in 'RCDU'})

    def test_kelvin_and_series_rules(self):
        self.assertEqual(D.KELVIN, [(('R2', '1'), ('R1', '1'))])
        self.assertEqual(set(D.nets()['SENDER']), {('J2', '1'), ('R1', '2'), ('D5', '1'), ('R3', '2')})


class TestConnectorsAndGrounds(unittest.TestCase):
    def test_harness_has_a_dedicated_ground_return(self):
        self.assertEqual(D.PARTS['J2'][2], {'1': 'SENDER', '2': 'GND', '3': 'SENSE', '4': 'GND'})

    def test_a_ground_beside_each_display_signal(self):
        j3 = D.PARTS['J3'][2]
        for pin in ('2', '4', '5'):      # 3.3 V, SCL, SDA
            with self.subTest(pin=pin):
                beside = {j3.get(str(int(pin) - 1)), j3.get(str(int(pin) + 1))}
                self.assertIn('GND', beside)
        self.assertEqual(sorted(j3.values()).count('GND'), 3)

    def test_display_end_matches_the_firmware(self):
        src = (ROOT / 'main/hal/Ads1115Source.cpp').read_text(encoding='utf-8')
        self.assertRegex(src, r'kSda = GPIO_NUM_17\b')
        self.assertRegex(src, r'kScl = GPIO_NUM_18\b')
        self.assertEqual(dict(D.DISPLAY_END)['18'], 'SCL')
        self.assertEqual(dict(D.DISPLAY_END)['17'], 'SDA')
        self.assertEqual(dict(D.DISPLAY_END)['3.3V'], '+3V3')
        self.assertIsNone(dict(D.DISPLAY_END)['5V'])

    def test_terminal_blocks_are_phoenix_mpt(self):
        self.assertEqual(D.BOM['J2'][:3], ('Phoenix Contact', '1725672', '277-1275-ND'))
        self.assertEqual(D.BOM['J3'][:3], ('Phoenix Contact', '1725698', '277-1277-ND'))
        self.assertEqual(len(D.PARTS['J2'][2]), 4)
        self.assertEqual(len(D.PARTS['J3'][2]), 6)


class TestBom(unittest.TestCase):
    def test_every_part_has_a_part_number(self):
        for ref in D.PARTS:
            with self.subTest(ref=ref):
                mf, mpn, _code, desc = D.BOM[ref]
                self.assertTrue(mf and mpn and desc)

    def test_lists_name_the_parts(self):
        md = (BOARD / 'BOM.md').read_text(encoding='utf-8')
        for mpn in ('1725672', '1725698', 'MBB0207VD4700BC100', 'P6KE18A-E3/54', 'FG26C0G1H104JNT06'):
            self.assertIn(mpn, md)
        asm = (BOARD / 'ASSEMBLY.md').read_text(encoding='utf-8')
        self.assertIn(f'{len(L.CUTS)} track cuts and {len(L.LINKS)} wire links', asm)
        self.assertTrue(re.search(r'86\.4 x 30\.5 mm', asm))


if __name__ == '__main__':
    unittest.main()
