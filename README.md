# Caterham Dry-Sump Oil Temperature Gauge

> **Hardware target:** Waveshare **ESP32-S3-Touch-AMOLED-1.75**, standard SKU 31261 or cased SKU 31262 (466×466 AMOLED, CO5300 controller). This firmware **does not** target the 2.1-inch RGB-LCD board, the `-1.75C` model, or the GNSS `-G` version without adaptation.

An independent **oil-temperature gauge** for the Caterham 420R dry-sump tank's resistive temperature sender (potentially Caterham part **71167**). No ECU wiring and no factory gauge modifications. An **ADS1115 16-bit I²C ADC** measures the voltage across the 470 Ω pull-up and across the sender (using a dedicated ground-sense wire to the tank), so the reading is independent of the 3.3 V rail and of ground offsets. The Waveshare board handles display, fault reporting and alarm presentation.

**Status: bench prototype.** Host unit tests pass and the firmware compiles without warnings under ESP-IDF 5.5.5 (normal and demo builds; versions pinned in `dependencies.lock`). On the bench, so far ([docs/VALIDATION.md](docs/VALIDATION.md)):
- The firmware runs on a Waveshare 31261, and both display layouts pass in the demo build.
- On the veroboard the ADS1115 is found, and the gauge reads a bench sender (stamped 6811) in ohms, following the warmth of a hand.
- `SENSOR OPEN`, `SENSOR SHORT` and `GROUND FAULT` each show when the fault is made.
- A local build with a published sender curve, never committed, agreed with a reference probe up to 89 °C. That is an indication, not a calibration.

Still owed: the resistance accuracy against known resistors, the measured Rp, a sender calibration up to 130 °C, both interface PCBs (designed, not yet built) and every in-car test. The firmware ships **without** a validated sender temperature curve, so it **shows ohms, not °C**, until you explicitly install measured calibration points. It is **not** a primary engine-protection instrument.

## Quick start

1. Read [hardware/BOM.md](hardware/BOM.md), [hardware/WIRING.md](hardware/WIRING.md), and [docs/SAFETY.md](docs/SAFETY.md) before wiring to the car.
2. On a **bench** (not in the vehicle) build the circuit on a breadboard ([hardware/breadboard.svg](hardware/breadboard.svg)) or the veroboard ([hardware/veroboard.svg](hardware/veroboard.svg)): the ADS1115 module on the display's H2 header (I²C on GPIO17/18), the 470 Ω pull-up, the three 1 kΩ / 100 nF input filters, and a known test resistor in place of the sender. Check all voltages with a meter before connecting the display board. Neither build has input protection, so neither goes in the car.
3. Install **ESP-IDF 5.5.5** on Fedora as described in [docs/SETUP_FEDORA.md](docs/SETUP_FEDORA.md).
4. `idf.py set-target esp32s3 && idf.py build`, then flash through the board's `/dev/serial/by-id/usb-Espressif_*` port, never a bare `/dev/ttyACM0`, which can be another USB device: `idf.py -p "$(ls /dev/serial/by-id/usb-Espressif_*)" flash monitor` (see [Connect the board and flash](docs/SETUP_FEDORA.md#connect-the-board-and-flash)).
5. Boot with an **uncalibrated** sensor and confirm the display shows resistance and `CALIBRATION REQUIRED`; simulate open and short faults on the bench.
6. Independently characterise the exact sender, generate `main/CalibrationData.h`, rebuild, and verify °C against a reference thermometer. See [docs/CALIBRATION.md](docs/CALIBRATION.md).
7. For the car, build one of the interface PCBs (below) and pass its bench checks. Then do the power, grounding, thermal, EMI and installation checks in [docs/INSTALLATION.md](docs/INSTALLATION.md).

`./scripts/test_host.sh` runs the host tests without any ESP32 hardware: the conversion, alarm and display logic, the calibration tool, the sender graph and both PCB designs' checks. GitHub Actions ([CI workflow](.github/workflows/ci.yml)) runs them, plus the normal and demo firmware builds, on every pull request and every push to `main`. `idf.py menuconfig` → **Caterham oil temperature gauge** → **Display synthetic sensor readings** runs a clearly marked BENCH DEMO; it must remain **off** for a real vehicle.

