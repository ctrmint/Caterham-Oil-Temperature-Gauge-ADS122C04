# CR-1: Close host-test coverage gaps in the alarm and conversion logic

- **Status:** done (all four tasks passed review; see [tasks.md](tasks.md))
- **Issue:** [#5](https://github.com/ctrmint/Caterham-Oil-Temperature-Gauge/issues/5)
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

## Acceptance criteria

| ID | Requirement | Criterion (all timestamps are simulated, 250 ms steps from a fresh `TemperatureState`) |
|----|-------------|------------------------------------------|
| AC-1 | REQ-T3 | A sustained reading at 120 °C (`at(120)`) leaves the alarm `Normal` at every update before 2500 ms, and the alarm and status are `Amber` from 2500 ms onward. |
| AC-2 | REQ-T3 | A sustained reading at 139 °C leaves the alarm `Normal` at 2250 ms and makes it `Red` at 2500 ms. |
| AC-3 | REQ-T3 | Once Amber is latched at 120 °C, a step to a sustained 139 °C keeps the alarm `Amber` at every update less than 2500 ms after the first update whose reported temperature is ≥ 130 °C, and makes it `Red` at every update from 2500 ms after that one. |
| AC-4 | REQ-T5 | (a) The sustained-fault reset (REQ-F2) gives the same result when its `kFaultResetMs` window crosses `UINT32_MAX`: timestamps start at `0xFFFFF000`, AC-2 is reproduced, and then a sustained fault begins 3000 ms after the start. (b) AC-2's debounce gives the same result when it crosses the wrap itself: a fresh state with timestamps starting at `0xFFFFFC00`. *Amended 2026-10-27 after T2 review: (b) added.* |
| AC-5 | REQ-F2 | With Red latched at 139 °C, open-circuit samples held for `kFaultResetMs − 250` ms keep the alarm `Red`. The next good sample, at 120 °C, shows a **smoothed** temperature strictly between 130 °C and 139 °C (so the filter was kept, not re-seeded at 120), with alarm `Red` and no new debounce. *Amended 2026-10-27 after T2 review: the post-fault sample was 139 °C, which could not detect a filter reset.* |
| AC-6 | REQ-T1 | A sustained calibrated reading at 30 °C has status `Cold`, `has_temperature == true` and alarm `Normal`. |
| AC-7 | REQ-C3 | At exactly the first and last table resistances (1100 Ω, 47 Ω), `temperatureFromResistance` returns 20 °C and 140 °C (±0.01), and `calibrationSide` returns 0. |
| AC-8 | REQ-T4 | While the temperature rises to 139 °C and then cools to 105 °C, `maximumC()` never decreases, and finally equals the highest reported temperature (±0.01). |
| AC-9 | REQ-F1 | `statusText` returns a non-empty string other than `"UNKNOWN"` for every `Status` enumerator, and no two enumerators share a string. |
| AC-10 | REQ-B2 | `./scripts/test_host.sh` passes. `git diff --stat` shows changes only in the two allowed test files. |

## Verification
- `./scripts/test_host.sh`: every suite passes.
- Reviewer mutation check: for each of AC-1 to AC-5, a one-line change in `main/core/Temperature.cpp` makes at least one new assertion fail. Suggested mutations:
  - for AC-1 and AC-2, change `>=config::kAlarmDebounceMs` to `>`, or remove the debounce;
  - for AC-3, share one escalation timer;
  - for AC-4, replace the unsigned subtraction with a signed comparison;
  - for AC-5, reset on every fault, or re-seed the filter on every fault (`filter_ready_=false`);
  - for AC-4 (b), compare `ms>=red_since_+kAlarmDebounceMs` instead of using unsigned subtraction.
- No hardware verification is needed for this change request.

## Out of scope
- Display rendering and ADS1115 driver tests: they need hardware or LVGL mocks, which would be a separate change request.
- Any change in behaviour, including the open question of whether `BelowRange` should clear a latched alarm immediately.
