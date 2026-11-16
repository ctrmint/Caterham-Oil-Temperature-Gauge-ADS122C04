# Architecture

```text
                     ┌─────────────────────────┐
 display 3.3V ────── │ VIN                     │
 display G ───┬───── │ GND      ADS122C04      │
 GPIO17 ◄─────┼────► │ SDA      24-bit, 0x40   │
 GPIO18 ──────┼────► │ SCL      2.048 V ref    │
              │      │   IDAC1 250 µA ──► A0   ├──[Rref 2.00 kΩ]──┐
              │      │                    A1   ├──[R1 2.2 kΩ]─────┴── SENDER ── signal wire ──┐
              │      │                    A2   ├──[R2 2.2 kΩ]──────── SENSE ─── sense wire ─┐ │
              │      └─────────────────────────┘                                            │ │
              │                                                      sender (Rs) ───────────┼─┘
              │                                                           │ body            │
              └── RETURN ── return wire ──────────────────────── TANK GND stud ◄────────────┘
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
                                   [LVGL gauge: Classic / Sweep]
                                       [466x466 CO5300 AMOLED]
```

See [hardware/wiring.svg](../hardware/wiring.svg) for the full schematic and [hardware/WIRING.md](../hardware/WIRING.md) for the build. There is no interface board: Rref, R1 and R2 solder to the module's pins, and the harness lands on a 3-way terminal strip. The display runs from its own USB-C socket. A USB supply needn't share the car's ground, so the harness carries a dedicated return wire from the tank bond stud to the gauge's ground, beside the ground-sense wire.

**Code layout:**
- **`main/hal/Ads122c04Source`** owns acquisition. It talks to the ADS122C04 over I²C port 0 (the Waveshare BSP uses port 1).
  - Before each conversion it writes all four configuration registers and reads each back with a CRC.
  - It checks every result's CRC16 and conversion counter, and hands voltages to the core.
  - If anything fails it resets the bus and retries on the next cycle, so the gauge recovers when the module is reconnected.
- **`main/core/` is platform-independent** and unit-tested with `g++`.
  - `AdcFrame.h` decodes and CRC-checks the ADC's I²C frames.
  - `Temperature` turns voltages into sender resistance, handles sender fault states and interpolates a user-validated, monotonically decreasing R/T calibration curve.
- **`main/display/`** exclusively handles Waveshare BSP/LVGL calls.
  - It draws a frame built from host-tested core helpers (`DisplayText`, `SweepModel`, `LayoutChoice`), so the text, the lit-segment count, the graduated colours and the layout rules are tested without hardware.
  - The two layouts (Classic and Sweep) are each built once and switched by hiding and showing.
  - The long-press (at least 1 s) is detected in the LVGL task.
- **`main/main.cpp`:**
  - stores the layout in NVS only when it changes, and restores it at power-up (Classic on first boot, or if the stored choice is missing or can't be read);
  - schedules acquisition at 4 Hz, updates the display at 4 Hz and reports CSV-like serial diagnostics at 1 Hz.

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

**Limitations:** ADC accuracy is not temperature-sender calibration. The ratiometric measurement removes the current source's error, the ADC's reference and gain errors and the ground offset, but not the tolerance and drift of Rref or thermal lag in the tank. There are no clamp diodes or TVS: the 2 kΩ and 2.2 kΩ series resistors keep a harness short to battery voltage within the ADC's input rating, but that is not load-dump qualification. A grounded, resistive automotive probe is neither a thermocouple nor a digital I²C temperature sensor. This gauge does not independently measure oil pressure, perform engine shutdown, or replace factory alerts.

## Board portability

Because the measurement lives on the ADS122C04, porting to another board only needs two free GPIOs for I²C and a 3.3 V rail. See [PORTING.md](PORTING.md) for adapting pin mapping and display HAL while leaving `main/core/` unchanged.

## Power / privacy

No wireless access points, internet client or network connections are started. No personal or vehicle telemetry is transmitted. There are no secrets or cloud dependencies. USB logs show measured resistance and optional calibrated temperature locally.
