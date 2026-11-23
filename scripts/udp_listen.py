#!/usr/bin/env python3
"""Monitor the gauge's UDP data stream (#33): a live dashboard, or one line per packet, and a CSV log.

The packet format is docs/UDP_STREAM.md (version 1). Join the laptop to the gauge's Wi-Fi network, then run:

    python3 scripts/udp_listen.py                 # a live dashboard in the terminal (q to quit)
    python3 scripts/udp_listen.py --csv run1.csv  # and log every packet
    python3 scripts/udp_listen.py --plain         # one line per packet, as when piped or on Windows
"""
import argparse
import csv
import locale
import math
import signal
import socket
import struct
import sys
import time
import zlib
from collections import deque

try:  # not in Windows' Python: plain mode there
    import curses
except ImportError:
    curses = None

MAGIC = b'OILG'
VERSION = 1
SIZE = 56
LAYOUT = struct.Struct('<4sBBHIIIBBH7fI')  # docs/UDP_STREAM.md
assert LAYOUT.size == SIZE
CALIBRATED, DEMO, HAS_TEMPERATURE, MAX_ABOVE_RANGE, HAS_MAX = 1, 2, 4, 8, 16
STATUS = ('ADC UNAVAILABLE', 'SUPPLY FAULT', 'GROUND FAULT', 'SENSOR SHORT', 'SENSOR OPEN', 'CALIBRATION REQUIRED',
          'BELOW CAL RANGE', 'ABOVE CAL RANGE', 'OIL WARMING', 'NORMAL', 'HIGH TEMPERATURE', 'CRITICAL TEMP')
CSV_COLUMNS = ('uptime_ms', 'status', 'alarm', 'temp_c', 'resistance_ohms', 'ref_v', 'sender_v', 'supply_v',
               'ground_offset_v', 'session', 'sequence', 'max_c', 'host_time')


def status_text(code):
    return STATUS[code] if code < len(STATUS) else f'UNKNOWN {code}'


def decode(data):
    """One packet as a dict, or ValueError saying why it was rejected."""
    if len(data) != SIZE:
        raise ValueError(f'length {len(data)}, expected {SIZE}')
    (magic, version, length, flags, session, sequence, uptime_ms, status, alarm, _reserved,
     temp_c, ohms, ref_v, sender_v, supply_v, ground_v, max_c, crc) = LAYOUT.unpack(data)
    if magic != MAGIC:
        raise ValueError('bad magic')
    if version != VERSION:
        raise ValueError(f'unknown version {version}')
    if length != SIZE:
        raise ValueError(f'length field {length}, expected {SIZE}')
    if zlib.crc32(data[:52]) != crc:
        raise ValueError('bad CRC')
    has_t = bool(flags & HAS_TEMPERATURE)
    return {
        'flags': flags, 'calibrated': bool(flags & CALIBRATED), 'demo': bool(flags & DEMO), 'has_temperature': has_t,
        'max_above_range': bool(flags & MAX_ABOVE_RANGE), 'has_max': bool(flags & HAS_MAX),
        'session': session, 'sequence': sequence, 'uptime_ms': uptime_ms,
        'status': status, 'alarm': alarm,
        # Fail closed: no temperature unless the gauge flagged one, whatever the bytes hold.
        'temp_c': temp_c if has_t and math.isfinite(temp_c) else None,
        'resistance_ohms': ohms if math.isfinite(ohms) else None,
        'ref_v': ref_v if math.isfinite(ref_v) else None,
        'sender_v': sender_v if math.isfinite(sender_v) else None,
        'supply_v': supply_v if math.isfinite(supply_v) else None,
        'ground_offset_v': ground_v if math.isfinite(ground_v) else None,
        'max_c': max_c if flags & HAS_MAX and math.isfinite(max_c) else None,
    }


def whole(v):
    """Whole degrees, halves away from zero, as the gauge rounds (DisplayText's roundHalfAway)."""
    return int(math.copysign(math.floor(abs(v) + 0.5), v))


