import importlib.util
import pathlib
import shutil
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('cr', 'scripts/cr.py')
cr = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cr)

REPO = pathlib.Path('.').resolve()
SPEC_INDEX = """# Spec

## 3. Change requests

| CR | Issue | Title | Status | Folder |
|----|-------|-------|--------|--------|
| CR-1 | [#5](x) | First | done | [docs/changes/CR-1/](changes/CR-1/) |
"""
PLAN = """### T1: First task
- kind: test
- status: ready

### T2: Second task
- kind: impl
- status: ready
"""


class CrTool(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = pathlib.Path(self.tmp.name)
        changes = root / 'docs' / 'changes'
        shutil.copytree(REPO / 'docs' / 'changes' / '_template', changes / '_template')
        (changes / 'CR-1').mkdir()
        (root / 'docs' / 'spec.md').write_text(SPEC_INDEX)
        self.saved = cr.ROOT, cr.CHANGES, cr.SPEC
        cr.ROOT, cr.CHANGES, cr.SPEC = root, changes, root / 'docs' / 'spec.md'
        cr.new('CR-2', 7, 'Second change', 'https://example.test/repo')
        tasks = changes / 'CR-2' / 'tasks.md'
        text = tasks.read_text()
        start = text.index('_No plan yet')
        tasks.write_text(text[:start] + PLAN + text[text.index('\n', start) + 1:])
        self.tasks = tasks

    def tearDown(self):
        cr.ROOT, cr.CHANGES, cr.SPEC = self.saved
        self.tmp.cleanup()

    def test_new_creates_draft_folder_and_index_row(self):
        self.assertEqual(cr.request_status('CR-2'), 'draft')
        self.assertIn('| CR-2 | [#7](https://example.test/repo/issues/7) | Second change | draft |', cr.SPEC.read_text())
        self.assertTrue(self.tasks.read_text().startswith('# CR-2 task plan'))
        with self.assertRaises(cr.CrError):
            cr.new('CR-2', 8, 'Again', 'x')
        with self.assertRaises(cr.CrError):
            cr.new('CR-5', 8, 'Skips a number', 'x')

    def test_set_status_keeps_request_and_index_in_step(self):
        cr.set_status('CR-2', 'active', '')
        self.assertEqual(cr.request_status('CR-2'), 'active')
        self.assertEqual(cr.active_crs(), ['CR-2'])
        self.assertEqual(cr.check('CR-2'), [])
        with self.assertRaises(cr.CrError):
            cr.set_status('CR-2', 'finished', '')

    def test_only_one_active_cr(self):
        cr.set_status('CR-2', 'active', '')
        cr.new('CR-3', 9, 'Third', 'x')
        with self.assertRaises(cr.CrError):
            cr.set_status('CR-3', 'active', '')

    def test_task_status(self):
        cr.task_status('CR-2', 'T1', 'done')
        self.assertEqual([t[2] for t in cr.tasks(self.tasks.read_text().split('\n'))], ['done', 'ready'])
        for bad in (('T9', 'done'), ('T1', 'finished')):
            with self.subTest(bad=bad), self.assertRaises(cr.CrError):
                cr.task_status('CR-2', *bad)

    def test_finding_ids_are_sequential_and_never_reused(self):
        cr.add_finding('CR-2', 'T1-F1', 1, 'major', 'Gap | with pipe', '')
        with self.assertRaises(cr.CrError):
            cr.add_finding('CR-2', 'T1-F1', 1, 'minor', 'Duplicate', '')
        with self.assertRaises(cr.CrError):
            cr.add_finding('CR-2', 'T1-F3', 1, 'minor', 'Skips F2', '')
        with self.assertRaises(cr.CrError):
            cr.add_finding('CR-2', 'T9-F1', 1, 'minor', 'Unknown task', '')
        with self.assertRaises(cr.CrError):
            cr.add_finding('CR-2', 'T1-F2', 1, 'huge', 'Bad severity', '')
        cr.add_finding('CR-2', 'T1-F2', 2, 'nit', 'Second', 'a | b')
        rows = cr.findings(self.tasks.read_text().split('\n'))
        self.assertEqual([(r['id'], r['status']) for r in rows], [('T1-F1', 'open'), ('T1-F2', 'open')])
        self.assertEqual(rows[0]['title'], 'Gap \\| with pipe')

    def test_close_finding(self):
        cr.add_finding('CR-2', 'T1-F1', 1, 'major', 'Gap', 'keep | this')
        cr.close_finding('CR-2', 'T1-F1', 'fixed', '2', '')
        row = cr.findings(self.tasks.read_text().split('\n'))[0]
        self.assertEqual((row['status'], row['closed'], row['notes']), ('fixed', '2', 'keep \\| this'))
        with self.assertRaises(cr.CrError):
            cr.close_finding('CR-2', 'T1-F1', 'accepted', '3', '')
        with self.assertRaises(cr.CrError):
            cr.close_finding('CR-2', 'T1-F9', 'fixed', '2', '')

    def test_log_appends_to_the_progress_log(self):
        before = self.tasks.read_text()
        cr.log('CR-2', 'T1', 'Review round 1', 'pass', 'line\nbreak')
        after = self.tasks.read_text()
        self.assertTrue(after.rstrip('\n').endswith('| T1 | Review round 1 | pass | line break |'))
        self.assertEqual(len(after.split('\n')), len(before.split('\n')) + 1)

    def test_check_catches_inconsistent_done(self):
        cr.add_finding('CR-2', 'T1-F1', 1, 'major', 'Gap', '')
        cr.set_status('CR-2', 'done', '')
        problems = cr.check('CR-2')
        self.assertIn('CR is done but not every task is done', problems)
        self.assertIn('CR is done but some findings are not settled', problems)

    def test_status_summary(self):
        cr.add_finding('CR-2', 'T1-F1', 1, 'major', 'Gap', '')
        text = cr.status('CR-2')
        self.assertIn('CR-2: draft', text)
        self.assertIn('T1-F1 major open', text)
        self.assertIn('next task: T1: First task', text)


class RealRecords(unittest.TestCase):
    def test_existing_change_requests_are_consistent(self):
        for d in sorted((REPO / 'docs' / 'changes').glob('CR-*')):
            with self.subTest(cr=d.name):
                self.assertEqual(cr.check(d.name), [])


if __name__ == '__main__':
    unittest.main()
