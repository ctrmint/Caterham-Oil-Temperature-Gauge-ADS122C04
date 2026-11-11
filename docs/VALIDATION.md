# Verification plan

## Tests already exercised in this package

- C++ unit tests: ratiometric resistance calculation, ground-offset cancellation and ground-fault window, broken ground-sense wire, uncalibrated lockout, open/short/supply/ADC faults; with the synthetic demo table, interpolation, below/above-range handling, alarm debounce, hysteresis (Red → Amber → Normal), above-range critical alarm, alarm retention through intermittent faults, and the below-range hold (a latched alarm is kept, with status `BelowRange` and no temperature, until `kBelowRangeHoldMs` after the first raw sample colder than the table (fault samples in between neither pause nor restart that time), then clears; no hold when no alarm is latched).
- Display host suites: `tests/display_text_test.cpp` (unitless reading text, `<min` / `>max`, °C to °F conversion and rounding, footer and fault text, `ohm` when uncalibrated), `tests/sweep_model_test.cpp` (lit-segment count and colour for the Sweep ring, scale labels, no segments when uncalibrated or on a fault) and `tests/layout_choice_test.cpp` (layout encode/decode with Classic fallback, long-press detector). The first two also run with `-DGAUGE_DEMO`.
- Python unit tests: monotonic calibration generator, minimum of 5 points, comment lines, malformed-row rejection, the example file being unusable, generated calibration flag.

Run: `./scripts/test_host.sh`.

## Hardware tests carried out

| Date | Test | Result | Record |
|---|---|---|---|
| 2026-11-11 | ESP-IDF build and flash on 31261 | **Pass.** One boot, then no resets or panics over 40 s of monitoring. The CO5300 display and CST9217 touch controller initialise, and the screen renders. | Firmware `fbcbe5c`, normal build, ESP-IDF 5.5.5. ESP32-S3 revision v0.2, 16 MB flash, 8 MB PSRAM. |
| 2026-11-11 | No ADS1115 fitted (first half of "ADS1115 unplugged") | **Pass.** The screen shows `--`, `ADC UNAVAILABLE` and `CHECK SENSOR AND WIRING`, with no °C. The log shows `ADS1115 not responding at 0x48 on GPIO17/18` and one `read failed; resetting I2C bus` line, never repeated, then `No verified sender curve: degrees C disabled`. Every CSV line reads `ADC UNAVAILABLE,NORMAL,NA` with zero voltages. Recovery on reconnection is still owed. | Serial log. Note: the CSV lines were about 1015 ms apart, not 1000 ms. Re-check the interval with the ADS1115 responding. |

## Tests that require physical hardware (not yet carried out)

| Test | Expected outcome | Record |
|---|---|---|
| ADS1115 detected at 0x48 on GPIO17/18 | Driver initialises; no I²C errors over a 1-hour soak | log |
| Bench known 120 Ω | Vs ≈0.671 V, Vp ≈2.629 V and R ≈120 Ω within measured system error | Vs, Vp and R |
| Bench 44 Ω / 470 Ω / 1,158 Ω | Correct resistance tracking | % error |
| 10 Ω added in the bond wire only | Resistance reading unchanged (ground-sense cancellation works) | R before/after |
| Sender open | `SENSOR OPEN`, no °C | photo/log |
| Sender short | `SENSOR SHORT`, no °C | photo/log |
| Ground-sense wire disconnected | `GROUND FAULT`, no °C | photo/log |
| Sender hotter than top calibration point (bench resistor) | `ABOVE CAL RANGE` in red after debounce | photo/log |
| Alarm latched, then sender colder than table (decade box) | `<min`, alarm colour kept for `kBelowRangeHoldMs` (10 s), never an in-table number, then clears | photo/log |
| ADS1115 unplugged / I²C lines shorted | `ADC UNAVAILABLE`, no °C, firmware recovers when reconnected (the no-ADS1115 half passed on 2026-11-11; recovery and shorted lines are still owed) | photo/log |
| 3.3 V rail outside 2.9–3.5 V (bench supply) | `SUPPLY FAULT`, no °C | photo/log |
| Uncalibrated build | No degrees display, explicit calibration warning | screenshot |
| Demo build, °C, both layouts (`idf.py -B /tmp/caterham-oil-gauge-build-demo ...`, see CLAUDE.md demo gate) | Title and unit label right, no unit beside the reading, ones digit fixed across the sweep, nothing overlaps; in Sweep the segments track the reading, labels read 40…140, colour goes green → amber → red → amber → green, ghost `888` visible | photos |
| Demo build, °F, both layouts: the demo command with `printf 'CONFIG_OIL_GAUGE_DEMO=y\nCONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y\n'` and a separate build directory (e.g. `/tmp/caterham-oil-gauge-build-demo-f`); a normal °F build is built the same way without the demo line | Sweep labels read 100…300; amber at about 239 and red at about 266; serial CSV still logs `temp_c` in °C | photos, serial log |
| Layout switching (demo build) | A long-press of at least 1 s switches layout; a short tap and a swipe do nothing; five switches leave no gap in the CSV; after a power cycle the last layout returns | photos, serial log |
| Normal uncalibrated build, both layouts | Unit reads `ohm`, reading `--` with `ADC UNAVAILABLE`, status text visible; in Sweep no segments lit and no scale numbers; with an ADS1115 and a test resistor fitted, a 4-digit ohms reading fits | photos |
| Calibrated bath at multiple points | Readings within verified error band | calibration report |
| Alarm debounce and reset hysteresis | No oscillation at threshold | event log |
| Engine cranking and alternator operation | No resets, surge damage or false readings | powered test |
| Vibration/thermal | Wiring/enclosure hold, stable function | inspection |

Until those tests are documented and passed, the project remains a **bench development prototype**.
