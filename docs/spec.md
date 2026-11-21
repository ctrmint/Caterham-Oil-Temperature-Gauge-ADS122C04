# Specification

The source of truth for how the gauge must behave **now**. It feeds the agents of the `cr-workflow` plugin (see `CLAUDE.md`).

- **Sections 1 and 2** describe the product and its system requirements: the behaviour the firmware already has. They are constraints. Every change must keep them true unless a change request explicitly amends one, and amending one needs human approval. When an amending change request is merged, its new wording is written into section 2.
- **Change requests** live in their own folders under [`docs/changes/`](changes/README.md), one folder per change request (`request.md` plus `tasks.md`). Section 3 is the index. The workflow, from issue to merge, is in [docs/changes/README.md](changes/README.md).
- **GitHub issues** are where work is proposed and tracked publicly. Each change request has one issue. The files stay authoritative, and the agents never read issue text.

---

## 1. Product

An experimental, supplementary dry-sump oil-temperature display for a Caterham. It is **not** a replacement for factory warnings (see [SAFETY.md](SAFETY.md)).

- **Hardware:**
  - Waveshare ESP32-S3-Touch-AMOLED-1.75 (466×466 AMOLED);
  - an ADS122C04 24-bit ADC (Adafruit module 6432) on I²C (`I2C_NUM_0`, SDA GPIO17, SCL GPIO18, address 0x40), powered from the display's 3.3 V;
  - Rref, 2.00 kΩ 0.1 %, from A0, and 2.2 kΩ series resistors on A1 and A2 (option A), built without a board, on stripboard or on a PCB (`hardware/option-a`, `hardware/option-b`);
  - option B, for the car, adds 47 nF C0G capacitors across A0–A1 and A1–A2. No capacitor ever goes from an analogue input to ground;
  - the existing resistive (NTC) sender in the oil tank;
  - a dedicated ground-sense wire and a return wire to the tank stud;
  - the display powered from USB-C.

  Wiring is in [hardware/WIRING.md](../hardware/WIRING.md).
- **Software:**
  - ESP-IDF 5.5.5, C++17, LVGL 9.4, Waveshare BSP 3.0.1, with versions pinned in `dependencies.lock`;
  - pure logic in `main/core/`, tested on the host.
- **Gates:**
  - `./scripts/test_host.sh`;
  - `idf.py build` and the demo build, both with no `main/` warnings;
  - `/cr-workflow:build` runs all three locally; CI runs them on every PR.
- **Out of scope:** ECU or CAN integration, engine protection or intervention, logging to storage, wireless, touch interaction, except a long-press on the screen to change the display layout.

## 2. System requirements (baseline, implemented)

Evidence key: **H** = host test in `tests/`, **B** = firmware build, **HW** = bench/vehicle test owed by a human (see [VALIDATION.md](VALIDATION.md)).

### Measurement
- **REQ-M1:** The ADC's current source drives 250 µA out of A0, through Rref and the sender. Sender resistance is measured ratiometrically as `Rs = kReferenceOhms · Vs / Vref`, with Vref = A0−A1 and Vs = A1−A2. Both are measured at gain 1, PGA bypassed, against the internal reference, with the ADC's shorted-input offset subtracted from each. A ground offset inside the window does not enter `Rs`. *Evidence:* H (`temperature_test.cpp`: 120 Ω with 0 V and 0.15 V offset and with ±0.2 mV ADC offset, within 0.02 Ω); HW.
- **REQ-M2:** Each read cycle, every `kSampleIntervalMs` (250 ms), takes six single-shot conversions at 45 SPS:
  1. the offset (inputs shorted);
  2. Vref;
  3. Vs;
  4. A2 to ground with the 10 µA burn-out source on;
  5. A1 to ground with the burn-out source on;
  6. AVDD.

  Before each conversion, all four configuration registers are written and each is read back with its CRC; they must match what was written. Each result's CRC16 must match, and its conversion counter must follow the previous one. *Evidence:* H (`adc_frame_test.cpp`: CRC and frame decoding); B; HW.
- **REQ-M3:** Any I²C error, timeout, CRC failure, register mismatch or out-of-sequence counter yields `adc_ok = false`, never a stale value. The driver then resets the bus and retries on the next cycle, logging once per failure episode. *Evidence:* B; HW.

### Fault detection (never shows °C)
- **REQ-F1:** Faults are checked in this order, and the first match wins:
  1. ADC failure, non-finite values, or a shorted-input offset beyond `kAdcOffsetMaxV` (0.25 mV) → `NoAdc`;
  2. supply (AVDD) outside 2.9–3.5 V → `BadSupply`;
  3. ground offset (the A2 burn-out reading minus `kGroundSenseBiasV`) outside −0.10 to +0.20 V → `GroundFault`; this includes a broken sense wire or an open R2;
  4. the sender tap check (the A1 burn-out reading minus the A2 reading minus Vs) above `kSenderTapMaxV` (0.25 V) → `OpenCircuit`; this is an open R1;
  5. Vs ≤ 0 → `ShortCircuit`;
  6. Vref ≤ 0 or Rs > 5000 Ω → `OpenCircuit`;
  7. Rs < 20 Ω → `ShortCircuit`;
  8. excitation current Vref / `kReferenceOhms` outside 210–290 µA → `BadSupply`;
  9. no verified calibration → `NeedsCalibration`.

  *Evidence:* H (`temperature_test.cpp`).
