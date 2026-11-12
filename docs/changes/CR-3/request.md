# CR-3: Display: selectable layouts (classic and segmented sweep), larger fixed-position digits, °F option

- **Status:** done. hardware checks owed (#24, #25)  (keep the docs/spec.md §3 index in step)
- **Issue:** [#20](https://github.com/ctrmint/Caterham-Oil-Temperature-Gauge/issues/20). *The scope was widened by the maintainer on 2026-11-11, while this was still a draft, to add a second, segmented-sweep layout and an on-screen way to choose between the layouts. The issue's text covers only the first part.*
- **Motivation:**
  - This is feedback from the first run on hardware: the demo build on a Waveshare 31261, 2026-11-11 (`docs/VALIDATION.md`).
  - **The reading is too small.** It uses the largest built-in font, Montserrat 48 px, on a 466 px screen.
  - **The reading moves sideways** as the digits change. The digits are proportional, and the label is re-centred on every update.
  - **The title and unit waste space.** `OIL TEMPERATURE` takes up a line, and the `°C` beside the reading takes room the digits need.
  - **There is no Fahrenheit option.**
  - **A second look is wanted:** an aftermarket-style face with a segmented sweep and seven-segment digits ([reference photo](https://s.turbifycdn.com/aah/supra/electronic-oil-temperature-gauge-191.jpg)). Take the general style only: no third-party names or logos, and the photo is not used as an asset. The user chooses between the two layouts on the gauge itself.
  - **Measured on the current code** (host check, discarded):
    - Uncalibrated firmware shows the resistance as up to 4 digits (`4999`).
    - The demo table spans 20–140 °C, which is 68–284 °F.
    - `%.0f` rounds halves to even (112.5 → `112`).
  - **Checked in the firmware configuration:**
    - LVGL's `lv_scale` and `lv_arc` are enabled.
    - Compressed fonts, FreeType and TinyTTF are disabled.
    - The Waveshare BSP registers the touch controller as an LVGL input device.
    - The `nvs` partition exists, but nothing calls `nvs_flash_init` yet.
- **Kind of work:** feature
- **Layouts:**
  - **Classic:** today's layout, with this CR's fixes.
    - The title reads `OIL °C` / `OIL °F` / `OIL ohm`.
    - Large digits sit in the centre, with no unit.
    - The status line, the bottom line and the existing arc stay.
  - **Sweep:** the reference style.
    - A ring of separate segments lights from the scale's start up to the reading, with numbered scale labels inside the ring.
    - Seven-segment digits sit in the centre, with unlit "ghost" segments (`888`) drawn dimly behind them.
    - A small unit (`°C` / `°F` / `ohm`) sits above `OIL TEMP`, at the lower right of the digits.
    - The status text, the `MAX` line and the demo banner are small and placed below.
- **Scope:**
  - May change:
    - `main/display/` (new files allowed), split so each layout builds and renders its own widgets behind a common interface;
    - new generated font sources under `main/display/`, holding only the characters the readings need. Record each one's generation command and licence next to it;
    - `main/CMakeLists.txt`, only to register the new sources and add `nvs_flash` to `REQUIRES` (approval-relevant);
    - `main/Kconfig.projbuild`, for a display-unit choice with °C as the default (approval-relevant);
    - `main/core/`, for new pure helpers. They cover the unit conversion and rounding, the title, unit, reading and `MAX` text, the lit-segment count and scale labels, the layout cycle, and decoding the stored layout value. They take the unit and layout as parameters and have no ESP-IDF includes;
    - `main/main.cpp`, only to initialise NVS, load and save the layout choice, and pass the unit and layout to the display;
    - `tests/`, and `scripts/test_host.sh` only to register a new test file;
    - the docs that describe the display: `README.md` (including how to change layout), `docs/ARCHITECTURE.md`, `docs/VALIDATION.md` and `docs/SAFETY.md` (layout changes while driving); and `docs/spec.md` §1 and §2, only at merge.
  - Must not change:
    - calibration (`main/core/Calibration.*`, `main/CalibrationData.h`);
    - alarm thresholds, timing and logic (`main/Config.h` constants, `TemperatureState`);
    - status classification (`convertSample`) and the status texts;
    - the CSV log line and its units: `temp_c` stays in °C with one decimal;
    - the colour precedence (REQ-D1);
    - `main/hal/`, `hardware/`, `sdkconfig.defaults`, `.github/`.
- **Requirements amended:** these need approval.
  - **§1 Product, out of scope.**
    - **Old:** "touch interaction"
    - **New:** "touch interaction, except a long-press on the screen to change the display layout"
  - **REQ-D1.**
    - **Old:** "The display shows the temperature, `> max °C` / `< min °C`, ohms when uncalibrated, or `--` on a fault, together with the status text. Colour precedence: Red alarm, then Amber alarm, then fault or AboveRange in amber, then BelowRange muted, otherwise green. A demo build shows `DEMO ONLY - NOT REAL`."
    - **New:**
      - **Layouts:** the display offers two layouts, Classic and Sweep. Every rule below applies to both.
      - **Unit label:** with a verified calibration, it shows `°C`, or `°F` when Fahrenheit is selected in menuconfig. Without one, it shows `ohm` in every state. In Classic this is the title, `OIL °C`; in Sweep it's a small `°C` above `OIL TEMP`.
      - **Main reading:** it has no unit. It shows one of these:
        - the temperature in whole degrees of the selected unit, rounded half away from zero;
        - `>max` or `<min` beyond the table, with the limits converted to the selected unit;
        - the resistance in whole ohms when uncalibrated;
        - `--` on a fault.
      - **Digit layout:** fixed-width characters, right-aligned, so the ones digit never moves.
      - **Sweep segments:** they light only for a temperature inside the table. Beyond the top of the table, they light up to the table's maximum. Below range, on a fault or when uncalibrated, none are lit. Scale numbers are hidden when uncalibrated.
      - **Shown in both layouts:**
        - the status text;
        - the bottom line: the `MAX` value (whole degrees of the selected unit, no unit symbol) while calibrated with a reading in or beyond the table; otherwise `TEMPERATURE NOT VERIFIED` (uncalibrated) or `CHECK SENSOR AND WIRING` (fault);
        - in a demo build, `DEMO ONLY - NOT REAL`.

        *Amended 2026-11-11 by the caller, under the maintainer's delegation, after T17's review. The approved text said "Always shown: … the `MAX` value", but a `MAX` in degrees can't be shown when uncalibrated without breaking REQ-C1. The firmware (unchanged from before CR-3) shows `MAX` only in the cases above. This wording records that behaviour; no code changes.*
      - **Colour precedence:** unchanged. It applies to the digits and the lit segments.
      - **Choosing a layout:** a long-press changes the layout. The choice is stored and restored at power-up.
  - REQ-C1 is unchanged: uncalibrated firmware never shows °C or °F, and never shows a temperature scale, in any layout.
  - approved: PurpleSentinel, 2026-11-11. The §1 and REQ-D1 amendments as worded above, with the defaults under *Open questions*. The maintainer's words: "proceed, i trust your judgement and will review the outcome. Build everything, pending calibration data".
- **Approval-relevant changes:** these need human approval.
  - approved: PurpleSentinel, 2026-11-11. `main/Kconfig.projbuild` (the display-unit choice) and `main/CMakeLists.txt` (the new sources and `nvs_flash`), as listed below, in the same words as above. A `sdkconfig.defaults` change and any dependency install are **not** covered by this approval.
  - `main/Kconfig.projbuild`: a new display-unit choice (°C default, °F).
  - `main/CMakeLists.txt`: register the new display and font sources, and add `nvs_flash`.
  - If a generated font needs an LVGL option, for example compressed fonts, that's a `sdkconfig.defaults` change and needs a separate approval. Prefer fonts that need no config change.
- **Hardware impact:** none

## Acceptance criteria

**"Calibrated"** means `calibrationValid()` is true. That is the `GAUGE_DEMO` host build or `CONFIG_OIL_GAUGE_DEMO` firmware today, or a real verified table. The helpers in `main/core/` take the unit and the layout as parameters, so host tests can cover every combination.

**Shared by both layouts**

| ID | Requirement | Criterion (observable, testable) |
|----|-------------|----------------------------------|
| AC-1 | REQ-D1 (amended), REQ-C1 | The unit label is `°C` or `°F` when calibrated, matching the selected unit. When not calibrated it is `ohm` for **every** `Status` value, both units and both layouts. Uncalibrated text never contains `°`. The Classic title is `OIL ` + the unit label; Sweep shows the unit label above `OIL TEMP`. *Evidence:* host tests in the uncalibrated build and the `GAUGE_DEMO` build; human check. |
| AC-2 | REQ-D1 (amended) | The main reading text contains no unit (`°`, `C`, `F` or `ohm`) for every `Status` value. It is:<ul><li>the whole-degree value when `has_temperature`;</li><li>`>` + max for `AboveRange`, and `<` + min for `BelowRange`;</li><li>whole ohms for `NeedsCalibration` with a finite positive resistance;</li><li>`--` otherwise.</li></ul>*Evidence:* host tests over every `Status` value. |
| AC-3 | REQ-D1 (amended) | Rounding is to the nearest whole degree, halves away from zero: 112.4 → `112`, 112.5 → `113`, −0.5 → `-1`. °F is °C × 9/5 + 32, converted **before** rounding: 115.0 °C → `239`, 130.0 °C → `266`, 114.7 °C → `238`. With the demo table, the limits are `<20` / `>140` in °C and `<68` / `>284` in °F. Whole ohms use the same rounding: 4999.6 → `5000`. *Evidence:* host tests. |
| AC-4 | REQ-D1 (amended) | In each layout, every character of the reading takes the same width, and the reading is right-aligned. The ones digit sits at the same screen position for `9`, `99`, `100`, `188`, `--`, `>140` and `4999`. *Evidence:* human check (photos across the demo sweep). If the padding is computed in `main/core/`, also a host test. |
| AC-5 | REQ-D1 (amended) | The reading's digits are at least **100 px** tall in Classic and at least **72 px** in Sweep (48 px today). In both, the widest reading (4 characters, such as `4999`, `>284` or `<-10`) fits without overlapping any other element. *Evidence:* both firmware builds; human check with photos. |
| AC-6 | REQ-D1 (amended), REQ-T4 | `MAX` is in whole degrees of the selected unit, without a unit symbol, for example `MAX 112  \|  123 ohm`. Above range it reads `MAX >140` (°C) or `MAX >284` (°F). The resistance keeps its `ohm` unit. Both layouts show it. *Evidence:* host test if the text is built in `main/core/`; human check. |
| AC-7 | REQ-T2, REQ-T3, REQ-D2, REQ-C1–C3 | These are unchanged under every unit and layout: the alarm thresholds and timing, the calibration, status classification, and the CSV line (`temp_c` in °C, one decimal). Every existing assertion in `tests/temperature_test.cpp` and `tests/demo_temperature_test.cpp` passes unmodified. The diff doesn't touch the files listed under "Must not change". *Evidence:* host tests; reviewer's diff check. |
| AC-8 | new | The display unit defaults to °C. A build from `sdkconfig.defaults` selects °C, and so do both CI builds. *Evidence:* the built `sdkconfig`. |

**Sweep layout**

| ID | Requirement | Criterion (observable, testable) |
|----|-------------|----------------------------------|
| AC-9 | REQ-D1 (amended) | **Segments.** The ring has a fixed number of segments, N (40 by default), over the scale: 40–150 °C, or 100–300 °F. The lit count is `clamp(floor((T − scale_min) / (scale_max − scale_min) × N), 0, N)`, with T in the selected unit, when `has_temperature`. It's 0 for `BelowRange`, for every fault and when uncalibrated. For `AboveRange` it's the count for the table's maximum. *Evidence:* host tests at the scale ends, at a segment boundary ± 0.01°, below and above the scale, for `AboveRange` with the demo table, and for every fault. |
| AC-10 | REQ-D1 (amended), REQ-C1 | **Scale labels.** They read 40, 60 … 140 in °C, or 100, 120 … 300 in °F. They are hidden whenever not calibrated. *Evidence:* host test of the label set and its visibility; human check. |
| AC-11 | REQ-D1 | **Colours.** The digits and lit segments follow the REQ-D1 colour precedence: red alarm, amber alarm, fault or `AboveRange` amber, `BelowRange` muted, otherwise green. Unlit segments and the ghost `888` are dim but visible. *Evidence:* human check in the demo build (green → amber → red → amber → green). |
| AC-12 | new | **Seven-segment digits.** Sweep's digits are seven-segment digits: each character cell has 7 segments, lit or dim. The segment pattern for each of `0`–`9` and `-` is the standard one, and a blank cell has every segment dim, which gives the ghost `888`. `<` and `>` are drawn in the UI font next to the digits, and still fit AC-4 and AC-5. *Evidence:* host test of the character-to-segment table, if it's in `main/core/`; human check. *Amended at activation, 2026-11-11, by the caller under the maintainer's delegation: the digits are drawn as shapes in code rather than from the DSEG7 font, so there's no third-party font or licence, and the size is unconstrained.* |

**Choosing a layout**

| ID | Requirement | Criterion (observable, testable) |
|----|-------------|----------------------------------|
| AC-13 | §1 (amended) | Pressing anywhere on the screen for at least **1 s** switches to the next layout (Classic → Sweep → Classic). Taps shorter than 1 s, and swipes, change nothing. *Evidence:* host test of the press-duration and cycle logic if it's in `main/core/`; human check. |
| AC-14 | new | The chosen layout is stored in NVS and restored at power-up. On first boot, or when the stored value is missing or invalid, the layout is Classic. NVS is written only when the layout changes. If NVS can't be initialised, the gauge still works in Classic, and logs this once. *Evidence:* host test of decoding the stored value (invalid → Classic); human check across a power cycle. |
| AC-15 | REQ-M2, REQ-D2 | Switching layout doesn't disturb measurement or logging. Through 5 switches, the serial CSV keeps its 1 s rhythm (no gap over 1.2 s), and the new layout shows the current state on the next display update, within 250 ms. *Evidence:* human check with the serial log. |
| AC-16 | REQ-D1 (amended), REQ-C1–C3, REQ-B1 | Each layout shows, correctly, the following, and no state is shown in only one layout:<ul><li>every `Status` value;</li><li>the status text;</li><li>`MAX`;</li><li>the uncalibrated ohms view;</li><li>the out-of-range views;</li><li>the demo banner, in a demo build.</li></ul>*Evidence:* human check (demo build for the temperature states; normal build for the fault and uncalibrated states). |
| AC-17 | new | Touch changes nothing except the layout. No setting that affects measurement, alarms or the unit can be changed by touch. *Evidence:* reviewer's check. |

**Build and docs**

| ID | Requirement | Criterion (observable, testable) |
|----|-------------|----------------------------------|
| AC-18 | REQ-B2 | `./scripts/test_host.sh` passes. The normal and demo firmware builds succeed with no `main/` warnings, and so does a local normal build with the °F option. |
| AC-19 | CLAUDE.md invariant 7 | The docs are updated in the same change: `README.md` (both layouts, and the long-press), `docs/ARCHITECTURE.md`, `docs/VALIDATION.md`, and `docs/SAFETY.md` (don't change layout while driving). *Evidence:* reviewer's check. |

## Verification

- `./scripts/test_host.sh` passes.
- `/cr-workflow:build` passes: the normal and demo builds with 0 `main/` warnings.
- **°F build (local, not CI):** a normal build in its own build directory, with the °F option added to the defaults (the same pattern as the demo gate). It must succeed with 0 `main/` warnings.
- **Mutation check.** Each of these must make a test fail:
  - rounding with `%.0f` (half to even) (AC-3);
  - converting to °F after rounding (AC-3);
  - not converting the limits or `MAX` to °F (AC-3, AC-6);
  - a `°` in uncalibrated text (AC-1);
  - a unit in the reading (AC-2);
  - an off-by-one in the lit-segment count, lighting segments beyond the table's maximum, or lighting any segment when uncalibrated or on a fault (AC-9);
  - showing scale labels when uncalibrated (AC-10);
  - an invalid stored layout not falling back to Classic (AC-14).
- **Human verification.** Check the device first: `lsusb` shows `303a:1001`; flash through `/dev/serial/by-id/usb-Espressif_*`. Take photos throughout.
  1. **Demo build, °C, both layouts.**
     - The unit label and title are right, and there's no unit beside the reading.
     - The ones digit stays put across the sweep.
     - Nothing overlaps, and the digits are clearly larger.
     - In Sweep: the segments track the reading and the labels read 40…140. The colour goes green → amber → red → amber → green, and the ghost `888` is visible.
  2. **Demo build, °F, both layouts.**
     - The labels read 100…300.
     - Amber appears at about `239` and red at about `266`.
     - The serial CSV still logs `temp_c` in °C.
  3. **Layout switching.**
     - A long-press of at least 1 s switches layout; a short tap and a swipe do nothing.
     - Five switches leave no gap in the CSV.
     - Power-cycle: the last layout comes back.
  4. **Normal uncalibrated build, both layouts.**
     - The unit reads `ohm`, the reading is `--` with `ADC UNAVAILABLE`, and the status text is visible.
     - In Sweep, no segments are lit and no scale numbers are shown.
     - If an ADS1115 and a test resistor are fitted, a 4-digit ohms reading fits.
  5. Flash the normal °C build back afterwards.

## Out of scope

- Choosing the unit on screen: it stays a menuconfig option.
- Any touch control other than the layout long-press.
- More than two layouts. A third one will be its own change request.
- Alarm-zone markings on the scale.
- Brightness control.
- Alarm behaviour.
- The status texts.
- The CSV units.
- Adding a °F variant to CI: that's a tooling change to `.github/workflows/` with its own PR.

## Open questions

**Answered at activation, 2026-11-11.** The maintainer said: "proceed, i trust your judgement and will review the outcome. Build everything, pending calibration data". So every default below is accepted. The maintainer will review the outcome, and plan review is delegated to the caller. The work goes as far as a green PR, without merging. Calibration data is still owed by a human, so the calibrated views are checked with the demo table. The caller's own decisions under this delegation are:
- **Sweep digits** are drawn in code (AC-12 amended above). The DSEG7 default is dropped.
- **Classic digits** are generated from LVGL's bundled Montserrat Medium with `lv_font_conv` 1.5.3, which needs an approved `config` task to install it outside the repo.

The defaults, as accepted:

- **Rounding near the alarm thresholds.** 114.5–114.99 °C shows as `115` but stays green until it reaches 115.0 °C. *Default: accept round-to-nearest (AC-3).*
- **Digit sizes.** *Default: at least 100 px in Classic and 72 px in Sweep (AC-5).*
- **Sweep details.** *Defaults:*
  - 40 segments;
  - a scale of 40–150 °C / 100–300 °F, labelled every 20;
  - green in normal running;
  - the ghost `888` shown;
  - the unit and `OIL TEMP` at the lower right, as in the reference.
- **Choosing a layout.** *Default: a 1 s long-press cycles through the layouts (AC-13).* The alternative is a tap that opens a small layout menu, which means more touch interaction while driving.
- **Typefaces.** *Defaults:* Classic's digits from Montserrat Medium (already shipped with LVGL under the SIL OFL); Sweep's from DSEG7 Classic (SIL OFL). Check the licence when adding it.
- **The `MAX` line (AC-6).** *Default: `MAX 112  |  123 ohm`.*
- **Approvals needed at activation:** the §1 and REQ-D1 amendments, `main/Kconfig.projbuild`, and `main/CMakeLists.txt`.
