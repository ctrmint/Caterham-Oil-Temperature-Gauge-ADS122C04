# Caterham oil-temperature gauge

ESP-IDF 5.5.5 C++17 firmware for a Waveshare ESP32-S3-Touch-AMOLED-1.75 (LVGL 9). It reads a dry-sump oil-temperature sender through an ADS1115 over I²C. This is a **safety-relevant instrument in a car**: a wrong but plausible temperature is worse than no reading. The firmware is supplementary to the factory warnings (`docs/SAFETY.md`).

This file is the **project contract** for the `cr-workflow` plugin (`ctrmint/cr-workflow`, installed as `cr-workflow@ctrmint-plugins`). The plugin's `planner`, `implementer` and `reviewer` agents read the sections below as part of their instructions, so keep these headings exactly as written. Run `python3 <plugin>/scripts/cr.py doctor` to check them.

## Project facts

- **Layout:**
  - `main/core/`: pure logic (`Calibration`, `Temperature`). It has no ESP-IDF includes, so it builds and tests on the host. Keep it that way.
  - `main/hal/Ads1115Source.*`: the I²C driver.
  - `main/display/GaugeDisplay.*`: the LVGL UI.
  - `main/Config.h`: every tunable constant, in `namespace config`.
  - `main/main.cpp`: the loop and CSV log.
  - `main/Kconfig.projbuild`: `CONFIG_OIL_GAUGE_DEMO`.
  - `tests/`: host tests (C++ suites; Python tests for the calibration tool).
  - `scripts/`: `test_host.sh`, `build_calibration.py`.
  - `hardware/`: wiring, BOM and `wiring.svg`.
  - `docs/spec.md`: system requirements and the change-request index.
  - `docs/changes/`: one folder per change request.
- **Toolchain:** ESP-IDF 5.5.5 at `~/esp/esp-idf`, LVGL 9.4, Waveshare BSP 3.0.1, with versions pinned in `dependencies.lock`. Host tests need g++ and Python 3.
- **Generated or third-party, never edited by hand and never in a diff:** `build/`, `build-*/`, `managed_components/`, `sdkconfig`, `sdkconfig.old`.
- **`main/CalibrationData.h`:** changes only through `scripts/build_calibration.py`, from measurements a human took, and only when a task says so.

## Gates

There is no separate linter or type checker. `-Werror` and a warning-free firmware build are the lint gate.

- **Host tests:** `./scripts/test_host.sh` must exit 0. It runs the C++ suites with `-Wall -Wextra -Werror` and every Python unittest.
- **Firmware build:** `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` must succeed, and `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` must print nothing.
- **Demo build:** the same no-warnings rule applies.
  ```
  . ~/esp/esp-idf/export.sh && D=/tmp/caterham-oil-gauge-build-demo && mkdir -p $D && printf 'CONFIG_OIL_GAUGE_DEMO=y\n' > $D/demo.defaults && idf.py -B $D -DSDKCONFIG=$D/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;$D/demo.defaults" build 2>&1 | tee /tmp/caterham-oil-gauge-build-demo.log
  ```
  Also check that `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' $D/sdkconfig` succeeds.
- **When to run what:** run both firmware builds whenever anything under `main/` changes. The host tests always run.
- **Where they run:** `/cr-workflow:build` runs all three locally, and `.github/workflows/ci.yml` runs the same gates on every PR and every push to `main`.

## Safety and design invariants

No change may weaken these. A request to change one needs explicit human approval through a change request, and the reviewer treats any violation as a **blocker**.

1. **Fail closed.**
   - The repo default `kCalibrationVerified` is `false`.
   - Uncalibrated firmware shows ohms, never °C.
   - Calibration points are never invented, estimated or copied from a datasheet.
2. **No extrapolation.**
   - Beyond the table, the status is `BelowRange` or `AboveRange` and no number is shown.
   - `AboveRange` drives a Red alarm.
   - `BelowRange` is not a fault.
3. **Demo isolation.** Synthetic curves and readings exist only under `CONFIG_OIL_GAUGE_DEMO` (firmware) or `GAUGE_DEMO` (host tests), and are unreachable in a normal build.
4. **Alarm behaviour.**
   - Escalation is debounced by `kAlarmDebounceMs`, with separate Amber and Red timers.
   - De-escalation is immediate, with `kAlarmResetHysteresisC` of hysteresis, and Red steps down through Amber.
   - **Exception (REQ-T3, approved in CR-2):** while an alarm is latched, raw samples colder than the calibration table are held out of the filter, the alarm and the maximum for `kBelowRangeHoldMs`, counted from the first cold sample. Then the state resets as for a sustained fault. This hold is intended behaviour, not a violation.
   - Faults shorter than `kFaultResetMs` keep the filter and the latched alarm. Sustained faults reset them.
   - The current wording of REQ-T3 in `docs/spec.md` §2, or an approved amendment in the active change request, is the authority.
5. **Measurement.**
   - The measurement is ratiometric and differential (Vp across `Rp`, Vs across the sender), with the same ±4.096 V gain on both channels.
   - The ADS1115 config register is read back after writing.
   - Every `esp_err_t` is checked.
   - A failed read gives `adc_ok = false`, never a stale or invented value.
   - No ADS1115 input can leave the GND to VDD range under the documented wiring.
   - `kPullUpOhms` is the measured value of `Rp`.
6. **Bus allocation.** The Waveshare BSP owns `I2C_NUM_1` on GPIO14/15. The ADS1115 is on `I2C_NUM_0`, with SDA on GPIO17 and SCL on GPIO18; ALRT on GPIO16 is optional.
7. **Docs move together.**
   - A circuit change updates `hardware/WIRING.md`, `hardware/BOM.md`, `hardware/wiring.svg` and the `docs/ARCHITECTURE.md` diagram together.
   - A change to a status, alarm or CSV column updates the docs that describe it (README, `docs/VALIDATION.md`, `docs/CALIBRATION.md`, `docs/ARCHITECTURE.md`).