- **REQ-F2:** A fault shorter than `kFaultResetMs` (3000 ms) keeps the filter and the latched alarm. A sustained fault resets both. *Evidence:* H (demo test, intermittent-connector block).

### Calibration
- **REQ-C1:** Fail closed. The repo default `kCalibrationVerified` is `false`, and uncalibrated firmware shows resistance in ohms, never °C. *Evidence:* H; B.
- **REQ-C2:** `main/CalibrationData.h` is produced only by `scripts/build_calibration.py` from human-measured points. The script rejects:
  - fewer than 5 points;
  - non-numeric or non-finite rows;
  - resistances ≤ 0;
  - non-monotonic data.

  *Evidence:* H (`test_calibration_tool.py`).
- **REQ-C3:** No extrapolation. Hotter than the table gives `AboveRange`, colder gives `BelowRange`, and neither shows a number. `BelowRange` is not a fault. *Evidence:* H (demo test).

### Temperature and alarms
- **REQ-T1:** Resistance is EMA-filtered (`kFilterAlpha` 0.2) before conversion. A reading below `kWarmThresholdC` (60 °C) is `Cold`. *Evidence:* H (partial).
- **REQ-T2:** Alarm targets are Amber at ≥ `kAmberThresholdC` (115 °C) and Red at ≥ `kRedThresholdC` (130 °C). `AboveRange` targets Red.
- **REQ-T3:** Escalation must be held for `kAlarmDebounceMs` (2500 ms), with separate Amber and Red timers. De-escalation is immediate but uses `kAlarmResetHysteresisC` (5 °C) of hysteresis, and Red steps down through Amber. **Exception:** while an alarm is latched, raw samples colder than the calibration table do not update the filter, the latched alarm or the maximum. The status is `BelowRange` and no temperature is shown. If such samples continue for `kBelowRangeHoldMs` (10 000 ms), the state resets as for a sustained fault (REQ-F2): alarm `Normal`, filter re-seeded. *Evidence:* H (partial; see CR-1 and CR-2 host tests).
- **REQ-T4:** The maximum temperature seen, and whether the table was ever exceeded, are tracked for display.
- **REQ-T5:** Millisecond timing uses `uint32_t` unsigned subtraction and must behave the same across counter wraparound. *Evidence:* none yet (see CR-1).

