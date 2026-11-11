# CR-3 task plan

Workflow and conventions: the cr-workflow plugin's `docs/workflow.md` (pointer in [docs/changes/README.md](../README.md)). Task format: the plugin's `planner` agent.

## Current plan

### 1. Summary

**Goal.** The gauge gets two layouts that the user picks with a long-press, and the choice is stored in NVS:
- **Classic:** today's face with a `OIL °C` / `OIL °F` / `OIL ohm` title, plus a reading with no unit, in fixed-width cells, with digits at least 100 px tall.
- **Sweep:** a ring of 40 segments with scale labels, and seven-segment digits at least 72 px tall with dim ghost segments.

A menuconfig option chooses °C (the default) or °F. Fail-closed, no-extrapolation, colour precedence, measurement, alarms and the CSV log stay exactly as they are.

**Approach.**
- **Core helpers, test first.** Every decision about text and numbers goes into three new pure modules in `main/core/`:
  - `DisplayText`: unit, title, rounding, reading, cells, MAX line;
  - `SweepModel`: segments, scale labels, seven-segment table;
  - `LayoutChoice`: layout cycle, decoding the stored value, long-press detector.

  Each comes as a test task followed by an impl task. The helpers call `calibrationValid()` themselves, so they fail closed. Their tests build in both the uncalibrated build and the `GAUGE_DEMO` build.
- **Display.** `GaugeDisplay` computes one `DisplayFrame` per update and passes it to `ClassicLayout` and `SweepLayout` through a `LayoutView` interface.
  - Both layouts are built once at start-up, and a switch only hides one and shows the other, so it allocates nothing.
  - Both layouts are updated on every render, so a switch never shows stale values.
- **Touch.** One event callback on the screen feeds the host-tested `gauge::LongPress`. A switch publishes the new layout through a `std::atomic`. `main.cpp` writes NVS only when the layout has changed, outside the LVGL task.
- **Digits.**
  - Classic's digits are a 148 px bitmap of LVGL's bundled Montserrat Medium, made by `lv_font_conv` 1.5.3. This is the only approval-gated task, and it sits near the end.
  - Sweep's digits are rectangles drawn in code.

**Alternatives I rejected.**
- `lv_indev_set_long_press_time` + `LV_EVENT_LONG_PRESSED`: it can't be host-tested, and it doesn't clearly reject a slow swipe.
- Rebuilding a layout on each switch: that allocates.
- Writing NVS inside the LVGL callback: that blocks the LVGL task.
- `lv_scale` sections for the ring: the styling is harder for little gain.

**Tasks:** 17 (T1–T17). Only T15 needs human approval.

### 2. Acceptance criteria

