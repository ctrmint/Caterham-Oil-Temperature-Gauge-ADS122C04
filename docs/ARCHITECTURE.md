# Architecture

```text
                     ┌─────────────────────────┐
 display 3.3V ────── │ VIN                     │
 display G ───┬───── │ GND      ADS122C04      │
 GPIO17 ◄─────┼────► │ SDA      24-bit, 0x40   │
 GPIO18 ──────┼────► │ SCL      2.048 V ref    │
              │      │   IDAC1 250 µA ──► A0   ├──┬──[Rref 2.00 kΩ]──┐
              │      │                         │ ═╧═ C1 47 nF        │
              │      │                         │ ═╤═ (option B)      │
              │      │                    A1   ├──┼──[R1 2.2 kΩ]─────┴── SENDER ── signal wire ──┐
              │      │                         │ ═╧═ C2 47 nF                                    │
              │      │                         │ ═╤═ (option B)                                  │
              │      │                    A2   ├──┴──[R2 2.2 kΩ]──────── SENSE ─── sense wire ─┐ │
              │      └─────────────────────────┘                                               │ │
              │                                              sender (Rs) ──────────────────────┼─┘
              │                                                   │ body                       │
              └── RETURN ── return wire ───────────────── TANK GND stud ◄──────────────────────┘
                                                                  │
                                                         bond strap ── chassis

   ADS122C04, six conversions per cycle      ESP32-S3 (Waveshare AMOLED 1.75)
   offset, Vref = A0 − A1, Vs = A1 − A2,  ──I²C──► [ADS122C04 driver (hal)]
   A2 and A1 with burn-out, AVDD                          │
                                         [Rs = Rref × Vs / Vref]
                                         [ADC/supply/excitation/ground/tap/open/short faults]
                                                          │
                                         [measured curve lookup]
                                         [EMA + warnings]
                                                          │
                                   [LVGL gauge: Classic / Sweep / Needle / Trace / Tape / OEM / White / Night / Nixie / Heritage / Track / Warm-up]
                                       [466x466 CO5300 AMOLED]
```

See [hardware/wiring.svg](../hardware/wiring.svg) for the full schematic and [hardware/WIRING.md](../hardware/WIRING.md) for the build. No interface board is needed: Rref, R1 and R2 can solder to the module's pins, with the harness on a 3-way terminal strip. Each option also has a checked stripboard layout and a two-layer PCB (`hardware/option-a/`, `hardware/option-b/`). Both keep ground copper away from A1, A2, SENDER and SENSE, because a short from those to ground gives a believable wrong reading. **Option B**, recommended for the car, adds C1 and C2 across the pin pairs. They filter interference near the ADC's 256 kHz sampling rate. There's never a capacitor from a pin to ground, so a shorted capacitor gives a fault, not a reading. The firmware is the same for both options. The display runs from its own USB-C socket. A USB supply needn't share the car's ground, so the harness carries a dedicated return wire from the tank bond stud to the gauge's ground, beside the ground-sense wire.

**Code layout:**
- **`main/hal/Ads122c04Source`** owns acquisition. It talks to the ADS122C04 over I²C port 0 (the Waveshare BSP uses port 1).
  - Before each conversion it writes all four configuration registers and reads each back with a CRC.
  - It checks every result's CRC16 and conversion counter, and hands voltages to the core.
  - If anything fails it resets the bus and retries on the next cycle, so the gauge recovers when the module is reconnected.
- **`main/core/` is platform-independent** and unit-tested with `g++`.
  - `AdcFrame.h` decodes and CRC-checks the ADC's I²C frames.
  - `Temperature` turns voltages into sender resistance, handles sender fault states and interpolates a user-validated, monotonically decreasing R/T calibration curve.
