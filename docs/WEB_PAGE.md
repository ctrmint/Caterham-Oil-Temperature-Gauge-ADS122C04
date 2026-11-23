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
```

- The columns between `uptime_ms` and `ground_offset_v` are the serial log's (REQ-D2), with the same values and decimals. `temp_c` is empty whenever the gauge had no temperature.
- **`phone_time`:** the gauge has no clock, so the page stamps each row with the phone's time (UTC), worked back from `uptime_ms` for rows fetched late.
- **`session` and `sequence`:** a new session starts at each power-up, and `sequence` counts rows from 0. A jump in `sequence` is a gap: rows the gauge had already dropped (older than 8 hours) before the phone asked for them.

**The phone keeps its copy.** The page saves the session in the browser's storage as rows arrive (every 15 s, and when the page is hidden or closed), so a reload, a locked phone or a closed tab doesn't lose it.
- **Earlier sessions:** when the gauge restarts, the page starts a new session and lists the old one under **Earlier sessions**, with its start time, length and its own **Download** button. The last three sessions are kept.
- **Storage full:** a phone browser holds a few megabytes per site, about one full 8-hour session. When it's full, the oldest sessions are dropped first. If even the current one won't fit, the page says so in amber: download the CSV to keep it.

**Catching up.** The page asks for the rows after the last one it has. If the phone was locked or out of range, the next answer brings everything it missed, up to 600 rows at a time, and the page asks again at once until it has caught up.

## The requests

All are `GET`, with `Cache-Control: no-store`.

- **`/`:** the page, built into the firmware (`main/web/index.html`, about 32 KB).
- **`/scale`:** the gauge's thresholds and colours, so the page draws with the gauge's own rules:
  ```
  {"v":1,"unit":"C","warm":60.0,"amber":115.0,"red":130.0,"dial":[30.0,130.0],"labels":[30,50,70,90,110,130],
   "segments":["#2F5BEA", ... 40 colours, Sweep's segments in order ...],
   "gradient":["#2F5BEA", ... the reading colour every 2 °C from 0 to 160 °C, 81 colours ...]}
  ```
- **`/log?since=N`:** the gauge's status and up to 600 rows from row `N`:
  ```
  {"v":1,"session":"2161ab7c","first":0,"next":1843,"demo":false,"calibrated":true,
   "now":{"uptime":1843210,"status":9,"alarm":9,"temp":92.4,"ohms":262.1,"ref":0.4866,"sender":0.0655,
          "supply":3.3169,"ground":-0.0021,"colour":"#2ED158","led":"steady","lit":25,"max":94.0,"maxAbove":false,"warmup":"WARM IN 6:12"},
   "bands":[372000,1105000,0,0,4500],
   "rows":[[1840,1840250,9,9,92.4,262.1,0.4866,0.0655,3.3169,-0.0021], ...]}
  ```
  - `first` and `next` are the oldest row the gauge still has and the next it will write. Rows older than `first` have been dropped.
  - Each row is `[sequence, uptime_ms, status, alarm, temp_c, ohms, ref_v, sender_v, supply_v, ground_offset_v]`.
  - Status and alarm are the UDP stream's frozen codes (`docs/UDP_STREAM.md`).
  - `colour`, `led` and `lit` are the gauge's own: the reading colour, Sweep's LED (`off`, `steady`, `slow` for a latched HIGH alarm, `fast` for CRITICAL) and Sweep's lit-segment count.
  - `bands` is Track's time in milliseconds per band, in the order warming, normal, high, critical, no reading.
  - `since` must be a plain decimal number. Anything else (`abc`, `-1`, an overflow) is treated as 0, the oldest row.
  - **Fail closed:** every value that isn't finite is `null`. `temp` and `temp_c` are `null` unless a verified calibration gave a finite temperature, and `max` is `null` uncalibrated or before the first reading. `maxAbove` is true when the maximum went beyond the table, and `max` is then the table's top. The page never computes a temperature: it shows the gauge's.

## How it's built

- **`main/core/WebLog`** (host-tested in `tests/web_log_test.cpp`): the log's ring of fixed-size rows, numbered from 0, in a buffer the caller allocates once; the `since` parser; and the JSON, written into a caller's buffer with a size limit, `null` for every non-finite value.
- **`main/net/WebServer`:** `WebSource` holds the ring (28 800 rows, 8 hours, about 1 MB of PSRAM, allocated once at start-up) and the latest status, each behind a short spinlock. The main loop appends one row a second at the serial log's moment and copies the status at each sample. It never waits on the network. `WebServer` runs `esp_http_server` in its own task at the lowest priority, below the main loop, with at most 4 connections. It copies rows out 32 at a time under the lock into a buffer made once, then sends them in chunks.
- **`main/net/WifiNet`** starts the server while the link is up and the state includes the web page, and stops it otherwise. If it can't start, Wi-Fi reports `Failed` and the gauge carries on as without it.
- **`main/web/index.html`:** one self-contained file of HTML, CSS and plain JavaScript, embedded with `EMBED_TXTFILES`. Its pure logic (digits, segments, trend, warm-up text, freshness, row merging, CSV) is in a separate script block that `tests/test_web_page.py` runs under Node. The test also checks the page loads nothing from elsewhere and asks only for `/log` and `/scale`.

## Limits

- **Plain HTTP:** the page isn't encrypted. Anyone on the same network who knows the address can read it, but can't change anything. When the gauge hosts the network, that's only people with its WPA2 password.
- **8 hours:** the gauge keeps the last 8 hours. A phone that was away longer gets the rows that are left, with a gap in `sequence`.
- **Power-off:** the gauge's copy is lost at power-off. The phone's copy is kept.
