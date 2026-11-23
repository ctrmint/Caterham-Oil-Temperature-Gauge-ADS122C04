import importlib.util
import math
import re
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
        self.assertEqual(t.feed(p(1, 0)), (True, []))
        self.assertEqual(t.feed(p(1, 1)), (True, []))
        self.assertEqual(t.feed(p(1, 4)), (True, ['2 packets lost']))
        self.assertEqual(t.feed(p(1, 5)), (True, []))
        self.assertEqual(t.feed(p(1, 7)), (True, ['1 packet lost']))
        self.assertEqual(t.feed(p(1, 6)), (True, ['out of order']))  # the one lost before 7, late: no longer lost
        self.assertEqual(t.feed(p(1, 8)), (True, []))
        self.assertEqual(t.feed(p(2, 0)), (True, ['gauge restarted (session 00000002)']))
        self.assertEqual(t.feed(p(2, 1)), (True, []))
        self.assertEqual((t.received, t.lost, t.duplicates, t.restarts), (9, 2, 0, 1))

    def test_tracker_skips_duplicates(self):
        t = udp.Tracker()
        p = lambda session, seq: {'session': session, 'sequence': seq}
        self.assertEqual(t.feed(p(7, 3166)), (True, []))
        self.assertEqual(t.feed(p(7, 3166)), (False, ['duplicate #3166 skipped']))  # as seen on the bench
        self.assertEqual(t.feed(p(7, 3167)), (True, []))
        self.assertEqual(t.feed(p(7, 3170)), (True, ['2 packets lost']))
        self.assertEqual(t.feed(p(7, 3168)), (True, ['out of order']))
        self.assertEqual(t.feed(p(7, 3168)), (False, ['duplicate #3168 skipped']))
        self.assertEqual(t.feed(p(7, 3170)), (False, ['duplicate #3170 skipped']))
        self.assertEqual(t.feed(p(7, 3165)), (True, ['out of order']))  # before the first seen: never counted lost
        self.assertEqual((t.received, t.lost, t.duplicates), (5, 1, 3))
        # The same numbers in a new session (the gauge restarted) aren't duplicates.
        self.assertEqual(t.feed(p(8, 3166)), (True, ['gauge restarted (session 00000008)']))
        self.assertEqual(t.feed(p(8, 3167)), (True, []))
        # Only the last WINDOW numbers are remembered.
        t = udp.Tracker()
        for seq in range(udp.Tracker.WINDOW + 10):
            self.assertTrue(t.feed(p(1, seq))[0])
        self.assertEqual(t.feed(p(1, udp.Tracker.WINDOW + 9))[0], False)
        self.assertEqual(t.feed(p(1, 0)), (True, ['out of order']))
        self.assertEqual(len(t._seen), udp.Tracker.WINDOW)

def numbered(name, seq):
    """A golden packet with another sequence number (and a fresh CRC)."""
    b = bytearray(dict((n, d) for n, _, d in golden())[name][:52])
    b[12:16] = struct.pack('<I', seq)
    return bytes(b) + struct.pack('<I', zlib.crc32(bytes(b)))


def monitor(name, count=8, start=1000.0, step=0.25, first=100):
    m = udp.Monitor('239.10.10.10', 5005)
    for i in range(count):
        m.receive(numbered(name, first + i), start + i * step)
    return m, start + (count - 1) * step


