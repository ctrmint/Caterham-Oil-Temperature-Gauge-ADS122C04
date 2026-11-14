# Calibrating the actual sender

The Caterham part listing identifies an electrically variable resistance, **not a full R/T calibration table**. Never equate a single ohmic reading to °C without establishing the actual curve for the fitted sensor. Before calibration the gauge intentionally shows ohms and warns `CALIBRATION REQUIRED`.

## Recommended method

1. Identify the *actual* fitted sensor; confirm it is a single-terminal resistance-to-ground device and the spade terminal is isolated from the tank body. The likely factory part is a Caerbont TT6811-03 (1/8" NPT, 40–140 °C). Its published curve (quoted in #42) is not a safe assumption either. A bench sender stamped 6811 didn't follow it: up to 89 °C it tracked the Bosch 0 280 130 026 curve (2.5 kΩ at 20 °C) instead. Use published curves only to sanity-check your own points, never for the table.
2. Record the **exact 470 Ω pull-up** with a calibrated DMM (circuit unpowered) and enter it in `main/Config.h`. With the ADS1115 ratiometric circuit this resistor is the reference for every reading: a 0.5 % error in its value is a 0.5 % error in sender resistance. The 1 kΩ input resistors do not need measuring.
3. Characterise resistance across **at least five, preferably 7–10**, independently measured temperature points across the operating range. With sensor removed use a calibrated temperature probe, liquid bath, stable equilibrium, and suitable insulated leads. Follow relevant high-temperature and oil-handling safety procedures; **do not use boiling water as a method to assert calibration above 100°C**.
4. If sender cannot be removed, you can compare it against an independently calibrated thermocouple securely attached near the sensing element, accounting for delay and temperature gradients. Never loosen sender or tank fitting while hot, pressurised, or full.
5. Measure ohms with the ESP32/ADS1115 circuit disconnected (never use a meter in resistance mode on a powered circuit); ensure the sensor body has a good earth return. Alternatively, once the ADS1115 is fitted and its resistance reading has been checked against known resistors, the gauge's own logged resistance can be used. If you calibrate with the sender installed, connect the ground-sense wire to the same tank stud that will be used in service, so the calibration includes the real sender-to-tank contact.
6. Put measurements in `measurements.csv` using the schema below, ascending temperatures. Exact values below are **deliberate placeholders** and MUST NOT be used as a real sender calibration.

```csv
# Do NOT COPY EXAMPLE NUMBERS INTO CALIBRATION
# temperature_c,resistance_ohms
# 20,1000
# 60,350
# 100,120
```

Create a *real* CSV with its first row `temperature_c,resistance_ohms` and only your actual measurements. Example validated command:

```bash
python3 scripts/build_calibration.py my_measured_sender.csv \
  --output main/CalibrationData.h \
  --accept-verified-measurements
idf.py build
```

This command requires an explicit acknowledgement, validates monotonic decrease and generates a compile-time table. The firmware **does not extrapolate** outside the measured table. Temperatures outside the measured range display `BELOW CAL RANGE` or `ABOVE CAL RANGE` (with `<min` / `>max` in the selected display unit) and no falsely precise number. `BELOW CAL RANGE` is not a fault and shows no number; if an alarm is already latched when the reading goes below the table (a probable wiring fault), the alarm colour is kept for `kBelowRangeHoldMs` (10 s) and then clears. **Above-range readings are treated as critical**, so calibrate up to at least the red threshold (130 °C by default) or the gauge will show red whenever the oil is hotter than your top point; the firmware logs a warning at boot if the table stops short. The demo setting uses its own **synthetic** table, independent of real sensor calibration.

## Recording points and the sender graph

Record each stable reading as a row of `docs/sender-measurements.csv`: `temperature_c`, `resistance_ohms`, `date`, `reference` (the temperature reference used) and `note`. The first two columns are the ones `scripts/build_calibration.py` reads.

[`docs/sender-curve.svg`](sender-curve.svg) plots these points against:
- the reference curves in `docs/sender-reference-curves.csv`. These are datasheet values for comparison only, in a format the calibration tool rejects;
- the bench observations in `docs/sender-observations.csv`;
- the fault and alarm thresholds in `main/Config.h`;
- the calibration table, once one is verified.

After changing any of these inputs, run `python3 scripts/plot_sender_curve.py` and commit the regenerated graph in the same change. The host tests fail while the committed graph is stale. To check without writing, run `python3 scripts/plot_sender_curve.py --check`.

## Expected tolerances

With the ADS1115 the converter itself is rarely the limit (125 µV steps, about 0.02 °C near 130 °C). The ratiometric, ground-sensed measurement cancels rail voltage, ADC gain error and ground offset. What remains: the tolerance and temperature drift of the 470 Ω pull-up, contact resistance at the sender spade and tank stud, the temperature gradient within the tank, sensor self-heating (a few mW) and curve approximation between your calibration points. Do not claim a precision class without testing across the entire installed system. A match at room temperature does not prove 120°C accuracy.

## Validating the resulting gauge

Test at stable reference conditions, including several points between 80–120°C if that is your intended oil-temperature range and a safe calibrated bath is available. Compare display versus independent reference; document the maximum error, repeatability and latency. Configure engine-specific caution/critical limits only after consulting Caterham/engine-builder recommendations. Mark the system as unverified until recorded bench and road checks are complete.