def describe(p):
    """One line for the screen: '--' wherever there's no value, as on the gauge."""
    temp = f'{p["temp_c"]:.1f} °C' if p['temp_c'] is not None else '--'
    ohms = f'{p["resistance_ohms"]:.1f} ohm' if p['resistance_ohms'] is not None else '-- ohm'
    text = f'#{p["sequence"]}  {p["uptime_ms"] / 1000:.2f} s  {status_text(p["status"])}  {temp}  {ohms}'
    if p['alarm'] != STATUS.index('NORMAL'):
        text += f'  alarm {status_text(p["alarm"])}'
    if p['max_c'] is not None:
        text += f'  MAX {">" if p["max_above_range"] else ""}{whole(p["max_c"])}'
    if p['demo']:
        text += '  DEMO'
    return text


def csv_row(p, host_time):
    """The REQ-D2 columns, then session, sequence, max_c and the host's clock; a missing value is an empty field."""
    def num(v, fmt):
        return '' if v is None else format(v, fmt)
    return [p['uptime_ms'], status_text(p['status']), status_text(p['alarm']), num(p['temp_c'], '.1f'),
            num(p['resistance_ohms'], '.1f'), num(p['ref_v'], '.4f'), num(p['sender_v'], '.4f'), num(p['supply_v'], '.4f'),
            num(p['ground_offset_v'], '.4f'), f'{p["session"]:08x}', p['sequence'], num(p['max_c'], '.1f'),
            f'{host_time:.3f}']


class Tracker:
    """Counts packets, losses (gaps in the sequence), duplicates and gauge restarts (a new session). Never fills a gap.

    A packet whose sequence number was already seen in this session (among the last WINDOW) is a duplicate: Wi-Fi and
    multicast-to-unicast conversion can deliver one twice. It isn't printed or logged again. A packet counted as lost
    that turns up late is taken off the lost count.
    """
    WINDOW = 1024  # about four minutes of packets

    def __init__(self):
        self.session = None
        self.first = self.next = None
        self.received = self.lost = self.duplicates = self.bad = self.restarts = 0
        self._recent = deque()
        self._seen = set()

    def _remember(self, seq):
        if len(self._recent) == self.WINDOW:
            self._seen.discard(self._recent.popleft())
        self._recent.append(seq)
        self._seen.add(seq)

    def feed(self, p):
        """(keep, notes): keep is False for a duplicate, which the caller skips; notes says what happened."""
        seq = p['sequence']
        if p['session'] != self.session:
            notes = []
            if self.session is not None:
                self.restarts += 1
                notes.append(f'gauge restarted (session {p["session"]:08x})')
            self.session = p['session']
            self.first, self.next = seq, seq + 1
            self._recent.clear()
            self._seen.clear()
            self._remember(seq)
            self.received += 1
            return True, notes
        if seq in self._seen:
            self.duplicates += 1
            return False, [f'duplicate #{seq} skipped']
        self.received += 1
        notes = []
        if seq > self.next:
            gap = seq - self.next
            self.lost += gap
            notes.append(f'{gap} packet{"s" if gap != 1 else ""} lost')
        elif seq < self.next:
            notes.append('out of order')
            if seq > self.first and self.lost > 0:
                self.lost -= 1  # counted lost in a gap, and here after all
        self.next = max(self.next, seq + 1)
        self._remember(seq)
        return True, notes


# The gauge's alarm thresholds in degC (main/Config.h: kAmberThresholdC, kRedThresholdC), for the chart's lines only. The
# test checks they match. Nothing here decides a status or colour from them: the gauge does that.
AMBER_C, RED_C = 115.0, 130.0
STALE_S, LOST_S = 2.0, 10.0    # no packet for this long: NO DATA; then STREAM LOST and no reading
HISTORY_S = 300.0              # the chart's span
TREND_S, TREND_STEADY = 30.0, 0.5  # Tape's rule: 30 s of clean readings, STEADY below 0.5 degC/min
IN_TABLE = {STATUS.index(n) for n in ('OIL WARMING', 'NORMAL', 'HIGH TEMPERATURE', 'CRITICAL TEMP')}

