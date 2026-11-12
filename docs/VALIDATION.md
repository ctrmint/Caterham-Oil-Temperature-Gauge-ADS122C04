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
| 2026-11-11 | Demo build (`CONFIG_OIL_GAUGE_DEMO`) on 31261 | **Pass (functional).** `DEMO ONLY - NOT REAL` is shown. The synthetic temperature sweeps through warming, `NORMAL`, amber `HIGH TEMPERATURE` and red `CRITICAL TEMP`, then steps back down through amber to normal, as observed by the user. This exercises the display's alarm states with a smooth synthetic signal. It does not replace the real-sensor "Alarm debounce and reset hysteresis" test below. Visual design problems were noted and are to be raised as change requests. | Observation by the user. Demo build from `main` at `8dfd710`. |
| 2026-11-12 | Display layouts (CR-3), demo build, °C, both layouts, and layout switching | **Pass.** Classic's title is `OIL °C` with no unit beside the reading, and Sweep shows `°C` above `OIL TEMP`. The ones digit stays fixed, nothing overlaps or is clipped (Classic's ring included), the digits are clearly larger, the Sweep segments track the reading, and the colours step green → amber → red → amber → green. A long-press of at least 1 s switches layout, while a tap or a swipe does nothing. Five switches leave no CSV gap over 1.2 s. The layout returns after a power cycle, and the first boot after `erase-flash` is Classic. | Observation by the user (#21). Firmware from `main` at `0668e1a`. These are CR-3's 40…140 scale and colours, which #27 then replaced. |
| 2026-11-12 | Display layouts (CR-3), demo build, °F | **Pass.** `OIL °F` / `°F`, labels 100…300, amber at about 239 and red at about 266, and the serial CSV still logs `temp_c` in °C. | Observation by the user (#21). Firmware from `main` at `0668e1a`. |
| 2026-11-12 | Display layouts (CR-3), normal uncalibrated build, no ADS1115 fitted, both layouts | **Pass.** The unit reads `ohm`, never °C or °F. The reading is `--`, with `ADC UNAVAILABLE` and `CHECK SENSOR AND WIRING`. Sweep has no lit segments and no scale numbers. A 4-digit ohms reading with an ADS1115 and test resistor is still owed (#24). | Observation by the user (#21). Firmware from `main` at `0668e1a`. |
| 2026-11-12 | Graduated colours, 30–130 °C dial, fainter ghost digits, centred Classic reading (#27): demo build, °C, both layouts | **Pass.** The digits, status text and Classic's arc blend from dark blue through light blue, green, yellow and amber to red as the reading rises, and back as it falls. They are at least amber while `HIGH TEMPERATURE` is shown and red while `CRITICAL TEMP` is shown. Each lit segment keeps the colour of its place on the dial, and the labels read 30…130. The ghost `8888` is faint, a 3-digit reading is centred in Classic, and dark blue is legible. | Observation by the user (#27). Firmware from `main` at `1a48813`. |
| 2026-11-12 | The same (#27): demo build, °F | **Pass.** `OIL °F` / `°F` and labels 86…266. The colours change at the same temperatures as in °C (red from about 248). `HIGH TEMPERATURE` comes on at about 239 and `CRITICAL TEMP` at about 266, and the serial CSV still logs `temp_c` in °C. | Observation by the user (#27). Firmware from `main` at `1a48813`. |
| 2026-11-12 | Sweep three-digit reading (#29): demo build, °C | **Pass.** Three seven-segment digits, larger than the earlier four, centred on the screen over a faint ghost `888`. Nothing touches the scale labels, the unit, `OIL TEMP` or the status line. The user's verdict: "the revised 7 segment looks brilliant". | Observation by the user (#29). Firmware from branch `sweep-three-digits` at `b330466`, the same code that #29 merged to `main`. |

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
| Normal uncalibrated build, both layouts, with an ADS1115 and a test resistor (1–4.7 kΩ) fitted | A 4-digit ohms reading fits, `CALIBRATION REQUIRED` and `TEMPERATURE NOT VERIFIED` are shown, and the unit reads `ohm` (#24). The no-ADS1115 half passed on 2026-11-12. | photos |
| Calibrated bath at multiple points | Readings within verified error band | calibration report |
| Alarm debounce and reset hysteresis | No oscillation at threshold | event log |
| Engine cranking and alternator operation | No resets, surge damage or false readings | powered test |
| Vibration/thermal | Wiring/enclosure hold, stable function | inspection |
| Sweep three-digit reading, demo build, °F | As in °C (passed on 2026-11-12): three centred digits over a faint `888`, with nothing touching the scale labels (86…266), the unit, `OIL TEMP` or the status line. The `<` / `>` sign beside the digits needs a reading beyond the table (#25). | photos |

Until those tests are documented and passed, the project remains a **bench development prototype**.
