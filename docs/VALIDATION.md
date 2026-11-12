# Verification plan

## Tests already exercised in this package

- C++ unit tests: ratiometric resistance calculation, ground-offset cancellation and ground-fault window, broken ground-sense wire, uncalibrated lockout, open/short/supply/ADC faults; with the synthetic demo table, interpolation, below/above-range handling, alarm debounce, hysteresis (Red → Amber → Normal), above-range critical alarm, alarm retention through intermittent faults, and the below-range hold (a latched alarm is kept, with status `BelowRange` and no temperature, until `kBelowRangeHoldMs` after the first raw sample colder than the table (fault samples in between neither pause nor restart that time), then clears; no hold when no alarm is latched).
- Display host suites: `tests/display_text_test.cpp` (unitless reading text, `<min` / `>max`, °C to °F conversion and rounding, footer and fault text, `ohm` when uncalibrated), `tests/sweep_model_test.cpp` (the lit-segment count and its cap at the table maximum when above range; the 30–130 °C / 86–266 °F scale and its labels and their visibility; no segments when uncalibrated, below range or on a fault; the seven-segment table; the graduated colours, their stops and half-way band boundaries, no hard steps, each segment's colour, and the reading colour never less severe than the latched alarm, amber on a fault or when uncalibrated; Sweep's three-cell reading with the `<` / `>` sign split off and `--` for anything longer) and `tests/layout_choice_test.cpp` (layout encode/decode with Classic fallback, long-press detector). The display-text and sweep-model suites also run with `-DGAUGE_DEMO`. The colour rules are host-tested; that `GaugeDisplay` and the layouts apply them (digits, status text, Classic's arc, each lit segment) is covered only by the human demo-build check below.
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
| Sender hotter than top calibration point (bench resistor) | `ABOVE CAL RANGE` and `>max` in red at once (the colour of the table maximum); the Red alarm latches after the debounce | photo/log |
| Alarm latched, then sender colder than table (decade box) | `<min`, alarm colour kept for `kBelowRangeHoldMs` (10 s), never an in-table number, then clears | photo/log |
| ADS1115 unplugged / I²C lines shorted | `ADC UNAVAILABLE`, no °C, firmware recovers when reconnected (the no-ADS1115 half passed on 2026-11-11; recovery and shorted lines are still owed) | photo/log |
| 3.3 V rail outside 2.9–3.5 V (bench supply) | `SUPPLY FAULT`, no °C | photo/log |
| Uncalibrated build | No degrees display, explicit calibration warning | screenshot |
| Demo build, °C, both layouts (`idf.py -B /tmp/caterham-oil-gauge-build-demo ...`, see CLAUDE.md demo gate) | Title and unit label right, no unit beside the reading, ones digit fixed across the sweep, nothing overlaps, digits clearly larger, and the widest 4-character reading fits; a 3-digit reading is centred in Classic; in Sweep the segments track the reading and labels read 30, 50, 70, 90, 110, 130; each lit segment keeps the colour of its place on the dial (dark blue → light blue → green → yellow → amber → red, blending); the digits, status text and Classic's arc blend through the same colours as the temperature rises and falls, are at least amber while `HIGH TEMPERATURE` is shown and red while `CRITICAL TEMP` is shown; the ghost `8888` is faint, close to the background, and darker than the unlit ring segments | photos |
| Demo build, °F, both layouts: the demo command with `printf 'CONFIG_OIL_GAUGE_DEMO=y\nCONFIG_OIL_GAUGE_UNIT_FAHRENHEIT=y\n'` and a separate build directory (e.g. `/tmp/caterham-oil-gauge-build-demo-f`); a normal °F build is built the same way without the demo line | Sweep labels read 86, 122, 158, 194, 230, 266; the colours change at the same temperatures as in °C (red from 248 °F); `HIGH TEMPERATURE` at about 239 and `CRITICAL TEMP` at about 266; serial CSV still logs `temp_c` in °C | photos, serial log |
| Layout switching (demo build) | A long-press of at least 1 s switches layout; a short tap and a swipe do nothing; five switches leave no gap in the CSV; after a power cycle the last layout returns | photos, serial log |
| Normal uncalibrated build, both layouts | Unit reads `ohm`, reading `--` with `ADC UNAVAILABLE`, status text visible; in Sweep no segments lit and no scale numbers; with an ADS1115 and a test resistor fitted, a 4-digit ohms reading fits | photos |
| Calibrated bath at multiple points | Readings within verified error band | calibration report |
| Alarm debounce and reset hysteresis | No oscillation at threshold | event log |
| Engine cranking and alternator operation | No resets, surge damage or false readings | powered test |
| Vibration/thermal | Wiring/enclosure hold, stable function | inspection |
| Sweep three-digit reading (demo build, °C and °F) | Three seven-segment digits, larger than the earlier four, centred on the screen, over a faint ghost `888`; nothing touches the scale labels, the unit, `OIL TEMP` or the status line. The `<` / `>` sign beside the digits needs a reading beyond the table (#25). | photos |

Until those tests are documented and passed, the project remains a **bench development prototype**.
