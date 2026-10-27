# CR-2: Keep a latched alarm when the sender suddenly reads colder than the calibration table

- **Status:** active  (keep the docs/spec.md §3 index in step)
- **Issue:** [#7](https://github.com/ctrmint/Caterham-Oil-Temperature-Gauge/issues/7)
- **Motivation:**
  - Hot oil cannot cool from 130 °C or more to below the bottom of the calibration table within seconds. A sudden reading colder than the table, while an alarm is latched, is far more likely to be a wiring or sender fault than real cooling. For example, a partly open or leaking connector reads as high resistance.
  - **Measured on the current code** (demo table, 250 ms samples):
    - With Red latched at 139 °C, one raw 1500 Ω sample clears the alarm **immediately**.
    - The EMA filter, which averages resistance, then reports plausible but false in-table temperatures (61.5 → 45.3 → 35.3 → 28.6 → 23.4 °C) for 1.25 s before the status becomes `BelowRange`.
  - So the alarm is lost, and a wrong but plausible temperature is shown. Checking the *filtered* status is not enough: the fix must act on the raw sample before it reaches the filter.
- **Kind of work:** fix
- **Scope:**
  - May change:
    - `main/core/Temperature.cpp` and `main/core/Temperature.h`;
    - `main/Config.h` (one new constant, `kBelowRangeHoldMs = 10000`);
    - `tests/demo_temperature_test.cpp`;
    - the docs that describe alarm behaviour: `docs/spec.md` §2 (only at merge), `docs/VALIDATION.md` and `docs/CALIBRATION.md`.
  - Must not change:
    - `hardware/`;
    - `main/hal/` (the ADS1115 driver);
    - `main/display/` (display colours and layout);
    - how a reading is classified as a fault, `BelowRange` or in-table (`convertSample`);
    - the CR-1 tests (existing assertions must pass unmodified).
- **Requirements amended:** REQ-T3.
  - approved: PurpleSentinel, 2026-10-27, REQ-T3 amendment as worded below, with `kBelowRangeHoldMs` = 10 000 ms.
  - **Old:** "Escalation must be held for `kAlarmDebounceMs` (2500 ms), with separate Amber and Red timers. De-escalation is immediate but uses `kAlarmResetHysteresisC` (5 °C) of hysteresis, and Red steps down through Amber."
  - **New:** "Escalation must be held for `kAlarmDebounceMs` (2500 ms), with separate Amber and Red timers. De-escalation is immediate but uses `kAlarmResetHysteresisC` (5 °C) of hysteresis, and Red steps down through Amber. **Exception:** while an alarm is latched, raw samples colder than the calibration table do not update the filter, the latched alarm or the maximum. The status is `BelowRange` and no temperature is shown. If such samples continue for `kBelowRangeHoldMs` (10 000 ms), the state resets as for a sustained fault (REQ-F2): alarm `Normal`, filter re-seeded."
  - REQ-C3 is unchanged: `BelowRange` is still not a fault, and no number is shown.
- **Hardware impact:** none

## Acceptance criteria

These are host tests, run with the synthetic demo table, simulated 250 ms steps, and `kCold` = `forOhms(1500)` (colder than the 1100 Ω end of the table). `kBelowRangeHoldMs` is 10 000 ms. Tests use the constant, not the literal. "Latched" means `at(T)` sustained for at least 4000 ms from a fresh `TemperatureState`.

| ID | Requirement | Criterion (observable, testable) |
|----|-------------|----------------------------------|
| AC-1 | REQ-T3 (amended) | With Red latched at 139 °C, a run of `kCold` samples gives `alarm == Red`, `status == BelowRange` and `has_temperature == false` at **every** update less than `kBelowRangeHoldMs` after the first `kCold` sample. |
| AC-2 | REQ-T3 (amended) | Same as AC-1 with Amber latched at 120 °C: `alarm == Amber` throughout. |
| AC-3 | REQ-T3 (amended) | With Red latched at 139 °C, `kCold` for `kBelowRangeHoldMs − 250` ms, then a single `at(139)` sample reports `has_temperature`, `temperature_c` within ±0.5 °C of 139, and `alarm == Red`. This shows the filter and the alarm were untouched, with no new debounce. |
| AC-4 | REQ-T3 (amended), REQ-F2 | With Red latched, at the first `kCold` update at or after `kBelowRangeHoldMs`: `alarm == Normal` and `status == BelowRange`. They stay that way while `kCold` continues. |
| AC-5 | REQ-T3 | Normal cooling is unchanged. With Red latched, cooling through the table in 0.25 °C steps from 139 to 105 °C changes the alarm exactly twice, Red → Amber → Normal. All existing assertions in `tests/demo_temperature_test.cpp` pass unmodified. |
| AC-6 | REQ-C3 | With no alarm latched (fresh state), `kCold` samples give `status == BelowRange` and `alarm == Normal` at every update. No hold applies. |
| AC-7 | REQ-T5 | AC-1 and AC-4 give the same results when the hold window crosses `UINT32_MAX` (for example, latch starting at `0xFFFFE000`). |
| AC-8 | REQ-B2 | `./scripts/test_host.sh` passes. The normal and demo firmware builds succeed with no `main/` warnings. |

## Verification

- `./scripts/test_host.sh` passes.
- `/build` passes: the normal and demo builds with 0 `main/` warnings.
- **Mutation check.** Each of these must make a test fail:
  - removing the hold, so `BelowRange` drives `updateAlarm(Normal)` as before (AC-1, AC-2);
  - feeding held samples into the EMA (AC-3);
  - never ending the hold (AC-4);
  - a signed or additive hold-timer comparison (AC-7);
  - applying the hold when no alarm is latched (AC-6).
- **Hardware verification (human).** This needs a build with a verified calibration table: uncalibrated firmware never raises an alarm, and the demo build ignores the sensor. Use a decade box.
  1. Set a resistance above the red threshold until CRITICAL latches.
  2. Switch to a value above the table's coldest point (~1500 Ω for a table like the demo one).
  3. Check that the gauge shows `< min °C` with the alarm still red for `kBelowRangeHoldMs`, and never a plausible in-table temperature. Then check that it clears.
  4. Record the result in the CR-2 Progress log.

## Out of scope

- Changing how a reading is classified as a fault, `BelowRange` or in-table.
- **Sudden drops that land inside the table.** For example, a jump from 139 °C to a raw reading of 60 °C still clears the alarm through normal hysteresis, and still shows the filtered values on the way. That needs a rate-of-change plausibility rule, which would be a separate CR if wanted.
- Display changes, such as a dedicated "SENSOR CHECK" text during the hold. The display already shows `< min °C` and the latched alarm colour.

## Open questions

_All resolved by the maintainer (PurpleSentinel), 2026-10-27._

1. **Hold duration (`kBelowRangeHoldMs`).** Resolved: **10 000 ms**, the recommended value. Real oil takes minutes to cool from alarm temperatures, so 10 s cannot hide genuine cooling, and it rides out connector bounce. The alternative, 3000 ms to match `kFaultResetMs`, was not chosen.
2. **REQ-T3 amendment.** Resolved: **approved** as worded under *Requirements amended*. The new wording goes into `docs/spec.md` §2 when CR-2 merges.
