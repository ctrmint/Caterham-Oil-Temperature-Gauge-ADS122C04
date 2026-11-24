# UDP data stream (#33)

The gauge can send its live data over Wi-Fi as **UDP multicast packets**: one per sample, four a second. A laptop or Raspberry Pi on the same network can record or display them with a simple decoder, such as `scripts/udp_listen.py`, without asking for anything. The stream is **send-only**. The gauge never listens on the stream's port, so nothing can be sent back to it this way.

Phones aren't a target. They need special permissions to receive multicast. The web page (#32, `docs/WEB_PAGE.md`) is for phones.

## Turning it on

1. **The network:** copy `main/wifi_secrets.example.h` to `main/wifi_secrets.h` and fill in the network name (SSID, 1–32 characters) and password (WPA2, 8–63). Git ignores the file: never commit it.
2. **The stream:** `idf.py menuconfig` → **Caterham oil temperature gauge** → **Wi-Fi (#32, #33, #34)**:
   - **UDP data stream:** on.
   - **Multicast group** and **port:** `239.10.10.10` and `5005` unless you change them.
3. **Build:** rebuild and flash. A first build after creating the file picks it up by itself.
4. **Switch it on:** press the **BOOT** button (the top side button) until the panel at the top of the screen reads `Wifi - Web Off, UDP On`. The gauge looks for the network:
   - **Found:** it joins as a client. The panel shows `Joined <network>` and the gauge's address.
   - **Not found:** it hosts the network itself, with the same name and password, at `192.168.4.1`. The panel shows `Hosting <network>`.
   - **Found but not joinable** (a wrong password, say): `Wifi failed`, `Can't join <network>`.
   - **A joined network that drops:** it rejoins if it can, and hosts the network once it's gone.

   The choice is saved and restored at power-up. Pressing again turns Wi-Fi off (`Wifi Off`).
5. **Listen:** join the laptop to that network, then run:

   ```
   python3 scripts/udp_listen.py                      # a live dashboard in the terminal (q to quit)
   python3 scripts/udp_listen.py --csv run1.csv       # and log every packet
   python3 scripts/udp_listen.py --plain              # one line per packet instead
   python3 scripts/udp_listen.py --iface 192.168.4.2  # the laptop's address on that network, if it has several
   ```

   The options:

   | Option | Default | What it does |
   |---|---|---|
   | `--group` | `239.10.10.10` | the multicast group, as set in menuconfig |
   | `--port` | `5005` | the port, as set in menuconfig |
   | `--iface` | `0.0.0.0` (any) | the laptop's own address on the gauge's network, to choose the interface |
   | `--csv FILE` | none | also write every packet to `FILE` (below) |
   | `--plain` | off | one line per packet instead of the dashboard |
   | `--quiet` | off | in plain mode, no line per packet, only the notes and the summary (useful with `--csv`) |

   If the laptop's firewall blocks incoming UDP, nothing arrives (Wireshark still shows the packets). Allow the port, for example on Fedora: `sudo firewall-cmd --zone=<zone> --add-port=5005/udp`.

## The monitor

In a terminal, `udp_listen.py` shows a dashboard that updates in place, four times a second (#37):

```
┌─ Caterham oil temperature ── 239.10.10.10:5005 · session 8f3a21c4 · 14:32:07 ┐
│                                                                              │
│    ██     ██   ██████    ██████             HIGH TEMPERATURE                 │
│  ████   ████       ██    ██     °C          alarm  HIGH TEMPERATURE (latched)│
│    ██     ██       ██    ██████             max    118 °C                    │
│    ██     ██       ██    ██  ██             trend  STEADY                    │
│  ██████ ██████     ██ ██ ██████                                              │
│                                                                              │
│  last 5 min                                                                  │
│               130 ┤························································  │
│                                                                ▂▂▂▂▂▂▂▂▂▂▂   │
│               115 ┤············································███████████·  │
│                                                                ███████████   │
│               100 └────────────────────────────────────────────────────────  │
│                                                                              │
│  sender 118.9 Ω   Vref 0.500 V   Vs 0.030 V   supply 3.31 V   ground 13.0 mV │
│  packets 240 · 4.0/s · lost 0 (0.0 %) · dup 0 · bad 0 · last 0.2 s ago       │
│  logging → run1.csv (1203 rows)                                              │
└─ q quit ─────────────────────────────────────────────────────────────────────┘
```

- **The reading:** in large digits, coloured by the status the gauge sent: blue warming, green normal, amber high, red critical, and amber for faults and no calibration. The colour is never less severe than the latched alarm. It shows `--` without a temperature, and whole ohms (`Ω`) when uncalibrated. The laptop never works out a band or a temperature itself.
- **Beside it:** the status, the latched alarm, the session maximum (`>140` once it went above the table) and the trend. The trend follows Tape's rule: it's shown only when the last 30 s are all clean readings inside the table, with `STEADY` below 0.5 °C/min.
- **The chart:** the last 5 minutes, with dotted lines at the alarm thresholds (115 and 130 °C, the same as `main/Config.h`). A missing reading is a gap.
- **Below:** the measurement voltages, the packet counters, and the CSV file and its row count when logging.
- **The demo build:** a red `DEMO - SYNTHETIC DATA, NOT REAL` banner.
- **When packets stop:** after 2 s the status bar turns amber, `NO DATA 2 s`. After 10 s a red `STREAM LOST - last data HH:MM:SS` banner appears, and the reading becomes a grey `--`, so an old reading never looks live.
- **Window size:** below 80 × 24 the chart is dropped; below 60 × 14 the monitor asks for a bigger window.
- **Quitting:** `q`, Ctrl-C or a kill restores the terminal and prints a one-line summary: received, lost, duplicates skipped, bad and restarts.

**Plain mode** (`--plain`, or automatic when the output is piped or redirected, and on Windows, whose Python has no `curses`) prints one line per packet to standard output:

```
#1843  460.25 s  NORMAL  92.4 °C  262.1 ohm  MAX 94
#1844  460.50 s  SENSOR OPEN  --  21003002.0 ohm  alarm HIGH TEMPERATURE  MAX >140  DEMO
```

- **The fields:** `#` and the sequence number, the uptime in seconds, the status, the temperature (`--` without one), the sender's ohms (`-- ohm` when not finite).
- **Added when they apply:** `alarm` and the latched alarm when it isn't `NORMAL`; `MAX` and the session maximum in whole °C (`>` when it went beyond the table); `DEMO` from a demo build.
- **Notes**, on standard error:
  - `Listening on 239.10.10.10:5005 (Ctrl-C to stop)` at the start;
  - `-- 2 packets lost`, `-- out of order`, `-- gauge restarted (session 8f3a21c4)` and `-- duplicate #1843 skipped`;
  - `skipped a packet: bad CRC` (or `bad magic`, `unknown version 2`, `length 40, expected 56`), printed once per kind.

**The summary:** on quitting, in either mode, a line on standard error such as `1203 received, 0 lost, 1 duplicates skipped, 0 bad, 0 restarts in 301 s`.

**In both modes:**
- **Losses and restarts:** the monitor notes lost packets (gaps in the sequence) and gauge restarts (a new session).
- **Duplicates:** it skips a sequence number it has already seen, so each packet is printed and logged once. Wi-Fi and a router's multicast-to-unicast conversion can deliver one twice.
- **Late packets:** a packet counted as lost that arrives late comes off the lost count.
- **CSV:** `--csv` writes every packet as it arrives (below).

## The monitor's CSV

`--csv FILE` writes a header, then one row per packet kept. Duplicates and bad packets aren't written. Each row is flushed to disk as it's written, so nothing is lost if the monitor is killed.

```
uptime_ms,status,alarm,temp_c,resistance_ohms,ref_v,sender_v,supply_v,ground_offset_v,session,sequence,max_c,host_time
460250,NORMAL,NORMAL,92.4,262.1,0.4866,0.0655,3.3169,-0.0021,2161ab7c,1843,94.0,1795444327.123
460500,SENSOR OPEN,HIGH TEMPERATURE,,21003002.0,0.0002,2.1003,3.3169,-0.0021,2161ab7c,1844,140.0,1795444327.373
```

| Column | What it holds |
|---|---|
| `uptime_ms` … `ground_offset_v` | the serial log's columns (REQ-D2), with the same values and decimals: status and alarm in words, °C and Ω to 0.1, volts to 0.0001 |
| `session` | the session ID, 8 hex digits |
| `sequence` | the packet's sequence number: four a second, from 0 at power-up |
| `max_c` | the session maximum in °C to 0.1, or empty when there's none. When the maximum went beyond the table it's the table's top, and **this column doesn't mark that**: the plain line and the dashboard show it as `>140`. |
| `host_time` | the laptop's clock when the packet arrived, in seconds since 1970 (UTC), to the millisecond |

- **Empty values:** `temp_c` is empty unless the packet's has-temperature flag is set. Any other value that isn't finite is empty too. The serial log writes `NA` instead.
- **On a fault** the measured columns are diagnostics, never a reading. They hold what was measured before the fault was found, or 0. The second row above is from a demo build with the sender open, a short fault while the HIGH alarm stays latched: with no excitation current, the resistance works out huge.
- **Demo builds:** the file **has no demo column**. Name a file recorded from a demo build yourself. The dashboard and the plain lines say `DEMO`.
- **Units:** always °C.

## The network

The packets stay on that network (TTL 1). Wi-Fi doesn't resend multicast frames, so the odd packet can be lost. The sequence number shows it, and nothing is ever filled in. On a joined network, the router has to pass multicast to Wi-Fi clients; most home routers do.

## The packet, version 1

56 bytes, little-endian, at fixed offsets:

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 4 | magic | ASCII `OILG` |
| 4 | 1 | version | `1` |
| 5 | 1 | length | `56`, the whole packet |
| 6 | 2 | flags | see below |
| 8 | 4 | session | random per power-up |
| 12 | 4 | sequence | 0 at power-up, +1 per packet |
| 16 | 4 | uptime_ms | as in the serial log |
| 20 | 1 | status | status code (below) |
| 21 | 1 | alarm | the latched alarm, as a status code: 9, 10 or 11 |
| 22 | 2 | reserved | 0 |
| 24 | 4 | temp_c | float32, °C; **NaN unless flag bit 2 is set** |
| 28 | 4 | resistance_ohms | float32 |
| 32 | 4 | ref_v | float32 |
| 36 | 4 | sender_v | float32 |
| 40 | 4 | supply_v | float32 |
| 44 | 4 | ground_offset_v | float32 |
| 48 | 4 | max_c | float32, the session maximum; NaN when there's none |
| 52 | 4 | crc32 | CRC-32 (IEEE, as zlib's `crc32`) of bytes 0–51 |

In Python: `struct.Struct('<4sBBHIIIBBH7fI')`.

**An example:** the golden packet `normal` from `tests/data/udp_packets.txt`.

```
 0: 4f 49 4c 47 01 38 15 00   OILG, version 1, length 56, flags 0x0015 (calibrated, has temperature, max exists)
 8: c4 21 3a 8f 32 07 00 00   session 8f3a21c4, sequence 1842
16: da 05 07 00 09 09 00 00   uptime 460250 ms, status 9 NORMAL, alarm 9 NORMAL, reserved
24: cd cc b8 42 cd 0c 83 43   temp_c 92.4, resistance_ohms 262.1
32: 00 00 00 3f 5a 2f 86 3d   ref_v 0.5, sender_v 0.06552
40: 33 33 53 40 a6 9b 44 3c   supply_v 3.3, ground_offset_v 0.012
48: 00 00 bc 42 e6 c5 0e 4f   max_c 94.0, crc32 0x4f0ec5e6
```

The monitor prints it as `#1842  460.25 s  NORMAL  92.4 °C  262.1 ohm  MAX 94`.

**Flags:**

| Bit | Meaning |
|---|---|
| 0 | calibrated: a verified calibration is built in |
| 1 | demo build: synthetic data, **not real** |
| 2 | has temperature: `temp_c` is a reading |
| 3 | the maximum went above the table (`max_c` is then the table's maximum, shown as `>max`) |
| 4 | a maximum exists |

The other bits are 0.

**Status codes,** frozen for version 1 (a `static_assert` ties them to the firmware):

| Code | Status |
|---|---|
| 0 | `ADC UNAVAILABLE` |
| 1 | `SUPPLY FAULT` |
| 2 | `GROUND FAULT` |
| 3 | `SENSOR SHORT` |
| 4 | `SENSOR OPEN` |
| 5 | `CALIBRATION REQUIRED` |
| 6 | `BELOW CAL RANGE` |
| 7 | `ABOVE CAL RANGE` |
| 8 | `OIL WARMING` |
| 9 | `NORMAL` |
| 10 | `HIGH TEMPERATURE` |
| 11 | `CRITICAL TEMP` |

**Fail closed:**
- The values are the same as in the serial CSV log (REQ-D2).
- With no temperature (a fault, beyond the table, or uncalibrated), bit 2 is clear and `temp_c` is NaN. A decoder must not show a temperature unless bit 2 is set.
- Every non-finite value is sent as the quiet NaN `0x7FC00000`.
- With no verified calibration there's no temperature and no maximum.
- The encoder never estimates or fills a value.

Any change to the layout or the codes raises the version. A decoder should skip versions it doesn't know.

## Tests

- **The golden packets:** `tests/data/udp_packets.txt` holds a set covering normal, warming, high, critical, below and above the table, a fault, the demo flag, the first packet, and uncalibrated.
- **The encoder:** `tests/udp_packet_test.cpp` re-encodes each golden packet and compares the bytes.
- **The decoder:** `tests/test_udp_listen.py` packs the same fields independently with `struct` and `zlib`, from the table above, and checks they match. It then decodes them with `scripts/udp_listen.py`. So the firmware, the decoder and this document can't drift apart.