## Changes that need human approval

Tasks touching these get `needs_human_approval: true`, unless `request.md` already records the approval.

- **Build, components and partitions:** `main/idf_component.yml`, `dependencies.lock`, `sdkconfig.defaults`, `main/Kconfig.projbuild`, `partitions.csv`, any `CMakeLists.txt`.
- **Calibration and thresholds:** `main/CalibrationData.h`, and the alarm thresholds and timing constants in `main/Config.h`.
- **The circuit:** anything that changes the wiring, the BOM or the analogue front end (`hardware/`).
- **CI and tooling:** `.github/workflows/`, `.claude/`.

## Human-only checks

Agents never do these. They go in a task's `human_verification`.

- **Device work:** flashing (`idf.py flash`), the serial monitor (`idf.py monitor`), `esptool`, bench measurements with a decade box or multimeter, and in-car tests. A build passing proves nothing about hardware.
- **Check the device before any flash or monitor.** The gauge enumerates as Espressif `303a:1001`. On the development laptop, `/dev/ttyACM0` has been a Raspberry Pi Pico. Use the `/dev/serial/by-id/usb-Espressif_*` path, never a bare `ttyACM0`.
- **Calibrated builds:** a bench test that needs alarms needs a build with a verified calibration table. That table is generated locally and must never be committed unless a change request says so.
- **Sender calibration:** measuring resistance against temperature.

## Testing strategy

- **Automatically testable:** `main/core/` (calibration, conversion, faults, alarm state machine), through host tests in `tests/`, built by `scripts/test_host.sh`.
  - `tests/temperature_test.cpp` uses the uncalibrated default table.
  - `tests/demo_temperature_test.cpp` builds with `-DGAUGE_DEMO`, so it has the synthetic 20–140 °C table for calibrated behaviour.
  - Its helpers are `forOhms`, `at`, `hold` and `kOpen`. `hold()` isn't wrap-safe, so use explicit `uint32_t` loops across millisecond wraparound.
- **The Python calibration tool** (`scripts/build_calibration.py`) is tested in `tests/test_calibration_tool.py`.
- **Not automatically testable:** `main/hal/`, `main/display/` and `main/main.cpp`. They're verified by both firmware builds plus a human check.
- **Test tasks may touch:** files in `tests/`, and `scripts/test_host.sh` only to register a new test file.
- **Pairing:** a behaviour change in `main/core/` gets a `test` task (tests fail first), then an `impl` task. For characterisation work the tests pass from the start, and the reviewer's mutation check proves their strength.

## Mutation testing

- **Copy into the scratch tree:** `main/`, `tests/`, `scripts/`, `examples/` (the calibration-tool tests read it) and `docs/`.
- **Test command in the copy:** `./scripts/test_host.sh`, run from inside the copy with `ulimit -c 0`.
- **Key code to mutate:** `main/core/Temperature.cpp` (alarm debounce, hysteresis, fault reset, below-range hold, filter, max tracking) and `main/core/Calibration.cpp` (table endpoints, interpolation, `calibrationSide`). An `int32_t` cast of an unsigned millisecond difference is an equivalent mutant for gaps under about 24 days.

## Review focus

- **Samples:** NaN and inf samples; zero and negative voltages.
- **Thresholds and boundaries:** readings exactly at the table endpoints and the alarm thresholds; hysteresis band edges.
- **Timing:**
  - `uint32_t` millisecond wraparound for every timer;
  - alternating fault and good samples;
  - fault samples during a below-range hold;
  - debounce timers carrying over across gaps.
- **Hardware failure paths:** an I²C failure partway through a read cycle; a broken ground-sense wire.
- **Embedded resources:**
  - stack use on the 8 KB main task;
  - heap allocation in the loop;
  - blocking calls and I²C timeouts;
  - `snprintf` sizes;
  - LVGL calls outside the display lock.
- **Untrusted input:** `scripts/build_calibration.py` must reject malformed, non-finite, non-monotonic or too-short CSV data.

## Code style

- **C++17:** compact style, `namespace gauge` for core logic, and constants in `config::` rather than literals. Match the comment density of the surrounding code.
- **Embedded hygiene:**
  - no heap allocation in the sample loop;
  - `snprintf` with `sizeof`;
  - compare `uint32_t` millisecond ticks with unsigned subtraction, so they are wrap-safe;
  - handle NaN and inf from samples explicitly.
- **Python:** stdlib only; follow the style of the neighbouring script.

## Git, PRs and merging

- Branch from `main`. Never commit to `main` directly.
- **Merge only when CI is green.** The required checks are `Host tests`, `Firmware build (normal)` and `Firmware build (demo)`, all `pass` in `gh pr checks <n>`. Branch protection is unavailable on this repo's plan, so nothing else enforces it.
- **Tooling changes** (CI, scripts, `.claude/`) go directly on a branch and PR, with no change request.
- Merging, opening issues and editing issues are outward actions. Do them when asked, or as a step the human has already requested.

## Change-request workflow and notifications

- Work on the gauge goes through change requests, run by the `cr-workflow` plugin: `/cr-workflow:spec-from-issue <n>`, then "make CR-n active", then `/cr-workflow:cr go`, `/cr-workflow:cr pr` and `/cr-workflow:cr merge`.
- The plugin's `docs/workflow.md` describes the process. The CR records are in `docs/changes/`.
- Slack status messages are sent by the plugin's `hooks/slack-notify.sh`. Agents never post, and nobody reads, prints or copies anything under `~/.config/claude-slack-notify/`.
