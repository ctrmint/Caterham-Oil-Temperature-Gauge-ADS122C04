import importlib.util
import re
import shutil
import tempfile
import unittest
from pathlib import Path


def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

plot=load('plot','scripts/plot_sender_curve.py')
cal=load('cal','scripts/build_calibration.py')
INPUTS=(plot.REFERENCE,plot.OBSERVATIONS,plot.MEASUREMENTS,plot.CONFIG,plot.CALIBRATION)


def words(svg):
    """The visible text, joined, so wrapped legend lines read as one phrase."""
    return ' '.join(re.findall(r'>([^<]*)</text>',svg))


class TestSenderCurve(unittest.TestCase):
    def copy_inputs(self,d):
        for rel in INPUTS:
            (Path(d)/rel).parent.mkdir(parents=True,exist_ok=True)
            shutil.copy(rel,Path(d)/rel)
        return Path(d)
    def test_committed_render_is_current(self):
        self.assertEqual(Path(plot.OUTPUT).read_text(encoding='utf-8'),plot.render('.'),
                         f'{plot.OUTPUT} is stale: run python3 scripts/plot_sender_curve.py')
    def test_render_follows_threshold_changes(self):
        with tempfile.TemporaryDirectory() as d:
            root=self.copy_inputs(d)
            config=root/plot.CONFIG
            config.write_text(config.read_text().replace('kOpenResistanceOhms = 5000.0f','kOpenResistanceOhms = 50000.0f'))
            svg=plot.render(root)
            self.assertNotEqual(svg,plot.render('.'))
            self.assertIn('SENSOR OPEN above 50 kΩ',words(svg))
    def test_measured_points_are_plotted(self):
        with tempfile.TemporaryDirectory() as d:
            root=self.copy_inputs(d)
            with open(root/plot.MEASUREMENTS,'a',encoding='utf-8') as f:
                f.write('60,600,2026-11-14,probe,test\n200,50,2026-11-14,probe,off the axes\n')
            svg=plot.render(root)
            self.assertIn('Measured points, docs/sender-measurements.csv (2) (1 outside the axes, not shown)',words(svg))
            self.assertNotIn('No measured points',words(svg))
    def test_unverified_table_is_not_plotted(self):
        self.assertIn('No verified calibration table',words(plot.render('.')))
    def test_verified_table_is_plotted(self):
        with tempfile.TemporaryDirectory() as d:
            root=self.copy_inputs(d)
            (root/plot.CALIBRATION).write_text(cal.generate([(20,2500),(40,1175),(60,596),(80,323),(100,187)]))
            svg=plot.render(root)
            self.assertIn('Verified calibration table, main/CalibrationData.h (5 points)',words(svg))
            self.assertNotIn('No verified calibration table',words(svg))
    def test_reference_curves_cannot_become_calibration(self):
        with self.assertRaises(ValueError):cal.load(Path(plot.REFERENCE))
    def test_reject_non_monotonic_reference(self):
        with tempfile.TemporaryDirectory() as d:
            root=self.copy_inputs(d)
            ref=root/plot.REFERENCE
            ref.write_text(ref.read_text().replace('60,596,510','60,900,510'))
            with self.assertRaises(ValueError):plot.render(root)
