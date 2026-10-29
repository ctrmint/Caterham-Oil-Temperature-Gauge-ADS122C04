#!/usr/bin/env python3
"""Change-request bookkeeping for docs/changes/CR-<n>/ (see docs/changes/README.md).

Edits only the lines it owns: task `- status:` lines, the Findings and Progress log
tables in tasks.md, the `- **Status:**` line in request.md and the CR row of the
docs/spec.md §3 index. Every command validates its input and refuses ambiguous edits.

  cr.py new CR-3 --issue 12 --title "..."         create the folder from _template, index as draft
  cr.py active                                    print the active CR (or nothing)
  cr.py status CR-3                               summary: CR status, tasks, open findings
  cr.py set-status CR-3 active [--note "..."]     request.md + spec index
  cr.py task-status CR-3 T2 done                  ready | blocked | done
  cr.py add-finding CR-3 T2-F1 --round 1 --severity major --title "..." [--notes "..."]
  cr.py close-finding CR-3 T2-F1 fixed --closed 2 [--notes "..."]
  cr.py log CR-3 --task T2 --event "..." --result "..." [--notes "..."]
  cr.py check CR-3                                consistency check, exit 1 on problems
"""
import argparse
import datetime
import pathlib
import re
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CHANGES = ROOT / 'docs' / 'changes'
SPEC = ROOT / 'docs' / 'spec.md'

CR_STATUSES = ('draft', 'active', 'done', 'withdrawn')
TASK_STATUSES = ('ready', 'blocked', 'done')
SEVERITIES = ('blocker', 'major', 'minor', 'nit')
FINDING_STATUSES = ('open', 'fixed', 'disputed', 'withdrawn', 'escalated', 'accepted', 'follow-up')
SETTLED = ('fixed', 'withdrawn', 'accepted', 'follow-up')
FINDINGS_HEADER = '| ID | Task | Raised | Severity | Title | Status | Closed | Notes |'
LOG_HEADER = '| Date | Task | Event | Result | Notes |'


class CrError(Exception):
    pass


def cr_id(text):
    if not re.fullmatch(r'CR-[1-9][0-9]*', text):
        raise CrError(f'bad CR id {text!r}: expected CR-<n>')
    return text


def cr_dir(cr):
    d = CHANGES / cr_id(cr)
    if not d.is_dir():
        raise CrError(f'{d.relative_to(ROOT)} does not exist')
    return d


def cell(text):
    """One table cell: a single line, pipes escaped."""
    text = ' '.join(str(text).split())
    return text.replace('|', '\\|')


def split_row(line):
    cells = re.split(r'(?<!\\)\|', line.strip())[1:-1]
    return [c.strip() for c in cells]


def table_span(lines, header):
    """(first data row index, end index) of the table whose header line is `header`."""
    for i, line in enumerate(lines):
        if line.strip() == header:
            if i + 1 >= len(lines) or not lines[i + 1].startswith('|--'):
                raise CrError(f'table {header!r} has no separator row')
            end = i + 2
            while end < len(lines) and lines[end].startswith('|'):
                end += 1
            return i + 2, end
    raise CrError(f'table not found: {header!r}')


def read_lines(path):
    return path.read_text(encoding='utf-8').split('\n')


def write_lines(path, lines):
    path.write_text('\n'.join(lines), encoding='utf-8')


# ---- tasks.md -----------------------------------------------------------------

def tasks(lines):
    """[(task id, title, status, index of its status line)] in plan order."""
    out, current = [], None
    for i, line in enumerate(lines):
        m = re.match(r'### (T[1-9][0-9]*): (.*)', line)
        if m:
            current = [m.group(1), m.group(2).strip(), None, None]
            out.append(current)
            continue
        m = re.match(r'- status: (\S+)', line)
        if m and current and current[2] is None:
            current[2], current[3] = m.group(1), i
    return [tuple(t) for t in out]


