# Verification plan

## Tests already exercised in this package

- C++ unit test: nominal resistance calculation, uncalibrated lockout, zero/open/fault supply, reference failure.
- Python unit tests: monotonic calibration generator, minimum point count, generated calibration flag.

Run: `./scripts/test_host.sh`.

## Tests that require physical hardware (not yet carried out)

| Test | Expected outcome | Record |
|---|---|---|
| ESP-IDF build and flash on 31261 or 31262 | Screen renders; no display reset loops | firmware version, board revision |
| Bench known 120 Ω | Sensor V ≈0.671 V and R ≈120 Ω within measured system error | V and R |
| Bench 44 Ω / 470 Ω / 1,158 Ω | Correct resistance tracking | % error |
| Sender open | `SENSOR OPEN` or conservative out-of-range, no °C | photo/log |
| Sender short | `SENSOR SHORT`, no °C | photo/log |
| Wrong reference divider / missing GPIO17 | `SUPPLY FAULT`, no °C | photo/log |
| Uncalibrated build | No degrees display, explicit calibration warning | screenshot |
| Calibrated bath at multiple points | Readings within verified error band | calibration report |
| Alarm debounce and reset hysteresis | No oscillation at threshold | event log |
| Engine cranking and alternator operation | No resets, surge damage or false readings | powered test |
| Vibration/thermal | Wiring/enclosure hold, stable function | inspection |

Until those tests are documented and passed, the project remains a **bench development prototype**.
