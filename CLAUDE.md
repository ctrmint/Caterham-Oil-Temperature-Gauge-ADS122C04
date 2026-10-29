# Caterham oil-temperature gauge

ESP-IDF 5.5.5 C++17 firmware for a Waveshare ESP32-S3-Touch-AMOLED-1.75 (LVGL 9). It reads a dry-sump oil-temperature sender through an ADS1115 over I²C. This is a **safety-relevant instrument in a car**: a wrong but plausible temperature is worse than no reading. The firmware is supplementary to the factory warnings (`docs/SAFETY.md`).

This file is the single source of project facts, gates and invariants for the main session and for the agents in `.claude/agents/`. Agent files hold only their role-specific rules.

## Project facts

- **Layout:**
  - `main/core/`: pure logic (`Calibration`, `Temperature`). It has no ESP-IDF includes, so it builds and tests on the host. Keep it that way.
  - `main/hal/Ads1115Source.*`: the I²C driver.
  - `main/display/GaugeDisplay.*`: the LVGL UI.
  - `main/Config.h`: every tunable constant, in `namespace config`.
  - `main/main.cpp`: the loop and CSV log.
  - `main/Kconfig.projbuild`: `CONFIG_OIL_GAUGE_DEMO`.
  - `tests/`: host tests (C++ suites; Python tests for the calibration tool and `scripts/cr.py`).
  - `scripts/`: `test_host.sh`, `build_calibration.py`, `cr.py`.
  - `hardware/`: wiring, BOM and `wiring.svg`.
  - `docs/spec.md`: system requirements and the change-request index.
  - `docs/changes/`: one folder per change request.
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
- Run both firmware builds whenever anything under `main/` changes.
- `/build` runs all three locally. `.github/workflows/ci.yml` runs the same gates on every PR and every push to `main`.

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
   - **Exception (REQ-T3, approved in CR-2):** while an alarm is latched, raw samples colder than the calibration table are held out of the filter, the alarm and the maximum for `kBelowRangeHoldMs`, counted from the first cold sample, and then the state resets as for a sustained fault. This hold is intended behaviour, not a violation.
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

## Code style

- **C++17:** compact style, `namespace gauge` for core logic, and constants in `config::` rather than literals. Match the comment density of the surrounding code.
- **Embedded hygiene:**
  - no heap allocation in the sample loop;
  - `snprintf` with `sizeof`;
  - compare `uint32_t` millisecond ticks with unsigned subtraction, so they are wrap-safe;
  - handle NaN and inf from samples explicitly.
- **Python:** stdlib only; follow the style of the neighbouring script.

## Hardware

- Flashing, the serial monitor, bench measurements and in-car tests are done by a human, never by an agent. A build passing proves nothing about hardware.
- **Before any flash or monitor, check the device.** The gauge enumerates as Espressif `303a:1001`. On this laptop `/dev/ttyACM0` has been a Raspberry Pi Pico. Use the `/dev/serial/by-id/usb-Espressif_*` path, never a bare `ttyACM0`.
- A bench test that needs alarms needs a build with a verified calibration table. That table is generated locally and must never be committed unless a change request says so.

## Change-request workflow

- Work on the gauge goes through change requests.
  - The workflow and conventions are in `docs/changes/README.md`.
  - The main session runs it with `/cr`.
  - `/spec-from-issue <n>` creates a CR from a GitHub issue.
- **Bookkeeping** for task status, findings, the progress log and CR status uses `scripts/cr.py`, not hand edits.
- **The agents:**
  - `planner` (read-only) plans;
  - `implementer` does one task;
  - `reviewer` checks one task and never edits.

  They talk only through the main session and the CR's files.
- **Tooling and workflow changes** (agents, commands, CI, scripts) go directly on a branch and PR, with no change request.

## Git, PRs and merging

- Branch from `main`. Never commit to `main` directly.
- **Merge only when CI is green.** Run `gh pr checks <n>` and see every check `pass`. Branch protection is unavailable on this repo's plan, so nothing else enforces it.
- Merging, opening issues and editing issues are outward actions. Do them when asked, or as a step the human has already requested.

## Slack notifications

- Only the main session posts, using `.claude/hooks/slack-notify.sh` and the lifecycle table in `docs/changes/README.md`.
- Agents never run it, and nobody reads, prints or copies anything under `~/.config/claude-slack-notify/`.
