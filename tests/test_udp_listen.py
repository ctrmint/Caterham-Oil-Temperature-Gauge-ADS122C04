import importlib.util
import math
from pathlib import Path
import struct
import unittest
import zlib

spec = importlib.util.spec_from_file_location('udp', 'scripts/udp_listen.py')
udp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(udp)

GOLDEN = Path('tests/data/udp_packets.txt')


def golden():
    """(name, fields, bytes) for every golden packet."""
    rows = []
    for line in GOLDEN.read_text().splitlines():
        if not line or line.startswith('#'):
            continue
        name, build, session, seq, uptime, flags, status, alarm, *vals, hexbytes = line.split()
        assert len(vals) == 7 and build in ('cal', 'uncal')
        f = {'session': int(session, 16), 'sequence': int(seq), 'uptime_ms': int(uptime), 'flags': int(flags),
             'status': int(status), 'alarm': int(alarm), 'vals': [float(v) for v in vals]}
        rows.append((name, f, bytes.fromhex(hexbytes)))
    return rows


def pack(f):
    """An independent packer, straight from docs/UDP_STREAM.md: field by field, then CRC-32 of bytes 0-51."""
    body = (b'OILG' + bytes([1, 56]) + struct.pack('<H', f['flags']) +
            struct.pack('<III', f['session'], f['sequence'], f['uptime_ms']) +
            bytes([f['status'], f['alarm'], 0, 0]) + b''.join(struct.pack('<f', v) for v in f['vals']))
    assert len(body) == 52
    return body + struct.pack('<I', zlib.crc32(body))


class TestUdpListen(unittest.TestCase):
    def test_golden_packets_match_the_documented_layout(self):
        rows = golden()
        self.assertEqual(len(rows), 11)
        for name, f, data in rows:
            with self.subTest(name=name):
                self.assertEqual(pack(f), data)

    def test_golden_packets_decode(self):
        for name, f, data in golden():
            with self.subTest(name=name):
                p = udp.decode(data)
                for key in ('session', 'sequence', 'uptime_ms', 'flags', 'status', 'alarm'):
                    self.assertEqual(p[key], f[key])
                temp, ohms, ref, sender, supply, ground, max_c = f['vals']
                if f['flags'] & udp.HAS_TEMPERATURE:
                    self.assertAlmostEqual(p['temp_c'], temp, places=4)
                else:
                    self.assertIsNone(p['temp_c'])
                    self.assertTrue(math.isnan(temp))
                for key, want in (('resistance_ohms', ohms), ('ref_v', ref), ('sender_v', sender), ('supply_v', supply),
                                  ('ground_offset_v', ground), ('max_c', max_c)):
                    if math.isnan(want):
                        self.assertIsNone(p[key])
                    else:
                        self.assertAlmostEqual(p[key], want, places=4)

    def test_states_in_the_golden_set(self):
        by = {name: udp.decode(data) for name, _, data in golden()}
        self.assertEqual(udp.status_text(by['normal']['status']), 'NORMAL')
        self.assertEqual(udp.status_text(by['high']['alarm']), 'HIGH TEMPERATURE')
        self.assertTrue(by['above']['max_above_range'] and by['above']['max_c'] == 140.0)
        self.assertTrue(by['demo']['demo'] and not by['normal']['demo'])
        self.assertFalse(by['uncal']['calibrated'])
        self.assertIsNone(by['uncal']['temp_c'])
        self.assertIsNone(by['fault']['resistance_ohms'])
        self.assertIsNone(by['first']['max_c'])

    def test_rejects_bad_packets(self):
        good = golden()[0][2]
        cases = {
            'short': good[:-1], 'long': good + b'\0', 'empty': b'',
            'magic': b'OILX' + good[4:],
            'version': good[:4] + bytes([2]) + good[5:],
            'length': good[:5] + bytes([55]) + good[6:],
            'crc': good[:52] + bytes([good[52] ^ 1]) + good[53:],
            'payload': good[:30] + bytes([good[30] ^ 1]) + good[31:],
        }
        for name, data in cases.items():
            with self.subTest(name=name), self.assertRaises(ValueError):
                udp.decode(data)

    def test_temperature_only_when_flagged(self):
        f = golden()[0][1].copy()
        f['flags'] &= ~udp.HAS_TEMPERATURE  # a real-looking temperature, not flagged: never shown
        p = udp.decode(pack(f))
        self.assertIsNone(p['temp_c'])
        self.assertIn('  --  ', udp.describe(p))

    def test_describe_and_csv(self):
        by = {name: udp.decode(data) for name, _, data in golden()}
        self.assertIn('NORMAL  92.4 °C  262.1 ohm', udp.describe(by['normal']))
        self.assertIn('MAX 94', udp.describe(by['normal']))
        self.assertIn('MAX >140', udp.describe(by['above']))
        self.assertIn('alarm CRITICAL TEMP', udp.describe(by['above']))
        self.assertIn('SENSOR OPEN  --  -- ohm', udp.describe(by['fault']))
        self.assertTrue(udp.describe(by['demo']).endswith('DEMO'))
        self.assertIn('CALIBRATION REQUIRED  --  123.4 ohm', udp.describe(by['uncal']))
        row = udp.csv_row(by['fault'], 1700000000.5)
        self.assertEqual(len(row), len(udp.CSV_COLUMNS))
        self.assertEqual(row[:5], [2275000, 'SENSOR OPEN', 'NORMAL', '', ''])
        self.assertEqual(row[9:], ['8f3a21c4', 9100, '90.0', '1700000000.500'])
        self.assertEqual(udp.csv_row(by['normal'], 0)[3], '92.4')
        self.assertEqual([udp.whole(v) for v in (132.5, 133.5, 132.4, -0.5, -20.5)], [133, 134, 132, -1, -21])  # as the gauge

    def test_tracker_counts_gaps_and_restarts(self):
        t = udp.Tracker()
        p = lambda session, seq: {'session': session, 'sequence': seq}
        self.assertEqual(t.feed(p(1, 0)), [])
        self.assertEqual(t.feed(p(1, 1)), [])
        self.assertEqual(t.feed(p(1, 4)), ['2 packets lost'])
        self.assertEqual(t.feed(p(1, 5)), [])
        self.assertEqual(t.feed(p(1, 7)), ['1 packet lost'])
        self.assertEqual(t.feed(p(1, 6)), ['out of order'])
        self.assertEqual(t.feed(p(1, 8)), [])
        self.assertEqual(t.feed(p(2, 0)), ['gauge restarted (session 00000002)'])
        self.assertEqual(t.feed(p(2, 1)), [])
        self.assertEqual((t.received, t.lost, t.restarts), (9, 3, 1))


if __name__ == '__main__':
    unittest.main()
