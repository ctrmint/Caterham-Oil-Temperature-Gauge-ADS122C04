#!/usr/bin/env python3
"""Listen to the gauge's UDP data stream (#33) and print it, or log it as CSV.

The packet format is docs/UDP_STREAM.md (version 1). Join the gauge's Wi-Fi network (OilGauge-XXXX), then run:

    python3 scripts/udp_listen.py                 # one line per packet
    python3 scripts/udp_listen.py --csv run1.csv  # also log every packet
"""
import argparse
import csv
import math
import signal
import socket
import struct
import sys
import time
import zlib

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
    """Counts packets, losses (gaps in the sequence) and gauge restarts (a new session). Never fills a gap."""
    def __init__(self):
        self.session = None
        self.next = None
        self.received = self.lost = self.bad = self.restarts = 0

    def feed(self, p):
        """Returns a list of notes about this packet: a restart, how many were lost before it, or out of order."""
        notes = []
        self.received += 1
        if p['session'] != self.session:
            if self.session is not None:
                self.restarts += 1
                notes.append(f'gauge restarted (session {p["session"]:08x})')
            self.session = p['session']
            self.next = p['sequence'] + 1
            return notes
        if p['sequence'] > self.next:
            gap = p['sequence'] - self.next
            self.lost += gap
            notes.append(f'{gap} packet{"s" if gap != 1 else ""} lost')
        elif p['sequence'] < self.next:
            notes.append('out of order')
        self.next = max(self.next, p['sequence'] + 1)
        return notes


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
    ap.add_argument('--quiet', action='store_true', help='no line per packet (with --csv)')
    args = ap.parse_args(argv)
    sock = open_socket(args.group, args.port, args.iface)
    tracker = Tracker()
    warned = set()
    start = time.time()
    out = open(args.csv, 'w', newline='', encoding='utf-8') if args.csv else None
    writer = csv.writer(out) if out else None
    if writer:
        writer.writerow(CSV_COLUMNS)
    print(f'Listening on {args.group}:{args.port} (Ctrl-C to stop)', file=sys.stderr, flush=True)
    # A kill (or the laptop shutting down) stops it as Ctrl-C does, so the summary prints and the CSV is complete.
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt()))
    try:
        while True:
            data, _ = sock.recvfrom(2048)
            now = time.time()
            try:
                p = decode(data)
            except ValueError as e:
                tracker.bad += 1
                if str(e) not in warned:
                    warned.add(str(e))
                    print(f'skipped a packet: {e}', file=sys.stderr, flush=True)
                continue
            for note in tracker.feed(p):
                print(f'-- {note}', file=sys.stderr, flush=True)
            if not args.quiet:
                print(describe(p), flush=True)
            if writer:
                writer.writerow(csv_row(p, now))
                out.flush()  # every row on disk at once: nothing lost if it's killed
    except KeyboardInterrupt:
        pass
    finally:
        if out:
            out.close()
        span = time.time() - start
        print(f'\n{tracker.received} received, {tracker.lost} lost, {tracker.bad} bad, {tracker.restarts} restarts '
              f'in {span:.0f} s', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