def findings(lines):
    start, end = table_span(lines, FINDINGS_HEADER)
    rows = []
    for i in range(start, end):
        c = split_row(lines[i])
        if len(c) != 8:
            raise CrError(f'malformed Findings row: {lines[i]!r}')
        rows.append(dict(zip(('id', 'task', 'raised', 'severity', 'title', 'status', 'closed', 'notes'), c), line=i))
    return rows


def task_status(cr, task, status):
    if status not in TASK_STATUSES:
        raise CrError(f'task status must be one of {", ".join(TASK_STATUSES)}')
    path = cr_dir(cr) / 'tasks.md'
    lines = read_lines(path)
    match = [t for t in tasks(lines) if t[0] == task]
    if not match:
        raise CrError(f'{task} is not in {cr} tasks.md')
    _, _, _, idx = match[0]
    if idx is None:
        raise CrError(f'{task} has no "- status:" line')
    lines[idx] = f'- status: {status}'
    write_lines(path, lines)
    return f'{cr} {task} status: {status}'


def add_finding(cr, fid, rnd, severity, title, notes):
    m = re.fullmatch(r'(T[1-9][0-9]*)-F([1-9][0-9]*)', fid)
    if not m:
        raise CrError(f'bad finding id {fid!r}: expected T<m>-F<k>')
    if severity not in SEVERITIES:
        raise CrError(f'severity must be one of {", ".join(SEVERITIES)}')
    if rnd < 1:
        raise CrError('round must be 1 or more')
    path = cr_dir(cr) / 'tasks.md'
    lines = read_lines(path)
    task = m.group(1)
    if task not in [t[0] for t in tasks(lines)]:
        raise CrError(f'{task} is not in {cr} tasks.md')
    rows = findings(lines)
    if any(r['id'] == fid for r in rows):
        raise CrError(f'{fid} already exists: finding IDs are never reused')
    taken = [int(r['id'].split('-F')[1]) for r in rows if r['id'].startswith(task + '-F')]
    expected = max(taken, default=0) + 1
    if int(m.group(2)) != expected:
        raise CrError(f'next finding ID for {task} is {task}-F{expected}, not {fid}')
    _, end = table_span(lines, FINDINGS_HEADER)
    lines.insert(end, '| ' + ' | '.join(cell(x) for x in (fid, task, rnd, severity, title, 'open', '', notes)) + ' |')
    write_lines(path, lines)
    return f'{cr} {fid} added ({severity}, open)'


def close_finding(cr, fid, status, closed, notes):
    if status not in FINDING_STATUSES or status == 'open':
        raise CrError(f'status must be one of {", ".join(s for s in FINDING_STATUSES if s != "open")}')
    path = cr_dir(cr) / 'tasks.md'
    lines = read_lines(path)
    match = [r for r in findings(lines) if r['id'] == fid]
    if not match:
        raise CrError(f'{fid} is not in the {cr} Findings table')
    r = match[0]
    if r['status'] in SETTLED:
        raise CrError(f'{fid} is already {r["status"]}')
    # Unchanged cells are written back as parsed (already escaped); only new values go through cell().
    r['status'], r['closed'] = status, cell(closed)
    if notes:
        r['notes'] = cell(notes)
    lines[r['line']] = '| ' + ' | '.join(r[k] for k in ('id', 'task', 'raised', 'severity', 'title', 'status', 'closed', 'notes')) + ' |'
    write_lines(path, lines)
    return f'{cr} {fid}: {status}'


def log(cr, task, event, result, notes, date=None):
    path = cr_dir(cr) / 'tasks.md'
    lines = read_lines(path)
    _, end = table_span(lines, LOG_HEADER)
    date = date or datetime.date.today().isoformat()
    lines.insert(end, '| ' + ' | '.join(cell(x) for x in (date, task, event, result, notes)) + ' |')
    write_lines(path, lines)
    return f'{cr} log: {task} {event}'


# ---- request.md and the spec index ---------------------------------------------

def request_status(cr):
    for line in read_lines(cr_dir(cr) / 'request.md'):
        m = re.match(r'- \*\*Status:\*\* ([a-z-]+)', line)
        if m:
            return m.group(1)
    raise CrError(f'{cr} request.md has no "- **Status:**" line')


