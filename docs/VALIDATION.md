# Verification plan

## Tests already exercised in this package

- C++ unit tests: ratiometric resistance calculation, ground-offset cancellation and ground-fault window, broken ground-sense wire, uncalibrated lockout, open/short/supply/ADC faults; with the synthetic demo table, interpolation, below/above-range handling, alarm debounce, hysteresis (Red → Amber → Normal), above-range critical alarm, alarm retention through intermittent faults, and the below-range hold (a latched alarm is kept, with status `BelowRange` and no temperature, until `kBelowRangeHoldMs` after the first raw sample colder than the table (fault samples in between neither pause nor restart that time), then clears; no hold when no alarm is latched).
- Python unit tests: monotonic calibration generator, minimum of 5 points, comment lines, malformed-row rejection, the example file being unusable, generated calibration flag.

Run: `./scripts/test_host.sh`.

## Hardware tests carried out

| Date | Test | Result | Record |
|---|---|---|---|
| 2026-11-11 | ESP-IDF build and flash on 31261 | **Pass.** One boot, then no resets or panics over 40 s of monitoring. The CO5300 display and CST9217 touch controller initialise, and the screen renders. | Firmware `fbcbe5c`, normal build, ESP-IDF 5.5.5. ESP32-S3 revision v0.2, 16 MB flash, 8 MB PSRAM. |
| 2026-11-11 | No ADS1115 fitted (first half of "ADS1115 unplugged") | **Pass.** The screen shows `--`, `ADC UNAVAILABLE` and `CHECK SENSOR AND WIRING`, with no °C. The log shows `ADS1115 not responding at 0x48 on GPIO17/18` and one `read failed; resetting I2C bus` line, never repeated, then `No verified sender curve: degrees C disabled`. Every CSV line reads `ADC UNAVAILABLE,NORMAL,NA` with zero voltages. Recovery on reconnection is still owed. | Serial log. Note: the CSV lines were about 1015 ms apart, not 1000 ms. Re-check the interval with the ADS1115 responding. |
| 2026-11-11 | Demo build (`CONFIG_OIL_GAUGE_DEMO`) on 31261 | **Pass (functional).** `DEMO ONLY - NOT REAL` is shown. The synthetic temperature sweeps through warming, `NORMAL`, amber `HIGH TEMPERATURE` and red `CRITICAL TEMP`, then steps back down through amber to normal, as observed by the user. This exercises the display's alarm states with a smooth synthetic signal. It does not replace the real-sensor "Alarm debounce and reset hysteresis" test below. Visual design problems were noted and are to be raised as change requests. | Observation by the user. Demo build from `main` at `8dfd710`. |

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
| Alarm latched, then sender colder than table (decade box) | `< min °C`, alarm colour kept for `kBelowRangeHoldMs` (10 s), never an in-table number, then clears | photo/log |
| ADS1115 unplugged / I²C lines shorted | `ADC UNAVAILABLE`, no °C, firmware recovers when reconnected (the no-ADS1115 half passed on 2026-11-11; recovery and shorted lines are still owed) | photo/log |
| 3.3 V rail outside 2.9–3.5 V (bench supply) | `SUPPLY FAULT`, no °C | photo/log |
| Uncalibrated build | No degrees display, explicit calibration warning | screenshot |
| Calibrated bath at multiple points | Readings within verified error band | calibration report |
| Alarm debounce and reset hysteresis | No oscillation at threshold | event log |
| Engine cranking and alternator operation | No resets, surge damage or false readings | powered test |
| Vibration/thermal | Wiring/enclosure hold, stable function | inspection |

Until those tests are documented and passed, the project remains a **bench development prototype**.
