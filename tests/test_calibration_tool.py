import tempfile
from pathlib import Path
import unittest
import importlib.util

spec=importlib.util.spec_from_file_location('cal','scripts/build_calibration.py')
cal=importlib.util.module_from_spec(spec)
spec.loader.exec_module(cal)

HEADER='temperature_c,resistance_ohms\n'
GOOD='20,800\n40,550\n60,350\n80,200\n100,110\n'

class TestCalibration(unittest.TestCase):
    def load(self,text):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'measurements.csv'
            p.write_text(text)
            return cal.load(p)
    def test_good(self):
        points=self.load(HEADER+GOOD)
        self.assertEqual(len(points),5)
        self.assertIn('kCalibrationVerified = true',cal.generate(points))
    def test_comments_and_blank_lines_ignored(self):
        self.assertEqual(len(self.load(HEADER+'# measured 2026-10-26\n'+GOOD+'\n')),5)
    def test_reject_increasing_resistance(self):
        with self.assertRaises(ValueError):self.load(HEADER+GOOD.replace('40,550','40,900'))
    def test_reject_not_enough_points(self):
        with self.assertRaises(ValueError):self.load(HEADER+'20,800\n60,350\n80,200\n100,110\n')
    def test_example_file_has_no_points(self):
        with self.assertRaises(ValueError):cal.load(Path('examples/measurements.example.csv'))
    def test_reject_malformed_rows(self):
        for bad in ('20\n','20,abc\n','20,\n','20,nan\n','20,-5\n'):
            with self.subTest(bad=bad),self.assertRaises(ValueError):
                self.load(HEADER+GOOD+bad)
if __name__=='__main__':unittest.main()
