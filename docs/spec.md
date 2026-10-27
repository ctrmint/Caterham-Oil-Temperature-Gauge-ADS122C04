# Specification

The source of truth for what the gauge must do. It feeds the agents in `.claude/agents/` (see the workflow in [tasks.md](tasks.md)).

- **Section 2: system requirements.** These describe the behaviour the firmware already has. They are constraints: every change must keep them true unless a change request explicitly amends one, and amending one needs human approval.
- **Section 3: active change request.** This is the only part the planner turns into tasks. There is one active change request at a time.
- To start new work, write a change request in section 3 using the template in section 4. Move the finished one to section 5.

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
  - `/build` runs all three.
- **Out of scope:** ECU or CAN integration, engine protection or intervention, logging to storage, wireless, touch interaction.

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
- **REQ-T3:** Escalation must be held for `kAlarmDebounceMs` (2500 ms), with separate Amber and Red timers. De-escalation is immediate but uses `kAlarmResetHysteresisC` (5 °C) of hysteresis, and Red steps down through Amber. *Evidence:* H (partial; see CR-1).
- **REQ-T4:** The maximum temperature seen, and whether the table was ever exceeded, are tracked for display.
- **REQ-T5:** Millisecond timing uses `uint32_t` unsigned subtraction and must behave the same across counter wraparound. *Evidence:* none yet (see CR-1).

### Display and logging
- **REQ-D1:** The display shows the temperature, `> max °C` / `< min °C`, ohms when uncalibrated, or `--` on a fault, together with the status text.
  - Colour precedence: Red alarm, then Amber alarm, then fault or AboveRange in amber, then BelowRange muted, otherwise green.
  - A demo build shows `DEMO ONLY - NOT REAL`.

  *Evidence:* B; HW.
- **REQ-D2:** Once per second, a CSV line is logged with these columns: `uptime_ms,status,alarm,temp_c,resistance_ohms,pullup_v,sender_v,supply_v,ground_offset_v`. *Evidence:* B; HW.

### Build and demo isolation
- **REQ-B1:** Synthetic curves and readings exist only under `CONFIG_OIL_GAUGE_DEMO` (firmware) / `GAUGE_DEMO` (host tests). *Evidence:* B (both builds).
- **REQ-B2:** Host tests pass with `-Wall -Wextra -Werror`, and both firmware builds compile with no warnings from `main/`. *Evidence:* `/build`.

---

## 3. Active change request

### CR-1: Close host-test coverage gaps in the alarm and conversion logic

- **Status:** active
- **Motivation:** The alarm logic is the most safety-relevant code that can be tested on the host. Several behaviours in REQ-T1, T3 and T5, and the calibration endpoints, have no direct test. A regression there would pass every current gate.
- **Kind of work:** characterisation tests only. The tests describe current, intended behaviour and are **expected to pass against the existing code**. They are proven meaningful by the reviewer's mutation check, not by failing first.
- **Scope:**
  - may change `tests/demo_temperature_test.cpp` and `tests/temperature_test.cpp`;
  - nothing under `main/`, `scripts/`, `hardware/` or build configuration changes.
- **If a test exposes a real defect:** do not fix it in this CR. Leave the test out, record the evidence in the task report, and escalate. The fix becomes its own change request.
- **Conventions:**
  - use the existing helpers (`forOhms`, `at`, `hold`, `kOpen`), plain `assert` and simulated millisecond timestamps;
  - use the synthetic demo table (20–140 °C) for calibrated behaviour;
  - one commented block per criterion, like the existing tests.

#### Acceptance criteria

| ID | Requirement | Criterion (all timestamps are simulated, 250 ms steps from a fresh `TemperatureState`) |
|----|-------------|------------------------------------------|
| AC-1 | REQ-T3 | A sustained reading at 120 °C (`at(120)`) leaves the alarm `Normal` at every update before 2500 ms, and the alarm and status are `Amber` from 2500 ms onward. |
| AC-2 | REQ-T3 | A sustained reading at 139 °C leaves the alarm `Normal` at 2250 ms and makes it `Red` at 2500 ms. |
| AC-3 | REQ-T3 | Once Amber is latched at 120 °C, a step to a sustained 139 °C keeps the alarm `Amber` at every update less than 2500 ms after the first update whose reported temperature is ≥ 130 °C, and makes it `Red` at every update from 2500 ms after that one. |
| AC-4 | REQ-T5 | AC-2 and the sustained-fault reset (REQ-F2) give the same results when the timestamps start at `0xFFFFF000` and wrap past `UINT32_MAX` during the test. |
| AC-5 | REQ-F2 | With Red latched, open-circuit samples held for `kFaultResetMs − 250` ms keep the alarm `Red`, and the next good 139 °C sample shows `Red` immediately, with no new debounce. |
| AC-6 | REQ-T1 | A sustained calibrated reading at 30 °C has status `Cold`, `has_temperature == true` and alarm `Normal`. |
| AC-7 | REQ-C3 | At exactly the first and last table resistances (1100 Ω, 47 Ω), `temperatureFromResistance` returns 20 °C and 140 °C (±0.01), and `calibrationSide` returns 0. |
| AC-8 | REQ-T4 | While the temperature rises to 139 °C and then cools to 105 °C, `maximumC()` never decreases, and finally equals the highest reported temperature (±0.01). |
| AC-9 | REQ-F1 | `statusText` returns a non-empty string other than `"UNKNOWN"` for every `Status` enumerator, and no two enumerators share a string. |
| AC-10 | REQ-B2 | `./scripts/test_host.sh` passes. `git diff --stat` shows changes only in the two allowed test files. |

#### Verification
- `./scripts/test_host.sh`: every suite passes.
- Reviewer mutation check: for each of AC-1 to AC-5, a one-line change in `main/core/Temperature.cpp` makes at least one new assertion fail. Suggested mutations:
  - for AC-1 and AC-2, change `>=config::kAlarmDebounceMs` to `>`, or remove the debounce;
  - for AC-3, share one escalation timer;
  - for AC-4, replace the unsigned subtraction with a signed comparison;
  - for AC-5, reset on every fault.
- No hardware verification is needed for this change request.

#### Out of scope
- Display rendering and ADS1115 driver tests: they need hardware or LVGL mocks, which would be a separate change request.
- Any change in behaviour, including the open question of whether `BelowRange` should clear a latched alarm immediately.

---

## 4. Change request template

```
### CR-<n>: <title>
- Status: draft | active | done | withdrawn
- Motivation: <why, with links to REQ IDs or field observations>
- Kind of work: feature | fix | tests | docs | hardware
- Scope: <files or areas that may change; what must not change>
- Requirements amended: <REQ IDs and new wording, or "none">  (needs human approval)
- Hardware impact: <circuit/BOM/wiring change, or "none">  (needs human approval)

#### Acceptance criteria
| ID | Requirement | Criterion (observable, testable) |
|----|-------------|----------------------------------|

#### Verification
- <host test / build command -> expected result>
- Hardware verification (human): <bench or vehicle steps, or "none">

#### Out of scope
- <deliberately excluded work>
```

## 5. Change history

| CR | Title | Status | Merged |
|----|-------|--------|--------|
| CR-1 | Close host-test coverage gaps in the alarm and conversion logic | active | |