def index_rows():
    lines = read_lines(SPEC)
    rows = {}
    for i, line in enumerate(lines):
        m = re.match(r'\| (CR-[1-9][0-9]*) \|', line)
        if m:
            rows[m.group(1)] = (i, split_row(line))
    return lines, rows


def set_status(cr, status, note):
    if status not in CR_STATUSES:
        raise CrError(f'CR status must be one of {", ".join(CR_STATUSES)}')
    if status == 'active':
        others = [c for c in active_crs() if c != cr]
        if others:
            raise CrError(f'{", ".join(others)} is already active: at most one CR is active at a time')
    req = cr_dir(cr) / 'request.md'
    lines = read_lines(req)
    idx = [i for i, l in enumerate(lines) if l.startswith('- **Status:**')]
    if len(idx) != 1:
        raise CrError(f'{cr} request.md must have exactly one "- **Status:**" line')
    lines[idx[0]] = f'- **Status:** {status}' + (f'. {cell(note)}' if note else '') + '  (keep the docs/spec.md §3 index in step)'
    spec_lines, rows = index_rows()
    if cr not in rows:
        raise CrError(f'{cr} has no row in the docs/spec.md §3 index')
    i, cells = rows[cr]
    if len(cells) != 5:
        raise CrError(f'malformed index row for {cr}')
    cells[3] = status + (f' ({cell(note)})' if note else '')
    spec_lines[i] = '| ' + ' | '.join(cells) + ' |'
    write_lines(req, lines)
    write_lines(SPEC, spec_lines)
    return f'{cr} status: {status}'


def active_crs():
    _, rows = index_rows()
    return [cr for cr, (_, cells) in rows.items() if len(cells) == 5 and cells[3].split()[0] == 'active']


def new(cr, issue, title, repo_url):
    cr_id(cr)
    d = CHANGES / cr
    if d.exists():
        raise CrError(f'{d.relative_to(ROOT)} already exists')
    _, rows = index_rows()
    if cr in rows:
        raise CrError(f'{cr} is already in the docs/spec.md index')
    nums = [int(p.name[3:]) for p in CHANGES.glob('CR-*') if re.fullmatch(r'CR-[1-9][0-9]*', p.name)]
    if int(cr[3:]) != max(nums, default=0) + 1:
        raise CrError(f'next CR is CR-{max(nums, default=0) + 1}, not {cr}')
    shutil.copytree(CHANGES / '_template', d)
    for name in ('request.md', 'tasks.md'):
        p = d / name
        p.write_text(p.read_text(encoding='utf-8').replace('CR-<n>', cr, 1), encoding='utf-8')
    req = d / 'request.md'
    req.write_text(req.read_text(encoding='utf-8').replace('<title>', cell(title), 1), encoding='utf-8')
    lines = read_lines(SPEC)
    last = max(i for i, l in enumerate(lines) if l.startswith('|'))
    link = f'[#{issue}]({repo_url}/issues/{issue})'
    lines.insert(last + 1, '| ' + ' | '.join((cr, link, cell(title), 'draft', f'[docs/changes/{cr}/](changes/{cr}/)')) + ' |')
    write_lines(SPEC, lines)
    return f'created docs/changes/{cr}/ (draft) and its index row'


# ---- status and check ----------------------------------------------------------

def status(cr):
    lines = read_lines(cr_dir(cr) / 'tasks.md')
    out = [f'{cr}: {request_status(cr)}']
    ts = tasks(lines)
    out.append('tasks: ' + (', '.join(f'{t[0]} {t[2]}' for t in ts) if ts else 'no plan yet'))
    open_f = [r for r in findings(lines) if r['status'] not in SETTLED]
    out.append('open findings: ' + (', '.join(f'{r["id"]} {r["severity"]} {r["status"]}' for r in open_f) if open_f else 'none'))
    nxt = [t for t in ts if t[2] == 'ready']
    out.append('next task: ' + (f'{nxt[0][0]}: {nxt[0][1]}' if nxt else 'none'))
    return '\n'.join(out)


