import tempfile
from pathlib import Path
import unittest
import importlib.util

spec=importlib.util.spec_from_file_location('cal','scripts/build_calibration.py')
cal=importlib.util.module_from_spec(spec)
spec.loader.exec_module(cal)

class TestCalibration(unittest.TestCase):
    def test_good(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'measurements.csv'
            p.write_text('temperature_c,resistance_ohms\n20,800\n60,350\n100,110\n')
            points=cal.load(p)
            self.assertEqual(len(points),3)
            self.assertIn('kCalibrationVerified = true',cal.generate(points))
    def test_reject_increasing_resistance(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'bad.csv'
            p.write_text('temperature_c,resistance_ohms\n20,800\n60,900\n100,110\n')
            with self.assertRaises(ValueError):cal.load(p)
    def test_reject_not_enough_points(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'few.csv'
            p.write_text('temperature_c,resistance_ohms\n20,800\n100,110\n')
            with self.assertRaises(ValueError):cal.load(p)
if __name__=='__main__':unittest.main()
