"""Checks on the interface PCB (hardware/pcb).

KiCad isn't available in CI, so ERC, DRC and schematic parity run in scripts/pcb_build.sh. These tests
cover what can be checked without it: that every generated file is current, the measurement and
protection topology, J1 against the firmware's pins, and the BOM.
"""
import hashlib
import importlib.util
import math
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / 'hardware/pcb/generator'


def load(name):
    spec = importlib.util.spec_from_file_location(name, GEN / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


D = load('design')
L = load('layout')
O = load('outputs')


def net(name):
    return set(D.nets()[name])


def pin(ref, num):
    return D.PARTS[ref][3][num]


class TestOutputsCurrent(unittest.TestCase):
    def manifest(self):
        lines = (ROOT / 'hardware/pcb/outputs.sha256').read_text(encoding='utf-8').splitlines()
        return dict(reversed(l.split('  ', 1)) for l in lines if l and not l.startswith('#'))

    def test_every_listed_file_matches(self):
        stale = [rel for rel, digest in self.manifest().items()
                 if hashlib.sha256((ROOT / rel).read_bytes()).hexdigest() != digest]
        self.assertEqual(stale, [], 'changed without a rebuild: run scripts/pcb_build.sh')

    def test_no_unlisted_files(self):
        self.assertEqual(sorted(self.manifest()), O.manifest_files(str(ROOT)),
                         'hardware/pcb has files the last build did not write: run scripts/pcb_build.sh')


class TestMeasurementTopology(unittest.TestCase):
    """The rules in hardware/WIRING.md, as they apply to this board."""

    def test_nothing_in_series_with_the_sender(self):
        # Harness pin, Rp, the TVS and the A3 tap's first resistor share one node and nothing else does.
        self.assertEqual(net('SENDER'), {('J2', '1'), ('R1', '2'), ('D3', '1'), ('R3', '1')})

    def test_rp_is_the_precision_reference(self):
        self.assertEqual((pin('R1', '1'), pin('R1', '2')), ('+3V3', 'SENDER'))
        self.assertTrue(D.BOM['R1'][3].startswith('Rp: 470R 0.1% 25ppm'))
        self.assertEqual(D.BOM['R1'][1], 'TNPW1206470RBEEA')

    def test_bias_is_on_the_harness_side_of_the_sense_tap(self):
        self.assertEqual(net('SENSE'), {('J2', '4'), ('R5', '1'), ('R7', '2')})
        self.assertEqual((pin('R7', '1'), D.PARTS['R7'][1]), ('+3V3', '100k'))

    def test_each_tap_is_two_stage_with_a_clamp_between(self):
        for tap, first, clamp, second, ain, cap, adc_pin in (
                ('TAP_S', 'R3', 'D1', 'R4', 'AIN3', 'C3', '7'),
                ('TAP_G', 'R5', 'D2', 'R6', 'AIN1', 'C2', '5')):
            with self.subTest(tap=tap):
                self.assertEqual(net(tap), {(first, '2'), (clamp, '3'), (second, '1')})
                self.assertEqual((pin(clamp, '1'), pin(clamp, '2')), ('GND', '+3V3'))   # BAV99: 1 anode, 2 cathode
                self.assertEqual(net(ain), {(second, '2'), (cap, '1'), ('U1', adc_pin)})
                self.assertEqual(pin(cap, '2'), 'GND')
                self.assertEqual((D.PARTS[first][1], D.PARTS[second][1]), ('1k 1206', '1k'))

    def test_a0_reads_the_top_of_rp_through_its_filter(self):
        self.assertEqual(net('AIN0'), {('R2', '2'), ('C1', '1'), ('U1', '4')})
        self.assertEqual(pin('R2', '1'), '+3V3')

    def test_tvs_and_supply_diode_orientation(self):
        self.assertEqual((pin('D3', '1'), pin('D3', '2')), ('SENDER', 'GND'))      # cathode on the signal
        self.assertEqual((pin('D4', '2'), pin('D4', '1')), ('5V_IN', '5V_DISP'))  # anode from the harness

    def test_adc_strapping(self):
        u1 = D.PARTS['U1'][3]
        self.assertEqual((u1['1'], u1['2'], u1['3'], u1['8']), ('GND', None, 'GND', '+3V3'))  # ADDR to GND: 0x48


class TestDisplayHeader(unittest.TestCase):
    def test_j1_follows_the_case_label(self):
        # Case label, left to right from the back: 18 17 16 RX TX 3.3V G 5V.
        self.assertEqual([pin('J1', str(i)) for i in range(1, 9)],
                         ['SCL', 'SDA', None, None, None, '+3V3', 'GND', '5V_DISP'])

    def test_j1_matches_the_firmware(self):
        src = (ROOT / 'main/hal/Ads1115Source.cpp').read_text(encoding='utf-8')
        self.assertRegex(src, r'kSda = GPIO_NUM_17\b')   # J1 pin 2, labelled 17
        self.assertRegex(src, r'kScl = GPIO_NUM_18\b')   # J1 pin 1, labelled 18
        self.assertRegex(src, r'kAddress = 0x48\b')
        self.assertEqual((pin('U1', '9'), pin('U1', '10')), ('SDA', 'SCL'))
        self.assertEqual(set(D.nets()['SDA']), {('J1', '2'), ('U1', '9'), ('R8', '2')})
        self.assertEqual(set(D.nets()['SCL']), {('J1', '1'), ('U1', '10'), ('R9', '2')})

    def test_board_sits_inside_the_case(self):
        self.assertLess(L.R_EDGE, L.CASE_RADIUS)
        self.assertLess(math.hypot(L.X_CLIP, L.Y_TOP), L.R_EDGE)
        self.assertLess(L.Y_TOP, 15.6 - 2)                 # clear of the SPK/BAT connectors
        self.assertEqual(L.PLACE['J1'][:2], (-8.89, -18.70))  # pin 1 of the row centred 18.70 mm below centre
        self.assertEqual(L.HOLES, [(-13.75, -14.7), (13.75, -14.7)])


class TestBom(unittest.TestCase):
    def test_every_part_has_a_part_number(self):
        for ref in D.PARTS:
            if ref.startswith('H'):
                continue
            with self.subTest(ref=ref):
                mf, mpn, lcsc, _ = D.BOM[ref]
                self.assertTrue(mf and mpn)
                if ref not in D.HAND_FIT:
                    self.assertRegex(lcsc, r'^C\d+$')

    def test_connectors_are_explicit_with_their_mating_parts(self):
        self.assertIn('43645-0400', D.BOM['J2'][3])
        extras = {e[3] for e in D.EXTRAS}
        self.assertTrue({'43645-0400', '43030-0007', '63819-0000'} <= extras)
        md = (ROOT / 'hardware/pcb/BOM.md').read_text(encoding='utf-8')
        for mpn in ('TSW-108-07-G-S', '43650-0415', '43645-0400', '43030-0007', '63819-0000'):
            self.assertIn(mpn, md)

    def test_board_house_files_leave_out_hand_fitted_parts(self):
        bom = (ROOT / 'hardware/pcb/fab/bom-jlcpcb.csv').read_text(encoding='utf-8')
        cpl = (ROOT / 'hardware/pcb/fab/cpl-jlcpcb.csv').read_text(encoding='utf-8')
        for ref in D.HAND_FIT:
            self.assertIsNone(re.search(rf'\b{ref}\b', bom), ref)
            self.assertIsNone(re.search(rf'^{ref},', cpl, re.M), ref)
        placed = {l.split(',')[0] for l in cpl.splitlines()[1:]}
        self.assertEqual(placed, {r for r in D.BOM if r not in D.HAND_FIT})


if __name__ == '__main__':
    unittest.main()
