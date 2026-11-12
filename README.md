# Caterham Dry-Sump Oil Temperature Gauge

> **Hardware target:** Waveshare **ESP32-S3-Touch-AMOLED-1.75**, standard SKU 31261 or cased SKU 31262 (466×466 AMOLED, CO5300 controller). This firmware **does not** target the 2.1-inch RGB-LCD board, the `-1.75C` model, or the GNSS `-G` version without adaptation.

An independent **oil-temperature gauge** for the Caterham 420R dry-sump tank's resistive temperature sender (potentially Caterham part **71167**). No ECU wiring and no factory gauge modifications. An **ADS1115 16-bit I²C ADC** measures the voltage across the 470 Ω pull-up and across the sender (using a dedicated ground-sense wire to the tank), so the reading is independent of the 3.3 V rail and of ground offsets. The Waveshare board handles display, fault reporting and alarm presentation.

**Status: engineering prototype.** Host unit tests pass and the firmware compiles without warnings under ESP-IDF 5.5.5 (normal and demo builds; versions pinned in `dependencies.lock`). On hardware it has only been through a first boot: on 2026-11-11 the normal build booted on a Waveshare 31261, the display rendered, and with no ADS1115 fitted it showed `ADC UNAVAILABLE` and no temperature (see [docs/VALIDATION.md](docs/VALIDATION.md)). The ADS1115, the measurement circuit, the sender and calibration have **not** yet been verified on hardware. The firmware ships **without** a validated sender temperature curve, so it **shows ohms, not °C**, until you explicitly install measured calibration points. It is **not** a primary engine-protection instrument.

## Quick start

1. Read [hardware/BOM.md](hardware/BOM.md), [hardware/WIRING.md](hardware/WIRING.md), and [docs/SAFETY.md](docs/SAFETY.md) before wiring to the car.
2. On a **bench** (not in the vehicle) wire the ADS1115 to the H2 expansion header (I²C on GPIO17/18), then the 470 Ω pull-up, the three 1 kΩ / 100 nF input filters and a known test resistor in place of the sender; check all voltages with a meter before connecting the PCB.
3. Install **ESP-IDF 5.5.5** on Fedora as described in [docs/SETUP_FEDORA.md](docs/SETUP_FEDORA.md).
4. `idf.py set-target esp32s3 && idf.py build`, then flash through the board's `/dev/serial/by-id/usb-Espressif_*` port, never a bare `/dev/ttyACM0`, which can be another USB device: `idf.py -p "$(ls /dev/serial/by-id/usb-Espressif_*)" flash monitor` (see [Connect the board and flash](docs/SETUP_FEDORA.md#connect-the-board-and-flash)).
5. Boot with an **uncalibrated** sensor and confirm the display shows resistance and `CALIBRATION REQUIRED`; simulate open and short faults on the bench.
6. Independently characterise the exact sender, generate `main/CalibrationData.h`, rebuild, and verify °C against a reference thermometer. See [docs/CALIBRATION.md](docs/CALIBRATION.md).
7. Only consider vehicle mounting after power-conditioning, grounding, thermal, EMI and installation checks.

`./scripts/test_host.sh` runs the host-based conversion and calibration tests without any ESP32 hardware. GitHub Actions ([CI workflow](.github/workflows/ci.yml)) runs them, plus the normal and demo firmware builds, on every pull request and every push to `main`. `idf.py menuconfig` → **Caterham oil temperature gauge** → **Display synthetic sensor readings** runs a clearly marked BENCH DEMO; it must remain **off** for a real vehicle.

## Display

There are two layouts of the same readings.

* **Classic:** the title reads `OIL °C`, `OIL °F` or `OIL ohm`. The reading itself has no unit: large fixed-width digits, right-aligned so the ones digit stays in place, with a 3-digit reading centred on the screen (a four-character one such as `>284` or a 4-digit resistance uses the full width). The bottom line reads like `MAX 112  |  123 ohm` (maximum, then the sender resistance).
* **Sweep:** a 40-segment ring around the edge covering 30–130 °C, with scale labels 30…130 (86…266 in °F). Each lit segment shows the colour of its own place on the scale. Large seven-segment digits sit over faint ghost segments, with the unit above `OIL TEMP` and the status, footer and demo text below.

**Colours:** the digits, the status text and Classic's arc are graduated by temperature, blending smoothly between bands: dark blue below 50 °C, light blue 50–69, green 70–95, yellow 96–105, amber 106–119 and red from 120 °C. The colour is never less severe than a latched alarm (amber at 115 °C, red at 130 °C, unchanged). A fault or an uncalibrated reading is amber.

Beyond the calibration table the reading shows `<min` or `>max` (for example `<20` or `>140`) and no temperature reading, and the unit stays in the label. Uncalibrated firmware shows `ohm` in both layouts, with no scale numbers and no lit segments in Sweep.

**Changing layout:** press anywhere on the screen for at least 1 s (a long-press). A short tap or a swipe does nothing. The choice is stored in flash and restored at power-up; it is Classic on first boot, or if the stored choice is missing or can't be read.

