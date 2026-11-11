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
  - ADS1115 16-bit ADC on I²C (`I2C_NUM_0`, SDA GPIO17, SCL GPIO18, address 0x48);
  - 470 Ω 0.1 % pull-up;
  - the existing resistive (NTC) sender in the oil tank;
  - a dedicated ground-sense wire to the tank stud.

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
- **REQ-M1:** Sender resistance is measured ratiometrically as `Rs = Rp · Vs / Vp`, with Vp = A0−A3 and Vs = A3−A1, both at ±4.096 V gain. A ground offset inside the window cancels out of `Rs`. *Evidence:* H (`temperature_test.cpp`: 120 Ω with 0 V and 0.15 V offset, within 0.02 Ω); HW.
- **REQ-M2:** Each read cycle takes 4 interleaved Vp/Vs pairs plus one A0 rail reading, every `kSampleIntervalMs` (250 ms). The ADS1115 config register is read back and must match what was written. *Evidence:* B; HW.
- **REQ-M3:** Any I²C error, timeout or read-back mismatch yields `adc_ok = false`. The driver then resets the bus and retries on the next cycle, logging once per failure episode. *Evidence:* B; HW.

### Fault detection (never shows °C)
- **REQ-F1:** Faults are checked in this order, and the first match wins:
  1. ADC failure or non-finite values → `NoAdc`;
  2. supply outside 2.9–3.5 V → `BadSupply`;
  3. ground offset outside −0.10 to +0.20 V → `GroundFault`; this includes a broken sense wire;
  4. Vs ≤ 0 or Rs < 20 Ω → `ShortCircuit`;
  5. Vp ≤ 0 or Rs > 5000 Ω → `OpenCircuit`;
  6. no verified calibration → `NeedsCalibration`.

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
- **REQ-D1:** The display offers two layouts, Classic and Sweep, and every rule below applies to both.
  - Unit label: with a verified calibration it shows `°C`, or `°F` when Fahrenheit is selected in menuconfig. Without one it shows `ohm` in every state. In Classic this is the title, `OIL °C`; in Sweep it is a small `°C` above `OIL TEMP`.
  - Main reading: it has no unit. It shows one of these:
    - the temperature in whole degrees of the selected unit, rounded half away from zero;
    - `>max` or `<min` beyond the table, with the limits converted to the selected unit;
    - the resistance in whole ohms when uncalibrated;
    - `--` on a fault.
  - Digit layout: fixed-width characters, right-aligned, so the ones digit never moves.
  - Sweep segments: they light only for a temperature inside the table. Beyond the top of the table they light up to the table's maximum. Below range, on a fault or when uncalibrated, none are lit. Scale numbers are hidden when uncalibrated.
  - Shown in both layouts:
    - the status text;
    - the bottom line: the `MAX` value (whole degrees of the selected unit, no unit symbol) while calibrated with a reading in or beyond the table; otherwise `TEMPERATURE NOT VERIFIED` (uncalibrated) or `CHECK SENSOR AND WIRING` (fault);
    - in a demo build, `DEMO ONLY - NOT REAL`.
  - Colour precedence (unchanged; it applies to the digits and the lit segments): Red alarm, then Amber alarm, then fault or AboveRange in amber, then BelowRange muted, otherwise green. It is checked on hardware only, not by a host test.
  - Choosing a layout: a long-press changes the layout. The choice is stored and restored at power-up.

  *Evidence:* H (`display_text_test.cpp`, `sweep_model_test.cpp`, `layout_choice_test.cpp`); B; HW.
- **REQ-D2:** Once per second, a CSV line is logged with these columns: `uptime_ms,status,alarm,temp_c,resistance_ohms,pullup_v,sender_v,supply_v,ground_offset_v`. *Evidence:* B; HW.

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
| CR-3 | [#20](https://github.com/ctrmint/Caterham-Oil-Temperature-Gauge/issues/20) | Display: selectable layouts (classic and segmented sweep), larger fixed-position digits, °F option | active | [docs/changes/CR-3/](changes/CR-3/) |
