# Web log page (#32)

The gauge can serve a **read-only web page** to a phone (or any browser) on its Wi-Fi network. It shows the live reading on a copy of the Sweep face, a chart of the session, the time in each temperature band, and a **Download CSV** button. Nothing needs installing, and the page loads nothing from the internet, so it works in a paddock with no signal.

The page can't change anything on the gauge. The server answers three `GET` requests and returns 404 for anything else.

## Turning it on

1. **The network:** copy `main/wifi_secrets.example.h` to `main/wifi_secrets.h` and fill in the network name (SSID, 1–32 characters) and password (WPA2, 8–63). Git ignores the file: never commit it. It's the same network as the UDP stream's (`docs/UDP_STREAM.md`).
2. **The page:** `idf.py menuconfig` → **Caterham oil temperature gauge** → **Wi-Fi (#32, #33, #34)** → **Web log page:** on. The UDP stream is a separate option, and either or both can be built in.
3. **Build:** rebuild and flash.
4. **Switch it on:** press the **BOOT** button (the top side button) until the panel at the top of the screen reads `Wifi - Web On, UDP Off` (or `Wifi - Web On, UDP On` for both). The gauge looks for the network:
   - **Found:** it joins as a client. The panel shows `Joined <network>` and the gauge's address, for example `10.81.1.65`.
   - **Not found:** it hosts the network itself, with the same name and password. The panel shows `Hosting <network>` and `192.168.4.1`.
5. **Open it:** join the phone to that network and open `http://<the address on the panel>/`, for example `http://192.168.4.1/` when the gauge is hosting. The phone may warn that the network has no internet; choose to stay connected.

The gauge keeps its copy of the log from power-up, whether Wi-Fi is on or not, so turning the page on later still gives the whole session (up to 8 hours).

## What the page shows

- **The dial:** the gauge's Sweep face. The gauge sends how many of its 40 segments are lit, their colours, the scale labels (none when uncalibrated) and the reading's colour, so the page draws exactly what Sweep shows. The LED is the gauge's own Sweep LED state: it pulses slowly for a latched HIGH TEMPERATURE alarm and quickly for CRITICAL TEMP.
- **Updated _n_ s ago:** how long since the gauge last answered. After 3 s with no answer the line turns amber. After 10 s it turns red, the dial greys out and shows `--` and `NO DATA`, so a stalled page is never mistaken for a live one.
- **Highest, Trend, Warm-up, Running:** the session maximum (`>` when it went beyond the calibration table), the rate of change (Tape's rule: the last 30 rows, all clean readings in the table and consecutive, otherwise `--`), the warm-up clock, and the time since power-up.
- **This session:** a chart of every row the phone has, in the gauge's temperature colours, with the High and Critical thresholds dashed. Rows with no temperature (a fault, beyond the table, uncalibrated) leave a gap, never a joined-up line. **10 min**, **1 hour** and **All** set the span. Touch or hover for the time and value of a row.
- **Time in each band:** Track's times since power-up: below 60°, 60–115°, 115–130°, 130° and above, and no reading.
- **Sensor readings:** the sender's ohms, the reference and sender voltages, the supply and the ground offset, and the session ID.
- **Download CSV:** see below.
- **Demo builds** show `DEMO ONLY - NOT REAL` across the top, and the CSV's name starts `DEMO-`.
- **Uncalibrated builds** show ohms and `CALIBRATION REQUIRED`, never degrees, like the gauge.

The page uses the gauge's unit (°C or °F from menuconfig). The JSON and the CSV are always in °C.

## Downloading the session

**Download CSV** saves every row the phone has for the current session, as `oil-temp-<session>-<YYYYMMDD>-<HHMM>.csv`, named with the phone's date and time at the session's first row:

```
phone_time,uptime_ms,status,alarm,temp_c,resistance_ohms,ref_v,sender_v,supply_v,ground_offset_v,session,sequence
2026-11-23T14:32:07.000Z,460250,NORMAL,NORMAL,92.4,262.1,0.4866,0.0655,3.3169,-0.0021,2161ab7c,460
2026-11-23T14:32:08.000Z,461250,SENSOR OPEN,NORMAL,,21003002.0,0.0002,2.1003,3.3169,-0.0021,2161ab7c,461
```

| Column | What it holds |
|---|---|
| `phone_time` | The phone's time when the row arrived, in UTC (ISO 8601, to the millisecond). The gauge has no clock. For rows fetched late, it's worked back from `uptime_ms`. |
| `uptime_ms` | Milliseconds since the gauge powered up, as in the serial log. |
| `status`, `alarm` | The gauge's words, as in the serial log (`NORMAL`, `HIGH TEMPERATURE`, ...). `alarm` is the latched alarm: `NORMAL`, `HIGH TEMPERATURE` or `CRITICAL TEMP`. |
| `temp_c` | °C to 0.1. **Empty** whenever the gauge had no temperature: a fault, beyond the table, or uncalibrated. |
| `resistance_ohms` | Ω to 0.1. Empty when it wasn't finite. |
| `ref_v`, `sender_v`, `supply_v`, `ground_offset_v` | Volts to 0.0001. Empty when not finite. |
| `session` | The session ID, 8 hex digits, new at each power-up. |
| `sequence` | The row number, from 0 at power-up, one per second. A jump is a gap: rows the gauge had already dropped (older than 8 hours) before the phone asked for them. |

- **On a fault** (the second row, the sender open) the measured columns are diagnostics, never a reading: what was measured before the fault was found, or 0. With the sender open there's no excitation current, so the resistance works out huge.
- **Against the serial log (REQ-D2):** the values and decimals are the same. The serial log writes `NA` where this file leaves `temp_c` empty.
- **Against the UDP stream:** the session ID is the same, but `sequence` here counts 1 s rows. The stream's sequence counts packets, four a second.
- **Demo builds:** the file's name starts `DEMO-`.
- **Units:** the file is always in °C, whatever the gauge's display unit.

**The phone keeps its copy.** The page saves the session in the browser's storage (`localStorage`, for the gauge's address) as rows arrive. It saves every 15 s when there are new rows, when the page is hidden or closed, before a download, and when the gauge restarts. A reload, a locked phone or a closed tab doesn't lose it.
- **What's stored:**
  - `oil.sessions`: the session IDs kept, oldest first (at most three).
  - `oil.<session>`: each session as `{"since": N, "demo": false, "rows": [[sequence, uptime_ms, status, alarm, temp_c, ohms, ref_v, sender_v, supply_v, ground_offset_v, phone_time], ...]}`.
  - `since` is the next row to ask for, and `phone_time` is in milliseconds since 1970.
  - Nothing else is stored, and nothing leaves the phone except the CSV you download.
- **Per address:** the storage belongs to the address the page was opened from. A session saved at `http://10.81.1.65/` doesn't show at `http://192.168.4.1/`, and the other way round.
- **Earlier sessions:** when the gauge restarts, the page starts a new session and lists the old one under **Earlier sessions**, with its start time, length and its own **Download** button. The last three sessions are kept.
- **Storage full:** a phone browser holds a few megabytes per site, about one full 8-hour session. When it's full, the oldest sessions are dropped first. If even the current one won't fit, the page says so in amber: download the CSV to keep it.

**Catching up.** The page asks for the rows after the last one it has. If the phone was locked or out of range, the next answer brings everything it missed, up to 600 rows at a time, and the page asks again at once until it has caught up.

## The requests

The server answers three `GET` requests, all with `Cache-Control: no-store`:

| Request | Answer |
|---|---|
| `GET /` | the page, `text/html; charset=utf-8`, with a `Content-Length` (about 32 KB, `main/web/index.html` built into the firmware) |
| `GET /scale` | `application/json`, with a `Content-Length` (about 1.3 KB): the thresholds, the dial and the colours |
| `GET /log?since=N` | `application/json`, sent with `Transfer-Encoding: chunked`: the status and up to 600 rows from row `N` |

A query string on `/` or `/scale` is ignored.

Everything else gets an error with a short text body, and changes nothing:

| Answer | When |
|---|---|
| `404 Not Found` | any other path, such as `/favicon.ico` |
| `405 Method Not Allowed` | any method but `GET` on those three paths: `POST`, `HEAD`, `PUT`, ... |
| `414 URI Too Long` | a URI over 512 characters |
| `431 Request Header Fields Too Large` | request headers over 2 KB |
| `500 Internal Server Error` | an answer that couldn't be built. Not expected: the answers are well inside their buffers. |

- **Connections:** at most four at once. A fifth closes the one used least recently.
- **Dropped connections:** if a connection drops part-way through `/log`, the gauge stops sending, and the page's next request starts again from its last row.
- **Requests the page makes:** `/` once, `/scale` once, then `/log` once a second (at once again while catching up).

### `/scale`

The gauge's thresholds and colours, so the page draws with the gauge's own rules. This is a calibrated build in °C:

```
{"v":1,"unit":"C","warm":60.0,"amber":115.0,"red":130.0,"dial":[30.0,130.0],"labels":[30,50,70,90,110,130],
 "segments":["#2F5BEA","#2F5BEA", ... 40 colours, Sweep's segments in order ...],
 "gradient":["#2F5BEA","#2F5BEA", ... the reading colour every 2 °C from 0 to 160 °C, 81 colours ...]}
```

| Field | What it holds |
|---|---|
| `v` | the format's version, `1` |
| `unit` | the display unit from menuconfig, `"C"` or `"F"` |
| `warm`, `amber`, `red` | the warm threshold and the two alarm thresholds, always in °C (`main/Config.h`) |
| `dial` | Sweep's scale, in the display unit: 30–130 °C, or 86–266 °F |
| `labels` | Sweep's scale labels in the display unit, **empty when uncalibrated**, as on the gauge |
| `segments` | the colours of Sweep's 40 segments, in order from the lower left |
| `gradient` | the gauge's reading colour every 2 °C from 0 to 160 °C (81 colours), for the chart's line |

### `/log?since=N`

The gauge's status and up to 600 rows from row `N`. This example is what the host code writes for a calibrated build at 92.4 °C:

```
{"v":1,"session":"2161ab7c","first":0,"next":1843,"demo":false,"calibrated":true,
 "now":{"uptime":1843210,"status":9,"alarm":9,"temp":92.4,"ohms":262.1,"ref":0.4866,"sender":0.0655,
        "supply":3.3169,"ground":-0.0021,"colour":"#5ED551","led":"steady","lit":24,"max":94.0,"maxAbove":false,
        "warmup":"WARM IN 6:12"},
 "bands":[372000,1105000,0,0,4500],
 "rows":[[1840,1840250,9,9,92.4,262.1,0.4866,0.0655,3.3169,-0.0021], ...]}
```

| Field | What it holds |
|---|---|
| `v` | the format's version, `1` |
| `session` | the session ID, 8 hex digits, random at each power-up; the same as the UDP stream's |
| `first`, `next` | the oldest row the gauge still has, and the next it will write. Rows older than `first` have been dropped. |
| `demo` | `true` in a demo build: synthetic data, **not real** |
| `calibrated` | `true` when a verified calibration is built in |
| `now.uptime` | milliseconds since power-up, at the latest sample |
| `now.status`, `now.alarm` | the UDP stream's frozen status codes (`docs/UDP_STREAM.md`); the alarm is 9, 10 or 11 |
| `now.temp` | °C to 0.1, or `null` without a temperature |
| `now.ohms` | Ω to 0.1 |
| `now.ref`, `now.sender`, `now.supply`, `now.ground` | volts to 0.0001 |
| `now.colour` | the gauge's reading colour, `#RRGGBB` |
| `now.led` | Sweep's LED: `off`, `steady`, `slow` (a latched HIGH alarm) or `fast` (CRITICAL) |
| `now.lit` | how many of Sweep's 40 segments are lit |
| `now.max` | the session maximum in °C to 0.1, or `null` |
| `now.maxAbove` | `true` when the maximum went beyond the table; `max` is then the table's top |
| `now.warmup` | the warm-up text: `""` uncalibrated, the time so far while warming (`2:35`), `WARM IN 6:12` or `WARM AT START` |
| `bands` | Track's time per band in milliseconds: warming, normal, high, critical, no reading |
| `rows` | up to 600 rows, each `[sequence, uptime_ms, status, alarm, temp_c, ohms, ref_v, sender_v, supply_v, ground_offset_v]`, with the same decimals as `now` |

- **`since`:** a plain decimal number, with `since=` at the start of the query or after `&`. Anything else (`abc`, `-1`, `12x`, a number over 32 bits, or a query over 63 characters) is treated as 0, the oldest row.
- **Row numbers:** they count 1 s rows. They aren't the UDP stream's sequence numbers, which count packets four a second.
- **Fail closed:**
  - Every value that isn't finite is `null`.
  - `temp` and `temp_c` are `null` unless a verified calibration gave a finite temperature.
  - `max` is `null` uncalibrated or before the first reading.
  - Uncalibrated, `led` is `off` and `lit` is 0.
  - The page never computes a temperature, a band, a colour or a segment count: it shows the gauge's.

## How it's built

- **`main/core/WebLog`** (host-tested in `tests/web_log_test.cpp`): the log's ring of fixed-size rows, numbered from 0, in a buffer the caller allocates once; the `since` parser; and the JSON, written into a caller's buffer with a size limit, `null` for every non-finite value.
- **`main/net/WebServer`:** `WebSource` holds the ring (28 800 rows, 8 hours, about 1 MB of PSRAM, allocated once at start-up) and the latest status, each behind a short spinlock. The main loop appends one row a second at the serial log's moment and copies the status at each sample. It never waits on the network. `WebServer` runs `esp_http_server` in its own task at the lowest priority, below the main loop, with at most 4 connections. It copies rows out 32 at a time under the lock into a buffer made once, then sends them in chunks.
- **`main/net/WifiNet`** starts the server while the link is up and the state includes the web page, and stops it otherwise. If it can't start, Wi-Fi reports `Failed` and the gauge carries on as without it.
- **`main/web/index.html`:** one self-contained file of HTML, CSS and plain JavaScript, embedded with `EMBED_TXTFILES`. Its pure logic (digits, trend, warm-up text, freshness, row merging, CSV) is in a separate script block that `tests/test_web_page.py` runs under Node. The test also checks the page loads nothing from elsewhere and asks only for `/log` and `/scale`.

## Limits

- **Supplementary:** like the gauge itself, the page never replaces the factory warnings, and the gauge's own status and LED are the alarm. Don't watch a phone while driving. See `docs/SAFETY.md`.
- **Plain HTTP:** the page isn't encrypted. Anyone on the same network who knows the address can read it, but can't change anything. When the gauge hosts the network, that's only people with its WPA2 password.
- **8 hours:** the gauge keeps the last 8 hours. A phone that was away longer gets the rows that are left, with a gap in `sequence`.
- **Power-off:** the gauge's copy is lost at power-off. The phone's copy is kept.