# Block digits, 3 x 5 cells, each cell two characters wide.
_GLYPHS = {
    '0': ('###', '# #', '# #', '# #', '###'), '1': (' # ', '## ', ' # ', ' # ', '###'),
    '2': ('###', '  #', '###', '#  ', '###'), '3': ('###', '  #', '###', '  #', '###'),
    '4': ('# #', '# #', '###', '  #', '  #'), '5': ('###', '#  ', '###', '  #', '###'),
    '6': ('###', '#  ', '###', '# #', '###'), '7': ('###', '  #', '  #', '  #', '  #'),
    '8': ('###', '# #', '###', '# #', '###'), '9': ('###', '# #', '###', '  #', '###'),
    '-': ('   ', '   ', '###', '   ', '   '), '.': (' ', ' ', ' ', ' ', '#'), ' ': ('   ',) * 5,
}
DIGIT_ROWS = 5


def big_text(text):
    """Five rows of block characters for text (digits, '.', '-', ' '); anything else is blank."""
    rows = [''] * DIGIT_ROWS
    for i, ch in enumerate(text):
        glyph = _GLYPHS.get(ch, _GLYPHS[' '])
        for r in range(DIGIT_ROWS):
            rows[r] += ('' if i == 0 else ' ') + glyph[r].replace('#', '██').replace(' ', '  ')
    return rows


def status_style(p):
    """The colour the gauge's own rules give this packet: never less severe than the latched alarm; red for CRITICAL
    and ABOVE CAL RANGE; amber for HIGH, faults and no calibration; blue warming or below the table; green NORMAL."""
    s, a = p['status'], p['alarm']
    if a == STATUS.index('CRITICAL TEMP') or s in (STATUS.index('CRITICAL TEMP'), STATUS.index('ABOVE CAL RANGE')):
        return 'red'
    if a == STATUS.index('HIGH TEMPERATURE') or s == STATUS.index('HIGH TEMPERATURE'):
        return 'amber'
    if s in (STATUS.index('OIL WARMING'), STATUS.index('BELOW CAL RANGE')):
        return 'blue'
    if s == STATUS.index('NORMAL'):
        return 'green'
    return 'amber'


def is_clean(p):
    return p['temp_c'] is not None and p['status'] in IN_TABLE


def trend(history, now):
    """degC per minute over the last TREND_S, or None unless that whole span is clean readings with no gap over 1.5 s
    (Tape's rule). history: (host_time, temp_c or None, clean) oldest first."""
    window = [h for h in history if now - h[0] <= TREND_S]
    if len(window) < 3 or window[0][0] > now - TREND_S + 1.0 or now - window[-1][0] > 1.5:
        return None
    if any(not clean for _, _, clean in window) or any(b[0] - a[0] > 1.5 for a, b in zip(window, window[1:])):
        return None
    n = len(window)
    mt = sum(t for t, _, _ in window) / n
    mv = sum(v for _, v, _ in window) / n
    den = sum((t - mt) ** 2 for t, _, _ in window)
    return None if den <= 0 else 60.0 * sum((t - mt) * (v - mv) for t, v, _ in window) / den


def trend_text(rate):
    if rate is None:
        return '--'
    if abs(rate) < TREND_STEADY:
        return 'STEADY'
    return f'{"▲" if rate > 0 else "▼"} {abs(rate):.1f} °C/min'


