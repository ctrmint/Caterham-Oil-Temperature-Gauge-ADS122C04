# Calibrating the actual sender

The Caterham part listing identifies an electrically variable resistance, **not a full R/T calibration table**. Never equate a single ohmic reading to °C without establishing the actual curve for the fitted sensor. Before calibration the gauge intentionally shows ohms and warns `CALIBRATION REQUIRED`.

## Recommended method

1. Identify the *actual* fitted sensor; confirm it is a single-terminal resistance-to-ground device and the spade terminal is isolated from the tank body.
2. Record the **exact 470 Ω pull-up** and the two **10 kΩ reference resistors** with a calibrated DMM and change `main/Config.h` if they differ materially from nominal.
3. Characterise resistance across **at least five, preferably 7–10**, independently measured temperature points across the operating range. With sensor removed use a calibrated temperature probe, liquid bath, stable equilibrium, and suitable insulated leads. Follow relevant high-temperature and oil-handling safety procedures; **do not use boiling water as a method to assert calibration above 100°C**.
4. If sender cannot be removed, you can compare it against an independently calibrated thermocouple securely attached near the sensing element, accounting for delay and temperature gradients. Never loosen sender or tank fitting while hot, pressurised, or full.
5. Measure ohms with the ESP32 circuit disconnected (never use a meter in resistance mode on a powered circuit); ensure the sensor body has a good earth return.
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

This command requires an explicit acknowledgement, validates monotonic decrease and generates a compile-time table. The firmware **does not extrapolate** outside the measured table. Temperatures outside the measured range display `OUT OF CAL RANGE` and no falsely precise number. The demo setting uses its own **synthetic** table, independent of real sensor calibration.

## Expected tolerances

ADC2 and PCB resistor error, vehicle ground differences, the temperature gradient within the tank, sensor self-heating and curve approximation all contribute. Do not claim a precision class without testing across the entire installed system. A match at room temperature does not prove 120°C accuracy. In particular, opening the sender can make ADC16 approach its upper unreliable measurement region, so the firmware uses an explicit open-circuit guard.

## Validating the resulting gauge

Test at stable reference conditions, including several points between 80–120°C if that is your intended oil-temperature range and a safe calibrated bath is available. Compare display versus independent reference; document the maximum error, repeatability and latency. Configure engine-specific caution/critical limits only after consulting Caterham/engine-builder recommendations. Mark the system as unverified until recorded bench and road checks are complete.