class TestDashboard(unittest.TestCase):
    def screen(self, m, now, width=80, height=24):
        c = udp.render(m, now, width, height)
        lines = c.lines()
        self.assertEqual(len(lines), height)
        for line in lines:
            self.assertEqual(len(line), width)
        return c, '\n'.join(lines)

    def test_thresholds_match_the_firmware(self):
        config = Path('main/Config.h').read_text()
        amber = float(re.search(r'kAmberThresholdC\s*=\s*([0-9.]+)f', config).group(1))
        red = float(re.search(r'kRedThresholdC\s*=\s*([0-9.]+)f', config).group(1))
        self.assertEqual((udp.AMBER_C, udp.RED_C), (amber, red))

    def test_big_digits(self):
        rows = udp.big_text('0')
        self.assertEqual(rows, ['██████', '██  ██', '██  ██', '██  ██', '██████'])
        self.assertEqual(udp.big_text('1')[0], '  ██  ')
        self.assertEqual(udp.big_text('-'), ['      ', '      ', '██████', '      ', '      '])
        for text in ('117.5', '-12.5', '1234', '--', '0'):
            rows = udp.big_text(text)
            self.assertEqual(len(rows), udp.DIGIT_ROWS)
            self.assertEqual(len({len(r) for r in rows}), 1)
            self.assertTrue(all(set(r) <= {' ', '█'} for r in rows))
        self.assertEqual(len(udp.big_text('117.5')[0]), 6 + 1 + 6 + 1 + 6 + 1 + 2 + 1 + 6)
        self.assertEqual(udp.big_text('x'), udp.big_text(' '))  # anything else is blank, never a wrong digit

    def test_colour_is_the_gauges_status(self):
        st = lambda status, alarm='NORMAL': {'status': udp.STATUS.index(status), 'alarm': udp.STATUS.index(alarm)}
        self.assertEqual(udp.status_style(st('NORMAL')), 'green')
        self.assertEqual(udp.status_style(st('OIL WARMING')), 'blue')
        self.assertEqual(udp.status_style(st('BELOW CAL RANGE')), 'blue')
        self.assertEqual(udp.status_style(st('HIGH TEMPERATURE', 'HIGH TEMPERATURE')), 'amber')
        self.assertEqual(udp.status_style(st('NORMAL', 'HIGH TEMPERATURE')), 'amber')  # still latched while cooling
        self.assertEqual(udp.status_style(st('CRITICAL TEMP', 'CRITICAL TEMP')), 'red')
        self.assertEqual(udp.status_style(st('ABOVE CAL RANGE')), 'red')
        self.assertEqual(udp.status_style(st('SENSOR OPEN', 'CRITICAL TEMP')), 'red')  # never calmer than the alarm
        for fault in ('ADC UNAVAILABLE', 'SUPPLY FAULT', 'GROUND FAULT', 'SENSOR SHORT', 'SENSOR OPEN', 'CALIBRATION REQUIRED'):
            self.assertEqual(udp.status_style(st(fault)), 'amber')

    def test_trend_on_tapes_rule(self):
        ramp = [(1000 + i * 0.25, 80 + 0.01 * i, True) for i in range(130)]  # 0.04 degC/s = 2.4 degC/min
        now = ramp[-1][0]
        self.assertAlmostEqual(udp.trend(ramp, now), 2.4, places=3)
        self.assertEqual(udp.trend_text(udp.trend(ramp, now)), '▲ 2.4 °C/min')
        self.assertEqual(udp.trend_text(-1.06), '▼ 1.1 °C/min')
        self.assertEqual(udp.trend_text(0.49), 'STEADY')
        self.assertEqual(udp.trend_text(None), '--')
        self.assertIsNone(udp.trend(ramp[-60:], now))                            # under 30 s of data
        dirty = list(ramp); dirty[-20] = (dirty[-20][0], None, False)
        self.assertIsNone(udp.trend(dirty, now))                                 # a fault in the window
        gap = [h for h in ramp if not (now - 20 < h[0] < now - 17)]
        self.assertIsNone(udp.trend(gap, now))                                   # a gap over 1.5 s
        self.assertIsNone(udp.trend(ramp, now + 2))                              # nothing for 2 s

    def test_normal_dashboard(self):
        m, now = monitor('high', count=240)
        m.csv_name, m.csv_rows = 'run1.csv', 1203
        c, text = self.screen(m, now + 0.2)
        self.assertTrue(text.startswith('┌─ Caterham oil temperature'))
        self.assertIn('239.10.10.10:5005 · session 8f3a21c4', text)
        self.assertIn('HIGH TEMPERATURE', text)
        self.assertIn('alarm  HIGH TEMPERATURE (latched)', text)
        self.assertIn('max    118 °C', text)
        self.assertIn('trend  STEADY', text)
        self.assertIn('packets 240 · 4.0/s · lost 0 (0.0 %) · dup 0 · bad 0 · last 0.2 s ago', text)
        self.assertIn('logging → run1.csv (1203 rows)', text)
        self.assertIn('sender 118.9 Ω', text)
        lines = c.lines()
        digits = udp.big_text('117.6')
        for r in range(udp.DIGIT_ROWS):
            self.assertEqual(lines[2 + r][3:3 + len(digits[r])], digits[r])
            self.assertEqual(c.style_at(2 + r, 3 + digits[r].index('█')), 'amber')
        self.assertEqual(c.style_at(2, 46), 'amber')  # the status line in the gauge's colour
        self.assertNotIn('DEMO', text)
        self.assertNotIn('NO DATA', text)
        for line in lines[1:-1]:
            self.assertEqual((line[0], line[-1]), ('│', '│'))
        self.assertEqual((lines[0][0], lines[0][-1], lines[-1][0], lines[-1][-1]), ('┌', '┐', '└', '┘'))

    def test_chart(self):
        m, now = monitor('high', count=240)
        c, text = self.screen(m, now)
        self.assertIn('last 5 min', text)
        self.assertIn('130 ┤', text)
        self.assertIn('115 ┤', text)
        lines = c.lines()
        bars = [(r, col) for r, line in enumerate(lines) for col, ch in enumerate(line) if ch in '▁▂▃▄▅▆▇█' and r > 7]
        self.assertTrue(bars)
        self.assertTrue(all(col > 50 for _, col in bars))  # only the last minute has data: the right of the chart
        self.assertTrue(all(c.style_at(r, col) == 'amber' for r, col in bars))  # 117.6 degC: between the lines
        hot, now = monitor('critical', count=40)
        c, _ = self.screen(hot, now)
        reds = [c.style_at(r, col) for r, line in enumerate(c.lines()) for col, ch in enumerate(line) if ch == '█' and r > 7]
        self.assertTrue(reds and set(reds) == {'red'})
        # A gap stays a gap: no bars for a fault.
        fault, now = monitor('fault', count=40)
        c, _ = self.screen(fault, now)
        self.assertFalse([ch for line in c.lines()[8:] for ch in line if ch in '▁▂▃▄▅▆▇█'])

    def test_states(self):
        m, now = monitor('demo')
        c, text = self.screen(m, now)
        self.assertIn('DEMO - SYNTHETIC DATA, NOT REAL', text)
        self.assertEqual(c.style_at(1, 40), 'banner')
        m, now = monitor('uncal')
        c, text = self.screen(m, now)
        self.assertIn('CALIBRATION REQUIRED', text)
        self.assertEqual(c.lines()[2][3:3 + len(udp.big_text('123')[0])], udp.big_text('123')[0])  # whole ohms
        self.assertIn(' Ω ', c.lines()[3])
        self.assertIn('max    --', text)
        self.assertIn('trend  --', text)
        self.assertNotIn('°C', '\n'.join(c.lines()[1:7]))  # never degrees uncalibrated
        m, now = monitor('fault')
        c, text = self.screen(m, now)
        self.assertIn('SENSOR OPEN', text)
        self.assertEqual(c.lines()[4][3:3 + 13], udp.big_text('--')[2])
        self.assertEqual(c.style_at(4, 3), 'amber')
        self.assertIn('sender -- Ω', text)
        m, now = monitor('above')
        c, text = self.screen(m, now)
        self.assertIn('ABOVE CAL RANGE', text)
        self.assertIn('max    >140 °C', text)
        self.assertIn('alarm  CRITICAL TEMP (latched)', text)
        self.assertEqual(c.style_at(4, 3), 'red')
        c, text = self.screen(udp.Monitor('239.10.10.10', 5005), 1000.0)
        self.assertIn('Waiting for the gauge...', text)
        self.assertIn('no data yet', text)

    def test_stale_then_lost(self):
        m, now = monitor('high', count=200)
        c, text = self.screen(m, now + 1.9)
        self.assertNotIn('NO DATA', text)
        c, text = self.screen(m, now + 2.0)
        self.assertIn('NO DATA 2 s', text)
        row = next(r for r, line in enumerate(c.lines()) if 'NO DATA' in line)
        self.assertEqual(c.style_at(row, c.lines()[row].index('NO DATA')), 'amber')
        self.assertEqual(c.lines()[2][3:9], udp.big_text('117.6')[0][:6])  # still the reading, for now
        c, text = self.screen(m, now + 10.0)
        self.assertIn('STREAM LOST - last data', text)
        self.assertIn('NO DATA 10 s', text)
        self.assertEqual(c.lines()[4][3:3 + 13], udp.big_text('--')[2])  # never the old reading as if live
        self.assertEqual(c.style_at(4, 3), 'grey')
        self.assertIn('trend  --', text)
        self.assertEqual(m.rate(now + 10.0), 0.0)

    def test_small_windows(self):
        m, now = monitor('high', count=40)
        c, text = self.screen(m, now, 66, 16)
        self.assertNotIn('last 5 min', text)  # no room for the chart
        self.assertIn('HIGH TEMPERATURE', text)
        self.assertIn('239.10.10.10:5005', c.lines()[0])
        c = udp.render(m, now, 50, 10)
        self.assertTrue(c.lines()[0].startswith('Make the window at least 60 x 14'))

    def test_monitor_skips_bad_and_duplicate_and_restarts(self):
        m = udp.Monitor('239.10.10.10', 5005)
        self.assertEqual(m.receive(numbered('normal', 1), 1000.0)[0]['sequence'], 1)
        self.assertEqual(m.receive(numbered('normal', 1), 1000.1), (None, ['duplicate #1 skipped']))
        self.assertEqual(m.receive(b'nonsense', 1000.2), (None, ['skipped a packet: length 8, expected 56']))
        self.assertEqual(m.receive(b'nonsense', 1000.3), (None, []))  # each reason once
        self.assertEqual((m.tracker.received, m.tracker.duplicates, m.tracker.bad, len(m.history)), (1, 1, 2, 1))
        other = bytearray(numbered('normal', 0)[:52]); other[8:12] = struct.pack('<I', 0x12345678)
        other = bytes(other) + struct.pack('<I', zlib.crc32(bytes(other)))
        p, notes = m.receive(other, 1000.5)
        self.assertEqual(notes, ['gauge restarted (session 12345678)'])
        self.assertEqual(len(m.history), 1)  # the chart starts again
        self.assertEqual(m.note, 'gauge restarted (session 12345678)')


if __name__ == '__main__':
    unittest.main()