## Interface hardware

| Build | What it is | Power | Status |
|---|---|---|---|
| Breadboard or bench veroboard ([hardware/WIRING.md](hardware/WIRING.md)) | Adafruit ADS1115 module and the measurement circuit, wired to the display's header | USB, on the bench | Bench-tested on the veroboard (2026-11-13/14). No input protection: **bench only** |
| [surface-mount-v1](hardware/pcb/surface-mount-v1/README.md) | 39 × 23 mm two-layer board that plugs into the display case's socket. Surface-mount parts, fitted by the board house | 5 V on its harness connector, from a separate automotive 12 V → 5 V converter | Designed, not yet built (#48) |
| [through-hole-v1](hardware/pcb/through-hole-v1/README.md) | 92 × 64 mm single-sided board, hand-soldered, in its own 3D-printed box, joined to the display by a cable of 30 cm or less | Fused, ignition-switched 12 V, through its own fuse, TVS, reverse-polarity diode and 5 V regulator | Designed, not yet built (#51) |
| [through-hole-usb-v1](hardware/pcb/through-hole-usb-v1/README.md) | 65 × 62 mm single-sided board, hand-soldered, in its own 3D-printed box, joined to the display by a cable of 30 cm or less | The display's own USB-C socket (in the car, an ignition-switched USB-C charger); the board takes the display's 3.3 V. The harness brings a dedicated ground return from the tank | Designed, not yet built (#54) |
| [veroboard-usb-v1](hardware/pcb/veroboard-usb-v1/README.md) | 86.4 × 30.5 mm stripboard (33 holes × 11 strips), hand-built, in its own box, with screw terminal blocks for the display cable and the harness | As through-hole-usb-v1: the display's USB-C socket, with a dedicated ground return from the tank | Designed, layout checked; not yet built (#55) |

All four interface boards have the same measurement and the same input protection: each harness tap is 1 kΩ, a clamp pair to 3.3 V and GND, then the filter's 1 kΩ, and a TVS sits on the signal wire. The firmware is the same for every build; set `kPullUpOhms` in `main/Config.h` to your measured Rp. Each board's folder has its parts list (with connectors and mating parts), its drawings (and for the PCBs, renders and Gerbers) and the checks to do before and after building it.

## Display

There are two layouts of the same readings.

* **Classic:** the title reads `OIL °C`, `OIL °F` or `OIL ohm`. The reading itself has no unit: large fixed-width digits, right-aligned so the ones digit stays in place, with a 3-digit reading centred on the screen (a four-character one such as `>284` or a 4-digit resistance uses the full width). The bottom line reads like `MAX 112  |  123 ohm` (maximum, then the sender resistance).
* **Sweep:** a 40-segment ring around the edge covering 30–130 °C, with scale labels 30…130 (86…266 in °F). Each lit segment shows the colour of its own place on the scale. Three large seven-segment digits, centred, sit over faint ghost segments; beyond the table a `<` or `>` sign appears just left of them. The unit sits above `OIL TEMP`, with the status, footer and demo text below. A resistance of 1000 Ω or more (uncalibrated only) shows as `--` in Sweep, and in full in Classic.

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
├── hardware/                       # wiring, schematic (wiring.svg), bench breadboard (breadboard.svg) and veroboard (veroboard.svg) layouts, and BOM
│   └── pcb/                        # interface PCBs, each self-contained: KiCad design, generator, build.sh, renders, Gerbers, parts lists
│       ├── surface-mount-v1/       # plugs onto the display; SMD, board-house assembled
│       ├── through-hole-v1/        # own box, cabled to the display, 12 V in; single-sided, hand-soldered
│       ├── through-hole-usb-v1/    # as through-hole-v1, but the display runs from USB-C; no 12 V input
│       └── veroboard-usb-v1/       # through-hole-usb-v1's circuit on stripboard, with terminal blocks
├── docs/                           # architecture, Fedora setup, calibration,
│                                   # installation, validation, safety, porting,
│                                   # sender R/T graph (sender-curve.svg) and its data
├── tests/                          # native C++ and Python tests
└── scripts/                        # calibration-header and sender-graph generators, test runner
```

## Design decisions

| Area | Choice |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-AMOLED-1.75, **standard or -B** |
| Display | 466×466 QSPI **CO5300** AMOLED, Waveshare BSP + LVGL 9; two layouts (Classic and Sweep), switched by a 1 s long-press; °C or °F set in menuconfig |
| ADC | **ADS1115** 16-bit I²C ADC at 0x48, ±4.096 V range; SDA GPIO17 (H2-6), SCL GPIO18 (H2-7); ALERT/RDY not used (GPIO16 / H2-8 left open) |
| Measurement | Differential: Vp = A0 − A3 across the pull-up, Vs = A3 − A1 across the sender; `Rs = Rp × Vs / Vp` (ratiometric, cancels rail and ground offset) |
| Sender interface | 470 Ω 0.1 % pull-up to 3.3 V; signal wire to the spade plus a **dedicated ground-sense wire** to the tank bond stud |
| Input noise | 1 kΩ series + 100 nF filter on A0, A1, A3 |
| Input protection | On all four interface boards: clamp pairs on the signal and ground-sense taps, and a TVS on the signal wire. Designed, not yet bench-tested or qualified in the car. The breadboard and the bench veroboard have none |
| Interface hardware | Four options for the car: surface-mount-v1 (plugs onto the display), and through-hole-v1, through-hole-usb-v1 and the veroboard-usb-v1 stripboard (each in its own box, cabled to the display); see [hardware/pcb](hardware/pcb/) |
| Power | USB on the bench. In the car: 5 V from a separate automotive converter (surface-mount-v1), the car's fused 12 V into through-hole-v1's own regulator, or a USB-C charger into the display (through-hole-usb-v1 and veroboard-usb-v1, with a dedicated ground return from the tank). Never 12 V to the display |
| Isolation | No connection to MBE ECU / existing instruments |
| Radio | Not used. The internal ADC2 is no longer needed, so Wi-Fi/BLE could be added later |
| Temperature calibration | Explicit sender measurements, no assumed Caterham curve |
| Safety | Fault detection, fail-closed calibration, no extrapolation; input protection on the PCBs, still to be tested |

## Important known limitations

* The **sender's resistance-versus-temperature curve is not verified**. The listed 1,158–44 Ω range is the 40 °C and 140 °C points of the likely factory sender's published curve (Caerbont TT6811-03, #42). It is not a calibration of the fitted sender. A bench sender stamped 6811 didn't follow that curve: up to 89 °C it tracked the Bosch 0 280 130 026 curve (2.5 kΩ at 20 °C) instead. [docs/sender-curve.svg](docs/sender-curve.svg) plots both curves against the firmware's thresholds.
* The ADS1115 cancels ground offset only while each input stays between its GND and VDD. Bond the gauge supply ground close to the tank bond (on the USB-powered boards the harness's own return wire does this) and keep the offset near 0 V; a 100 kΩ bias resistor makes a broken ground-sense wire show as `GROUND FAULT`. Extremely cold high-resistance readings may be outside the calibrated range. Readings over 5 kΩ, from a sender colder than about 10 °C, currently show `SENSOR OPEN` (#40).
* Loose chassis grounds, alternator noise, load dumps and coolant/oil leaks are **vehicle integration hazards**. The I²C bus must stay short and never runs to the tank. On surface-mount-v1 it stays on the display; the through-hole boards' display cable must be 30 cm or less, and their one-hour I²C soak tests are still owed (#51, #54).
* An open A3 or A1 tap (a cracked joint or a lifted socket contact) gives a plausible but wrong resistance with no fault (#39). The interface boards solder every tap, and a firmware check is proposed in #39.
* None of the interface boards has been built yet. Their input protection is designed but untested, so don't assume it survives a load dump. through-hole-v1 needs about 7 V at its input to hold 5 V, so the display may reset while the engine cranks (#43).
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