1. **AC-1 (unit label and title; `ohm` and never `°` when uncalibrated):** T1, T2, T8, T10, T13. Human: request.md Verification steps 1, 2, 4.
2. **AC-2 (reading has no unit; content for each `Status`):** T1, T2, T8, T11.
3. **AC-3 (round half away from zero; convert to °F before rounding; limits `<20`/`>140`/`<68`/`>284`; whole ohms):** T1, T2.
4. **AC-4 (fixed-width cells, right-aligned, ones digit fixed):** T1, T2 (cells), T8, T11, T16. Human: steps 1, 4.
5. **AC-5 (Classic ≥ 100 px, Sweep ≥ 72 px, a 4-character reading fits):** T11, T15, T16. Human: steps 1, 4.
6. **AC-6 (MAX in whole degrees of the unit, `MAX >140`/`>284`, both layouts):** T1, T2, T8, T10.
7. **AC-7 (alarms, calibration, classification and CSV unchanged; existing tests unmodified):** every task, through its `files_must_not_touch`. T13 also checks the CSV. Reviewer's diff check.
8. **AC-8 (°C is the default):** T7, T13.
9. **AC-9 (lit-segment count):** T3, T4, T10.
10. **AC-10 (scale labels; hidden when uncalibrated):** T3, T4, T10. Human: steps 1, 2, 4.
11. **AC-11 (colours for digits and segments; dim unlit segments and ghosts):** T9 (precedence moved verbatim), T10, T11. Human: step 1.
12. **AC-12 (seven-segment digits drawn in code; `<`/`>` in the UI font):** T3, T4, T11. Human: step 1.
13. **AC-13 (long-press ≥ 1 s cycles the layout; taps and swipes do nothing):** T5, T6, T12. Human: step 3.
14. **AC-14 (NVS store and restore; invalid or missing → Classic; write only on change; NVS failure → Classic, logged once):** T5, T6, T13. Human: step 3.
15. **AC-15 (switching doesn't disturb measurement or the CSV; new layout current within 250 ms):** T10, T12, T13. Human: step 3.
16. **AC-16 (every state shown in both layouts):** T8, T9, T10, T11 (one shared `DisplayFrame`). Human: steps 1, 4.
17. **AC-17 (touch changes only the layout):** T12. Reviewer's check.
18. **AC-18 (host tests, normal, demo and local °F builds without `main/` warnings):** the gates of every task; the °F build in T7, T13 and T16; Definition of done.
19. **AC-19 (docs):** T14, plus T17 for `docs/spec.md`.

### 3. Assumptions

1. **CMake approval.** The approval recorded in request.md for `main/CMakeLists.txt` ("the new sources and `nvs_flash`") covers the new `main/core/*.cpp` sources as well as the display and font sources. The caller confirmed this reading. T2, T4, T6, T9, T10 and T13 therefore have `needs_human_approval: false`.
2. **`main/Config.h` is not touched.** The request's scope excludes it. The new constants are named `constexpr` values in the new core headers (`namespace gauge`): `kReadingCells`, `kSweepSegments`, the scale ends, `kLongPressMs` and `kLongPressSlopPx`. There are no literals.
3. **The helpers fail closed even for impossible inputs.** When `calibrationValid()` is false:
   - the reading is whole ohms (for `NeedsCalibration` with a finite, positive resistance) or `--`, even for a hand-built `has_temperature` reading;
   - the footer never shows `MAX`;
   - no segment is lit, and the scale labels are hidden.
4. **Non-finite and oversized values.** A non-finite value, or one with magnitude ≥ 1e6, shows as `--`. A reading longer than 4 characters shows as `  --` and is never truncated. No table within about −99…999 in the selected unit can produce one.
5. **Footer.** The footer's ohms use the same half-away-from-zero rounding. The fallback texts `TEMPERATURE NOT VERIFIED` and `CHECK SENSOR AND WIRING` and the "no maximum yet" threshold (`max_c > -200`) are unchanged from `main/display/GaugeDisplay.cpp:106-118`.
6. **Classic font size: 148 px**, worked out from LVGL's own Montserrat Medium 48 px table (`lv_font_montserrat_48.c:11913-11922`):
   - the digits have `box_h` 34 at 48 px, so their height is 0.70 em; at 148 px that is about 104 px;
   - the widest advance is `4`, at 514/16 px, so 0.669 em; at 148 px that is about 99 px;
   - four cells are therefore about 396 px wide.

   That doesn't fit inside today's arc (430 px, 18 px wide; the inner chord at the digits' edges is about 380 px). T16 therefore enlarges the arc to about 456 px and thins it to 14 px, which gives an inner radius of 214 px. The arc stays, with the same range, angles and °C value rule.
7. **The font needs no LVGL config change.** It is 4 bpp, uncompressed and has no kerning. Its glyphs fit the non-LARGE `lv_font_fmt_txt` limits (`box_h` ≤ 255, `adv_w` < 256 px, under 1 MB). `sdkconfig.defaults` is untouched.
8. **Sweep has 4 equal seven-segment cells.** AC-4 and AC-5 need four for `4999`, `>284` and `<-10`.
   - Following AC-12 literally ("a blank cell has every segment dim"), a blank reading shows a dim `8888`, not `888`.
   - A cell showing `<` or `>` hides its segments and shows the glyph in built-in Montserrat 48, the largest UI font enabled.
9. **Long-press rule.** The switch fires while the finger is still down, at 1000 ms. Moving more than 40 px in x or y from the press point (my choice of slop) cancels the press until release. A press can switch at most once.
10. **NVS layout.** Namespace `oil_gauge`, key `layout`, stored as a `u8` (Classic = 0, Sweep = 1). If NVS can't be initialised or opened, the gauge starts in Classic and logs one warning. Switching still works, but the choice isn't stored.
11. **Kconfig names.** The choice is `OIL_GAUGE_DISPLAY_UNIT`, with `OIL_GAUGE_UNIT_CELSIUS` (the default) and `OIL_GAUGE_UNIT_FAHRENHEIT`. `main.cpp` maps only an explicit `CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT` to °F.
12. **`docs/CALIBRATION.md` gets one line changed** (line 31, `< min °C` / `> max °C`). The request doesn't list this file, but invariant 7 requires the docs that describe the display text to move with it. Only that sentence changes.
13. **Sweep geometry.** The ring covers 270° with the same orientation as the Classic arc, open at the bottom. The status text, `MAX` line and demo banner sit in that open area.
14. **Font install.** The `lv_font_conv` install needs `npm`/Node and network access for that one command. It installs outside the repo, into `/tmp/cr3-lvfc`, and `--ignore-scripts` stops package install scripts from running.
15. **T15 ordering.** `T15 depends_on: [T14]` is an ordering dependency only. It keeps the approval-gated install last, as the caller asked.

### 4. Open questions

None blocking. The maintainer delegated these choices. Two judgement calls are worth a glance at plan review: Assumption 8 (the dim `8888` ghost) and Assumption 12 (the one-line edit to `docs/CALIBRATION.md`).

### 5. Tasks

### T1: Add failing host tests for the display text helpers
- kind: test
- status: done
- depends_on: []
- needs_human_approval: false
- install: none
- goal: Fix in tests, before the code exists, what both layouts will print: the unit label, title, unitless reading, rounding, °F conversion, the 4 fixed-width cells and the MAX line. The tests cover both the uncalibrated build and the GAUGE_DEMO build (AC-1, AC-2, AC-3, AC-4 padding, AC-6).
- files_may_touch:
  - tests/display_text_test.cpp
  - scripts/test_host.sh
- files_must_not_touch:
  - tests/temperature_test.cpp
  - tests/demo_temperature_test.cpp
  - tests/test_calibration_tool.py
  - scripts/build_calibration.py
  - main/
  - docs/
  - hardware/
  - sdkconfig.defaults
  - dependencies.lock
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **Registration.** `scripts/test_host.sh` gains two compile-and-run steps, after the demo suite and before the `python3` line.
    - Both compile `tests/display_text_test.cpp` with `main/core/Calibration.cpp`, `main/core/Temperature.cpp` and `main/core/DisplayText.cpp`, using `-std=c++17 -Wall -Wextra -Werror -Imain`.
    - One builds without `-DGAUGE_DEMO`, to `/tmp/caterham-oil-gauge-tests/display_text_tests`. The other builds with it, to `/tmp/caterham-oil-gauge-tests/display_text_demo_tests`.
    - This is the "register a new test file" change that CLAUDE.md allows. Naming the new core source is part of registering it. No existing line changes.
  - **Build guard.** `gauge::calibrationValid()` is asserted true with `GAUGE_DEMO` and false without it, so neither run can pass vacuously.
  - **Rounding (AC-3).** `roundHalfAway` gives 112.4→112, 112.5→113, −0.5→−1, −0.4→0 and 4999.6→5000. It returns false for NaN, +inf and −inf.
  - **Conversion.** `toUnit(100,Celsius)==100`, `toUnit(100,Fahrenheit)==212` and `toUnit(-40,Fahrenheit)==-40`.
  - **Unit label and title (AC-1).**
    - Uncalibrated: `unitLabel` is `ohm` and `titleText` is `OIL ohm`, for both units.
    - Demo: they are `°C` / `OIL °C` and `°F` / `OIL °F`.
    - In both builds, `titleText(u)` equals `"OIL "` + `unitLabel(u)`.
  - **Reading, demo build (AC-2, AC-3).** Hand-built readings give:
    - with `has_temperature`, in °C: `112` (112.4), `113` (112.5) and `-1` (−0.5);
    - with `has_temperature`, in °F: `239` (115.0 °C), `266` (130.0 °C) and `238` (114.7 °C, which proves the conversion happens before rounding);
    - `AboveRange`: `>140` / `>284`;
    - `BelowRange`: `<20` / `<68`;
    - `NeedsCalibration` at 4999.6 Ω: `5000`. At NaN, +inf, 0 or −5 Ω: `--`;
    - every other fault status: `--`;
    - `has_temperature` with a NaN temperature: `--`.
  - **Reading, uncalibrated build.** `NeedsCalibration` gives `5000` at 4999.6 Ω and `123` at 123.4 Ω. A hand-built `has_temperature` reading, `AboveRange` and `BelowRange` each give `--`.
  - **No unit anywhere (AC-1, AC-2).**
    - Loop over every `Status` value, from 0 to `Status::Red` as in `tests/temperature_test.cpp:39`, for both units and in both builds. The reading never contains `°` (`"\xC2\xB0"`), `C`, `F` or `ohm`.
    - In the uncalibrated build, the title, unit label, reading and footer never contain `°`.
  - **Cells (AC-4).**
    - `readingCells` turns `9`, `99`, `100`, `188`, `--`, `>140` and `4999` into `   9`, `  99`, ` 100`, ` 188`, `  --`, `>140` and `4999`.
    - The output is always exactly 4 characters plus a NUL, and the input's last character is always at index 3.
    - Input longer than 4 characters gives `  --`.
  - **MAX line, demo build (AC-6).**
    - A `Normal` reading at 112.4 °C and 123.4 Ω, with max 112.4: `MAX 112  |  123 ohm`.
    - Max 112.5 in °F: `MAX 235  |  123 ohm`.
    - `max_above_range`: `MAX >140  |  123 ohm` (°C) and `MAX >284  |  123 ohm` (°F).
    - Max −999 (no maximum yet): `123 ohm`.
    - A `BelowRange` reading still shows the MAX line.
    - `NeedsCalibration`: `TEMPERATURE NOT VERIFIED`. Every other fault: `CHECK SENSOR AND WIRING`.
    - The footer never contains `°`.
  - **MAX line, uncalibrated build.** `NeedsCalibration` gives `TEMPERATURE NOT VERIFIED`. Every other status, including a hand-built `has_temperature` reading, gives `CHECK SENSOR AND WIRING`. The footer never contains `MAX`.
  - **Output.** Each run prints one "passed" line that names its build, for example `Display text tests passed (uncalibrated)`.
  - **Mutations.** Together the tests kill these request.md mutations:
    - half-to-even (`%.0f`) rounding;
    - converting to °F after rounding;
    - not converting the limits or `MAX` to °F;
    - a `°` in uncalibrated text;
    - a unit in the reading.
- verification:
  - `./scripts/test_host.sh; echo "exit=$?"` -> both existing C++ suites print their "passed" lines, then the first new compile fails with `core/DisplayText.h: No such file or directory`, and the exit code is non-zero. This is the expected failing state before T2.
  - `git diff --stat <task base>` -> only `scripts/test_host.sh` and `tests/display_text_test.cpp`
  - `git diff <task base> -- scripts/test_host.sh | grep '^-' | grep -v '^---'` -> prints nothing (additions only)
- human_verification: none
- notes:
  - **API contract** for `main/core/DisplayText.h`, in `namespace gauge`. T2 implements exactly this:
    - `#include "Temperature.h"`, `<stddef.h>`, `<stdint.h>`
    - `enum class Unit : uint8_t { Celsius, Fahrenheit };`
    - `constexpr size_t kReadingCells = 4;`, `constexpr size_t kReadingTextSize = 12;`, `constexpr size_t kFooterTextSize = 48;`
    - `float toUnit(float celsius, Unit unit);`: °F = °C × 9/5 + 32.
    - `bool roundHalfAway(float value, long &out);`: false when not finite or |value| ≥ 1e6.
    - `const char* unitLabel(Unit unit);` and `const char* titleText(Unit unit);`: both return static strings.
    - `void readingText(const Reading &r, Unit unit, char *out, size_t size);`
    - `void readingCells(const char *text, char *out, size_t size);`: `size` is at least `kReadingCells + 1`.
    - `void footerText(const Reading &r, float max_c, bool max_above_range, Unit unit, char *out, size_t size);`
  - **Inputs.** Build `gauge::Reading` values by hand rather than through `convertSample`, so the inputs are exact. Compare strings with `std::strcmp`.
  - **Build-specific expectations** go in `#ifdef GAUGE_DEMO` blocks. The shared loops run in both builds.
  - The demo table is 20–140 °C (`main/core/Calibration.cpp:8-11`).
  - Match the style of `tests/temperature_test.cpp`.
- safety_notes: Pins invariants 1 (fail closed) and 2 (no number beyond the table) for the new text paths. The existing test files are untouched (AC-7).
- size: M

### T2: Implement the display text helpers in main/core
- kind: impl
- status: done
- depends_on: [T1]
- needs_human_approval: false
- install: none
- goal: Add `main/core/DisplayText.{h,cpp}` with exactly the API in T1's notes, so T1's tests pass, and register the source in the firmware build.
- files_may_touch:
  - main/core/DisplayText.h
  - main/core/DisplayText.cpp
  - main/CMakeLists.txt
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/Calibration.*
  - main/core/Temperature.*
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/display/
  - main/main.cpp
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **No platform includes.** The files include only the C/C++ standard headers plus `Temperature.h` and `Calibration.h`. There is no ESP-IDF or LVGL include.
  - **`readingText` rules.**
    - When calibrated:
      - `has_temperature` with a finite temperature → whole degrees of the unit;
      - `AboveRange` → `>` plus the converted, then rounded, `calibrationMaxC()`;
      - `BelowRange` → `<` plus the converted, then rounded, `calibrationMinC()`;
      - `NeedsCalibration` with a finite resistance above 0 → whole ohms;
      - anything else → `--`.
    - When uncalibrated, only the `NeedsCalibration` ohms branch applies; everything else gives `--`.
    - A value that can't be rounded gives `--`.
    - `has_temperature` is checked before the status, as in `main/display/GaugeDisplay.cpp:84-105`.
  - **`footerText`** follows the branch structure of `GaugeDisplay.cpp:106-118`, with the new format. The MAX line appears only when calibrated and the reading has a temperature or is beyond the table. The `-200` threshold for "no maximum yet" is a named constant.
  - **Buffers.** Every write is bounded by the `size` argument and NUL-terminated, even when truncated. There is no heap allocation.
  - **CMake.** `main/CMakeLists.txt` changes only by adding `"core/DisplayText.cpp"` to `SRCS`.
- verification:
  - `./scripts/test_host.sh` -> exits 0, and prints the existing "passed" lines plus `Display text tests passed (uncalibrated)` and `(demo)`, plus the Python OK
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `git diff --stat <task base>` -> only the three files above
- human_verification: none
- notes:
  - **Rounding.** Use `std::lround` on a value that is finite and range-checked. Don't use `floor(v + 0.5f)`, which rounds 0.49999997f up.
  - **Conversion.** Compute °F in float as `c * 9.0f / 5.0f + 32.0f`.
  - **Strings.** Write `°` as a UTF-8 literal, as `GaugeDisplay.cpp:85` does. The titles are static strings: `"OIL °C"`, `"OIL °F"`, `"OIL ohm"`.
  - Print `long` with `%ld`.
  - This is a testable impl and T1 is its test task. Test files are read-only. If a test looks wrong, stop and report.
  - The CMakeLists change is approved in request.md ("Approval-relevant changes": the new sources).
- safety_notes: Puts invariants 1 and 2 into the display text: uncalibrated readings show only ohms or `--`, and no number appears beyond the table. `convertSample`, `statusText` and the calibration code are untouched.
- size: M

### T3: Add failing host tests for the Sweep model (segments, scale labels, seven-segment table)
- kind: test
- status: done
- depends_on: [T2]
- needs_human_approval: false
- install: none
- goal: Fix in tests the lit-segment count, the scale labels and their visibility, and the seven-segment patterns, in both builds (AC-9, AC-10, AC-12), before `SweepModel` exists.
- files_may_touch:
  - tests/sweep_model_test.cpp
  - scripts/test_host.sh
- files_must_not_touch:
  - tests/temperature_test.cpp
  - tests/demo_temperature_test.cpp
  - tests/display_text_test.cpp
  - tests/test_calibration_tool.py
  - scripts/build_calibration.py
  - main/
  - docs/
  - hardware/
  - sdkconfig.defaults
  - dependencies.lock
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **Registration.** `scripts/test_host.sh` gains two compile-and-run steps, placed after the display-text steps.
    - Both compile `tests/sweep_model_test.cpp` with `main/core/Calibration.cpp`, `main/core/Temperature.cpp`, `main/core/DisplayText.cpp` and `main/core/SweepModel.cpp`, using the same flags as before.
    - One builds without `-DGAUGE_DEMO`, to `/tmp/caterham-oil-gauge-tests/sweep_model_tests`. The other builds with it, to `/tmp/caterham-oil-gauge-tests/sweep_model_demo_tests`.
    - Additions only. This is the allowed registration of a new test file.
  - **Build guard** as in T1.
  - **Scale (AC-9, AC-10).**
    - `sweepScale(Celsius)` is {40, 150} and `sweepScale(Fahrenheit)` is {100, 300}. `kSweepSegments == 40`.
    - `scaleLabelCount` is 6 for °C and 11 for °F.
    - `scaleLabel` returns 40, 60, …, 140 for °C and 100, 120, …, 300 for °F, in that order.
  - **Visibility (AC-10).** `scaleLabelsVisible()` is false when uncalibrated and true in the demo build.
  - **Lit count, demo build (AC-9).** Hand-built `has_temperature` readings give:
    - in °C: 40.0 → 0, 150.0 → 40, 67.5 → 10 (a segment boundary), 67.49 → 9, 67.51 → 10, 30.0 (below the scale) → 0, 160.0 (above the scale) → 40;
    - in °F: 60.0 °C (140 °F, a boundary) → 8, 59.99 °C → 7, 60.01 °C → 8, 37.0 °C (below the °F scale) → 0, 150.0 °C (302 °F) → 40.
  - **Above range.** `AboveRange` with the demo table gives 36 in °C (floor(100/110 × 40)) and 36 in °F (floor(184/200 × 40)), never 40.
  - **Nothing lit.** These give 0:
    - `BelowRange`, including with `alarm = Red` latched;
    - every fault status from `NoAdc` to `NeedsCalibration`;
    - `has_temperature` with a NaN temperature.
  - **Uncalibrated build.** Every status, in both units, gives 0. This includes a hand-built `has_temperature` reading at 100 °C and `AboveRange`.
  - **Seven-segment patterns (AC-12).** Bits 0–6 are segments a–g.
    - `'0'` 0x3F, `'1'` 0x06, `'2'` 0x5B, `'3'` 0x4F, `'4'` 0x66, `'5'` 0x6D, `'6'` 0x7D, `'7'` 0x07, `'8'` 0x7F, `'9'` 0x6F, `'-'` 0x40, `' '` 0x00.
    - `'<'`, `'>'`, `'A'` and `'\0'` all give 0x00.
  - **Mutations.** The tests kill these:
    - an off-by-one in the lit count;
    - lighting segments beyond the table's maximum;
    - lighting any segment when uncalibrated or on a fault;
    - showing scale labels when uncalibrated.
  - **Output.** Each run prints one "passed" line that names its build.
- verification:
  - `./scripts/test_host.sh; echo "exit=$?"` -> every earlier suite passes, then the new compile fails with `core/SweepModel.h: No such file or directory`, and the exit code is non-zero. This is the expected failing state before T4.
  - `git diff --stat <task base>` -> only `scripts/test_host.sh` and `tests/sweep_model_test.cpp`
  - `git diff <task base> -- scripts/test_host.sh | grep '^-' | grep -v '^---'` -> prints nothing
- human_verification: none
- notes:
  - **API contract** for `main/core/SweepModel.h`, in `namespace gauge`. T4 implements exactly this:
    - `#include "DisplayText.h"`, for `Unit` and `Reading`
    - `constexpr int kSweepSegments = 40;`
    - `struct SweepScale { float min; float max; };` and `SweepScale sweepScale(Unit unit);`
    - `int scaleLabelCount(Unit unit);` and `int scaleLabel(Unit unit, int index);`, valid for `0 <= index < count`
    - `bool scaleLabelsVisible();`
    - `int litSegments(const Reading &r, Unit unit);`
    - `uint8_t sevenSegmentMask(char c);`
  - **Lit-count rule (AC-9).** `clamp(floor((T − min) / (max − min) × N), 0, N)`, with T in the selected unit.
    - T is the reading's temperature when `has_temperature` is set, or `calibrationMaxC()` for `AboveRange`.
    - The count is 0 otherwise, and always 0 when `!calibrationValid()`.
  - **Boundary values.** The boundaries above are exact in float: 67.5 °C is 27.5/110 = 0.25, and 60 °C is 140 °F. Don't use values such as 139 °C, which sit exactly on a boundary only in decimal.
- safety_notes: Pins invariant 1 (no segments or scale numbers without a verified calibration) and invariant 2 (above range lights only up to the table's maximum).
- size: M

### T4: Implement the Sweep model in main/core
- kind: impl
- status: done
- depends_on: [T3]
- needs_human_approval: false
- install: none
- goal: Add `main/core/SweepModel.{h,cpp}` with exactly the API in T3's notes, so T3's tests pass, and register the source in the firmware build.
- files_may_touch:
  - main/core/SweepModel.h
  - main/core/SweepModel.cpp
  - main/CMakeLists.txt
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/Calibration.*
  - main/core/Temperature.*
  - main/core/DisplayText.*
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/display/
  - main/main.cpp
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **No platform includes.** The scale ends (40/150 °C, 100/300 °F), the label step (20) and the segment count are named constants.
  - **`litSegments` order of checks:**
    1. returns 0 when `!calibrationValid()`, when `isFault(r.status)` or when the status is `BelowRange`;
    2. otherwise uses the temperature if `has_temperature`, or `calibrationMaxC()` for `AboveRange`;
    3. returns 0 for any non-finite value;
    4. otherwise converts with `toUnit`, applies floor, then clamps to [0, `kSweepSegments`].
  - **`scaleLabelsVisible()`** returns `calibrationValid()`.
  - **`sevenSegmentMask`** is a fixed table with the masks in T3.
  - **CMake.** `main/CMakeLists.txt` changes only by adding `"core/SweepModel.cpp"` to `SRCS`.
- verification:
  - `./scripts/test_host.sh` -> exits 0; both sweep suites print "passed"
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `git diff --stat <task base>` -> only the three files above
- human_verification: none
- notes:
  - Compute the count as `(t - min) * N / (max - min)` in float, then `floorf`.
  - This is a testable impl and T3 is its test task. Test files are read-only.
  - The CMakeLists change is approved in request.md.
- safety_notes: Invariants 1 and 2: nothing is lit without calibration, below range or on a fault, and above range lights only up to the table maximum.
- size: S

### T5: Add failing host tests for the layout choice and the long-press detector
- kind: test
- status: done
- depends_on: []
- needs_human_approval: false
- install: none
- goal: Fix in tests the layout cycle, the decoding of the stored layout (invalid → Classic) and the 1 s long-press rule, including taps, swipes and millisecond wraparound (AC-13, AC-14).
- files_may_touch:
  - tests/layout_choice_test.cpp
  - scripts/test_host.sh
- files_must_not_touch:
  - tests/temperature_test.cpp
  - tests/demo_temperature_test.cpp
  - tests/display_text_test.cpp
  - tests/sweep_model_test.cpp
  - tests/test_calibration_tool.py
  - scripts/build_calibration.py
  - main/
  - docs/
  - hardware/
  - sdkconfig.defaults
  - dependencies.lock
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **Registration.** `scripts/test_host.sh` gains one compile-and-run step: `tests/layout_choice_test.cpp` with `main/core/LayoutChoice.cpp`, the same flags, output `/tmp/caterham-oil-gauge-tests/layout_choice_tests`. Additions only. This is the allowed registration of a new test file.
  - **Cycle.** `nextLayout(Classic) == Sweep` and `nextLayout(Sweep) == Classic`.
  - **Stored value (AC-14).**
    - `encodeLayout` gives 0 for Classic and 1 for Sweep.
    - `decodeLayout(true,0) == Classic` and `decodeLayout(true,1) == Sweep`.
    - `decodeLayout(true,2)`, `(true,255)`, `(false,0)` and `(false,1)` are all Classic.
    - `decodeLayout(true, encodeLayout(x)) == x` for both layouts.
  - **Long press (AC-13).** Coordinates are in px and times in ms.
    - `hold` before any `press` → false.
    - Press at (100,100) at t0, then hold at the same point: at t0+999 → false; at t0+1000 → true; at t0+1001 and t0+5000 → false (one switch per press).
    - **Tap:** press, holds up to t0+900, release. Later `hold` calls with no new press → false. A new press restarts the timer: +999 → false, +1000 → true.
    - **Swipe:** press at (100,100), hold at (141,100) at t0+200 → false, then hold back at (100,100) at t0+2000 → still false. The same applies with a 41 px move in y.
    - **Jitter:** holds at (140,60), which is exactly the 40 px slop in both axes, give true at t0+1000.
    - **Wrap:** press at `0xFFFFFE00u`. Hold at +999 (after wrapping) → false; at +1000 → true. Use explicit `uint32_t` arithmetic.
  - **Constants.** `kLongPressMs == 1000` and `kLongPressSlopPx == 40`. The other assertions use the constants.
  - **Mutation.** An invalid stored layout that doesn't fall back to Classic makes a test fail.
  - **Output.** The run prints one "passed" line.
- verification:
  - `./scripts/test_host.sh; echo "exit=$?"` -> every earlier suite passes, then the new compile fails with `core/LayoutChoice.h: No such file or directory`, and the exit code is non-zero. This is the expected failing state before T6.
  - `git diff --stat <task base>` -> only `scripts/test_host.sh` and `tests/layout_choice_test.cpp`
  - `git diff <task base> -- scripts/test_host.sh | grep '^-' | grep -v '^---'` -> prints nothing
- human_verification: none
- notes:
  - **API contract** for `main/core/LayoutChoice.h`, in `namespace gauge`. It needs only `<stdint.h>`. T6 implements exactly this:
    - `enum class Layout : uint8_t { Classic = 0, Sweep = 1 };`
    - `constexpr uint32_t kLongPressMs = 1000;` and `constexpr int32_t kLongPressSlopPx = 40;`
    - `Layout nextLayout(Layout current);`
    - `Layout decodeLayout(bool found, uint8_t stored);` and `uint8_t encodeLayout(Layout layout);`
    - `class LongPress { public: void press(int32_t x, int32_t y, uint32_t ms); bool hold(int32_t x, int32_t y, uint32_t ms); void release(); };`
  - **`hold` semantics.** It returns true exactly once per press: the first call at which `uint32_t(ms - press_ms) >= kLongPressMs`, provided |dx| and |dy| from the press point have never exceeded `kLongPressSlopPx` during that press. A larger move cancels the press until `release()`.
  - Don't use the `hold()` helper from `demo_temperature_test.cpp`; it isn't wrap-safe.
- safety_notes: The touch logic is kept pure and host-tested. It can only choose a layout (AC-17).
- size: S

### T6: Implement the layout choice and long-press detector in main/core
- kind: impl
- status: done
- depends_on: [T5]
- needs_human_approval: false
- install: none
- goal: Add `main/core/LayoutChoice.{h,cpp}` with exactly the API in T5's notes, so T5's tests pass, and register the source in the firmware build.
- files_may_touch:
  - main/core/LayoutChoice.h
  - main/core/LayoutChoice.cpp
  - main/CMakeLists.txt
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/Calibration.*
  - main/core/Temperature.*
  - main/core/DisplayText.*
  - main/core/SweepModel.*
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/display/
  - main/main.cpp
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - No ESP-IDF or LVGL includes, and no heap.
  - The timers compare with wrap-safe unsigned subtraction: `uint32_t(ms - since) >= kLongPressMs`.
  - `decodeLayout` returns Sweep only for `found && stored == 1`, and Classic for everything else.
  - `main/CMakeLists.txt` changes only by adding `"core/LayoutChoice.cpp"` to `SRCS`.
- verification:
  - `./scripts/test_host.sh` -> exits 0; the layout suite prints "passed"
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `git diff --stat <task base>` -> only the three files above
- human_verification: none
- notes:
  - This is a testable impl and T5 is its test task. Test files are read-only.
  - The CMakeLists change is approved in request.md.
- safety_notes: REQ-T5-style wrap safety for the new timer. Nothing here can reach measurement or alarms.
- size: S

### T7: Add the display-unit choice to Kconfig (°C default)
- kind: config
- status: done
- depends_on: []
- needs_human_approval: false
- install: none
- goal: Add a menuconfig choice between °C (the default) and °F under "Caterham oil temperature gauge", so the unit can be chosen at build time (AC-8). Nothing reads it until T13.
- files_may_touch:
  - main/Kconfig.projbuild
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/
  - main/display/
  - main/main.cpp
  - main/CMakeLists.txt
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **The choice.** Inside the existing `menu`, add `choice OIL_GAUGE_DISPLAY_UNIT`:
    - prompt "Display temperature unit", with `default OIL_GAUGE_UNIT_CELSIUS`;
    - `config OIL_GAUGE_UNIT_CELSIUS` (bool "Degrees Celsius") and `config OIL_GAUGE_UNIT_FAHRENHEIT` (bool "Degrees Fahrenheit");
    - help text saying it affects calibrated display readings only, that the CSV log always records `temp_c` in °C, and that uncalibrated firmware shows ohms.
  - `OIL_GAUGE_DEMO` is unchanged.
  - A build from `sdkconfig.defaults` selects °C. The normal and demo gate builds both select °C.
- verification:
  - `./scripts/test_host.sh` -> exits 0
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_UNIT_CELSIUS=y' sdkconfig` -> succeeds
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds; `grep -q '^CONFIG_OIL_GAUGE_UNIT_CELSIUS=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `. ~/esp/esp-idf/export.sh && idf.py -B /tmp/caterham-oil-gauge-build-c -DIDF_TARGET=esp32s3 -DSDKCONFIG=/tmp/caterham-oil-gauge-build-c/sdkconfig -DSDKCONFIG_DEFAULTS=sdkconfig.defaults reconfigure` -> succeeds; `grep -q '^CONFIG_OIL_GAUGE_UNIT_CELSIUS=y' /tmp/caterham-oil-gauge-build-c/sdkconfig` -> succeeds; `grep -c '^CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y' /tmp/caterham-oil-gauge-build-c/sdkconfig` -> prints 0
  - `. ~/esp/esp-idf/export.sh && F=/tmp/caterham-oil-gauge-build-f && mkdir -p $F && printf 'CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y\n' > $F/unit.defaults && idf.py -B $F -DIDF_TARGET=esp32s3 -DSDKCONFIG=$F/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;$F/unit.defaults" build 2>&1 | tee /tmp/caterham-oil-gauge-build-f.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-f.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y' /tmp/caterham-oil-gauge-build-f/sdkconfig` -> succeeds
  - `git diff --stat <task base>` -> only `main/Kconfig.projbuild`
- human_verification: none
- notes:
  - **Approval.** request.md records the approval: "`main/Kconfig.projbuild` (the display-unit choice)", PurpleSentinel, 2026-11-11.
  - **Target flag.** `-DIDF_TARGET=esp32s3` matches CI (`.github/workflows/ci.yml:61`) and keeps a fresh build directory from defaulting to esp32.
  - **`sdkconfig.defaults` is not touched.** °C is the Kconfig default, so CI picks it up automatically.
  - The file uses 4-space indentation (`main/Kconfig.projbuild:1-8`).
- safety_notes: Only the display unit can be chosen, and only at build time. The CSV stays in °C (AC-7). No touch path can reach it (AC-17).
- size: S

### T8: Show the Classic title, fixed-width reading and MAX line from the core helpers
- kind: impl
- status: done
- depends_on: [T2]
- needs_human_approval: false
- install: none
- goal: Turn today's screen into the Classic layout of the amended REQ-D1:
  - the title is `titleText(unit)`;
  - the reading has no unit and sits in 4 equal, right-aligned cells;
  - the bottom line is `footerText(...)`.

  The digits stay in built-in Montserrat 48 until T16.
- files_may_touch:
  - main/display/GaugeDisplay.h
  - main/display/GaugeDisplay.cpp
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/main.cpp
  - main/CMakeLists.txt
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **`begin` signature.** It becomes `bool begin(gauge::Unit unit = gauge::Unit::Celsius)`, so `main/main.cpp` compiles unchanged. The unit is stored in a member.
  - **Title.** The title label shows `gauge::titleText(unit_)` instead of `OIL TEMPERATURE`.
  - **Reading cells.**
    - The single `temp_` label is replaced by `gauge::kReadingCells` labels of equal width W. W is the largest `lv_font_get_glyph_width(font, c, 0)` over `0123456789-<>` for the digit font, computed once in `begin()`.
    - The cells sit side by side as one horizontally centred group, and each shows one character of `readingCells(readingText(...))` centred in its cell. The ones digit is therefore always in the rightmost cell, at a fixed x.
  - **Footer.** The footer shows `gauge::footerText(...)`. `main/display/` no longer formats any temperature or unit itself.
  - **Unchanged.** The status text, the colour precedence (`GaugeDisplay.cpp:78-82`), the arc behaviour (`:39-52`, and the arc values in `:84-105`) and the demo banner all stay as they are.
  - **Lock.** Every LVGL call stays between `bsp_display_lock` and `bsp_display_unlock`.
  - **Allocation.** No new heap allocation per render:
    - the cells use `lv_label_set_text_static` with a static table of one-character strings;
    - the footer uses `lv_label_set_text_static` on a buffer of `gauge::kFooterTextSize` owned by `GaugeDisplay`;
    - the title uses `lv_label_set_text_static`.
- verification:
  - `./scripts/test_host.sh` -> exits 0
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `grep -n 'OIL TEMPERATURE\|°C\|%.1f\|%.0f' main/display/GaugeDisplay.cpp` -> prints nothing
  - `git diff --stat <task base>` -> only the two files above
- human_verification: Deferred to the device session after T16 (request.md Verification steps 1 and 4).
  - Normal build: the title is `OIL ohm`, the reading is `--` and `ADC UNAVAILABLE` shows.
  - Demo build: the title is `OIL °C`, there's no unit beside the reading, and the ones digit doesn't move.
- notes:
  - **No host test.** `main/display/` isn't host-testable (CLAUDE.md "Testing strategy"). The logic it relies on was test-first in T1 and T2. Verification is the two firmware builds plus the human check.
  - **Cell labels** use `LV_LABEL_LONG_CLIP` and `LV_TEXT_ALIGN_CENTER`.
  - **Keep in place for now:** the `kBg` and other colour constants and `center_text`. T9 moves them.
- safety_notes: Invariant 1: the title and reading come from the fail-closed core helpers, so uncalibrated firmware shows only `ohm`, ohms or `--`. Colour precedence (REQ-D1) is unchanged.
- size: M

### T9: Put the Classic layout behind a common layout interface
- kind: impl
- status: done
- depends_on: [T8]
- needs_human_approval: false
- install: none
- goal: Restructure without any visible change. `GaugeDisplay` computes one `DisplayFrame` per update and hands it to a `ClassicLayout` that builds and renders its own widgets behind a `LayoutView` interface. This is ready for a second layout (request scope: "split so each layout builds and renders its own widgets behind a common interface").
- files_may_touch:
  - main/display/Layouts.h
  - main/display/ClassicLayout.cpp
  - main/display/GaugeDisplay.h
  - main/display/GaugeDisplay.cpp
  - main/CMakeLists.txt
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/main.cpp
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **`main/display/Layouts.h` declares:**
    - the colour palette, moved from `GaugeDisplay.cpp:11-16`;
    - `struct DisplayFrame`, with `uint32_t colour`, `char cells[gauge::kReadingCells + 1]`, `const char* title`, `const char* unit`, `const char* status`, `const char* footer`, `int classic_arc` and `bool demo`;
    - the abstract `class LayoutView`, with `virtual void build(lv_obj_t* screen, gauge::Unit unit)`, `virtual void render(const DisplayFrame& f)`, `void setVisible(bool)` and a protected `lv_obj_t* root_`;
    - `class ClassicLayout : public LayoutView`.
  - **Containers.** Each layout creates one full-screen container (466×466, centred, transparent background, no border or padding). All its widgets are children of that container. The container and every child have `LV_OBJ_FLAG_CLICKABLE` and `LV_OBJ_FLAG_SCROLLABLE` removed, so presses reach the screen.
  - **`GaugeDisplay`** owns `ClassicLayout` as a member (no `new` or `malloc`).
    - `begin()` builds it under the display lock.
    - `render()` fills one `DisplayFrame` under the lock and calls `classic_.render(frame)`. The frame uses the T8 core calls, the colour precedence moved verbatim from `GaugeDisplay.cpp:78-82`, and the arc value rule moved verbatim from `:84-105`.
  - **No visible change from T8.**
  - **CMake.** `main/CMakeLists.txt` changes only by adding `"display/ClassicLayout.cpp"` to `SRCS`.
- verification:
  - `./scripts/test_host.sh` -> exits 0
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `grep -n 'lv_label_create\|lv_arc_create' main/display/GaugeDisplay.cpp` -> prints nothing (widgets now live in the layout)
  - `git diff --stat <task base>` -> only the five files above
- human_verification: Deferred to the device session after T16. The screen must look exactly as it did after T8 (request.md Verification steps 1 and 4).
- notes:
  - **No host test.** This is a display refactor and isn't host-testable. Most of the diff is moved code.
  - **Lock.** `build()` and `render()` are called only with the display lock held. Note that in the header.
  - **C++ features.** Virtual calls need no RTTI or exceptions, so ESP-IDF's defaults are fine.
- safety_notes: The colour precedence (REQ-D1) is moved verbatim, not rewritten. The reviewer should diff the moved block.
- size: M

### T10: Add the Sweep layout: segment ring, scale labels and small texts
- kind: impl
- status: done
- depends_on: [T9, T4, T6]
- needs_human_approval: false
- install: none
- goal: Add `SweepLayout`, built once next to Classic. It has a 40-segment ring that lights from the scale start up to the reading, numbered scale labels inside the ring, a small unit label above `OIL TEMP`, and small status, MAX and demo texts below (AC-9, AC-10, AC-11 for segments, AC-16). Its digits come in T11.
- files_may_touch:
  - main/display/Layouts.h
  - main/display/SweepLayout.cpp
  - main/display/GaugeDisplay.h
  - main/display/GaugeDisplay.cpp
  - main/CMakeLists.txt
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/main.cpp
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **`begin` signature** becomes `bool begin(gauge::Unit unit = gauge::Unit::Celsius, gauge::Layout layout = gauge::Layout::Classic)`. It builds both layouts once, under the lock, and shows only `layout`. The other layout's container is `LV_OBJ_FLAG_HIDDEN`. `main/main.cpp` compiles unchanged.
  - **Frame fields.** `DisplayFrame` gains `int lit_segments` and `bool scale_visible`, filled in `render()` from `gauge::litSegments(r, unit_)` and `gauge::scaleLabelsVisible()`.
  - **Ring.**
    - `gauge::kSweepSegments` separate segment objects, created once over a 270° sweep with the same orientation as the Classic arc (start at 135°, open at the bottom).
    - Segment i, counting from the scale start, shows `frame.colour` when `i < frame.lit_segments`. Otherwise it shows a dim colour that is still visible.
  - **Scale labels.**
    - There are `gauge::scaleLabelCount(unit)` labels. Their text comes from `gauge::scaleLabel(unit, i)`, formatted once into static buffers owned by the layout.
    - Each sits inside the ring at the angle of its value on `gauge::sweepScale(unit)`.
    - All are hidden whenever `!frame.scale_visible`. They are also hidden at build time when uncalibrated.
  - **Small texts.**
    - `frame.unit` sits small above `OIL TEMP`, at the lower right of the digit area.
    - The status text (in `frame.colour`), `frame.footer` and the demo banner (shown when `frame.demo`) are small and placed below the digit area, in the ring's open bottom.
    - A digit area of at least 72 px tall and 4 cells is reserved, empty, for T11.
  - **Rendering.**
    - `render()` updates both layouts on every call, the hidden one included, so a later switch never shows stale values.
    - A segment's colour or a label's visibility changes only when it differs from the last render.
    - There is no heap allocation in `render()`.
  - **Clickable flags.** Every object created is non-clickable and non-scrollable.
  - **CMake.** `main/CMakeLists.txt` changes only by adding `"display/SweepLayout.cpp"` to `SRCS`.
- verification:
  - `./scripts/test_host.sh` -> exits 0
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `git diff --stat <task base>` -> only the five files above
- human_verification: Deferred to the device session after T16 (request.md Verification steps 1, 2 and 4).
  - The segments track the reading. The labels read 40…140 in °C and 100…300 in °F.
  - Uncalibrated: no segment is lit and no number shows.
  - Colours go green → amber → red → amber → green.
  - Nothing overlaps.
- notes:
  - **No host test.** Display code isn't host-testable. The count and label rules were test-first in T3 and T4.
  - **Segment objects.** Prefer objects with small bounding boxes, such as one `lv_line` per segment, or `lv_arc` objects whose indicator part is transparent. A full-size `lv_arc` invalidates the whole ring on each colour change.
  - **Unused labels.** For °C, only 6 of the at most 11 label objects are created.
  - **Fit check.** Check that every label's corners sit inside the ring's inner radius and clear of the reserved digit area.
- safety_notes:
  - Invariant 1: the segments and scale numbers come from `litSegments` and `scaleLabelsVisible`, which fail closed.
  - Invariant 2: above range, the ring lights only up to the table maximum.
  - REQ-D1: one shared `frame.colour`.
  - Rendering both layouts avoids showing a stale temperature after a switch.
- size: M

### T11: Draw Sweep's seven-segment digits with ghost segments
- kind: impl
- status: done
- depends_on: [T10]
- needs_human_approval: false
- install: none
- goal: Fill Sweep's digit area with 4 equal seven-segment cells drawn as LVGL rectangles (AC-12 as amended). Lit segments use the status colour and unlit ones a dim ghost colour. `<` and `>` are drawn in the UI font, and the reading is right-aligned with a fixed ones digit (AC-2, AC-4, AC-5, AC-11, AC-12).
- files_may_touch:
  - main/display/SweepLayout.cpp
  - main/display/Layouts.h
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/
  - main/display/GaugeDisplay.*
  - main/display/ClassicLayout.cpp
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/main.cpp
  - main/CMakeLists.txt
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **Cells.** There are `gauge::kReadingCells` cells of equal width at a fixed position, as a right-aligned group.
    - Each cell has 7 segment objects (a–g), created once.
    - The digit height is at least 72 px; aim for about 96–104 px.
    - The 4-cell block fits inside the scale labels without overlapping them, the unit / `OIL TEMP` texts or the status texts.
  - **Content.** For cell i, with `c = frame.cells[i]`:
    - if `c` is `<` or `>`, the cell's 7 segments are hidden and a label in built-in `lv_font_montserrat_48` shows `c` centred in the cell;
    - otherwise that label is hidden, and segment s shows `frame.colour` when bit s of `gauge::sevenSegmentMask(c)` is set, or the ghost colour when it isn't. A blank cell is therefore a dim 8.
  - **Updates.** Only segments or labels whose state changed get new style values. There is no heap allocation in `render()`.
  - **Clickable flags.** Every new object is non-clickable and non-scrollable.
- verification:
  - `./scripts/test_host.sh` -> exits 0
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `git diff --stat <task base>` -> only the two files above
- human_verification: Deferred to the device session after T16 (request.md Verification steps 1 and 4).
  - The ghost segments are visible. The digits are at least 72 px tall (photo with a ruler).
  - The ones digit doesn't move across the demo sweep. `4999` fits, if an ADS1115 and a test resistor are fitted.
- notes:
  - **No host test.** Display code isn't host-testable. The segment table was test-first in T3 and T4.
  - **Segment shapes.** Use plain `lv_obj` rectangles with a small radius. Horizontal segments a, d and g and vertical segments b, c, e and f should have a thickness of about 10 px.
  - **Ghost appearance.** See Assumption 8: a blank reading is a dim `8888`.
- safety_notes: REQ-D1 colour precedence applies to the lit segments through `frame.colour`. The reading comes only from `frame.cells`, so the fail-closed text applies.
- size: M

### T12: Switch layout with a 1 s long-press anywhere on the screen
- kind: impl
- status: done
- depends_on: [T11, T6]
- needs_human_approval: false
- install: none
- goal: A press of at least 1 s anywhere switches Classic → Sweep → Classic by hiding one layout and showing the other. The new layout is handed to the main task without blocking the LVGL task. Taps, swipes and any other touch do nothing (AC-13, AC-15, AC-17).
- files_may_touch:
  - main/display/GaugeDisplay.h
  - main/display/GaugeDisplay.cpp
  - main/display/ClassicLayout.cpp
  - main/display/SweepLayout.cpp
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/
  - main/display/Layouts.h
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/main.cpp
  - main/CMakeLists.txt
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **Callback registration.** `begin()` registers one event callback on `lv_screen_active()` with `lv_obj_add_event_cb`, with `this` as user data, under the display lock.
  - **Events handled.** The callback handles only:
    - `LV_EVENT_PRESSED`, which calls `press`;
    - `LV_EVENT_PRESSING`, which calls `hold`;
    - `LV_EVENT_RELEASED` and `LV_EVENT_PRESS_LOST`, which call `release`.

    It reads the point with `lv_indev_get_point(lv_indev_active(), &p)` and the time with `lv_tick_get()`, and feeds a `gauge::LongPress` member.
  - **On a long press.** When `hold()` returns true:
    - the active layout becomes `gauge::nextLayout(active)`;
    - the old container is hidden and the new one shown;
    - the new layout is stored in a `std::atomic<uint8_t>` as pending.

    Nothing else changes on touch: not the unit, calibration, alarms, `Config`, the frame or measurement.
  - **Handing over.** The new `bool takeLayoutChange(gauge::Layout &out)` returns true once per switch, by exchanging the atomic. It makes no LVGL call and takes no lock.
  - **Callback limits.** The callback never calls `bsp_display_lock`, because it already runs in the LVGL task, which holds the recursive LVGL mutex. It never touches NVS, never blocks and never allocates.
  - **Clickable flags.** No child object of either layout is clickable. The layout files may be edited only to clear a clickable flag that T9–T11 missed.
- verification:
  - `./scripts/test_host.sh` -> exits 0
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `grep -rn 'lv_obj_add_event_cb\|lv_indev_add_event_cb' main/` -> exactly one line, in `main/display/GaugeDisplay.cpp`
  - `grep -rn 'nvs_' main/display/` -> prints nothing
  - `git diff --stat <task base>` -> only files from the allowlist
- human_verification: Deferred to the device session after T16 (request.md Verification step 3).
  - A press of at least 1 s switches the layout. A short tap and a swipe do nothing.
  - Five switches leave no CSV gap over 1.2 s.
  - The new layout shows current values at once.
- notes:
  - **No host test.** Display code isn't host-testable. The press rule was test-first in T5 and T6.
  - **Why the screen object.** Events reach the screen only if no child is clickable. `lv_obj_create` children are clickable by default; labels aren't. The existing arc already removes the flag (`GaugeDisplay.cpp:46`). The screen is already non-scrollable (`GaugeDisplay.cpp:37`).
  - **Why `render()` and the callback can't race.** `render()` and the callback are serialised by the LVGL mutex: `render()` takes it with `bsp_display_lock`, and the LVGL task holds it while dispatching events. Both use `active_`.
  - **Rejected alternative.** `lv_indev_set_long_press_time` + `LV_EVENT_LONG_PRESSED` isn't used; see the Summary.
- safety_notes:
  - AC-17: touch can only choose a layout.
  - Invariant 4 and measurement are unaffected, because the callback only hides and shows objects and sets an atomic.
  - "LVGL calls outside the display lock" (Review focus): every LVGL call is in `begin()`/`render()` under the lock or in the LVGL task's own event callback.
- size: M

### T13: Initialise NVS, restore and store the layout, and pass the unit to the display
- kind: impl
- status: done
- depends_on: [T12, T7]
- needs_human_approval: false
- install: none
- goal: `main.cpp` initialises NVS (erasing and retrying when needed) and restores the stored layout (Classic if it's missing or invalid). It maps the Kconfig unit to `gauge::Unit`, starts the display with both, and writes NVS only when the layout changes. If NVS fails, the gauge carries on in Classic with one log line (AC-1, AC-8, AC-14, AC-15).
- files_may_touch:
  - main/main.cpp
  - main/CMakeLists.txt
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/
  - main/display/
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **CMake.** `main/CMakeLists.txt` changes only by adding `nvs_flash` to `REQUIRES`.
  - **Unit.** `CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT` gives `gauge::Unit::Fahrenheit`. Anything else gives `Celsius`.
  - **Start-up, before `display.begin`:**
    - call `nvs_flash_init()`;
    - on `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`, call `nvs_flash_erase()` and then `nvs_flash_init()` once more;
    - call `nvs_open("oil_gauge", NVS_READWRITE, &h)` and `nvs_get_u8(h, "layout", &v)`;
    - set the layout to `gauge::decodeLayout(err == ESP_OK, v)`.

    If init or open fails, there is exactly one `ESP_LOGW` line, the layout is Classic, and no other NVS call is made for the rest of the run.
  - **`display.begin(unit, layout)`.**
  - **In the loop.** If `display_ok` and `display.takeLayoutChange(next)` and NVS is available and `next` differs from the last stored value:
    - call `nvs_set_u8` and `nvs_commit`, checking every `esp_err_t`;
    - on success, record `next` as the stored value;
    - on failure, log one `ESP_LOGW` for that save.

    No NVS call happens otherwise.
  - **CSV unchanged.** The CSV header line and the CSV log block (`main/main.cpp:40` and `:67-75`) are unchanged byte for byte. The sample, display and log intervals and the demo block are unchanged.
  - **Loop and stack.** No heap allocation in the loop and no large stack locals.
- verification:
  - `./scripts/test_host.sh` -> exits 0
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_UNIT_CELSIUS=y' sdkconfig` -> succeeds
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds; `grep -q '^CONFIG_OIL_GAUGE_UNIT_CELSIUS=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `. ~/esp/esp-idf/export.sh && F=/tmp/caterham-oil-gauge-build-f && mkdir -p $F && printf 'CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y\n' > $F/unit.defaults && idf.py -B $F -DIDF_TARGET=esp32s3 -DSDKCONFIG=$F/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;$F/unit.defaults" build 2>&1 | tee /tmp/caterham-oil-gauge-build-f.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-f.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y' /tmp/caterham-oil-gauge-build-f/sdkconfig` -> succeeds
  - `git diff <task base> -- main/main.cpp | grep -E '^[-+].*(CSV|snprintf\(temp)'` -> prints nothing
  - `grep -rn 'nvs_' main/ | grep -v '^main/main.cpp'` -> prints nothing
  - `git diff --stat <task base>` -> only the two files above
- human_verification: Request.md Verification steps 2 and 3, in the device session after T16.
  - Demo °F build: the labels read 100…300, amber appears at about `239` and red at about `266`, and the serial CSV still logs `temp_c` in °C.
  - Switch to Sweep, then power-cycle: Sweep comes back.
  - On first boot after `idf.py erase-flash`, the layout is Classic.
- notes:
  - **No host test.** `main.cpp` isn't host-testable. The decoding was test-first in T5 and T6.
  - **Approval.** request.md approves `nvs_flash` in `REQUIRES`.
  - **Includes.** `nvs_flash.h` and `nvs.h`. Keep the NVS code in two small functions in the anonymous namespace, next to `ticks_ms()` (`main/main.cpp:14-17`).
  - **Timing.** `nvs_commit` runs only after a user switch, so the 1 s CSV rhythm isn't affected (AC-15). The reviewer checks that the stack stays within the 8 KB main task.
- safety_notes: Invariant 5's "every `esp_err_t` is checked" applies to the new calls as well. REQ-D2 CSV is unchanged (AC-7). `nvs_flash_erase()` clears only the `nvs` partition (`partitions.csv:2`); nothing else is stored there.
- size: S

### T14: Document both layouts, the long-press, the °F option and the new display text
- kind: docs
- status: done
- depends_on: [T13]
- needs_human_approval: false
- install: none
- goal: Update the docs that describe the display, in the same change (AC-19, CLAUDE.md invariant 7). They cover both layouts, how to change layout, the °F menuconfig option, the unitless reading and its `<min`/`>max` form, the new hardware checks, and the advice not to change layout while driving.
- files_may_touch:
  - README.md
  - docs/ARCHITECTURE.md
  - docs/VALIDATION.md
  - docs/SAFETY.md
  - docs/CALIBRATION.md
- files_must_not_touch:
  - docs/spec.md
  - docs/changes/
  - main/
  - tests/
  - scripts/
  - hardware/
  - sdkconfig.defaults
  - dependencies.lock
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **README.md:**
    - describes Classic and Sweep;
    - explains how to change layout: press anywhere for at least 1 s; the choice is restored at power-up; Classic on first boot or if storage fails;
    - explains how to choose °F: `idf.py menuconfig` → Caterham oil temperature gauge → Display temperature unit; °C is the default, and the CSV stays in °C;
    - says uncalibrated firmware shows `ohm` in both layouts;
    - lists the new display and core files in the repository tree, and mentions both layouts in the "Display" row of the design-decisions table.
  - **docs/ARCHITECTURE.md:**
    - the display paragraph covers the frame built from host-tested core helpers, the two layouts built once and switched by hiding and showing, the long-press handled in the LVGL task, and `main.cpp` storing the layout in NVS only on change;
    - the `[LVGL gauge]` node in the diagram names both layouts;
    - the out-of-range lines (`:49-50`) show `<20` / `>140` style text, with the unit in the label.
  - **docs/VALIDATION.md:**
    - "Tests already exercised" lists the three new host suites;
    - the "not yet carried out" table gains rows for request.md Verification steps 1–4, with the local °F and demo-°F build commands;
    - the below-range row (`:29`) reads `<min` instead of `< min °C`.
  - **docs/SAFETY.md** gains a bullet:
    - touch does one thing only: a press of at least 1 s switches the layout;
    - it can't change the unit, calibration or alarms;
    - choose the layout before driving and don't change it while driving.
  - **docs/CALIBRATION.md:** only the parenthesis on line 31 changes, to `<min` / `>max` in the selected display unit.
  - **Accuracy.** Nothing claims a hardware check has passed.
- verification:
  - `grep -n -i 'long-press\|long press' README.md docs/SAFETY.md` -> at least one match in each file
  - `grep -n 'Sweep' README.md docs/ARCHITECTURE.md docs/VALIDATION.md` -> matches in all three files
  - `grep -n 'Fahrenheit\|°F' README.md docs/VALIDATION.md` -> matches in both files
  - `grep -n '< min °C\|> max °C\|< 20 °C\|> 140 °C\|OIL TEMPERATURE' README.md docs/ARCHITECTURE.md docs/VALIDATION.md docs/CALIBRATION.md docs/SAFETY.md` -> prints nothing
  - `./scripts/test_host.sh` -> exits 0
  - `git diff --stat <task base>` -> only the five files above
- human_verification: none
- notes:
  - **Line references.**
    - README: menuconfig `:19`, tree `:21-39`, design table `:43-54`.
    - ARCHITECTURE: diagram `:31`, display paragraph `:37`, out-of-range lines `:49-50`.
    - VALIDATION: tests `:5`, below-range row `:29`.
    - CALIBRATION: `:31`. SAFETY: display bullet `:15`.
  - **Demo °F command** for VALIDATION: the CLAUDE.md demo command with `printf 'CONFIG_OIL_GAUGE_DEMO=y\nCONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y\n'` and a separate build directory.
  - **Why `CALIBRATION.md`:** see Assumption 12.
  - **Fonts.** The font file and its licence are documented in T17, after they exist.
- safety_notes: Invariant 7 (docs move together). SAFETY.md gains the driving advice that the request requires.
- size: M

### T15: Generate the Classic digit font from LVGL's Montserrat Medium (lv_font_conv 1.5.3)
- kind: config
- status: done
- depends_on: [T14]
- needs_human_approval: true
- approved: PurpleSentinel, 2026-11-11, npm install --prefix /tmp/cr3-lvfc --ignore-scripts lv_font_conv@1.5.3 (the maintainer ran this exact command themselves on 2026-11-11, after the approval request)
- install:
  - npm install --prefix /tmp/cr3-lvfc --ignore-scripts lv_font_conv@1.5.3
- goal: Produce one uncompressed 4 bpp LVGL font source holding only the glyphs the Classic reading needs, at 148 px, from LVGL's bundled Montserrat Medium. Record the tool, version, exact command and licence next to it, and register it in the build. It needs no LVGL config change (AC-5).
- files_may_touch:
  - main/display/fonts/gauge_digits_148.c
  - main/display/fonts/Montserrat-OFL.txt
  - main/CMakeLists.txt
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/
  - main/display/GaugeDisplay.*
  - main/display/Layouts.h
  - main/display/ClassicLayout.cpp
  - main/display/SweepLayout.cpp
  - main/main.cpp
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - README.md
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **Install.** The approved install command is run exactly as written. Everything it writes stays in `/tmp/cr3-lvfc`, outside the repo.
  - **Generation command**, run from the repo root:
    `/tmp/cr3-lvfc/node_modules/.bin/lv_font_conv --font managed_components/lvgl__lvgl/scripts/built_in_font/Montserrat-Medium.ttf -r 0x20,0x2D,0x30-0x39,0x3C,0x3E --size 148 --bpp 4 --no-compress --no-prefilter --no-kerning --format lvgl --lv-include lvgl.h -o main/display/fonts/gauge_digits_148.c`
    - Its glyphs are space, `-`, `0`–`9`, `<` and `>` only.
    - The public symbol is `const lv_font_t gauge_digits_148`.
  - **Glyph checks.**
    - Each of `0`–`9` has `box_h` ≥ 100.
    - `bitmap_format = 0` (uncompressed) and `bpp = 4`.
    - No glyph exceeds the non-LARGE `lv_font_fmt_txt` limits (`box_h` ≤ 255, `adv_w` < 4096/16 px), so `sdkconfig.defaults` stays untouched.
  - **Header comment.** A comment added at the top of the file is the only hand edit. It records:
    - the tool and version (`lv_font_conv 1.5.3`), the exact install command and the exact generation command;
    - the source font: Montserrat-Medium.ttf from LVGL 9.4.0's `scripts/built_in_font/`;
    - the licence (SIL Open Font License 1.1, Copyright 2011 The Montserrat Project Authors), pointing to `Montserrat-OFL.txt` in the same folder.
  - **Licence file.** `main/display/fonts/Montserrat-OFL.txt` is a byte-identical copy of `managed_components/lvgl__lvgl/scripts/built_in_font/font_license/Montserrat/OFL.txt`.
  - **CMake.** `main/CMakeLists.txt` changes only by adding `"display/fonts/gauge_digits_148.c"` to `SRCS`.
  - **Nothing else generated is added to the repo.** The font isn't used yet; T16 switches Classic to it.
- verification:
  - `grep '"version"' /tmp/cr3-lvfc/node_modules/lv_font_conv/package.json` -> `"version": "1.5.3",`
  - The generation command above -> exits 0 and writes `main/display/fonts/gauge_digits_148.c`
  - `grep -c 'box_h = 1[0-9][0-9]' main/display/fonts/gauge_digits_148.c` -> 10 (the ten digits; `<`, `>` and `-` are shorter)
  - `grep -c 'U+00' main/display/fonts/gauge_digits_148.c` -> 14 (one per glyph)
  - `grep -n 'bitmap_format = 0\|\.bpp = 4\|const lv_font_t gauge_digits_148\|#include "lvgl.h"\|lv_font_conv 1.5.3' main/display/fonts/gauge_digits_148.c` -> shows each item
  - `cmp main/display/fonts/Montserrat-OFL.txt managed_components/lvgl__lvgl/scripts/built_in_font/font_license/Montserrat/OFL.txt` -> exit 0
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `./scripts/test_host.sh` -> exits 0
  - `git status --porcelain` -> only `main/CMakeLists.txt` and the new `main/display/fonts/` files
- human_verification: none
- notes:
  - **Why approval is needed.** The install isn't covered by the approvals in request.md, which explicitly exclude any dependency install. Approving this task approves exactly the one `npm install` command.
  - **No repo manifest.** The npm manifest and lockfile live in `/tmp/cr3-lvfc`. No repo manifest or lockfile changes.
  - **Size computation.**
    - In LVGL's `lv_font_montserrat_48.c`, the digits have `box_h` 34, which is 0.70 em.
    - The widest advance is `4`, at 514/16 px, which is 0.669 em.
    - At 148 px, the digits are about 104 px tall and the widest cell is about 99 px.
  - **Why `--lv-include lvgl.h`.** ESP-IDF doesn't define `LV_LVGL_H_INCLUDE_SIMPLE`, and `lvgl/lvgl.h` doesn't resolve, because the component directory is `lvgl__lvgl` (`managed_components/lvgl__lvgl/env_support/cmake/esp.cmake:11-14`).
  - **Stop and report, without editing the generated code beyond the header comment, if:**
    - the install fails (no node/npm, or no network);
    - a flag isn't recognised by 1.5.3;
    - the file doesn't compile warning-free;
    - any digit's `box_h` is below 100.
  - **Review scope.** The generated file is large but mechanical: review the command and the checks, not the bytes.
- safety_notes: No change to measurement, alarms, the LVGL configuration or `sdkconfig.defaults`. The font licence is carried with the font, as the request requires.
- size: M

### T16: Use the generated 148 px font for Classic's digits and fit the reading inside the ring
- kind: impl
- status: done
- depends_on: [T15]
- needs_human_approval: false
- install: none
- goal: Classic's 4 reading cells use `gauge_digits_148`, so the digits are at least 100 px tall. The ring and the texts are rearranged so the widest reading (`4999`, `>284`, `<-10`) overlaps nothing (AC-4, AC-5).
- files_may_touch:
  - main/display/ClassicLayout.cpp
  - main/display/Layouts.h
- files_must_not_touch:
  - tests/
  - scripts/
  - main/core/
  - main/display/GaugeDisplay.*
  - main/display/SweepLayout.cpp
  - main/display/fonts/
  - main/main.cpp
  - main/CMakeLists.txt
  - main/CalibrationData.h
  - main/Config.h
  - main/hal/
  - main/Kconfig.projbuild
  - main/idf_component.yml
  - hardware/
  - docs/
  - sdkconfig.defaults
  - dependencies.lock
  - partitions.csv
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **Font.** `gauge_digits_148` is declared with `extern "C" { LV_FONT_DECLARE(gauge_digits_148) }` and used for the 4 cells. The cell width is computed as in T8 from that font (about 99 px), so the block is about 396 px wide.
  - **Arc.** The arc's inner radius becomes at least 214 px, for example `lv_arc` 456×456 with a 14 px arc width on both parts. Its range, rotation, background angles and value rule are unchanged.
  - **Positions.**
    - The digits' glyph box is vertically centred on the screen centre.
    - The title sits above it, with the demo banner above the title.
    - The status and footer sit below, in the open bottom of the arc.
  - **Overlap rule.** For every text box corner (x, y) relative to the centre, x² + y² < (inner radius − 4)². The text boxes don't overlap each other.
  - **Compile-time check.** A `static_assert` on squared integer constants checks that the reading block fits inside the ring: (block width/2)² + (glyph box height/2)² ≤ (inner radius − 4)².
  - **Classic's digits no longer use `lv_font_montserrat_48`.**
- verification:
  - `./scripts/test_host.sh` -> exits 0
  - `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` -> prints nothing
  - The demo build exactly as in CLAUDE.md "Gates" -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-demo.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_DEMO=y' /tmp/caterham-oil-gauge-build-demo/sdkconfig` -> succeeds
  - `. ~/esp/esp-idf/export.sh && F=/tmp/caterham-oil-gauge-build-f && mkdir -p $F && printf 'CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y\n' > $F/unit.defaults && idf.py -B $F -DIDF_TARGET=esp32s3 -DSDKCONFIG=$F/sdkconfig -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;$F/unit.defaults" build 2>&1 | tee /tmp/caterham-oil-gauge-build-f.log` -> succeeds; `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build-f.log` -> prints nothing; `grep -q '^CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y' /tmp/caterham-oil-gauge-build-f/sdkconfig` -> succeeds
  - `grep -n 'gauge_digits_148' main/display/ClassicLayout.cpp` -> at least one match
  - `git diff --stat <task base>` -> only files from the allowlist
- human_verification: Request.md Verification steps 1 and 4, with photos.
  - The digits are clearly larger, at least 100 px (photo with a ruler).
  - The ones digit stays put across the demo sweep. Nothing overlaps.
  - With an ADS1115 and a test resistor fitted, `4999` fits.
  - `>140` and `<20` need a reading outside the table, which the demo curve (25–135 °C) never produces. Check them with a locally generated, never-committed calibration table and a decade box (CLAUDE.md "Human-only checks"), or record them as owed.
- notes:
  - **No host test.** Display code isn't host-testable. The cell padding was test-first in T1 and T2.
  - **Fit numbers.** With an inner radius of 214 px and a glyph box about 106 px high, the inner chord at the box edge is about 414 px against a 396 px block, leaving about 9 px each side. Take the real `box_h` and `adv_w` from the generated file's `glyph_dsc`.
  - **Positioning.** Position by the glyph box, not the label box: the label's `line_height` is larger than the digits.
- safety_notes: Display only. The colour precedence and the reading text are unchanged.
- size: M

### T17: Write the approved §1 and REQ-D1 wording into docs/spec.md and note the font licence in the README
- kind: docs
- status: done
- depends_on: [T16, T14]
- needs_human_approval: false
- install: none
- goal: Merge the amended requirements, as approved in request.md, into the requirements file. Point the README's licence section at the committed Montserrat-derived font.
- files_may_touch:
  - docs/spec.md
  - README.md
- files_must_not_touch:
  - docs/changes/
  - docs/ARCHITECTURE.md
  - docs/VALIDATION.md
  - docs/SAFETY.md
  - docs/CALIBRATION.md
  - main/
  - tests/
  - scripts/
  - hardware/
  - sdkconfig.defaults
  - dependencies.lock
  - .github/
  - .claude/
  - build/
  - build-*/
  - managed_components/
  - sdkconfig
  - sdkconfig.old
- acceptance_criteria:
  - **§1 "Out of scope"** (`docs/spec.md:30`): "touch interaction" becomes "touch interaction, except a long-press on the screen to change the display layout". Nothing else in §1 changes.
  - **REQ-D1** (`docs/spec.md:72-76`) is replaced by the approved "New" wording from request.md, kept in substance and order:
    - Layouts, Unit label, Main reading, Digit layout, Sweep segments, Always shown, Colour precedence (unchanged), Choosing a layout.
    - Its evidence line becomes H (`display_text_test.cpp`, `sweep_model_test.cpp`, `layout_choice_test.cpp`), B, HW.
  - **Unchanged:** REQ-C1 and every other requirement, the §3 index (kept by `cr.py`) and the evidence key.
  - **README "License":** one or two sentences saying that Classic's digits are a bitmap font generated from Montserrat Medium (SIL Open Font License 1.1), and that the licence and generation record are in `main/display/fonts/`. The repository tree lists `display/fonts/`.
- verification:
  - `grep -c 'touch interaction, except a long-press on the screen to change the display layout' docs/spec.md` -> 1
  - `grep -n 'half away from zero\|Sweep segments\|Choosing a layout' docs/spec.md` -> matches inside REQ-D1
  - `git diff <task base> -- docs/spec.md | grep '^[-+]' | grep -v '^[-+][-+]' | grep -c 'REQ-C1\|REQ-T\|REQ-M\|REQ-F\|REQ-B\|REQ-D2\|| CR-'` -> 0
  - `grep -n 'OFL\|Open Font License' README.md` -> at least one match
  - `./scripts/test_host.sh` -> exits 0
  - `git diff --stat <task base>` -> only the two files above
- human_verification: none
- notes:
  - The approved wording is in request.md under "Requirements amended", approved by PurpleSentinel on 2026-11-11.
  - Keep the existing formatting of `docs/spec.md`: bold IDs, nested bullets and an *Evidence:* line.
  - README "License" is at `:75-77` and the repository tree at `:21-39`.
- safety_notes: Records the amended REQ-D1. REQ-C1 ("uncalibrated firmware never shows °C or °F, and never shows a temperature scale") stays as it is.
- size: S

### 6. Execution order

1. T1: failing tests for the display text helpers
2. T2: display text helpers
3. T3: failing tests for the Sweep model
4. T4: Sweep model
5. T5: failing tests for the layout choice and long press
6. T6: layout choice and long press
7. T7: Kconfig unit choice
8. T8: Classic title, cells and MAX line from the core helpers
9. T9: layout interface and `ClassicLayout`
10. T10: Sweep ring, scale labels and small texts
11. T11: Sweep seven-segment digits
12. T12: long-press switching
13. T13: NVS, unit and layout in `main.cpp`
14. T14: docs (README, ARCHITECTURE, VALIDATION, SAFETY, CALIBRATION)
15. T15: font generation. **Needs approval** of the exact `npm install` command.
16. T16: Classic 148 px digits and ring fit
17. T17: `docs/spec.md` §1 / REQ-D1 and the README font licence

### 7. Risks

**Technical**
- **The font task (T15) may not run in the agent's environment.** It needs node/npm and network access for one install. It also assumes `lv_font_conv` 1.5.3 supports `--lv-include` and `--no-kerning`. If any of this fails, the task stops as blocked. Every other task is already done by then, so only Classic's digit size (AC-5) is waiting.
- **Generated C must build without `main/` warnings** under ESP-IDF's `-Wall -Wextra`. LVGL's own fonts use the same format, so the risk is low, and T15 runs both builds.
- **Redraw cost of 68 small objects** (40 ring segments and 28 digit segments). Updating only on change, and using objects with small bounding boxes, keeps 4 Hz redraws cheap. A full-ring `lv_arc` per segment would invalidate the whole ring.
- **Touch may not reach the screen.** Any clickable child swallows presses: `lv_obj_create` children are clickable by default. T9–T12 require the flag to be cleared, and T12's grep checks for a single handler.
- **NVS writes stall the main loop.** `nvs_commit` and flash operations briefly stall both cores, but only on a user switch. AC-15 needs the human's CSV-gap check.
- **Stack and allocation.** Watch the 8 KB main-task stack when NVS calls are added (T13). `lv_label_set_text` allocates in LVGL's heap; the plan uses `lv_label_set_text_static` to keep render allocation-free.
- **Float boundaries in tests.** T3 uses values that are exact in float. A careless choice, such as 139 °C, would make the tests flaky.
- **Intermediate states.** Between T10 and T12, Sweep can't be reached on hardware. This is expected; the human checks happen after T16.

**Safety**
- **Stale values after a switch.** Mitigated by updating both layouts on every render (T10).
- **Fail-closed text.** Uncalibrated readings must never show `°`, a temperature, segments or scale numbers. This is pinned by host tests in both builds (T1, T3) and by helpers that query `calibrationValid()` themselves.
- **Rounding near thresholds.** 114.5–114.99 °C shows as `115` while still green. This is the accepted default (request.md, Open questions).
- **The CSV must stay byte-identical.** T13's diff grep checks it.

**Scope**
- **`docs/CALIBRATION.md`** gets a one-line edit under invariant 7 but isn't in the request's docs list (Assumption 12).
- **The Classic arc is enlarged and thinned** so the 148 px reading fits (Assumption 6).
- **The Sweep ghost reads `8888`, not `888`** (Assumption 8).
- **New constants live in core headers, not `main/Config.h`,** because the request keeps `Config.h` out of scope (Assumption 2).

**Repo observations**
- `tests/__pycache__/test_cr_tool.cpython-314.pyc` and `scripts/__pycache__/cr.cpython-314.pyc` have no matching sources. They are untracked and ignored, and `unittest` discovery skips them, so they're harmless.
- No embedded instructions or other suspicious content were found in the files read.

### 8. Out of scope / follow-ups

- **Add a °F variant to CI** (`.github/workflows/ci.yml`). The request excludes it; it's a separate tooling PR.
- **Update `docs/PORTING.md:8,17-18`.** It still points at `GaugeDisplay.cpp` for widgets and typography, which now live in `ClassicLayout.cpp` and `SweepLayout.cpp`.
- **Add the new core files to the "Key code to mutate" list** in CLAUDE.md "Mutation testing": `DisplayText.cpp`, `SweepModel.cpp` and `LayoutChoice.cpp`. This is a CLAUDE.md change.
- **Move the REQ-D1 colour precedence into `main/core/`** so it can be host-tested. It's moved verbatim in this CR.
- **Already excluded by the request:** choosing the unit on screen, other touch controls, a third layout, alarm-zone markings, brightness control.

### 9. Definition of done

**Gates (CLAUDE.md "Gates" and AC-18)**
- `./scripts/test_host.sh` exits 0. It runs the existing C++ suites, the display-text, sweep-model and layout-choice suites (in both builds where they apply), and the Python tests.
- **Normal build:** `. ~/esp/esp-idf/export.sh && idf.py build 2>&1 | tee /tmp/caterham-oil-gauge-build.log` succeeds.
  - `grep -n 'main/.*warning:' /tmp/caterham-oil-gauge-build.log` prints nothing.
  - `grep -q '^CONFIG_OIL_GAUGE_UNIT_CELSIUS=y' sdkconfig` succeeds.
- **Demo build:** as in CLAUDE.md, it succeeds.
  - The `main/` warnings grep prints nothing.
  - `grep -q '^CONFIG_OIL_GAUGE_DEMO=y'` and `grep -q '^CONFIG_OIL_GAUGE_UNIT_CELSIUS=y'` succeed on `/tmp/caterham-oil-gauge-build-demo/sdkconfig`.
- **Local °F build:** as in T7, T13 and T16, it succeeds.
  - The `main/` warnings grep prints nothing.
  - `CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y` is set.
- **CI:** `Host tests`, `Firmware build (normal)` and `Firmware build (demo)` all pass.

**Reviewer checks**
- **CR-level review** against AC-1 to AC-19, the invariants and the docs rule. This includes the AC-7 diff check that none of the "Must not change" files changed, and the AC-17 check.
- **Mutation check** (request.md Verification) on `DisplayText.cpp`, `SweepModel.cpp` and `LayoutChoice.cpp`:
  - half-to-even rounding;
  - converting to °F after rounding;
  - not converting the limits or `MAX`;
  - a `°` in uncalibrated text;
  - a unit in the reading;
  - an off-by-one in the lit count, lighting beyond the table maximum, or lighting segments when uncalibrated or on a fault;
  - scale labels visible when uncalibrated;
  - an invalid stored layout not falling back to Classic.

**Human verification still owed** (request.md Verification). First check the device: `lsusb` shows `303a:1001`, and you flash through `/dev/serial/by-id/usb-Espressif_*`. Take photos throughout.
1. **Demo build, °C, both layouts.**
   - The title and unit are right, and there's no unit beside the reading.
   - The ones digit is fixed, nothing overlaps, and the digits are at least 100 px (Classic) and at least 72 px (Sweep).
   - Sweep's segments track the reading and its labels read 40…140.
   - The colour goes green → amber → red → amber → green.
   - The ghost `8888` is visible.
2. **Demo build, °F, both layouts.**
   - The labels read 100…300.
   - Amber appears at about `239` and red at about `266`.
   - The CSV still logs `temp_c` in °C.
3. **Layout switching.**
   - A press of at least 1 s switches the layout; a tap and a swipe don't.
   - Five switches leave no CSV gap over 1.2 s.
   - After a power cycle, the last layout comes back.
4. **Normal uncalibrated build, both layouts.**
   - `ohm`, `--` and `ADC UNAVAILABLE` show, along with the status text.
   - In Sweep, no segments are lit and no scale numbers show.
   - With an ADS1115 and a test resistor fitted, a 4-digit ohms reading fits.
5. **Flash the normal °C build back.**

Still owed besides: `>140` and `<20` on hardware. They need a reading outside the table, which means a local, never-committed calibration table and a decade box; otherwise record them as owed. CR-2's owed hardware check is unaffected.

Files read for this plan:
- the CR: `/home/mrsqr/codex/Caterham-Oil-Temperature-Gauge/docs/changes/CR-3/request.md` and `tasks.md`;
- the requirements: `/home/mrsqr/codex/Caterham-Oil-Temperature-Gauge/docs/spec.md`;
- `main/display/GaugeDisplay.{h,cpp}`, `main/main.cpp`, `main/CMakeLists.txt`, `main/Kconfig.projbuild`, `main/Config.h`, `main/core/*`, `scripts/test_host.sh`, `tests/*.cpp`;
- the docs, the BSP and `esp_lvgl_adapter` sources, and LVGL 9.4's font and indev headers.

---

## Rounds

One row per review round, kept by the caller with cr.py start-round and record-review.

| Task | Round | Base | Review commit | Verdict |
|------|-------|------|---------------|---------|
| T1 | 1 | cd28248e19b2d08ad78fd64d2511ecb7e315bf37 | 75f20fb9be50389fd829d2724f545ca9dfba21c4 | fix |
| T1 | 2 | cd28248e19b2d08ad78fd64d2511ecb7e315bf37 | 316a9acb4210111b0bd26e192a4d9e18ad505e37 | fix |
| T1 | 3 | cd28248e19b2d08ad78fd64d2511ecb7e315bf37 | df72d02fb8b002348b045d5524a85d212ce39900 | pass |
| T2 | 1 | 35b8fe024748f4ed89aea8b609f454ad33c51c2d | 9f5ed3d67fab921ce84c1e4e00d9eb053d9b0475 | fix |
| T2 | 2 | 35b8fe024748f4ed89aea8b609f454ad33c51c2d | c35daee05b769bf08fa8819cbba662b21395a9ab | pass |
| T3 | 1 | d691858128e9c0bf701167375df67843b325cc3c | a237b381b8c18d77dfa9f1b109b95b1036ffbc57 | fix |
| T3 | 2 | d691858128e9c0bf701167375df67843b325cc3c | c5fe8add8b651c907f29507e4f618536d92a6497 | pass |
| T4 | 1 | f24185f8a250f77f424d61cb890ce2c6eb3e53ad | 8f198be1744961233152cb9c68b2164983ca6fd2 | pass |
| T5 | 1 | 4883a907f4274571a1079ab5ed8d8d99e6941489 | ab317f2b0aa1ea62c58b705686317e2c5848f9e5 | fix |
| T5 | 2 | 4883a907f4274571a1079ab5ed8d8d99e6941489 | d5a16c2a0d0cf7ba06ce9560fb50645dc9d81d38 | pass |
| T6 | 1 | e78d12f63e6b382865d17141dc3c57859968f5b1 | 6c9fb7ae93dd23db3bb2c5b8265f2274d36e6a8b | escalate |
| T5 | 3 | d2d9bd23c1f66d9da8b698340f1014d0905375fd | 6c490fdc1c2fdde1c0d69fa16c32995caf48914f | pass |
| T6 | 2 | e78d12f63e6b382865d17141dc3c57859968f5b1 | 32884e1fd6bb71a72f6f974e2566707f36fb4231 | pass |
| T7 | 1 | a441d22fdb9257c0ed9a45966da09a083b49b26e | 2208fcf8b2c0149d357287235316fff849495d5f | pass |
| T8 | 1 | 35c58619b6b4905a9096b002fba1a873e2977976 | bb408de25dd20e4fb5832fd44f4e91f08c07951c | pass |
| T9 | 1 | 04367decf1cb1784de1616f51c8458a93a6a47ac | 0e25301a32b8395f64f7df72c65d10360c11fda1 | pass |
| T10 | 1 | 86fee404af0975310d6efc64b58128d46231b532 | a2908bb020b56a9ddbc3ce9e26e9246883c47c50 | fix |
| T10 | 2 | 86fee404af0975310d6efc64b58128d46231b532 | 026de85352b363a4566935bf8c5b0ca9bab26ee4 | pass |
| T11 | 1 | a151af6370d905bff6621bf6a39710d17b17ccb9 | 5344c23e7d865c600756f7695c22a6a232bb3ef8 | pass |
| T12 | 1 | e037d667b71b4831c9d3dc52b2a03d85955c0118 | 56c4210bdf38330db53b1d5747ac7719758221f7 | pass |
| T13 | 1 | c21e48b28265a484eb915b40eed20ae6fe5f896f | 311d21ecc879213b9e635c06b953a4f1551a9b62 | pass |
| T14 | 1 | 1ca045595d4de90244b87d87c6ab2b2ff7d57a76 | 1aeb316fe73731226b667a494f42f774cc78d1a5 | fix |
| T14 | 2 | 1ca045595d4de90244b87d87c6ab2b2ff7d57a76 | a7d8227fa8717cde546e61e60294b14ccd4bd101 | fix |
| T14 | 3 | 1ca045595d4de90244b87d87c6ab2b2ff7d57a76 | 54a344d69f977870b8d06615fa39100daa781c47 | pass |
| T15 | 1 | 713799c3f0f6581556bd0f2040319c0c9b81a4e6 | 0c90f576ea8b1954a5342e4de7aa3f920f442582 | pass |
| T16 | 1 | df3b9203834192086b6b91bb9d16857d1cc51779 | 6e9e555cfb3ce2642b8e193ad51a8aac1e850d0a | pass |
| T17 | 1 | 7619a3f7347ccf419d3ea8d14ecf16e507dd4f7a | 7b9a4c9fd8598c0e9fb81649229e58f7c28df06d | pass |
| CR-3 | 1 | cd28248e19b2d08ad78fd64d2511ecb7e315bf37 | c82a2c38a675429eb7f1e2dd17999eca9dc0ca58 | fix |
| T17 | 2 | 37019eec7e71a2f62243757a1de1469dd7b3576e | 9d2163352a953f51a0187fccd8dca8c4555c4884 | pass |
| CR-3 | 2 | cd28248e19b2d08ad78fd64d2511ecb7e315bf37 |  |  |

---

## Findings

Review findings, kept by the caller. Columns and statuses: see "Review rounds" in the workflow document.

| ID | Task | Raised | Severity | Title | Status | Closed | Notes |
|----|------|--------|----------|-------|--------|--------|-------|
| T1-F1 | T1 | 1 | major | Half-to-even rounding on the ohms reading and footer ohms survives (no exact-half ohms input) | fixed | 2 |  |
| T1-F2 | T1 | 1 | major | Converting MAX to F after rounding survives (112.5 C gives 235 either way) | fixed | 2 |  |
| T1-F3 | T1 | 1 | major | Above-range MAX tests pass max_c equal to the table max, so printing max_c instead of calibrationMaxC() survives | fixed | 2 |  |
| T1-F4 | T1 | 1 | minor | roundHalfAway \|value\| >= 1e6 contract clause untested | fixed | 2 |  |
| T1-F5 | T1 | 1 | nit | Non-finite and non-positive ohms checks run only in the demo build | fixed | 2 |  |
| T1-F6 | T1 | 2 | blocker | Footer assertion at line 138 expects '123 ohm' from a Normal reading without has_temperature, contradicting the T2 footer contract | fixed | 3 |  |
| T2-F1 | T2 | 1 | blocker | No-maximum threshold -200 is a literal, not the named constant the AC requires | fixed | 2 |  |
| T2-F2 | T2 | 1 | nit | Header and namespace style differs from neighbouring core .cpp files | fixed | 2 |  |
| T3-F1 | T3 | 1 | major | Lit count only tested with Status::Normal; Cold/Amber/Red readings unpinned (dark ring in alarm and Red full-ring mutants survive) | fixed | 2 |  |
| T3-F2 | T3 | 1 | major | Fault and BelowRange guards never exercised in the calibrated build (guard removal and latched-Red fault mutants survive) | fixed | 2 |  |
| T3-F3 | T3 | 1 | minor | Only NaN tested as non-finite temperature; +inf/-inf untested | fixed | 2 |  |
| T4-F1 | T4 | 1 | nit | The '-' and blank seven-segment masks are bare literals outside the mask table | accepted | 1 | Style nit; every mask is pinned by tests, no behaviour impact (caller, under maintainer delegation) |
| T5-F1 | T5 | 1 | major | No second long press on the same detector, and every press is at (100,100) (fire-once-ever and first-press-point mutants survive) | fixed | 2 |  |
| T5-F2 | T5 | 1 | major | Slop checked only as one jump before the threshold (slow-swipe and time-before-slop mutants survive) | fixed | 2 |  |
| T5-F3 | T5 | 1 | major | Hold-before-press assertion passes only because of its coordinates (armed-from-construction mutant with zero origin survives) | fixed | 2 |  |
| T5-F4 | T5 | 1 | nit | Literal 1000 stands in for kLongPressMs at line 42 | fixed | 2 |  |
| T6-F1 | T6 | 1 | major | Press-point x/y confusion survives the layout tests (every test press is on the diagonal x == y) | follow-up | 2026-11-11 | Decision: test gap, moved to T5-F5 (T5 reopened); T6 code unchanged and re-reviewed after |
| T6-F2 | T6 | 1 | major | Restarting a press with a second press() before release() is not tested | follow-up | 2026-11-11 | Decision: test gap, moved to T5-F6 (T5 reopened); T6 code unchanged and re-reviewed after |
| T5-F5 | T5 | 2 | major | No off-diagonal press point: an x/y swap in press or hold survives (from T6-F1) | fixed | 3 |  |
| T5-F6 | T5 | 2 | major | No re-press without release: a press() that keeps the old time and point survives (from T6-F2) | fixed | 3 |  |
| T8-F1 | T8 | 1 | minor | Unknown reading character renders as a blank cell instead of failing closed to -- | follow-up | 1 | Carried into T9, which moves this code into ClassicLayout (same files); the T9 implementer is told to fix it and the T9 reviewer to check it |
| T8-F2 | T8 | 1 | nit | Cells start blank before the first render instead of the previous -- placeholder | follow-up | 1 | Carried into T9, which moves this code into ClassicLayout (same files); the T9 implementer is told to fix it and the T9 reviewer to check it |
| T8-F3 | T8 | 1 | nit | Per-cell truncating offset gives uneven pitch for odd cell widths (latent until T16) | follow-up | 1 | Carried into T9, which moves this code into ClassicLayout (same files); the T9 implementer is told to fix it and the T9 reviewer to check it |
| T8-F4 | T8 | 1 | nit | Stale ohm comment in render and now-unused Calibration.h and cstdio includes | follow-up | 1 | Carried into T9, which moves this code into ClassicLayout (same files); the T9 implementer is told to fix it and the T9 reviewer to check it |
| T9-F1 | T9 | 1 | nit | Dash cell string picked by bare index kCellText[11]; no compile-time check tying kCellChars to kCellText | follow-up | 1 | Carried into T16 (ClassicLayout.cpp is in its allowlist): pick the dash via cellText('-') and static_assert the table lengths |
| T9-F2 | T9 | 1 | nit | Header lock note covers build() and render() but not setVisible(), which also calls LVGL | follow-up | 1 | Carried into T10 (Layouts.h is in its allowlist): extend the lock note to every LayoutView method |
| T10-F1 | T10 | 1 | blocker | Sweep shows OIL degC / OIL ohm (Classic title) where the criterion requires OIL TEMP below the unit | fixed | 2 |  |
| T10-F2 | T10 | 1 | minor | SweepLayout adds about 1.2 KB to the 8 KB main-task stack (app_main frame now 0x610) | fixed | 2 |  |
| T10-F3 | T10 | 1 | nit | Sweep unit, title and status labels show LVGL default Text until the first render | fixed | 2 |  |
| T11-F1 | T11 | 1 | nit | Sweep digits start blank, while Classic starts as -- | accepted | 1 | Cosmetic, one loop pass before the first render; a dim 8888 is not a plausible value (caller, under maintainer delegation) |
| T12-F1 | T12 | 1 | nit | The no-switch sentinel 0xFF is a bare literal in three places | accepted | 1 | Style nit in one small file; no later task may touch GaugeDisplay, and the three uses sit within six lines (caller, under maintainer delegation) |
| T14-F1 | T14 | 1 | major | VALIDATION claims a host test covers Sweep ring colour | fixed | 3 |  |
| T14-F2 | T14 | 1 | minor | SAFETY calibration bullet still names only °C | fixed | 2 |  |
| T14-F3 | T14 | 1 | minor | Demo °C validation row omits the digit-size check | fixed | 2 |  |
| T14-F4 | T14 | 1 | nit | Ambiguous wording in README Display text | fixed | 2 |  |
| T16-F1 | T16 | 1 | minor | Compile-time fit check uses font metrics copied by hand (kCellMax, kGlyphH), not tied to the runtime cell width and line height | follow-up | 1 | Correct today (runtime width 99 and line height 106 match the constants); tie them to the font at runtime in a later CR, before the font is ever regenerated (caller, under maintainer delegation) |
| T16-F2 | T16 | 1 | nit | boxInside computes max(\|top\|,\|top+h\|) in one unreadable nested ternary | accepted | 1 | Readability only; reviewer verified it by hand and by mutation |
| T17-F1 | T17 | 1 | minor | REQ-D1 says the MAX value is always shown; it is shown only while calibrated with a reading in or beyond the table (request.md amended) | follow-up | 2026-11-11 | Carried into the CR-level review: the reviewer is told to check docs/spec.md REQ-D1 against the amended request.md wording, so a finding there reopens T17 |
| T17-F2 | T17 | 1 | blocker | docs/spec.md REQ-D1 still has the superseded 'Always shown' MAX bullet, not the amended 'Shown in both layouts' wording | fixed | 2 | CR-3 review round 1 |
| T14-F5 | T14 | 3 | minor | VALIDATION.md expects a ghost 888 but the 4-cell Sweep layout shows 8888 | follow-up | 2026-11-11 | Fixed in T14's reopened round (the one change that round makes); the reviewer checks it |

---

## Progress log

Newest last. One row per implementer run, review round, approval, escalation or human check.

| Date | Task | Event | Result | Notes |
|------|------|-------|--------|-------|
| 2026-11-11 | CR-3 | Maintainer decisions | active | All defaults accepted; four approvals recorded; plan review delegated to the caller; stop at a green PR |
| 2026-11-11 | CR-3 | Plan saved | 17 tasks | Planner output saved; reviewed by the caller under the maintainer's delegation |
| 2026-11-11 | CR-3 | Plan approved | go | Plan review delegated to the caller by the maintainer (2026-11-11) |
| 2026-11-11 | T1 | Implementer round 1 | done | files: scripts/test_host.sh, tests/display_text_test.cpp; gates: tests=fail |
| 2026-11-11 | T1 | Review round 1 | fix | raised: T1-F1, T1-F2, T1-F3, T1-F4, T1-F5 |
| 2026-11-11 | T1 | Implementer round 2 | done | files: tests/display_text_test.cpp; gates: tests=fail; responses: T1-F1 fixed, T1-F2 fixed, T1-F3 fixed, T1-F4 fixed, T1-F5 fixed |
| 2026-11-11 | T1 | Review round 2 | fix | raised: T1-F6; closed: T1-F1 fixed, T1-F2 fixed, T1-F3 fixed, T1-F4 fixed, T1-F5 fixed |
| 2026-11-11 | T1 | Implementer round 3 | done | files: tests/display_text_test.cpp; gates: tests=fail; responses: T1-F6 fixed |
| 2026-11-11 | T1 | Review round 3 | pass | closed: T1-F6 fixed |
| 2026-11-11 | T2 | Implementer round 1 | done | files: main/CMakeLists.txt, main/core/DisplayText.cpp, main/core/DisplayText.h; gates: tests=pass build-normal=pass build-demo=pass |
| 2026-11-11 | T2 | Review round 1 | fix | raised: T2-F1, T2-F2 |
| 2026-11-11 | T2 | Implementer round 2 | done | files: main/core/DisplayText.cpp; gates: tests=pass build-normal=pass build-demo=pass; responses: T2-F1 fixed, T2-F2 fixed |
| 2026-11-11 | T2 | Review round 2 | pass | closed: T2-F1 fixed, T2-F2 fixed |
| 2026-11-11 | T3 | Implementer round 1 | done | files: scripts/test_host.sh, tests/sweep_model_test.cpp; gates: tests=fail |
| 2026-11-11 | T3 | Review round 1 | fix | raised: T3-F1, T3-F2, T3-F3 |
| 2026-11-11 | T3 | Implementer round 2 | done | files: tests/sweep_model_test.cpp; gates: tests=fail; responses: T3-F1 fixed, T3-F2 fixed, T3-F3 fixed |
| 2026-11-11 | T3 | Review round 2 | pass | closed: T3-F1 fixed, T3-F2 fixed, T3-F3 fixed |
| 2026-11-11 | T4 | Implementer round 1 | done | files: main/CMakeLists.txt, main/core/SweepModel.cpp, main/core/SweepModel.h; gates: tests=pass build=pass build-demo=pass |
| 2026-11-11 | T4 | Review round 1 | pass | raised: T4-F1 |
| 2026-11-11 | T5 | Implementer round 1 | done | files: scripts/test_host.sh, tests/layout_choice_test.cpp; gates: tests=fail |
| 2026-11-11 | T5 | Review round 1 | fix | raised: T5-F1, T5-F2, T5-F3, T5-F4 |
| 2026-11-11 | T5 | Implementer round 2 | done | files: tests/layout_choice_test.cpp; gates: tests=fail; responses: T5-F1 fixed, T5-F2 fixed, T5-F3 fixed, T5-F4 fixed |
| 2026-11-11 | T5 | Review round 2 | pass | closed: T5-F1 fixed, T5-F2 fixed, T5-F3 fixed, T5-F4 fixed |
| 2026-11-11 | T6 | Implementer round 1 | done | files: main/CMakeLists.txt, main/core/LayoutChoice.cpp, main/core/LayoutChoice.h; gates: tests=pass build=pass demo-build=pass |
| 2026-11-11 | T6 | Escalated | T6-F1 |  |
| 2026-11-11 | T6 | Escalated | T6-F2 |  |
| 2026-11-11 | T6 | Review round 1 | escalate | raised: T6-F1, T6-F2; escalated: T6-F1, T6-F2 |
| 2026-11-11 | T6 | Decision | reopen-T5 | Caller under maintainer delegation: reopen T5 to add an off-diagonal press point and a re-press-without-release case; then re-review T6 against them |
| 2026-11-11 | T5 | Implementer round 3 | done | files: tests/layout_choice_test.cpp; gates: tests=pass; responses: T5-F5 fixed, T5-F6 fixed |
| 2026-11-11 | T5 | Review round 3 | pass | closed: T5-F5 fixed, T5-F6 fixed |
| 2026-11-11 | T6 | Review round 2 | pass |  |
| 2026-11-11 | T7 | Implementer round 1 | done | files: main/Kconfig.projbuild; gates: tests=pass build-normal=pass build-demo=pass build-f=pass |
| 2026-11-11 | T7 | Review round 1 | pass |  |
| 2026-11-11 | T8 | Implementer round 1 | done | files: main/display/GaugeDisplay.cpp, main/display/GaugeDisplay.h; gates: tests=pass build-normal=pass build-demo=pass |
| 2026-11-11 | T8 | Review round 1 | pass | raised: T8-F1, T8-F2, T8-F3, T8-F4 |
| 2026-11-11 | T9 | Scope note | T8 follow-ups | T9 also fixes T8-F1..F4 and the subtitle allocation (lv_label_set_text_static), inside its own allowlist |
| 2026-11-11 | T9 | Implementer round 1 | done | files: main/CMakeLists.txt, main/display/ClassicLayout.cpp, main/display/GaugeDisplay.cpp, main/display/GaugeDisplay.h, main/display/Layouts.h; gates: tests=pass build=pass demo-build=pass |
| 2026-11-11 | T9 | Review round 1 | pass | raised: T9-F1, T9-F2 |
| 2026-11-11 | T10 | Implementer round 1 | done | files: main/CMakeLists.txt, main/display/GaugeDisplay.cpp, main/display/GaugeDisplay.h, main/display/Layouts.h, main/display/SweepLayout.cpp; gates: tests=pass build=pass demo-build=pass |
| 2026-11-11 | T10 | Review round 1 | fix | raised: T10-F1, T10-F2, T10-F3 |
| 2026-11-11 | T10 | Implementer round 2 | done | files: main/display/Layouts.h, main/display/SweepLayout.cpp; gates: tests=pass build=pass demo-build=pass; responses: T10-F1 fixed, T10-F2 fixed, T10-F3 fixed |
| 2026-11-11 | T10 | Review round 2 | pass | closed: T10-F1 fixed, T10-F2 fixed, T10-F3 fixed |
| 2026-11-11 | T11 | Implementer round 1 | done | files: main/display/Layouts.h, main/display/SweepLayout.cpp; gates: tests=pass build=pass demo-build=pass |
| 2026-11-11 | T11 | Review round 1 | pass | raised: T11-F1 |
| 2026-11-11 | T12 | Implementer round 1 | done | files: main/display/GaugeDisplay.cpp, main/display/GaugeDisplay.h; gates: tests=pass build=pass demo-build=pass |
| 2026-11-11 | T12 | Review round 1 | pass | raised: T12-F1 |
| 2026-11-11 | T13 | Implementer round 1 | done | files: main/CMakeLists.txt, main/main.cpp; gates: tests=pass build=pass demo-build=pass f-build=pass |
| 2026-11-11 | T13 | Review round 1 | pass |  |
| 2026-11-11 | T13 | Verification note | nvs grep wording | The plan's grep for nvs_ outside main.cpp matches the approved nvs_flash REQUIRES entry; limited to source files it prints nothing (reviewer and caller agree) |
| 2026-11-11 | T14 | Implementer round 1 | done | files: README.md, docs/ARCHITECTURE.md, docs/CALIBRATION.md, docs/SAFETY.md, docs/VALIDATION.md; gates: tests=pass |
| 2026-11-11 | T14 | Review round 1 | fix | raised: T14-F1, T14-F2, T14-F3, T14-F4 |
| 2026-11-11 | T14 | Implementer round 2 | done | files: README.md, docs/ARCHITECTURE.md, docs/SAFETY.md, docs/VALIDATION.md; gates: tests=pass; responses: T14-F1 fixed, T14-F2 fixed, T14-F3 fixed, T14-F4 fixed |
| 2026-11-11 | T14 | Review round 2 | fix | closed: T14-F2 fixed, T14-F3 fixed, T14-F4 fixed |
| 2026-11-11 | T14 | Implementer round 3 | done | files: docs/VALIDATION.md; gates: tests=pass; responses: T14-F1 fixed |
| 2026-11-11 | T14 | Review round 3 | pass | closed: T14-F1 fixed |
| 2026-11-11 | T15 | Approval | approved | PurpleSentinel, 2026-11-11, npm install --prefix /tmp/cr3-lvfc --ignore-scripts lv_font_conv@1.5.3 (the maintainer ran this exact command themselves on 2026-11-11, after the approval request) |
| 2026-11-11 | T15 | Implementer round 1 | done | files: main/CMakeLists.txt, main/display/fonts/Montserrat-OFL.txt, main/display/fonts/gauge_digits_148.c; gates: tests=pass build=pass demo-build=pass |
| 2026-11-11 | T15 | Review round 1 | pass |  |
| 2026-11-11 | T16 | Implementer round 1 | done | files: main/display/ClassicLayout.cpp; gates: tests=pass build=pass demo-build=pass f-build=pass |
| 2026-11-11 | T16 | Review round 1 | pass | raised: T16-F1, T16-F2 |
| 2026-11-11 | T17 | Implementer round 1 | done | files: README.md, docs/spec.md; gates: tests=pass |
| 2026-11-11 | T17 | Review round 1 | pass |  |
| 2026-11-11 | T17 | Decision | amend-req-d1 | Caller under maintainer delegation: REQ-D1 'Always shown' amended in request.md to match the fail-closed footer; T17 to carry the amended wording into docs/spec.md |
| 2026-11-11 | T17 | Escalated | T17-F1 |  |
| 2026-11-11 | T17 | Decision | must-fix | Caller under maintainer delegation: T17-F1 must be fixed (carry the amended REQ-D1 wording into docs/spec.md); it stays escalated until fixed |
| 2026-11-11 | T17 | Decision | to-cr-review | cr.py does not start a fix round after a pass; T17-F1 is routed through the CR-level review instead |
| 2026-11-11 | CR-3 | Review round 1 | fix | raised: T17-F2, T14-F5 |
| 2026-11-11 | T14 | Escalated | T14-F5 |  |
| 2026-11-11 | T14 | Decision | must-fix | Caller under maintainer delegation: T14-F5 must be fixed (the hardware checklist must describe the 8888 ghost the firmware draws) |
| 2026-11-11 | T14 | Follow-up note | T14-F5 open as follow-up | Correction to the T14-F5 note: cr.py closes a reopened task with no open findings without a new round, so docs/VALIDATION.md still says ghost 888. The human-check issue and the PR state the correct 8888; fix the doc in a follow-up |
| 2026-11-11 | T17 | Implementer round 2 | done | files: docs/spec.md; gates: tests=pass; responses: T17-F2 fixed |
| 2026-11-11 | T17 | Review round 2 | pass | closed: T17-F2 fixed |