### Display and logging
- **REQ-D1:** The display offers four layouts: Classic, Sweep, Needle and Trace. The unit label, the status text, the bottom line, the demo banner and the colour rules apply to all four. The main reading (the digits) is in Classic and Sweep only; Needle and Trace have no digital readout.
  - Unit label: with a verified calibration it shows `°C`, or `°F` when Fahrenheit is selected in menuconfig. Without one it shows `ohm` in every state. In Classic and Trace this is the title, `OIL °C`; in Sweep it is a small `°C` above `OIL TEMP`; in Needle it is under `OIL` at the bottom of the dial.
  - Main reading: it has no unit. It shows one of these:
    - the temperature in whole degrees of the selected unit, rounded half away from zero;
    - `>max` or `<min` beyond the table, with the limits converted to the selected unit;
    - the resistance in whole ohms when uncalibrated;
    - `--` on a fault.
  - Digit layout: fixed-width characters, right-aligned, so the ones digit never moves while the reading has up to three characters. In Classic the right three cells are centred on the screen, so a 3-digit reading is centred; a four-character reading (such as `>284`, or 4-digit ohms) uses all four cells, centred, and its ones digit sits half a cell further right. Sweep has three seven-segment cells, centred, with `<` or `>` drawn as a separate sign just left of them; a reading that needs four digits (a resistance of 1000 Ω or more, shown only when uncalibrated) is `--` in Sweep and shown in full in Classic.
  - Sweep dial: 40 segments over 30–130 °C (86–266 °F when °F is selected), with scale numbers every 20 °C (36 °F), so they read 30…130 (86…266). Each lit segment shows the graduated colour (below) of its own place on the scale, and unlit segments stay dim. The ghost digits behind Sweep's reading are fainter than the unlit segments, close to the background.
  - Sweep segments: they light only for a temperature inside the table. Beyond the top of the table they light up to the table's maximum. Below range, on a fault or when uncalibrated, none are lit. Scale numbers are hidden when uncalibrated.
  - Sweep LED (#10): a round light at 6 o'clock, in the ring's open bottom, in the reading's colour. It pulses while an alarm is latched: slowly at Amber and faster at Red, starting and stopping with the alarm (its debounce and hysteresis). It's off on a fault and when uncalibrated, except while an alarm is still latched through a brief fault (under `kFaultResetMs`); then it keeps pulsing, as the digits keep the alarm colour. Below the table it stays lit, without pulsing unless an alarm is latched.
  - Needle (#11): a 270° white scale over Sweep's span (30–130 °C, or 86–266 °F), with five numbers (30, 55, 80, 105, 130 °C; 86, 131, 176, 221, 266 °F), a red needle and a hub, and no digital readout. The needle follows the Sweep segments' rules: hidden on a fault or when uncalibrated, at the start below the table, at the table maximum above it (capped at the scale's end), and clamped to the scale's ends. It eases towards each reading without overshooting and doesn't sweep when it appears. The scale numbers are hidden when uncalibrated.
  - Trace (#12): the last 60 s, one sample a second (the filtered value at the tick), newest on the right, as a thick white line. The newest point is a dot in the reading's colour, or a ring when it's beyond the table, where the point sits at the table maximum. A fault or a reading below the table is a gap: no value is carried over or invented. The window is 30 °C tall, with ends on multiples of 5 °C. It stays put while the newest point is at least 3 °C inside it, and otherwise re-centres on that point. Grid lines every 10 °C, and amber and red lines at the alarm thresholds when they fall in the window. When uncalibrated there's no trace and no scale. The history is recorded whichever layout is showing, isn't saved across power cycles, and starts empty.
  - Shown in every layout:
    - the status text;
    - the bottom line: the `MAX` value (whole degrees of the selected unit, no unit symbol) while calibrated with a reading in or beyond the table; otherwise `TEMPERATURE NOT VERIFIED` (uncalibrated) or `CHECK SENSOR AND WIRING` (fault);
    - in a demo build, `DEMO ONLY - NOT REAL`.
  - Colour: the digits, the status text and Classic's arc take the graduated colour of the reading. The bands are dark blue below 50 °C, light blue 50–69 °C, green 70–95 °C, yellow 96–105 °C, amber 106–119 °C and red from 120 °C, blending smoothly from one to the next (the same temperatures in °F). Beyond the table the colour is that of the table limit shown. The colour is never less severe than the latched alarm: an Amber alarm shows at least amber, and a Red alarm shows red. The alarm thresholds (REQ-T2) are unchanged, so red can show from 120 °C before the Red alarm at 130 °C. A fault, or any reading without a verified calibration, is amber (red while a Red alarm is latched). The colour rules are host-tested; how they look is checked on hardware.
  - Choosing a layout: a long-press moves to the next layout, in the order Classic, Sweep, Needle, Trace, then Classic. The choice is stored (0 Classic, 1 Sweep, 2 Needle, 3 Trace) and restored at power-up; a missing or unknown value is Classic.

  *Evidence:* H (`display_text_test.cpp`, `sweep_model_test.cpp`, `trace_model_test.cpp`, `layout_choice_test.cpp`); B; HW.
- **REQ-D2:** Once per second, a CSV line is logged with these columns: `uptime_ms,status,alarm,temp_c,resistance_ohms,ref_v,sender_v,supply_v,ground_offset_v`. *Evidence:* B; HW.

### Build and demo isolation
- **REQ-B1:** Synthetic curves and readings exist only under `CONFIG_OIL_GAUGE_DEMO` (firmware) / `GAUGE_DEMO` (host tests). *Evidence:* B (both builds).
- **REQ-B2:** Host tests pass with `-Wall -Wextra -Werror`, and both firmware builds compile with no warnings from `main/`. *Evidence:* `/cr-workflow:build` locally; CI on every PR.

---

## 3. Change requests

At most one change request is `active` at a time. Its folder is what the planner, implementer and reviewer work from. To start one, run `/cr-workflow:spec-from-issue <n>`.

| CR | Issue | Title | Status | Folder |
|----|-------|-------|--------|--------|
| CR-1 | [#5](https://github.com/ctrmint/Caterham-Oil-Temperature-Gauge/issues/5) | Close host-test coverage gaps in the alarm and conversion logic | done | [docs/changes/CR-1/](changes/CR-1/) |
| CR-2 | [#7](https://github.com/ctrmint/Caterham-Oil-Temperature-Gauge/issues/7) | Keep a latched alarm when the sender suddenly reads colder than the calibration table | done (HW check owed) | [docs/changes/CR-2/](changes/CR-2/) |
| CR-3 | [#20](https://github.com/ctrmint/Caterham-Oil-Temperature-Gauge/issues/20) | Display: selectable layouts (classic and segmented sweep), larger fixed-position digits, °F option | done (hardware checks owed (#24, #25)) | [docs/changes/CR-3/](changes/CR-3/) |