**Choosing °F:** `idf.py menuconfig` → **Caterham oil temperature gauge** → **Display temperature unit**, then rebuild. °C is the default. The unit applies to the display only; the serial CSV log stays in °C.

## Repository structure

```text
.
├── main/
│   ├── main.cpp                    # main sampling / display loop
│   ├── Config.h                    # pull-up value and warning settings
│   ├── CalibrationData.h           # fail-closed uncalibrated default
│   ├── core/                       # platform-independent resistance & temp math,
│   │                               # plus display text (DisplayText.*), sweep ring
│   │                               # model (SweepModel.*) and layout choice/long-press
│   │                               # (LayoutChoice.*)
│   ├── hal/Ads1115Source.*         # ADS1115 over I²C (port 0, GPIO17/18)
│   ├── display/GaugeDisplay.*      # Waveshare BSP + LVGL 9 gauge; layout switching
│   │   ├── ClassicLayout.cpp       # Classic layout
│   │   ├── SweepLayout.cpp         # Sweep layout
│   │   ├── Layouts.h               # shared layout interface
│   │   └── fonts/                  # generated Classic digit font + its licence
│   ├── Kconfig.projbuild           # explicitly opt-in simulation
│   └── idf_component.yml          # Waveshare BSP / LVGL dependencies
├── hardware/                       # wiring, schematic (wiring.svg) and BOM
├── docs/                           # architecture, Fedora setup, calibration,
│                                   # installation, validation, safety, porting
├── tests/                          # native C++ and Python tests
└── scripts/                        # sender curve generator, tests
```

## Design decisions

| Area | Choice |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-AMOLED-1.75, **standard or -B** |
| Display | 466×466 QSPI **CO5300** AMOLED, Waveshare BSP + LVGL 9; two layouts (Classic and Sweep), switched by a 1 s long-press; °C or °F set in menuconfig |
| ADC | **ADS1115** 16-bit I²C ADC at 0x48, ±4.096 V range; SDA GPIO17 (H2-6), SCL GPIO18 (H2-7), optional ALERT/RDY GPIO16 (H2-8) |
| Measurement | Differential: Vp = A0 − A3 across the pull-up, Vs = A3 − A1 across the sender; `Rs = Rp × Vs / Vp` (ratiometric, cancels rail and ground offset) |
| Sender interface | 470 Ω 0.1 % pull-up to 3.3 V; signal wire to the spade plus a **dedicated ground-sense wire** to the tank bond stud |
| Input noise | 1 kΩ series + 100 nF filter on A0, A1, A3; protection stage required for vehicle use |
| Isolation | No connection to MBE ECU / existing instruments |
| Radio | Not used. The internal ADC2 is no longer needed, so Wi-Fi/BLE could be added later |
| Temperature calibration | Explicit sender measurements, no assumed Caterham curve |
| Safety | Fault detection, fail-closed calibration, external overvoltage protection required |

## Important known limitations

* The **sender's resistance-versus-temperature curve is not verified**. The listed 1,158–44 Ω range does not establish a full calibrated temperature curve.
* The ADS1115 cancels ground offset only while each input stays between its GND and VDD. Bond the gauge supply ground close to the tank bond and keep the offset near 0 V; a 100 kΩ bias resistor makes a broken ground-sense wire show as `GROUND FAULT`. Extremely cold high-resistance readings may be outside the calibrated range.
* Loose chassis grounds, alternator noise, load dumps and coolant/oil leaks are **vehicle integration hazards**. The I²C bus must stay short and inside the gauge enclosure.
* The board and its exposed contacts need an enclosure rated for the chosen location. A dashboard instrument must not obscure any legally required indicators or the driver's view.
* Calibrate up to at least the red threshold. Readings hotter than the top calibration point show `ABOVE CAL RANGE` and are treated as **critical** (red), because the real temperature is unknown.
* Alarm levels (60/115/130 °C) are **illustrative configurable defaults**, not Caterham- or engine-approved limits. Review with engine builder/vehicle documentation before deployment.

## Sources

- [Waveshare board and product variants](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75)
- [Waveshare verified header pin mapping](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/HARDWARE_REFERENCE.md)
- [Waveshare ESP-IDF 02_lvgl_demo_v9 main](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/examples/esp-idf/02_lvgl_demo_v9/main/main.c)
- [TI ADS1115 datasheet](https://www.ti.com/lit/ds/symlink/ads1115.pdf)
- [Adafruit ADS1115 guide](https://learn.adafruit.com/adafruit-4-channel-adc-breakouts)
- [Espressif ESP32-S3 I²C master driver](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/i2c.html)
- [Caterham part listing](https://caterhamparts.co.uk/senders/17-temperature-sender-water-all-variants.html)

## License

MIT, see [LICENSE](LICENSE). Classic's digits are a bitmap font generated from Montserrat Medium (SIL Open Font License 1.1); the licence and the generation record (in the header of the generated file) are in [main/display/fonts/](main/display/fonts/). Third-party libraries are **not vendored** and keep their own licenses when installed by the Espressif component manager.
