import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

PAGE = Path('main/web/index.html')


class TestWebPage(unittest.TestCase):
    """The phone page (#32), served by the gauge: self-contained, small, and its logic tested under Node."""
    def setUp(self):
        self.html = PAGE.read_text(encoding='utf-8')

    def test_works_with_no_internet(self):
        # Nothing loaded from anywhere else: no external URLs (the SVG namespace is a name, not a fetch), no linked
        # stylesheets or scripts, no imports or remote images.
        urls = set(re.findall(r'https?://[^\s"\'`<>)]+', self.html))
        self.assertEqual(urls, {'http://www.w3.org/2000/svg'})
        self.assertNotRegex(self.html, r'<link\b|<script[^>]*\bsrc=|@import|<img\b|<iframe\b')
        self.assertEqual(set(re.findall(r'url\(([^)]*)\)', self.html)), {'#glow'})

    def test_asks_only_for_the_log_and_the_scale(self):
        fetched = set(re.findall(r"fetch\(\s*[`'\"]([^`'\"?$]+)", self.html))
        self.assertEqual(fetched, {'/scale', '/log'})
        self.assertNotRegex(self.html, r"method:\s*['\"](POST|PUT|DELETE)")  # read-only

    def test_size_and_parts(self):
        self.assertLess(len(self.html.encode('utf-8')), 48 * 1024)  # embedded in the firmware
        for part in ('id="dial"', 'id="status"', 'id="live"', 'id="chart"', 'id="bands"', 'id="download"', 'id="demo"',
                     'name="viewport"', 'prefers-reduced-motion', '<script id="logic">'):
            self.assertIn(part, self.html)

    @unittest.skipUnless(shutil.which('node'), 'needs Node.js for the page logic')
    def test_page_logic(self):
        logic = re.search(r'<script id="logic">(.*?)</script>', self.html, re.S).group(1)
        checks = r'''
const L = module.exports, assert = require('assert');
const scaleC = {unit: 'C', warm: 60, amber: 115, red: 130, dial: [30, 130], gradient: Array.from({length: 81}, (_, i) => '#' + String(i).padStart(6, '0'))};
const scaleF = Object.assign({}, scaleC, {unit: 'F'});
// Whole numbers as the gauge rounds them; units.
assert.deepStrictEqual([L.whole(117.5), L.whole(117.4), L.whole(-0.5), L.whole(-20.5)], [118, 117, -1, -21]);
assert.strictEqual(L.toUnit(100, 'F'), 212); assert.strictEqual(L.toUnit(100, 'C'), 100);
// The digits: whole degrees; whole ohms uncalibrated, -- from 1000; -- with nothing.
assert.strictEqual(L.digits({temp: 117.5}, true, 'C'), '118');
assert.strictEqual(L.digits({temp: 117.5}, true, 'F'), '244');
assert.strictEqual(L.digits({temp: null, ohms: 120}, true, 'C'), '--');
assert.strictEqual(L.digits({temp: null, ohms: 123.4}, false, 'C'), '123');
assert.strictEqual(L.digits({temp: null, ohms: 1234}, false, 'C'), '--');
assert.strictEqual(L.digits({temp: null, ohms: 999.5}, false, 'C'), '--');
assert.strictEqual(L.digits({temp: null, ohms: 999.4}, false, 'C'), '999');
assert.strictEqual(L.digits({temp: null, ohms: null}, false, 'C'), '--');
assert.strictEqual(L.digits(null, true, 'C'), '--');
// Tape's trend rule.
const ramp = Array.from({length: 40}, (_, i) => ({seq: i, up: i * 1000, st: 9, t: 80 + 0.04 * i}));
assert.ok(Math.abs(L.trend(ramp, 39000) - 2.4) < 1e-9);
assert.strictEqual(L.trendText(L.trend(ramp, 39000), 'C'), '▲ 2.4 °C/min');
assert.strictEqual(L.trendText(-1.06, 'C'), '▼ 1.1 °C/min');
assert.strictEqual(L.trendText(1, 'F'), '▲ 1.8 °F/min');
assert.strictEqual(L.trendText(0.49, 'C'), 'Steady');
assert.strictEqual(L.trend(ramp.slice(-29), 39000), null);                  // too few
assert.strictEqual(L.trend(ramp, 39000 + 3001), null);                       // the newest too old
const gap = ramp.slice(); gap.splice(30, 1); assert.strictEqual(L.trend(gap, 39000), null);          // a missing row
const fault = ramp.map(r => Object.assign({}, r)); fault[35].t = null; assert.strictEqual(L.trend(fault, 39000), null);
const above = ramp.map(r => Object.assign({}, r)); above[35].st = 7; assert.strictEqual(L.trend(above, 39000), null);
assert.deepStrictEqual(['', 'WARM AT START', 'WARM IN 6:12', '2:35'].map(L.warmText), ['--', 'Warm at start', 'Warm in 6:12', 'Warming 2:35']);
// Times, freshness, labels and colours.
assert.deepStrictEqual([L.duration(0), L.duration(59999), L.duration(372000), L.duration(3600000), L.duration(3723000)], ['0:00', '0:59', '6:12', '1:00:00', '1:02:03']);
assert.deepStrictEqual([L.freshness(null), L.freshness(2.9), L.freshness(3), L.freshness(9.9), L.freshness(10)], ['waiting', 'live', 'stale', 'stale', 'lost']);
assert.deepStrictEqual(L.bandLabels(scaleC), ['Below 60°', '60–115°', '115–130°', '130° and above', 'No reading']);
assert.deepStrictEqual(L.bandLabels(scaleF), ['Below 140°', '140–239°', '239–266°', '266° and above', 'No reading']);
assert.strictEqual(L.colourAt(80, scaleC), '#000040'); assert.strictEqual(L.colourAt(-5, scaleC), '#000000');
assert.strictEqual(L.colourAt(500, scaleC), '#000080'); assert.strictEqual(L.colourAt(null, scaleC), '#8597A3');
assert.strictEqual(L.statusText(10), 'HIGH TEMPERATURE'); assert.strictEqual(L.statusText(42), 'UNKNOWN 42');
// Rows and the phone's clock.
const row = L.toRow([7, 5000, 9, 9, 92.4, 262.1, 0.5, 0.0655, 3.3, 0.012], 9000, 1700000000000);
assert.deepStrictEqual([row.seq, row.up, row.t, row.pt], [7, 5000, 92.4, 1700000000000 - 4000]);
// Merging: each row once, in order, whatever arrives.
const r = (seq) => ({seq, up: seq * 1000});
assert.deepStrictEqual(L.merge([r(0), r(1)], [r(2), r(3)]).map(x => x.seq), [0, 1, 2, 3]);
assert.deepStrictEqual(L.merge([r(0), r(1), r(2)], [r(1), r(2), r(3)]).map(x => x.seq), [0, 1, 2, 3]);
assert.deepStrictEqual(L.merge([r(0), r(2)], [r(1)]).map(x => x.seq), [0, 1, 2]);
assert.deepStrictEqual(L.merge([], []), []);
// The CSV: REQ-D2's columns with the phone's time; an empty field where there's no value.
const csv = L.csv([{seq: 3, up: 3000, st: 4, al: 9, t: null, ohm: null, ref: 0.5, snd: 2.1, sup: 3.3, gnd: 0.012, pt: Date.UTC(2026, 10, 23, 14, 32, 7)},
  {seq: 4, up: 4000, st: 10, al: 10, t: 117.46, ohm: 120.84, ref: 0.4866, snd: 0.0294, sup: 3.3169, gnd: -0.0021, pt: Date.UTC(2026, 10, 23, 14, 32, 8)}], '2161ab7c');
assert.strictEqual(csv, 'phone_time,uptime_ms,status,alarm,temp_c,resistance_ohms,ref_v,sender_v,supply_v,ground_offset_v,session,sequence\n' +
  '2026-11-23T14:32:07.000Z,3000,SENSOR OPEN,NORMAL,,,0.5000,2.1000,3.3000,0.0120,2161ab7c,3\n' +
  '2026-11-23T14:32:08.000Z,4000,HIGH TEMPERATURE,HIGH TEMPERATURE,117.5,120.8,0.4866,0.0294,3.3169,-0.0021,2161ab7c,4\n');
console.log('page logic ok');
'''
        with tempfile.TemporaryDirectory() as d:
            js = Path(d) / 'logic_test.cjs'  # CommonJS, whatever package.json says further up
            js.write_text(logic + checks, encoding='utf-8')
            r = subprocess.run(['node', str(js)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr + r.stdout)
            self.assertIn('page logic ok', r.stdout)


if __name__ == '__main__':
    unittest.main()
