# Architecture

```text
[Caterham tank-mounted resistive sender]
       | spade signal + metal tank ground return
       v
[470 Ω 3.3V pullup / surge & ESD front end / RC filter]
       | GPIO16 (ADC2 ch5)                [3.3V rail]
       |                                     |
       |                           [10k/10k reference divider]
       |                                     | GPIO17 (ADC2 ch6)
       +---------------------+---------------+
                             v
                [ESP32-S3 ADC2 oneshot]
                [curve-fit ADC calibration]
                             |
                   [resistance calculator]
                   [open/short/rail faults]
                             |
                   [measured curve lookup]
                   [EMA + warnings]
                             |
                       [LVGL gauge]
                  [466x466 CO5300 AMOLED]
```

`main/hal/AnalogSource` owns ESP-IDF ADC2 channel acquisition, oversampling and voltage calibration. `main/core/` is **platform-independent** and unit-tested with `g++`; it processes voltages and rail monitoring, handles sender fault states and interpolates a user-validated monotonically decreasing R/T calibration curve. `main/display/` exclusively handles Waveshare BSP/LVGL calls. `main/main.cpp` schedules acquisition at 4 Hz, updates display at 4 Hz and reports CSV-like serial diagnostics at 1 Hz.

## Measurement safety model

* ADC calibration driver unavailable → `ADC UNAVAILABLE` and **no temperature**.
* Rail 3.3 V not within configured 2.9–3.5 V → `SUPPLY FAULT`.
* Low sender resistance under 20 Ω → `SENSOR SHORT`.
* Sender voltage at/above 87% of measured rail → `SENSOR OPEN` (avoids unstable upper ADC range).
* No human-verified curve → `CALIBRATION REQUIRED` and ohms only.
* Outside measured R/T endpoints → `OUT OF CAL RANGE`, no extrapolation.
* Excess temperature sustained for configured debounce → warning/critical state. The default alarm limits are **placeholders**.

**Limitations:** ADC calibration is silicon voltage calibration, **not** temperature-sender calibration. A grounded, resistive automotive probe is neither a thermocouple nor a digital I²C temperature sensor. This gauge does not independently measure oil pressure, perform engine shutdown, or replace factory alerts.

## Board portability

ESP32-S3's ADC2 GPIO16 and GPIO17 are specific to this Waveshare 1.75 configuration. They may be unavailable or occupied on other boards. See [PORTING.md](PORTING.md) for adapting pin mapping and display HAL while leaving `main/core/` unchanged.

## Power / privacy

No wireless access points, internet client or network connections are started. No personal or vehicle telemetry is transmitted. There are no secrets or cloud dependencies. USB logs show measured resistance and optional calibrated temperature locally.