def check(cr):
    problems = []
    lines = read_lines(cr_dir(cr) / 'tasks.md')
    st = request_status(cr)
    if st not in CR_STATUSES:
        problems.append(f'request.md status {st!r} is not one of {CR_STATUSES}')
    _, rows = index_rows()
    if cr not in rows:
        problems.append('no docs/spec.md index row')
    elif rows[cr][1][3].split()[0] != st:
        problems.append(f'index says {rows[cr][1][3].split()[0]!r} but request.md says {st!r}')
    ts = tasks(lines)
    ids = [t[0] for t in ts]
    if len(ids) != len(set(ids)):
        problems.append('duplicate task IDs')
    for t in ts:
        if t[2] not in TASK_STATUSES:
            problems.append(f'{t[0]} status {t[2]!r} is not one of {TASK_STATUSES}')
    fs = findings(lines)
    fids = [r['id'] for r in fs]
    if len(fids) != len(set(fids)):
        problems.append('duplicate finding IDs')
    for r in fs:
        if r['severity'] not in SEVERITIES:
            problems.append(f'{r["id"]} severity {r["severity"]!r} is not valid')
        if r['status'] not in FINDING_STATUSES:
            problems.append(f'{r["id"]} status {r["status"]!r} is not valid')
        if r['task'] not in ids:
            problems.append(f'{r["id"]} refers to unknown task {r["task"]}')
    if st == 'done':
        if any(t[2] != 'done' for t in ts):
            problems.append('CR is done but not every task is done')
        if any(r['status'] not in SETTLED for r in fs):
            problems.append('CR is done but some findings are not settled')
    return problems


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('new'); s.add_argument('cr'); s.add_argument('--issue', type=int, required=True); s.add_argument('--title', required=True)
    s.add_argument('--repo-url', default='https://github.com/ctrmint/Caterham-Oil-Temperature-Gauge')
    sub.add_parser('active')
    s = sub.add_parser('status'); s.add_argument('cr')
    s = sub.add_parser('set-status'); s.add_argument('cr'); s.add_argument('status'); s.add_argument('--note', default='')
    s = sub.add_parser('task-status'); s.add_argument('cr'); s.add_argument('task'); s.add_argument('status')
    s = sub.add_parser('add-finding'); s.add_argument('cr'); s.add_argument('id'); s.add_argument('--round', type=int, required=True)
    s.add_argument('--severity', required=True); s.add_argument('--title', required=True); s.add_argument('--notes', default='')
    s = sub.add_parser('close-finding'); s.add_argument('cr'); s.add_argument('id'); s.add_argument('status')
    s.add_argument('--closed', required=True); s.add_argument('--notes', default='')
    s = sub.add_parser('log'); s.add_argument('cr'); s.add_argument('--task', required=True); s.add_argument('--event', required=True)
    s.add_argument('--result', required=True); s.add_argument('--notes', default='')
    s = sub.add_parser('check'); s.add_argument('cr')
    a = p.parse_args(argv)
    try:
        if a.cmd == 'new': print(new(a.cr, a.issue, a.title, a.repo_url.rstrip('/')))
        elif a.cmd == 'active': print('\n'.join(active_crs()))
        elif a.cmd == 'status': print(status(a.cr))
        elif a.cmd == 'set-status': print(set_status(a.cr, a.status, a.note))
        elif a.cmd == 'task-status': print(task_status(a.cr, a.task, a.status))
        elif a.cmd == 'add-finding': print(add_finding(a.cr, a.id, a.round, a.severity, a.title, a.notes))
        elif a.cmd == 'close-finding': print(close_finding(a.cr, a.id, a.status, a.closed, a.notes))
        elif a.cmd == 'log': print(log(a.cr, a.task, a.event, a.result, a.notes))
        elif a.cmd == 'check':
            probs = check(a.cr)
            print('\n'.join(probs) if probs else f'{a.cr}: ok')
            return 1 if probs else 0
    except CrError as e:
        print('ERROR:', e, file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
