# Verification plan

## Tests already exercised in this package

- C++ unit tests: ratiometric resistance calculation, ground-offset cancellation and ground-fault window, broken ground-sense wire, uncalibrated lockout, open/short/supply/ADC faults; with the synthetic demo table, interpolation, below/above-range handling, alarm debounce, hysteresis (Red → Amber → Normal), above-range critical alarm, alarm retention through intermittent faults, and the below-range hold (a latched alarm is kept, with status `BelowRange` and no temperature, until `kBelowRangeHoldMs` after the first raw sample colder than the table (fault samples in between neither pause nor restart that time), then clears; no hold when no alarm is latched).
- Python unit tests: monotonic calibration generator, minimum of 5 points, comment lines, malformed-row rejection, the example file being unusable, generated calibration flag.

Run: `./scripts/test_host.sh`.

## Tests that require physical hardware (not yet carried out)

| Test | Expected outcome | Record |
|---|---|---|
| ESP-IDF build and flash on 31261 or 31262 | Screen renders; no display reset loops | firmware version, board revision |
| ADS1115 detected at 0x48 on GPIO17/18 | Driver initialises; no I²C errors over a 1-hour soak | log |
| Bench known 120 Ω | Vs ≈0.671 V, Vp ≈2.629 V and R ≈120 Ω within measured system error | Vs, Vp and R |
| Bench 44 Ω / 470 Ω / 1,158 Ω | Correct resistance tracking | % error |
| 10 Ω added in the bond wire only | Resistance reading unchanged (ground-sense cancellation works) | R before/after |
| Sender open | `SENSOR OPEN`, no °C | photo/log |
| Sender short | `SENSOR SHORT`, no °C | photo/log |
| Ground-sense wire disconnected | `GROUND FAULT`, no °C | photo/log |
| Sender hotter than top calibration point (bench resistor) | `ABOVE CAL RANGE` in red after debounce | photo/log |
| Alarm latched, then sender colder than table (decade box) | `< min °C`, alarm colour kept for `kBelowRangeHoldMs` (10 s), never an in-table number, then clears | photo/log |
| ADS1115 unplugged / I²C lines shorted | `ADC UNAVAILABLE`, no °C, firmware recovers when reconnected | photo/log |
| 3.3 V rail outside 2.9–3.5 V (bench supply) | `SUPPLY FAULT`, no °C | photo/log |
| Uncalibrated build | No degrees display, explicit calibration warning | screenshot |
| Calibrated bath at multiple points | Readings within verified error band | calibration report |
| Alarm debounce and reset hysteresis | No oscillation at threshold | event log |
| Engine cranking and alternator operation | No resets, surge damage or false readings | powered test |
| Vibration/thermal | Wiring/enclosure hold, stable function | inspection |

Until those tests are documented and passed, the project remains a **bench development prototype**.