class Monitor:
    """Everything the dashboard shows: the latest packet, the last HISTORY_S of readings, and the counters."""
    def __init__(self, group, port, csv_name=None):
        self.group, self.port, self.csv_name = group, port, csv_name
        self.tracker = Tracker()
        self.last = None
        self.last_time = None
        self.history = deque()   # (host_time, temp_c or None, clean)
        self.arrivals = deque()  # host times of the last few seconds' packets, for packets/s
        self.csv_rows = 0
        self.note, self.note_time = '', 0.0
        self.warned = set()

    def receive(self, data, now):
        """Decodes and tracks one datagram. Returns (packet, notes): packet is None for a bad or duplicate one."""
        try:
            p = decode(data)
        except ValueError as e:
            self.tracker.bad += 1
            first = str(e) not in self.warned
            self.warned.add(str(e))
            return None, [f'skipped a packet: {e}'] if first else []
        keep, notes = self.tracker.feed(p)
        if notes:
            self.note, self.note_time = notes[-1], now
        if not keep:
            return None, notes
        if any(n.startswith('gauge restarted') for n in notes):
            self.history.clear()  # a new session: the old chart isn't this run's
        self.last, self.last_time = p, now
        self.history.append((now, p['temp_c'], is_clean(p)))
        self.arrivals.append(now)
        while self.history and now - self.history[0][0] > HISTORY_S:
            self.history.popleft()
        while self.arrivals and now - self.arrivals[0] > 5.0:
            self.arrivals.popleft()
        return p, notes

    def rate(self, now):
        """Packets a second over the last few seconds; 0 once they've stopped."""
        recent = [t for t in self.arrivals if now - t <= 5.0]
        if len(recent) < 2 or now - recent[-1] > STALE_S:
            return 0.0
        return (len(recent) - 1) / max(recent[-1] - recent[0], 1e-3)


class Canvas:
    """A screen of text with coloured spans, built without a terminal so it can be tested."""
    def __init__(self, width, height):
        self.width, self.height = width, height
        self.rows = [[' '] * width for _ in range(height)]
        self.spans = []  # (row, col, length, style)

    def put(self, row, col, text, style=None):
        if not 0 <= row < self.height or col >= self.width or col < 0:
            return
        text = text[:self.width - col]
        for i, ch in enumerate(text):
            self.rows[row][col + i] = ch
        if style and text:
            self.spans.append((row, col, len(text), style))

    def lines(self):
        return [''.join(r) for r in self.rows]

    def style_at(self, row, col):
        found = None
        for r, c, n, style in self.spans:
            if r == row and c <= col < c + n:
                found = style
        return found


def clock(t):
    return time.strftime('%H:%M:%S', time.localtime(t))


def draw_chart(canvas, top, left, width, height, history, now):
    """The last HISTORY_S as columns of block characters, with dotted lines at the alarm thresholds. A column with no
    reading stays empty: a gap, never a joined-up line."""
    blocks = ' ▁▂▃▄▅▆▇█'
    values = [v for _, v, _ in history if v is not None]
    lo = min([AMBER_C - 15.0] + values)
    hi = max([RED_C + 5.0] + values)
    lo, hi = 10.0 * math.floor((lo - 5.0) / 10.0), 10.0 * math.ceil(hi / 10.0)  # the lowest reading still shows a bar
    plot_w = width - 6
    cols = [None] * plot_w
    for t, v, _ in history:
        if v is None or now - t > HISTORY_S:
            continue
        i = int((t - (now - HISTORY_S)) / HISTORY_S * (plot_w - 1))
        if 0 <= i < plot_w:
            cols[i] = v if cols[i] is None else max(cols[i], v)
    levels = height * 8
    def level(v):
        return max(0, min(levels, round((v - lo) / (hi - lo) * levels)))
    for label, value, style in ((f'{RED_C:.0f}', RED_C, 'red'), (f'{AMBER_C:.0f}', AMBER_C, 'amber')):
        row = top + height - 1 - min(height - 1, int((value - lo) / (hi - lo) * height))
        canvas.put(row, left, f'{label:>4} ┤', style)
        canvas.put(row, left + 6, '·' * plot_w, style)
    for i, v in enumerate(cols):
        if v is None:
            continue
        full = level(v)
        style = 'red' if v >= RED_C else ('amber' if v >= AMBER_C else 'green')
        for r in range(height):
            fill = max(0, min(8, full - 8 * (height - 1 - r)))
            if fill:
                canvas.put(top + r, left + 6 + i, blocks[fill], style)
    canvas.put(top + height, left + 5, '└' + '─' * plot_w, 'dim')
    canvas.put(top + height, left, f'{lo:>4.0f}', 'dim')


