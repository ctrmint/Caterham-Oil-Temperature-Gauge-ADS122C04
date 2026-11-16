# Verification plan

## Tests already exercised in this package

- C++ unit tests (`tests/temperature_test.cpp`):
  - the ratiometric resistance calculation, with the ADC offset subtracted, and the offset self-test limit;
  - the ground-offset window and its edges, and the broken ground-sense wire;
  - the open A1 tap check and its limit;
  - the excitation-current window and a wrong Rref;
  - NaN and inf in every field;
  - the supply window edges, uncalibrated lockout, and the open and short thresholds.

  With the synthetic demo table (`tests/demo_temperature_test.cpp`): interpolation, below/above-range handling, alarm debounce, hysteresis (Red → Amber → Normal), above-range critical alarm, alarm retention through intermittent faults, and the below-range hold (a latched alarm is kept, with status `BelowRange` and no temperature, until `kBelowRangeHoldMs` after the first raw sample colder than the table (fault samples in between neither pause nor restart that time), then clears; no hold when no alarm is latched).
- Display host suites: `tests/display_text_test.cpp` (unitless reading text, `<min` / `>max`, °C to °F conversion and rounding, footer and fault text, `ohm` when uncalibrated), `tests/sweep_model_test.cpp` (the lit-segment count and its cap at the table maximum when above range; the 30–130 °C / 86–266 °F scale and its labels and their visibility; no segments when uncalibrated, below range or on a fault; the seven-segment table; the graduated colours, their stops and half-way band boundaries, no hard steps, each segment's colour, and the reading colour never less severe than the latched alarm, amber on a fault or when uncalibrated; Sweep's three-cell reading with the `<` / `>` sign split off and `--` for anything longer) and `tests/layout_choice_test.cpp` (layout encode/decode with Classic fallback, long-press detector). The display-text and sweep-model suites also run with `-DGAUGE_DEMO`. The colour rules are host-tested; that `GaugeDisplay` and the layouts apply them (digits, status text, Classic's arc, each lit segment) is covered only by the human demo-build check below.
- Python unit tests: monotonic calibration generator, minimum of 5 points, comment lines, malformed-row rejection, the example file being unusable, generated calibration flag. For the sender graph (`tests/test_sender_curve.py`): the committed `docs/sender-curve.svg` matches its inputs; it follows threshold changes, measured points and a verified table, and never plots the unverified default; the reference curves are rejected by the calibration tool; and non-monotonic reference data is rejected.
- ADC frame tests (`tests/adc_frame_test.cpp`): the CRC-16-CCITT check value; register and conversion frames decoded; every single-bit error rejected; a reset chip's unsealed reply rejected; and 24-bit two's-complement codes at zero, ±1 and both full scales.

Run: `./scripts/test_host.sh`.

## Hardware tests carried out

| Date | Test | Result | Record |
|---|---|---|---|
| 2026-11-11 | ESP-IDF build and flash on 31261 | **Pass.** One boot, then no resets or panics over 40 s of monitoring. The CO5300 display and CST9217 touch controller initialise, and the screen renders. | Firmware `fbcbe5c`, normal build, ESP-IDF 5.5.5. ESP32-S3 revision v0.2, 16 MB flash, 8 MB PSRAM. |
| 2026-11-11 | No ADC fitted (earlier ADC driver) | **Pass.** The screen shows `--`, `ADC UNAVAILABLE` and `CHECK SENSOR AND WIRING`, with no °C. The log shows the driver's "not responding" line and one "read failed" line, never repeated, then `No verified sender curve: degrees C disabled`. Every CSV line reads `ADC UNAVAILABLE,NORMAL,NA` with zero voltages. | Serial log. Note: the CSV lines were about 1015 ms apart, not 1000 ms. Re-check the interval with the ADC responding. |
| 2026-11-11 | Demo build (`CONFIG_OIL_GAUGE_DEMO`) on 31261 | **Pass (functional).** `DEMO ONLY - NOT REAL` is shown. The synthetic temperature sweeps through warming, `NORMAL`, amber `HIGH TEMPERATURE` and red `CRITICAL TEMP`, then steps back down through amber to normal, as observed by the user. This exercises the display's alarm states with a smooth synthetic signal. It does not replace the real-sensor "Alarm debounce and reset hysteresis" test below. Visual design problems were noted and are to be raised as change requests. | Observation by the user. Demo build from `main` at `8dfd710`. |
| 2026-11-12 | Display layouts (CR-3), demo build, °C, both layouts, and layout switching | **Pass.** Classic's title is `OIL °C` with no unit beside the reading, and Sweep shows `°C` above `OIL TEMP`. The ones digit stays fixed, nothing overlaps or is clipped (Classic's ring included), the digits are clearly larger, the Sweep segments track the reading, and the colours step green → amber → red → amber → green. A long-press of at least 1 s switches layout, while a tap or a swipe does nothing. Five switches leave no CSV gap over 1.2 s. The layout returns after a power cycle, and the first boot after `erase-flash` is Classic. | Observation by the user (#21). Firmware from `main` at `0668e1a`. These are CR-3's 40…140 scale and colours, which #27 then replaced. |
| 2026-11-12 | Display layouts (CR-3), demo build, °F | **Pass.** `OIL °F` / `°F`, labels 100…300, amber at about 239 and red at about 266, and the serial CSV still logs `temp_c` in °C. | Observation by the user (#21). Firmware from `main` at `0668e1a`. |
| 2026-11-12 | Display layouts (CR-3), normal uncalibrated build, no ADC fitted, both layouts | **Pass.** The unit reads `ohm`, never °C or °F. The reading is `--`, with `ADC UNAVAILABLE` and `CHECK SENSOR AND WIRING`. Sweep has no lit segments and no scale numbers. A 4-digit ohms reading with an ADC and test resistor is still owed. | Observation by the user (#21). Firmware from `main` at `0668e1a`. |
| 2026-11-12 | Graduated colours, 30–130 °C dial, fainter ghost digits, centred Classic reading (#27): demo build, °C, both layouts | **Pass.** The digits, status text and Classic's arc blend from dark blue through light blue, green, yellow and amber to red as the reading rises, and back as it falls. They are at least amber while `HIGH TEMPERATURE` is shown and red while `CRITICAL TEMP` is shown. Each lit segment keeps the colour of its place on the dial, and the labels read 30…130. The ghost `8888` is faint, a 3-digit reading is centred in Classic, and dark blue is legible. | Observation by the user (#27). Firmware from `main` at `1a48813`. |
| 2026-11-12 | The same (#27): demo build, °F | **Pass.** `OIL °F` / `°F` and labels 86…266. The colours change at the same temperatures as in °C (red from about 248). `HIGH TEMPERATURE` comes on at about 239 and `CRITICAL TEMP` at about 266, and the serial CSV still logs `temp_c` in °C. | Observation by the user (#27). Firmware from `main` at `1a48813`. |
| 2026-11-12 | Sweep three-digit reading (#29): demo build, °C | **Pass.** Three seven-segment digits, larger than the earlier four, centred on the screen over a faint ghost `888`. Nothing touches the scale labels, the unit, `OIL TEMP` or the status line. The user's verdict: "the revised 7 segment looks brilliant". | Observation by the user (#29). Firmware from branch `sweep-three-digits` at `b330466`, the same code that #29 merged to `main`. |
| 2026-11-14 | Earlier ADC front end, local bench build with the Bosch 0 280 130 026 **datasheet** curve, never committed and approved by the user for bench use only; bench sensor (a brass single-spade sender stamped 6811 on its hex) at room temperature, then in hot water beside a Sonoff reference probe | **Indicative pass, room temperature to 89 °C.** It reads 19 °C in a room the user estimates at 17–20 °C. In hot water it agreed with the reference probe all the way up to 89 °C. So this sender, although stamped 6811, follows the Bosch curve over that range. On the published TT6811-03 curve it would have read about 64 °C at 59 °C, and about 100 °C at 89 °C. It is **not a calibration**: the reference isn't a calibrated probe, no individual points were recorded, the reference resistor was nominal, and nothing above 89 °C was checked. It says something about the sender, not about the current front end. | Observation by the user. `main/` as at `b330466` plus a local `CalibrationData.h`. |

## Tests that require physical hardware (not yet carried out)

| Test | Expected outcome | Record |
|---|---|---|
| ADS122C04 detected at 0x40 on GPIO17/18 (WIRING.md bench step 3) | Driver initialises; no `ADC UNAVAILABLE` and no I²C errors over a 1-hour soak with the display lead as fitted (#1) | log |
| Bench known 120 Ω, measured Rref in `kReferenceOhms` (steps 4–5) | Vs ≈ 30 mV, Vref ≈ 0.50 V and R ≈ 120 Ω within measured system error (#1) | Vs, Vref and R |
| Bench 44 Ω / 470 Ω / 1,158 Ω / 5 kΩ (step 9) | Correct resistance tracking, within about 0.2 % (#1) | % error |
| Sender open and shorted (step 6) | `SENSOR OPEN` and `SENSOR SHORT`, never a reading (#1) | photo/log |
| Tank point about 50 mV below and above gauge GND (step 8) | No fault; `ground_offset_v` about ∓0.05 V; resistance within about 0.2 % of the normal reading. At about −0.11 V, `GROUND FAULT` (#1) | CSV log |
| Ground-sense wire disconnected (step 10) | `GROUND FAULT`, never a reading (#1) | photo/log |
| R1 and R2 each unsoldered from the harness side (step 11) | `SENSOR OPEN` and `GROUND FAULT` respectively, never a reading (#1) | photo/log |
| Short to 14.4 V through a 50 mA limit, signal wire then sense wire (step 12) | A fault while applied; no damage; correct readings afterwards; Rref re-measured unchanged (#1) | photo/log, Rref |
| Return wire disconnected, display on a USB power bank | A fault (`SENSOR OPEN` or `GROUND FAULT`), never a reading (#1) | photo/log |
| Wrong Rref fitted (for example 2.4 kΩ with 2.0 kΩ configured) | `SUPPLY FAULT` (#1) | photo/log |
| Sender hotter than top calibration point (bench resistor) | `ABOVE CAL RANGE` and `>max` in red at once (the colour of the table maximum); the Red alarm latches after the debounce | photo/log |
| Alarm latched, then sender colder than table (decade box) | `<min`, alarm colour kept for `kBelowRangeHoldMs` (10 s), never an in-table number, then clears | photo/log |
| ADC unplugged / I²C lines shorted | `ADC UNAVAILABLE`, no °C, firmware recovers when reconnected; the log shows one "read failed" line per episode and one "recovered" line (#1) | photo/log |
| Display's 3.3 V outside 2.9–3.5 V (bench supply on VIN) | `SUPPLY FAULT`, no °C | photo/log |
| Uncalibrated build | No degrees display, explicit calibration warning | screenshot |
| Normal uncalibrated build, both layouts, with the ADC and a test resistor (1–4.7 kΩ) fitted | A 4-digit ohms reading fits, `CALIBRATION REQUIRED` and `TEMPERATURE NOT VERIFIED` are shown, and the unit reads `ohm` | photos |
| Sender colder than about 4 °C on the Bosch curve (resistance over 5 kΩ) | `SENSOR OPEN`, because `kOpenResistanceOhms` is 5 kΩ. A reading or `BELOW CAL RANGE` would be better; raising the limit needs the compliance margin rechecked | photo/log |
| In the car, display on a USB-C charger | `ground_offset_v` stays well inside −0.10…+0.20 V with the engine running and electrical loads on | CSV log |
| Calibrated bath at multiple points | Readings within verified error band | calibration report |
| Alarm debounce and reset hysteresis | No oscillation at threshold | event log |
| Front-end box | The module, resistors and terminal strip stay put and no lead flexes under a shake test; the cables are strain-relieved (#1) | inspection |
| Engine cranking and alternator operation | No resets, surge damage or false readings | powered test |
| Vibration/thermal | Wiring/enclosure hold, stable function (#43) | inspection |
| Sweep three-digit reading, demo build, °F | As in °C (passed on 2026-11-12): three centred digits over a faint `888`, with nothing touching the scale labels (86…266), the unit, `OIL TEMP` or the status line. The `<` / `>` sign beside the digits needs a reading beyond the table (#25). | photos |

Until those tests are documented and passed, the project remains a **bench development prototype**.
