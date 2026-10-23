# Caterham Dry-Sump Oil Temperature Gauge

> **Hardware target:** Waveshare **ESP32-S3-Touch-AMOLED-1.75**, standard SKU 31261 or cased SKU 31262 (466×466 AMOLED, CO5300 controller). This firmware **does not** target the 2.1-inch RGB-LCD board, the `-1.75C` model, or the GNSS `-G` version without adaptation.

An independent **oil-temperature gauge** for the Caterham 420R dry-sump tank's resistive temperature sender (potentially Caterham part **71167**). No ECU wiring, no factory gauge modifications, and **no external ADC board**. Two accessible ESP32-S3 ADC2 pins measure the sender and the 3.3 V sensor-excitation rail. The board handles display, measurement, fault reporting and alarm presentation.

**Status: engineering prototype.** Host unit tests pass. A physical Waveshare board and sender were not available for firmware compilation, electrical validation or calibration. ESP-IDF/LVGL integration is based on the published Waveshare example and BSP; it requires on-board verification. The firmware ships **without** a validated sender temperature curve, so it **shows ohms, not °C**, until you explicitly install measured calibration points. It is **not** a primary engine-protection instrument.

## Quick start

1. Read [hardware/BOM.md](hardware/BOM.md), [hardware/WIRING.md](hardware/WIRING.md), and [docs/SAFETY.md](docs/SAFETY.md) before wiring to the car.
2. On a **bench** (not in the vehicle) wire the 470 Ω pull-up and both ADC circuits using the expansion header and an appropriate protected interface; check all voltages with a meter before connecting the PCB.
3. Install **ESP-IDF 5.5.5** on Fedora as described in [docs/SETUP_FEDORA.md](docs/SETUP_FEDORA.md).
4. `idf.py set-target esp32s3 && idf.py build && idf.py -p /dev/ttyACM0 flash monitor`
5. Boot with an **uncalibrated** sensor and confirm the display shows resistance and `CALIBRATION REQUIRED`; simulate open and short faults on the bench.
6. Independently characterise the exact sender, generate `main/CalibrationData.h`, rebuild, and verify °C against a reference thermometer. See [docs/CALIBRATION.md](docs/CALIBRATION.md).
7. Only consider vehicle mounting after power-conditioning, grounding, thermal, EMI and installation checks.

`./scripts/test_host.sh` runs the host-based conversion and calibration tests without any ESP32 hardware. `idf.py menuconfig` → **Caterham oil temperature gauge** → **Display synthetic sensor readings** runs a clearly marked BENCH DEMO; it must remain **off** for a real vehicle.

## Repository structure

```text
.
├── main/
│   ├── main.cpp                    # main sampling / display loop
│   ├── Config.h                    # divider parameters and warning settings
│   ├── CalibrationData.h           # fail-closed uncalibrated default
│   ├── core/                       # platform-independent resistance & temp math
│   ├── hal/AnalogSource.*          # ESP32-S3 ADC2 GPIO16 + GPIO17
│   ├── display/GaugeDisplay.*      # Waveshare BSP + LVGL 9 gauge
│   ├── Kconfig.projbuild           # explicitly opt-in simulation
│   └── idf_component.yml          # Waveshare BSP / LVGL dependencies
├── hardware/                       # wiring, circuit and BOM
├── docs/                           # architecture, Fedora setup, calibration,
│                                   # installation, validation, safety, porting
├── tests/                          # native C++ and Python tests
├── scripts/                        # sender curve generator, tests
└── .github/workflows/              # native test CI
```

## Design decisions

| Area | Choice |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-AMOLED-1.75, **standard or -B** |
| Display | 466×466 QSPI **CO5300** AMOLED, Waveshare BSP + LVGL 9 |
| ADC input | GPIO16 / ADC2 channel 5 (sender voltage) |
| Supply reference | GPIO17 / ADC2 channel 6 (3.3 V midpoint of 10 kΩ / 10 kΩ divider) |
| Sender interface | 470 Ω 1% pull-up to 3.3 V; tank body/metal adapter provides return |
| Input noise | 10 kΩ series + 100 nF filter at ADC16, protection stage required for vehicle use |
| Acquisition | Calibrated ADC2 oneshot, 24 samples/channel, 250 ms processing |
| Isolation | No connection to MBE ECU / existing instruments |
| Radio | **Wi-Fi/BLE not used** (ADC2 shares resources with radio) |
| Temperature calibration | Explicit sender measurements, no assumed Caterham curve |
| Safety | Fault detection, fail-closed calibration, external overvoltage protection required |

## Important known limitations

* The **sender's resistance-versus-temperature curve is not verified**. The listed 1,158–44 Ω range does not establish a full calibrated temperature curve.
* This ESP32 board exposes ADC2 pins; Wi-Fi must remain disabled. If wireless telemetry becomes a requirement, prefer a board with exposed **ADC1**, or add a dedicated ADC.
* ESP32-S3 12 dB attenuation cannot reliably resolve rail-level input (~3.3 V). The firmware treats an input above 87% of its supply as an open sender. Extremely cold high-resistance readings may be outside the calibrated range.
* The ADC measures electronics ground-referenced voltage; loose chassis grounds, alternator noise, load dumps and coolant/oil leaks are **vehicle integration hazards**.
* The board and its exposed contacts need an enclosure rated for the chosen location. A dashboard instrument must not obscure any legally required indicators or the driver's view.
* Alarm levels (60/115/130 °C) are **illustrative configurable defaults**, not Caterham- or engine-approved limits. Review with engine builder/vehicle documentation before deployment.

## Sources

- [Waveshare board and product variants](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75)
- [Waveshare verified header pin mapping](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/HARDWARE_REFERENCE.md)
- [Waveshare ESP-IDF 02_lvgl_demo_v9 main](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/examples/esp-idf/02_lvgl_demo_v9/main/main.c)
- [Espressif ESP32-S3 ADC oneshot driver](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/adc_oneshot.html)
- [Espressif ESP32-S3 ADC calibration](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/adc_calibration.html)
- [Caterham part listing](https://caterhamparts.co.uk/senders/17-temperature-sender-water-all-variants.html)

## License

MIT, see [LICENSE](LICENSE). Third-party libraries are **not vendored** and keep their own licenses when installed by the Espressif component manager.