def render(m, now, width, height):
    """The whole dashboard for monitor m at time now, as a Canvas."""
    c = Canvas(width, height)
    if width < 60 or height < 14:
        c.put(0, 0, 'Make the window at least 60 x 14, or use --plain.', 'amber')
        return c
    inner = width - 2
    p, age = m.last, (now - m.last_time) if m.last_time is not None else None
    lost = age is not None and age >= LOST_S
    stale = age is not None and age >= STALE_S
    # The frame, with the title and the stream's details.
    parts = [f'{m.group}:{m.port}'] + ([f'session {p["session"]:08x}'] if p else []) + [clock(now)]
    right = ' ' + ' · '.join(parts) + ' '
    while len(parts) > 1 and 28 + len(right) + 2 > width:  # too narrow: drop the session, then the clock
        parts.pop(1 if len(parts) == 3 else -1)
        right = ' ' + ' · '.join(parts) + ' '
    top = '┌─ Caterham oil temperature ' + '─' * max(0, inner - 27 - len(right)) + right + '┐'
    c.put(0, 0, top[:width - 1] + '┐', 'frame')
    for r in range(1, height - 1):
        c.put(r, 0, '│', 'frame')
        c.put(r, width - 1, '│', 'frame')
    note = m.note if m.note and now - m.note_time < 10.0 else ''
    bottom = '└─ q quit ' + (f'─ {note} ' if note else '')
    c.put(height - 1, 0, (bottom + '─' * width)[:width - 1] + '┘', 'frame')
    c.put(0, 3, 'Caterham oil temperature', 'title')
    if p and p['demo']:
        c.put(1, 2, 'DEMO - SYNTHETIC DATA, NOT REAL'.center(inner - 2), 'banner')
    # The reading: what the gauge sent, in its colour; '--' with no temperature, or once the stream is lost.
    if p is None:
        text, unit, style, status_line = '--', '', 'grey', ('Waiting for the gauge...', 'grey')
    else:
        style = 'grey' if lost else status_style(p)
        if lost or (p['temp_c'] is None and p['calibrated']):
            text, unit = '--', '°C' if p['calibrated'] else 'Ω'
        elif p['temp_c'] is not None:
            text, unit = f'{p["temp_c"]:.1f}', '°C'
        elif p['resistance_ohms'] is not None:
            text, unit = f'{whole(p["resistance_ohms"])}', 'Ω'
        else:
            text, unit = '--', 'Ω'
        status_line = (status_text(p['status']), style)
    for r, row in enumerate(big_text(text)):
        c.put(2 + r, 3, row, style)
    c.put(3, 4 + len(big_text(text)[0]), unit, style)
    col = 46 if width >= 80 else 4 + len(big_text(text)[0]) + 4
    c.put(2, col, status_line[0], status_line[1])
    if p:
        alarm = 'none' if p['alarm'] == STATUS.index('NORMAL') else f'{status_text(p["alarm"])} (latched)'
        c.put(3, col, 'alarm  ', 'dim')
        c.put(3, col + 7, alarm, 'grey' if lost else 'red' if p['alarm'] == STATUS.index('CRITICAL TEMP') else
              ('amber' if p['alarm'] == STATUS.index('HIGH TEMPERATURE') else None))
        maxc = f'{">" if p["max_above_range"] else ""}{whole(p["max_c"])} °C' if p['max_c'] is not None else '--'
        c.put(4, col, 'max    ', 'dim')
        c.put(4, col + 7, maxc)
        c.put(5, col, 'trend  ', 'dim')
        c.put(5, col + 7, '--' if lost else trend_text(trend(m.history, now)))
    # Stale data: a warning, then the stream counted as lost.
    if lost:
        c.put(7, 2, f'STREAM LOST - last data {clock(m.last_time)}'.center(inner - 2), 'banner')
    # The chart, if there's room.
    row = 8
    chart_h = height - 15  # leaves the axis, a gap, three lines of figures and the frame
    if chart_h >= 3 and width >= 80:
        c.put(row, 3, 'last 5 min', 'dim')
        draw_chart(c, row, 15, width - 18, chart_h, m.history, now)
        row += chart_h + 2
    # Measurements, counters and logging.
    if p:
        def v(x, fmt, scale=1.0):
            return '--' if x is None else format(x * scale, fmt)
        gap = '   ' if width >= 80 else '  '
        c.put(row, 3, f'sender {v(p["resistance_ohms"], ".1f")} Ω{gap}Vref {v(p["ref_v"], ".3f")} V{gap}'
                      f'Vs {v(p["sender_v"], ".3f")} V{gap}supply {v(p["supply_v"], ".2f")} V{gap}'
                      f'{"ground" if width >= 80 else "gnd"} {v(p["ground_offset_v"], ".1f", 1000.0)} mV', 'dim')
    t = m.tracker
    total = t.received + t.lost
    loss = f'{100.0 * t.lost / total:.1f} %' if total else '0.0 %'
    counts = f'packets {t.received} · {m.rate(now):.1f}/s · lost {t.lost} ({loss}) · dup {t.duplicates} · bad {t.bad} · '
    c.put(row + 1, 3, counts)
    if age is None:
        c.put(row + 1, 3 + len(counts), 'no data yet', 'grey')
    elif stale:
        c.put(row + 1, 3 + len(counts), f'NO DATA {age:.0f} s', 'red' if lost else 'amber')
    else:
        c.put(row + 1, 3 + len(counts), f'last {age:.1f} s ago' if width >= 80 else f'{age:.1f} s')
    c.put(row + 2, 3, f'logging → {m.csv_name} ({m.csv_rows} rows)' if m.csv_name else 'not logging (--csv FILE to log)',
          'dim')
    for r in range(1, height - 1):  # last, so no long line can cover the frame
        c.put(r, width - 1, '│', 'frame')
    return c


