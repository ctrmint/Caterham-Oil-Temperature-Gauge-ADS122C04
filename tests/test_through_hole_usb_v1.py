"""Checks on the through-hole-usb-v1 interface PCB (hardware/pcb/through-hole-usb-v1).

KiCad isn't available in CI, so ERC, DRC and schematic parity run in that folder's build.sh. These tests cover
what can be checked without it: that every generated file is current, that the board really is single-sided and
through-hole only, the measurement and protection topology, that its only supply is the display's 3.3 V, the
ground pins (including the harness's dedicated return to the tank), the display cable against the firmware's
pins, and the parts list.
"""
import hashlib
import importlib.util
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOARD = ROOT / 'hardware/pcb/through-hole-usb-v1'
GEN = BOARD / 'generator'


SIBLINGS = ('design', 'module', 'layout', 'outputs')   # the generator's own imports, shared names with the other boards


def load(name):
    """Load a generator module so that its own `import design` finds this board's design, not another's."""
    saved = {k: sys.modules.pop(k) for k in SIBLINGS if k in sys.modules}
    spec = importlib.util.spec_from_file_location(f'thuv1_{name}', GEN / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    sys.path.insert(0, str(GEN))
    try:
        spec.loader.exec_module(module)
    finally:
        sys.path.remove(str(GEN))
        for k in SIBLINGS:
            sys.modules.pop(k, None)
        sys.modules.update(saved)
    return module


D = load('design')
O = load('outputs')


def net(name):
    return set(D.nets()[name])


def pin(ref, num):
    return D.PARTS[ref][3][num]


class TestOutputsCurrent(unittest.TestCase):
    def manifest(self):
        lines = (BOARD / 'outputs.sha256').read_text(encoding='utf-8').splitlines()
        return dict(reversed(line.split('  ', 1)) for line in lines if line and not line.startswith('#'))

    def test_every_listed_file_matches(self):
        stale = [rel for rel, digest in self.manifest().items()
                 if hashlib.sha256((BOARD / rel).read_bytes()).hexdigest() != digest]
        self.assertEqual(stale, [], 'changed without a rebuild: run hardware/pcb/through-hole-usb-v1/build.sh')

    def test_no_unlisted_files(self):
        self.assertEqual(sorted(self.manifest()), O.manifest_files(str(BOARD)),
                         'the board folder has files the last build did not write: run its build.sh')


class TestSingleSidedThroughHole(unittest.TestCase):
    pcb = (BOARD / f'{D.PROJECT}.kicad_pcb').read_text(encoding='utf-8')

    def test_every_track_and_pour_on_bottom_copper(self):
        layers = re.findall(r'\(segment\b.*?\(layer "([^"]+)"\)', self.pcb, re.S)
        self.assertTrue(layers)
        self.assertEqual(set(layers), {'B.Cu'})
        self.assertIsNone(re.search(r'\(arc\b', self.pcb))
        pours = re.findall(r'\(zone\s*\(net "[^"]*"\)\s*\(layer "([^"]+)"\)', self.pcb)
        self.assertEqual(pours, ['B.Cu'])

    def test_no_vias(self):
        self.assertIsNone(re.search(r'\(via\b', self.pcb))

    def test_no_surface_mount_footprints(self):
        self.assertIsNone(re.search(r'\(attr smd', self.pcb))
        self.assertEqual(len(re.findall(r'\(attr through_hole', self.pcb)), len([r for r in D.PARTS if r[0] != 'H']))

    def test_only_bottom_copper_goes_to_the_board_house(self):
        import zipfile
        with zipfile.ZipFile(BOARD / f'fab/{D.PROJECT}-gerbers.zip') as z:
            names = z.namelist()
        self.assertFalse([n for n in names if 'F_Cu' in n or '_In' in n])
        self.assertTrue([n for n in names if 'B_Cu' in n])


class TestMeasurementTopology(unittest.TestCase):
    """The rules in hardware/WIRING.md, as they apply to this board."""

    def test_nothing_in_series_with_the_sender(self):
        self.assertEqual(net('SENDER'), {('J2', '1'), ('R1', '2'), ('D5', '1'), ('R3', '1')})

    def test_rp_is_the_precision_reference(self):
        self.assertEqual((pin('R1', '1'), pin('R1', '2')), ('+3V3', 'SENDER'))
        self.assertEqual(D.BOM['R1'][1], 'MBB0207VD4700BC100')   # Vishay: D = 25 ppm/K, B = 0.1 %

    def test_bias_is_on_the_harness_side_of_the_sense_tap(self):
        self.assertEqual(net('SENSE'), {('J2', '2'), ('R5', '1'), ('R7', '2')})
        self.assertEqual((pin('R7', '1'), D.PARTS['R7'][1]), ('+3V3', '100k'))

    def test_each_tap_is_two_stage_with_clamps_between(self):
        for tap, first, up, down, second, ain, cap, adc in (('TAP_S', 'R3', 'D1', 'D2', 'R4', 'AIN3', 'C3', '2'),
                                                            ('TAP_G', 'R5', 'D3', 'D4', 'R6', 'AIN1', 'C2', '4')):
            with self.subTest(tap=tap):
                self.assertEqual(net(tap), {(first, '2'), (up, '2'), (down, '1'), (second, '1')})
                self.assertEqual(pin(up, '1'), '+3V3')     # 1N4148 cathode to 3.3 V
                self.assertEqual(pin(down, '2'), 'GND')    # anode to ground
                self.assertEqual(net(ain), {(second, '2'), (cap, '1'), ('U1', adc)})
                self.assertEqual(pin(cap, '2'), 'GND')

    def test_a0_reads_the_top_of_rp(self):
        self.assertEqual(net('AIN0'), {('R2', '2'), ('C1', '1'), ('U1', '5')})
        self.assertEqual(pin('R2', '1'), '+3V3')

    def test_signal_wire_tvs(self):
        self.assertEqual((pin('D5', '1'), pin('D5', '2')), ('SENDER', 'GND'))   # cathode on the wire
        self.assertEqual(D.BOM['D5'][1], 'P6KE18A-E3/54')

    def test_module_pins(self):
        u1 = D.PARTS['U1'][3]
        # Adafruit 1085: 1 A+, 2 A3, 3 A2, 4 A1, 5 A0, 6 A-; 7 VIN, 8 GND, 9 SCL, 10 SDA, 11 ADDR, 12 ALRT
        self.assertEqual([u1[str(i)] for i in range(1, 13)],
                         [None, 'AIN3', None, 'AIN1', 'AIN0', None, '+3V3', 'GND', 'SCL', 'SDA', None, None])


class TestPoweredByTheDisplay(unittest.TestCase):
    """The display runs from its own USB-C socket; the board's only supply is the display's 3.3 V."""

    def test_no_other_supply(self):
        self.assertEqual(set(D.nets()), {'+3V3', 'GND', 'SDA', 'SCL', 'SENDER', 'SENSE',
                                         'TAP_S', 'TAP_G', 'AIN0', 'AIN1', 'AIN3'})
        self.assertEqual([r for r in D.PARTS if r.startswith(('U', 'F', 'L', 'Q'))], ['U1'])

    def test_3v3_arrives_only_from_the_display_cable(self):
        connector_pins = {(r, p) for r, p in net('+3V3') if r.startswith('J')}
        self.assertEqual(connector_pins, {('J3', '4')})

    def test_nothing_on_the_displays_5v_pin(self):
        self.assertIsNone(dict(D.DISPLAY_END)['5V'])
        self.assertEqual(sorted(set(D.PARTS['J3'][3].values())), ['+3V3', 'GND', 'SCL', 'SDA'])


class TestGroundPins(unittest.TestCase):
    def test_harness_brings_a_dedicated_ground_return(self):
        # Without the return wire there is no path for the sender current, so the gauge faults rather than reads.
        self.assertEqual(D.PARTS['J2'][3], {'1': 'SENDER', '2': 'SENSE', '3': 'GND', '4': 'GND'})

    def test_a_ground_beside_each_display_signal(self):
        j3 = D.PARTS['J3'][3]
        # Micro-Fit 2x3: pin 4 sits above pin 1, 5 above 2, 6 above 3, so each signal pairs with the ground below it
        wiring = {ref: use for ref, _title, use in D.PINOUTS}['J3']
        for sig, gnd in (('4', '1'), ('5', '2'), ('6', '3')):
            with self.subTest(pin=sig):
                self.assertNotEqual(j3[sig], 'GND')
                self.assertEqual(j3[gnd], 'GND')
                self.assertIn(f'pin {sig})', wiring[gnd])   # the parts list's wiring text names the same pair

    def test_multiple_ground_pins(self):
        gnd = net('GND')
        self.assertGreaterEqual(len([r for r, p in gnd if r == 'J2']), 2)
        self.assertGreaterEqual(len([r for r, p in gnd if r == 'J3']), 3)
        self.assertGreaterEqual(len([r for r, p in gnd if r.startswith('TP')]), 2)

    def test_return_is_tracked_to_the_display_ground(self):
        """The harness return reaches the display's ground pins by tracks, not only through the pour."""
        L = load('layout')
        pcb = (BOARD / f'{D.PROJECT}.kicad_pcb').read_text(encoding='utf-8')
        segs = re.findall(r'\(segment\s*\(start ([-\d.]+) ([-\d.]+)\)\s*\(end ([-\d.]+) ([-\d.]+)\)'
                          r'\s*\(width [\d.]+\)\s*\(layer "B.Cu"\)\s*\(net "GND"\)', pcb)
        self.assertTrue(segs)
        links = {}
        for x0, y0, x1, y1 in segs:
            a, b = (round(float(x0), 2), round(float(y0), 2)), (round(float(x1), 2), round(float(y1), 2))
            links.setdefault(a, set()).add(b)
            links.setdefault(b, set()).add(a)

        def at(ref, n):   # pad position in the board file: J2 and J3 sit at rot 180, pins 3 mm apart along -x
            x, y = L.PLACE[ref][0] - 3.0 * (n - 1), L.PLACE[ref][1]
            return round(100 + x, 2), round(100 - y, 2)

        seen, todo = set(), [at('J2', 3)]
        while todo:
            p = todo.pop()
            if p not in seen:
                seen.add(p)
                todo.extend(links.get(p, ()))
        self.assertIn(at('J3', 1), seen)


class TestDisplayCable(unittest.TestCase):
    def test_display_end_matches_the_case_label_and_firmware(self):
        src = (ROOT / 'main/hal/Ads1115Source.cpp').read_text(encoding='utf-8')
        self.assertRegex(src, r'kSda = GPIO_NUM_17\b')
        self.assertRegex(src, r'kScl = GPIO_NUM_18\b')
        self.assertRegex(src, r'kAddress = 0x48\b')
        self.assertEqual(dict(D.DISPLAY_END)['18'], 'SCL')
        self.assertEqual(dict(D.DISPLAY_END)['17'], 'SDA')
        self.assertEqual(dict(D.DISPLAY_END)['3.3V'], '+3V3')
        self.assertEqual(net('SDA'), {('J3', '6'), ('U1', '10'), ('R8', '2')})
        self.assertEqual(net('SCL'), {('J3', '5'), ('U1', '9'), ('R9', '2')})

    def test_display_is_reached_only_by_cable(self):
        self.assertNotIn('PinHeader', ' '.join(p[2] for p in D.PARTS.values()))


class TestBom(unittest.TestCase):
    def test_every_part_has_a_part_number(self):
        for ref in D.PARTS:
            if ref.startswith('H'):
                continue
            with self.subTest(ref=ref):
                mf, mpn, _code, desc = D.BOM[ref]
                self.assertTrue(mf and mpn and desc)

    def test_connectors_have_their_mating_parts(self):
        extras = {e[3] for e in D.EXTRAS}
        self.assertTrue({'43645-0400', '43025-0600', '43030-0007', 'TSW-108-07-G-S'} <= extras)
        md = (BOARD / 'BOM.md').read_text(encoding='utf-8')
        for mpn in ('43650-0400', '43045-0600', '43645-0400', '43025-0600', '43030-0007'):
            self.assertIn(mpn, md)

    def test_named_for_the_board(self):
        self.assertEqual(D.PROJECT, 'through-hole-usb-v1')
        for rel in (f'{D.PROJECT}.kicad_pcb', f'{D.PROJECT}.kicad_sch', f'{D.PROJECT}-bom.csv',
                    f'fab/{D.PROJECT}-gerbers.zip', f'box/{D.PROJECT}-box.svg'):
            self.assertTrue((BOARD / rel).is_file(), rel)


if __name__ == '__main__':
    unittest.main()
