# Architecture

```text
   3V3 (H2-3) ──┬───────────────────────────────┐
                │                               │ VDD
                ├─────────── 1k ──► A0    ┌─────┴──────────┐
                │                         │    ADS1115     ├── SDA ◄──► GPIO17 (H2-6)
           [Rp 470 Ω]                     │  16-bit, 0x48  ├── SCL ◄─── GPIO18 (H2-7)
                │                         │  ±4.096 V PGA  ├── ALRT   (not used; leave open)
   SENDER_NODE  ├─────────── 1k ──► A3    │                │
                │                         └─────┬──────────┘
                │ signal wire                   │ GND ── H2-2
       [harness protection]               (every input: 100 nF to GND)
                │
   [Caterham tank-mounted resistive sender]
                │ sender body
          TANK GND stud ──── ground-sense wire ──┬── 1k ──► A1
                │                                └── 100k ── 3V3 (break detect)
                │
           bond strap ── chassis ── gauge supply ground

   ADS1115 differential readings          ESP32-S3 (Waveshare AMOLED 1.75)
   Vp = A0 − A3, Vs = A3 − A1    ──I²C──► [ADS1115 driver (hal)]
                                                  │
                                         [Rs = Rp × Vs / Vp]
                                         [open/short/rail/ground faults]
                                                  │
                                         [measured curve lookup]
                                         [EMA + warnings]
                                                  │
                                   [LVGL gauge: Classic / Sweep]
                                       [466x466 CO5300 AMOLED]
```

See [hardware/wiring.svg](../hardware/wiring.svg) for the full schematic.

`main/hal/Ads1115Source` owns acquisition: it talks to the ADS1115 over I²C port 0 (the Waveshare BSP uses port 1), takes the differential readings at the same gain, and hands voltages to the core. If a transfer fails it resets the bus and retries on the next cycle, so the gauge recovers when the module is reconnected. `main/core/` is **platform-independent** and unit-tested with `g++`; it turns voltages into sender resistance, handles sender fault states and interpolates a user-validated monotonically decreasing R/T calibration curve. `main/display/` exclusively handles Waveshare BSP/LVGL calls. It draws a frame built from host-tested core helpers (`DisplayText`, `SweepModel`, `LayoutChoice`), so the text, the lit-segment count, the graduated colours and the layout rules are tested without hardware. The two layouts (Classic and Sweep) are each built once and switched by hiding and showing; the long-press (at least 1 s) is detected in the LVGL task. `main.cpp` stores the layout in NVS only when it changes, and restores it at power-up (Classic on first boot, or if the stored choice is missing or can't be read). `main/main.cpp` schedules acquisition at 4 Hz, updates the display at 4 Hz and reports CSV-like serial diagnostics at 1 Hz.

The ADS1115 is used in single-shot mode at 128 samples/s. One measurement cycle is four interleaved Vp/Vs pairs plus one rail reading (nine conversions, about 80 ms), well inside the 250 ms sample interval. The core computes `Rs = Rp × Vs / Vp` and the tank ground offset as `supply − (Vp + Vs)`, which equals `V(A1)`.

## Measurement safety model

* ADS1115 not answering on I²C, or a conversion times out → `ADC UNAVAILABLE` and **no temperature**.
* Excitation rail (A0 single-ended) not within the configured 2.9–3.5 V → `SUPPLY FAULT`.
* Tank ground offset outside −0.10…+0.20 V, or the ground-sense wire broken (its 100 kΩ bias pulls A1 to 3.3 V) → `GROUND FAULT`, **no temperature**.
* `Vs ≈ 0` / sender resistance under 20 Ω → `SENSOR SHORT`.
* `Vp ≈ 0` (no current through the pull-up) → `SENSOR OPEN`. This no longer depends on the unreliable top end of the ESP32 ADC.
* No human-verified curve → `CALIBRATION REQUIRED` and ohms only.
* Colder than the first calibration point → `BELOW CAL RANGE`, shown as `<20` style text, with the unit in the label; not a fault and raises no alarm. If an alarm is already latched, it is held (status is `BelowRange`, no temperature shown) until `kBelowRangeHoldMs` (10 s) after the first such sample, then cleared. Fault samples in between neither pause nor restart that time.
* Hotter than the last calibration point → `ABOVE CAL RANGE`, shown as `>140` style text, with the unit in the label, and treated as **critical** (debounced Red). No extrapolation in either direction.
* Excess temperature sustained for the configured debounce (2.5 s) → warning/critical state. Alarms clear with 5 °C hysteresis, and Red steps down through Amber. The default alarm limits are **placeholders**.
* A fault shorter than 3 s keeps the filter and the latched alarm (shown in the alarm colour with the fault text), so a vibrating connector cannot keep restarting the alarm debounce. A longer fault resets them.

**Limitations:** ADC accuracy is not temperature-sender calibration. The ratiometric measurement removes rail, ADC-gain and ground-offset errors, but not the tolerance and drift of the 470 Ω pull-up or thermal lag in the tank. A grounded, resistive automotive probe is neither a thermocouple nor a digital I²C temperature sensor. This gauge does not independently measure oil pressure, perform engine shutdown, or replace factory alerts.

## Board portability

Because the measurement lives on the ADS1115, porting to another board only needs two free GPIOs for I²C and a 3.3 V rail. See [PORTING.md](PORTING.md) for adapting pin mapping and display HAL while leaving `main/core/` unchanged.

## Power / privacy

No wireless access points, internet client or network connections are started. No personal or vehicle telemetry is transmitted. There are no secrets or cloud dependencies. USB logs show measured resistance and optional calibrated temperature locally.