STYLES_256 = {'green': 42, 'blue': 39, 'amber': 214, 'red': 203, 'grey': 244, 'dim': 248, 'frame': 240, 'title': 255}
STYLES_8 = {'green': 'GREEN', 'blue': 'BLUE', 'amber': 'YELLOW', 'red': 'RED', 'grey': 'WHITE', 'dim': 'WHITE',
            'frame': 'WHITE', 'title': 'WHITE'}


def _attributes():
    """curses attributes for each style, in colour where the terminal has it."""
    attrs = {name: 0 for name in list(STYLES_256) + ['banner']}
    if not curses.has_colors():
        attrs.update(red=curses.A_BOLD, amber=curses.A_BOLD, banner=curses.A_REVERSE | curses.A_BOLD,
                     title=curses.A_BOLD)
        return attrs
    curses.start_color()
    try:
        curses.use_default_colors()
        background = -1
    except curses.error:
        background = curses.COLOR_BLACK
    rich = curses.COLORS >= 256
    for i, name in enumerate(STYLES_256, start=1):
        colour = STYLES_256[name] if rich else getattr(curses, 'COLOR_' + STYLES_8[name])
        curses.init_pair(i, colour, background)
        attrs[name] = curses.color_pair(i)
    curses.init_pair(len(STYLES_256) + 1, curses.COLOR_WHITE, curses.COLOR_RED)
    attrs['banner'] = curses.color_pair(len(STYLES_256) + 1) | curses.A_BOLD
    for name in ('green', 'blue', 'amber', 'red', 'title'):
        attrs[name] |= curses.A_BOLD
    if not rich:
        attrs['grey'] |= curses.A_DIM
        attrs['dim'] |= curses.A_DIM
    return attrs