- **`main/display/`** exclusively handles Waveshare BSP/LVGL calls.
  - It draws a frame built from host-tested core helpers (`DisplayText`, `SweepModel`, `TraceModel`, `TapeModel`, `OemModel`, `WhiteModel`, `NightModel`, `NixieModel`, `HeritageModel`, `TrackModel`, `WarmupModel`, `WifiChoice`, `LayoutChoice`), so the text, the lit-segment count, the graduated colours, the LED and needle states, the trace's samples and window, the tape's position and the rate of change, the OEM, White and Heritage dials' needles, OEM's wordmark, White's status window, Night's quiet state and Heritage's legend (one shared "all is well" rule), Night's heartbeat, Nixie's tubes and numeral strokes, Track's band times, bars and time text, Warm-up's ring and clock, and the layout rules are tested without hardware.
  - The twelve layouts (Classic, Sweep, Needle, Trace, Tape, OEM, White, Night, Nixie, Heritage, Track and Warm-up) are each built once and switched by hiding and showing.
  - The LED's pulse (and OEM's symbol's) and the needles' and tape's easing run on LVGL timers in the LVGL task, created once at start-up, so nothing is allocated per frame.
  - The long-press (at least 1 s) is detected in the LVGL task.
  - **Draw buffers:** `GaugeDisplay` starts the display as the BSP's `bsp_display_start()` does, with one change. LVGL's two draw buffers (20 lines each, about 37 KB together) are in internal DMA-capable RAM, taken once at start-up before Wi-Fi.
    - **Why:** the BSP puts them in PSRAM, which the SPI bus can't send from. The SPI driver then borrowed a ~46 KB internal block for every frame. Once Wi-Fi and the web server had fragmented internal RAM, no such block was left: every frame failed with `ESP_ERR_NO_MEM`, and the screen and touch froze until a reboot (found 2026-11-23).
    - **Now:** drawing never allocates. The serial log reports the internal RAM left at start-up, when Wi-Fi comes up and when the web page starts.
  - The Wi-Fi panel (#34) is one object on LVGL's top layer, above every layout, in a band across the top that each layout keeps clear of its status lines and LED.
- **`main/net/WifiNet`** (#33, #34) runs Wi-Fi and the UDP data stream in one task at the lowest priority, below the main loop, so it can never delay a reading.
  - Started, it scans for the network in `main/wifi_secrets.h` (git-ignored): it joins it as a client if it's there, and hosts it (same name and password, 192.168.4.1) if it isn't. A joined network that drops is rejoined, or hosted once it's gone.
  - The main loop only sets the requested state and copies the newest sample into a one-slot mailbox under a spinlock; neither blocks or allocates.
  - The task starts and stops Wi-Fi, opens a non-blocking multicast socket (TTL 1) on the current interface, encodes each sample with `UdpPacket` and sends it, dropping a packet rather than wait.
  - It's send-only. If anything fails to start, it reports `Failed` and the gauge carries on as without Wi-Fi.
  - While the link is up and the state includes the web page (#32), it runs `WebServer`, and stops it otherwise.
- **`main/net/WebServer`** (#32) serves the read-only web page (`docs/WEB_PAGE.md`).
  - `WebSource` holds the page's log, a ring of 8 hours of 1 s rows allocated once in PSRAM, and the latest status, each behind a short spinlock. The rows and JSON come from `main/core/WebLog`, which is host-tested.
  - `esp_http_server` runs in its own task at the lowest priority, with at most 4 connections. It answers `GET /`, `/scale` and `/log?since=N` only, copying rows out 32 at a time under the lock into a buffer made once and sending them in chunks.
  - The page (`main/web/index.html`) is embedded in the firmware. It polls `/log` once a second and keeps its own copy of the session in the browser.
- **`main/main.cpp`:**
  - stores the layout in NVS only when it changes, and restores it at power-up (Classic on first boot, or if the stored choice is missing or can't be read);
  - schedules acquisition at 4 Hz, updates the display at 4 Hz and reports CSV-like serial diagnostics at 1 Hz.
  - feeds every reading to the trace's history (`TraceModel`), which keeps one sample a second, to Track's time per band (`TrackModel`) and to the warm-up clock (`WarmupModel`), whichever layout is showing;
  - reads the BOOT button every loop (`WifiChoice`'s debounced short press), moves the Wi-Fi state on, and hands each sample to the network task while the stream runs (#33, #34);
  - when the web page is built in, copies the status to it at each sample and appends a row at each 1 s log moment, from power-up (#32).

**The measurement cycle.** The ADS122C04 runs single-shot at 45 samples/s, gain 1, with the PGA bypassed, so inputs are valid down to 0.1 V below its GND. Its internal current source drives 250 µA out of A0, through Rref and then the sender. One measurement cycle is six conversions, about 150 ms, well inside the 250 ms sample interval:
1. the ADC's offset, with the inputs shorted;
2. `Vref = A0 − A1`;
3. `Vs = A1 − A2`;
4. A2 to ground, with the 10 µA burn-out source on;
5. A1 to ground, with the burn-out source on;
6. AVDD.

The core computes `Rs = Rref × Vs / Vref` after subtracting the offset. It takes the tank ground offset as the A2 reading minus the burn-out source's drop across R2.

## Measurement safety model

* ADS122C04 not answering on I²C; a register, CRC or counter mismatch; or a conversion timing out → `ADC UNAVAILABLE` and **no temperature**. So is an ADC offset (inputs shorted) beyond 0.25 mV.
* AVDD (the display's 3.3 V) outside the configured 2.9–3.5 V → `SUPPLY FAULT`. So is an excitation current `Vref / Rref` outside 210–290 µA, which is how a missing or wrong Rref shows up.
* Tank ground offset outside −0.10…+0.20 V, or the ground-sense wire or R2 open (the burn-out source pulls A2 up) → `GROUND FAULT`, **no temperature**.
* An open R1 / A1 tap (the burn-out source pulls A1 above the sender node) → `SENSOR OPEN`, never a plausible reading.
* `Vs ≈ 0` / sender resistance under 20 Ω → `SENSOR SHORT`.
* `Vref ≈ 0` (no current through Rref), or over 5 kΩ → `SENSOR OPEN`.
* No human-verified curve → `CALIBRATION REQUIRED` and ohms only.
* Colder than the first calibration point → `BELOW CAL RANGE`, shown as `<20` style text, with the unit in the label; not a fault and raises no alarm. If an alarm is already latched, it is held (status is `BelowRange`, no temperature shown) until `kBelowRangeHoldMs` (10 s) after the first such sample, then cleared. Fault samples in between neither pause nor restart that time.
* Hotter than the last calibration point → `ABOVE CAL RANGE`, shown as `>140` style text, with the unit in the label, and treated as **critical** (debounced Red). No extrapolation in either direction.
* Excess temperature sustained for the configured debounce (2.5 s) → warning/critical state. Alarms clear with 5 °C hysteresis, and Red steps down through Amber. The default alarm limits are **placeholders**.
* A fault shorter than 3 s keeps the filter and the latched alarm (shown in the alarm colour with the fault text), so a vibrating connector cannot keep restarting the alarm debounce. A longer fault resets them.

**Limitations:** ADC accuracy is not temperature-sender calibration. The ratiometric measurement removes the current source's error, the ADC's reference and gain errors and the ground offset, but not the tolerance and drift of Rref or thermal lag in the tank. Option A has no input filter, so interference near the ADC's 256 kHz sampling rate can reach the reading unseen; option B filters the difference between the inputs, but common-mode interference is left to the ADC's own rejection. There are no clamp diodes or TVS: the 2 kΩ and 2.2 kΩ series resistors keep a harness short to battery voltage within the ADC's input rating, but that is not load-dump qualification. A grounded, resistive automotive probe is neither a thermocouple nor a digital I²C temperature sensor. This gauge does not independently measure oil pressure, perform engine shutdown, or replace factory alerts.

## Board portability

Because the measurement lives on the ADS122C04, porting to another board only needs two free GPIOs for I²C and a 3.3 V rail. See [PORTING.md](PORTING.md) for adapting pin mapping and display HAL while leaving `main/core/` unchanged.

## Power / privacy

No wireless access points, internet client or network connections are started. No personal or vehicle telemetry is transmitted. There are no secrets or cloud dependencies. USB logs show measured resistance and optional calibrated temperature locally.
