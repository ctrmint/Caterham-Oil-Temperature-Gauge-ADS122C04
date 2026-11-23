# UDP data stream (#33)

The gauge can send its live data over Wi-Fi as **UDP multicast packets**: one per sample, four a second. A laptop or Raspberry Pi on the same network can record or display them with a simple decoder, such as `scripts/udp_listen.py`, without asking for anything. The stream is **send-only**. The gauge never listens on the stream's port, so nothing can be sent back to it this way.

Phones aren't a target. They need special permissions to receive multicast. The web page in #32 is for phones.

## Turning it on

1. **The network:** copy `main/wifi_secrets.example.h` to `main/wifi_secrets.h` and fill in the network name (SSID, 1–32 characters) and password (WPA2, 8–63). Git ignores the file: never commit it.
2. **The stream:** `idf.py menuconfig` → **Caterham oil temperature gauge** → **Wi-Fi (#33, #34)**:
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
   python3 scripts/udp_listen.py                      # one line per packet
   python3 scripts/udp_listen.py --csv run1.csv       # and log every packet
   python3 scripts/udp_listen.py --iface 192.168.4.2  # the laptop's address on that network, if it has several
   ```

   Each packet prints as one line, such as `#1843  460.25 s  NORMAL  92.4 °C  262.1 ohm  MAX 94`. It shows `--` where there's no temperature, and `DEMO` on every line from a demo build. The decoder notes lost packets (gaps in the sequence) and gauge restarts (a new session). Ctrl-C prints a summary.

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