def _paint(screen, canvas, attrs):
    screen.erase()
    for r, line in enumerate(canvas.lines()):
        try:
            screen.addstr(r, 0, line)
        except curses.error:
            pass  # the bottom-right cell
    for r, col, n, style in canvas.spans:
        try:
            screen.addstr(r, col, canvas.lines()[r][col:col + n], attrs.get(style, 0))
        except curses.error:
            pass
    screen.refresh()


def run_dashboard(sock, m, on_packet):
    """The live dashboard until q or Ctrl-C. on_packet(p, now) logs each new packet."""
    def loop(screen):
        try:
            curses.curs_set(0)
        except curses.error:
            pass
        screen.nodelay(True)
        attrs = _attributes()
        sock.settimeout(0.05)
        drawn = 0.0
        while True:
            now = time.time()
            try:
                for _ in range(64):  # everything waiting, then draw
                    data, _ = sock.recvfrom(2048)
                    now = time.time()
                    p, _notes = m.receive(data, now)
                    if p:
                        on_packet(p, now)
            except (socket.timeout, BlockingIOError):
                pass
            key = screen.getch()
            if key in (ord('q'), ord('Q')):
                return
            if key == curses.KEY_RESIZE or now - drawn >= 0.25:
                height, width = screen.getmaxyx()
                _paint(screen, render(m, now, width, height), attrs)
                drawn = now
    locale.setlocale(locale.LC_ALL, '')  # curses draws the block and box characters in the locale's encoding
    curses.wrapper(loop)


def open_socket(group, port, iface):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('', port))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, socket.inet_aton(group) + socket.inet_aton(iface))
    return s


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--group', default='239.10.10.10', help='multicast group (menuconfig: Multicast group)')
    ap.add_argument('--port', type=int, default=5005, help='port (menuconfig: Multicast port)')
    ap.add_argument('--iface', default='0.0.0.0', help="this computer's address on the gauge's network, if needed")
    ap.add_argument('--csv', metavar='FILE', help='also write every packet to FILE')
    ap.add_argument('--plain', action='store_true', help='one line per packet instead of the dashboard')
    ap.add_argument('--quiet', action='store_true', help='no line per packet (plain mode, with --csv)')
    args = ap.parse_args(argv)
    dashboard = not args.plain and sys.stdout.isatty() and sys.stdin.isatty()
    if dashboard and curses is None:
        print('No curses in this Python (Windows): one line per packet instead.', file=sys.stderr)
        dashboard = False
    sock = open_socket(args.group, args.port, args.iface)
    m = Monitor(args.group, args.port, args.csv)
    start = time.time()
    out = open(args.csv, 'w', newline='', encoding='utf-8') if args.csv else None
    writer = csv.writer(out) if out else None
    if writer:
        writer.writerow(CSV_COLUMNS)

    def on_packet(p, now):
        if writer:
            writer.writerow(csv_row(p, now))
            out.flush()  # every row on disk at once: nothing lost if it's killed
            m.csv_rows += 1

    # A kill (or the laptop shutting down) stops it as Ctrl-C does: the terminal is restored, the summary prints and
    # the CSV is complete.
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt()))
    try:
        if dashboard:
            run_dashboard(sock, m, on_packet)
        else:
            print(f'Listening on {args.group}:{args.port} (Ctrl-C to stop)', file=sys.stderr, flush=True)
            while True:
                data, _ = sock.recvfrom(2048)
                now = time.time()
                p, notes = m.receive(data, now)
                for note in notes:
                    print(note if note.startswith('skipped') else f'-- {note}', file=sys.stderr, flush=True)
                if p is None:
                    continue
                if not args.quiet:
                    print(describe(p), flush=True)
                on_packet(p, now)
    except KeyboardInterrupt:
        pass
    finally:
        if out:
            out.close()
        t = m.tracker
        print(f'\n{t.received} received, {t.lost} lost, {t.duplicates} duplicates skipped, {t.bad} bad, '
              f'{t.restarts} restarts in {time.time() - start:.0f} s', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
