# Caterham Dry-Sump Oil Temperature Gauge

> **Hardware target:** Waveshare **ESP32-S3-Touch-AMOLED-1.75**, standard SKU 31261 or cased SKU 31262 (466×466 AMOLED, CO5300 controller). This firmware **does not** target the 2.1-inch RGB-LCD board, the `-1.75C` model, or the GNSS `-G` version without adaptation.

An independent **oil-temperature gauge** for the Caterham 420R dry-sump tank's resistive temperature sender (potentially Caterham part **71167**). It needs no ECU wiring and no factory gauge modifications.
- **Measurement:** an **ADS122C04 24-bit I²C ADC** drives its own 250 µA current source through a reference resistor and then the sender, and compares the voltage across each. A dedicated ground-sense wire runs to the tank. So the reading is independent of the current's accuracy, the 3.3 V rail and ground offsets.
- **Hardware:** option A is three resistors on the ADC module's pins. Option B, recommended for the car, adds two filter capacitors across the pins. Either can be built without a board, on stripboard, or on a PCB. The display runs from USB-C.
- **Display:** the Waveshare board handles display, fault reporting and alarm presentation.

**Status: bench prototype, new front end.** Host unit tests pass and the firmware compiles without warnings under ESP-IDF 5.5.5 (normal and demo builds; versions pinned in `dependencies.lock`). On the bench, so far ([docs/VALIDATION.md](docs/VALIDATION.md)):
- The firmware runs on a Waveshare 31261, and the Classic and Sweep layouts pass in the demo build. Sweep's LED and the Needle and Trace layouts (#10–#12) build and pass the host tests, but haven't been checked on the screen yet.
- With the earlier front end, a bench sender stamped 6811 followed the Bosch 0 280 130 026 curve up to 89 °C against a reference probe. That is an indication about the sender, not a calibration.

**Still owed:**
- every bench test of the ADS122C04 front end ([hardware/WIRING.md](hardware/WIRING.md), bench steps), for whichever option is built;
- the measured Rref;
- a sender calibration up to 130 °C;
- every in-car test, including the engine-running noise check.

The firmware ships **without** a validated sender temperature curve, so it **shows ohms, not °C**, until you explicitly install measured calibration points. It is **not** a primary engine-protection instrument.

## Quick start

1. Read [hardware/BOM.md](hardware/BOM.md), [hardware/WIRING.md](hardware/WIRING.md), and [docs/SAFETY.md](docs/SAFETY.md) before wiring to the car.
2. On a **bench** (not in the vehicle), wire the circuit on a breadboard as in [hardware/WIRING.md](hardware/WIRING.md):
   - the ADS122C04 module to the display's socket (3.3V, G, and I²C on GPIO17/18);
   - Rref, R1 and R2;
   - a known test resistor in place of the sender.

   Check all voltages with a meter before connecting the display board.
3. Install **ESP-IDF 5.5.5** on Fedora as described in [docs/SETUP_FEDORA.md](docs/SETUP_FEDORA.md).
4. `idf.py set-target esp32s3 && idf.py build`, then flash through the board's `/dev/serial/by-id/usb-Espressif_*` port, never a bare `/dev/ttyACM0`, which can be another USB device: `idf.py -p "$(ls /dev/serial/by-id/usb-Espressif_*)" flash monitor` (see [Connect the board and flash](docs/SETUP_FEDORA.md#connect-the-board-and-flash)).
5. Boot with an **uncalibrated** sensor and confirm the display shows resistance and `CALIBRATION REQUIRED`; simulate open and short faults on the bench.
6. Independently characterise the exact sender, generate `main/CalibrationData.h`, rebuild, and verify °C against a reference thermometer. See [docs/CALIBRATION.md](docs/CALIBRATION.md).
7. For the car, solder the same parts to the module, fit the terminal strip and the box ([hardware/WIRING.md](hardware/WIRING.md), "Building it without a board"), and pass the bench steps, including the short-to-battery test. Then do the power, grounding, thermal, EMI and installation checks in [docs/INSTALLATION.md](docs/INSTALLATION.md).

`./scripts/test_host.sh` runs the host tests without any ESP32 hardware: the conversion, fault, alarm and display logic, the ADC's frame and CRC decoding, a circuit simulation of both front-end options, the stripboard and PCB checks, the calibration tool and the sender graph. GitHub Actions ([CI workflow](.github/workflows/ci.yml)) runs them, plus the normal and demo firmware builds, on every pull request and every push to `main`. `idf.py menuconfig` → **Caterham oil temperature gauge** → **Display synthetic sensor readings** runs a clearly marked BENCH DEMO; it must remain **off** for a real vehicle.

## Interface hardware

The front end is small enough to need no interface board. **Option A** is three resistors soldered to the Adafruit ADS122C04 module, with a 3-way terminal strip for the harness, all in a small box beside the display. **Option B**, recommended for the car, adds two 47 nF capacitors across the module's analogue pins. They filter interference near the ADC's 256 kHz sampling rate, and a shorted one gives a fault rather than a reading.

Each option has its own folder: [hardware/option-a](hardware/option-a/README.md) and [hardware/option-b](hardware/option-b/README.md). Each folder has:
- a description of the design;
- a complete parts list;
- a checked stripboard layout;
- a two-layer PCB, as a KiCad project with Gerbers.

[hardware/WIRING.md](hardware/WIRING.md) has the circuit, the build without a board and the bench steps.

| Part | Where | Why |
|---|---|---|
| Rref, 2.00 kΩ 0.1 % ≤ 25 ppm/°C | A0 to the sender wire | Carries the ADC's 250 µA current to the sender; the voltage across it is the reference. Set `kReferenceOhms` in `main/Config.h` to its measured value |
| R1, 2.2 kΩ | A1 to the sender wire | Sender high side. Limits fault current |
| R2, 2.2 kΩ | A2 to the ground-sense wire | Sender low side. Limits fault current |

The display runs from its own USB-C socket (in the car, an ignition-switched USB-C charger), and the module takes the display's 3.3 V. The harness is three wires: signal, ground sense, and a return wire from the gauge's ground to the tank bond stud.

## Display

There are seven layouts of the same readings.

* **Classic:** the title reads `OIL °C`, `OIL °F` or `OIL ohm`. The reading itself has no unit: large fixed-width digits, right-aligned so the ones digit stays in place, with a 3-digit reading centred on the screen (a four-character one such as `>284` or a 4-digit resistance uses the full width). The bottom line reads like `MAX 112  |  123 ohm` (maximum, then the sender resistance).
* **Sweep:** a 40-segment ring around the edge covering 30–130 °C, with scale labels 30…130 (86…266 in °F). Each lit segment shows the colour of its own place on the scale. Three large seven-segment digits, centred, sit over faint ghost segments; beyond the table a `<` or `>` sign appears just left of them. The unit sits above `OIL TEMP`, with the status, footer and demo text below. A resistance of 1000 Ω or more (uncalibrated only) shows as `--` in Sweep, and in full in Classic.
  * **The LED:** a round light at 6 o'clock, in the open bottom of the ring. It glows in the reading's colour and pulses while an alarm is latched: slowly at amber (`HIGH TEMPERATURE`, from 115 °C after the 2.5 s debounce) and faster at red. It's off on a sensor fault and in an uncalibrated build. The exception is a brief fault while an alarm is latched: the LED keeps pulsing, as the digits keep the alarm colour.
* **Needle:** an analogue dial. A white scale runs from lower left to lower right over 30–130 °C, with five numbers (30, 55, 80, 105, 130; 86…266 in °F), a tick between each pair and a pointer at each number. A red needle sits under a dark hub, with `OIL` above the unit at the bottom. There's no digital readout. The needle follows the same rules as Sweep's segments. It's hidden on a fault or when uncalibrated, rests at the start below the table, and sits at the table maximum above it. It moves smoothly, never past the reading, and doesn't sweep at power-up.
* **Trace:** the last 60 s, one point a second, newest on the right, as a thick line in the colours of the temperatures that made it: each point's colour, the same as the digits' at that moment, carries into the line around it. The newest point is a dot in its colour, or a ring when it's beyond the table. The window is 30 °C tall, with grid lines every 10 °C. It moves only when the newest point comes within 3 °C of its top or bottom, then re-centres. Thin amber and red lines mark 115 °C and 130 °C when they're in view. A fault or a reading below the table leaves a gap, never a carried-over value. The LED from Sweep sits at 6 o'clock under the status lines, with the same colours and pulses. There's no digital readout and, uncalibrated, no trace. The history is kept whichever layout is showing, and starts empty at power-up.
* **Tape:** an aircraft-style moving scale. The tape scrolls past a fixed pointer, so the scale moves rather than the needle. It shows 40 °C at a time, with ticks every degree and numbers every 10. A strip down its edge is in each temperature's colour, and amber and red bars mark 115 °C and 130 °C, so the hot zones come into view before the oil gets there. Past the ends of the calibration table the tape is hatched, with no numbers, and a small marker shows the session maximum. The reading sits in a window by the pointer: `>130` beyond the table, `--` on a fault, and ohms when uncalibrated. Under it is the rate of change, such as `▲ 2.4 °C/min`, `▼ 1.1 °C/min` or `STEADY` (under 0.5 °C/min). The rate comes from the last 30 one-second samples and is hidden unless all 30 are clean readings inside the table. The tape eases to each reading and is hidden on a fault or when uncalibrated.
* **OEM:** styled on the Caterham factory gauges. A black face in a dark rim, with a short grey tick every 10 °C from 50 to 130 °C and four numbers, 70, 90, 110 and 130 (158…266 in °F), mapped one for one onto the factory oil-pressure gauge's 0–8 bar. 90 is at 12 o'clock with the unit under it, and 50 has a tick but no number. An off-white tapered needle with a counterweight sits under a black hub. It follows Needle's rules on this span, so it rests at the start below 50 °C and below the table, sits at the end above it, and is hidden on a fault or when uncalibrated. It doesn't drop to the stop on a fault, because a resting needle would look like cold oil. There's no digital readout.
  * **The oil symbol is the LED:** an oil can with a thermometer below the hub, lit in the reading's colour and pulsing with the alarm, exactly as the LED on Sweep and Trace. It's dark grey when the LED would be off.
  * **CATERHAM:** the word shows at the bottom only while all is well: `NORMAL` or `OIL WARMING`, no alarm latched, a calibrated build that isn't the demo. Anything else (an alarm, a fault, a reading beyond the table, no calibration) puts the status line and footer in its place, so the dial changes visibly the moment something needs attention. The demo build never shows it.
* **White:** styled on a white-faced aftermarket oil-temperature gauge. A white face in a black ring, with a black tick every 2 °C from 40 to 140 °C (long at each 20, medium at the other tens) and six numbers, 40, 60, 80, 100, 120 and 140 (104…284 in °F), with 90 at 12 o'clock. A red tapered needle with a short counterweight sits under a black hub, with `OIL` and the unit below it. The needle follows Needle's rules on this span: it rests at the start below 40 °C and below the table, sits at the table maximum above it, and is hidden on a fault or when uncalibrated. There's no digital readout.
  * **The LED:** the same LED as on Sweep and Trace, at 6 o'clock in a round black socket, so its colours and glow show against the white. When it's off, the socket shows a dark-grey lens.
  * **The status window:** a black window at the bottom, running down into the ring, with the status line and footer in their usual colours. It shows only when something needs attention, which is exactly when OEM would hide `CATERHAM`, so the face stays clean while all is well. The demo build always shows it.
  * **Brightness:** a white face is much brighter than the dark layouts, especially at night. The firmware has no brightness control, so long-press to a dark layout if it dazzles.

The status line and the footer are on every layout, so alarms and faults always show as text. On OEM they take the wordmark's place, and on White they appear in a black window, whenever anything needs attention.

**Colours:** the digits, the status text and Classic's arc are graduated by temperature, blending smoothly between bands: dark blue below 50 °C, light blue 50–69, green 70–95, yellow 96–105, amber 106–119 and red from 120 °C. The colour is never less severe than a latched alarm (amber at 115 °C, red at 130 °C, unchanged). A fault or an uncalibrated reading is amber.

Beyond the calibration table the reading shows `<min` or `>max` (for example `<20` or `>140`) and no temperature reading, and the unit stays in the label. Uncalibrated firmware shows `ohm` in every layout. There are no scale numbers, no lit segments, no LED (OEM's symbol stays dark grey, and White's socket shows its dark-grey lens), no needle, no trace, and no tape or rate.

**Changing layout:** press anywhere on the screen for at least 1 s (a long-press). Each press moves to the next layout: Classic, Sweep, Needle, Trace, Tape, OEM, White, then Classic again. A short tap or a swipe does nothing. The choice is stored in flash and restored at power-up; it is Classic on first boot, or if the stored choice is missing or can't be read.

**Choosing °F:** `idf.py menuconfig` → **Caterham oil temperature gauge** → **Display temperature unit**, then rebuild. °C is the default. The unit applies to the display only; the serial CSV log stays in °C.

## Repository structure

```text
.
├── main/
│   ├── main.cpp                    # main sampling / display loop
│   ├── Config.h                    # reference resistor value, fault and warning settings
│   ├── CalibrationData.h           # fail-closed uncalibrated default
│   ├── core/                       # platform-independent resistance & temp math,
│   │                               # ADC frame/CRC decoding (AdcFrame.h),
│   │                               # plus display text (DisplayText.*), sweep ring,
│   │                               # LED and needle model (SweepModel.*), the trace's
│   │                               # history (TraceModel.*), the tape and rate of change
│   │                               # (TapeModel.*), the OEM and White dials
│   │                               # (OemModel.*, WhiteModel.*)
│   │                               # and layout choice/long-press
│   │                               # (LayoutChoice.*)
│   ├── hal/Ads122c04Source.*       # ADS122C04 over I²C (port 0, GPIO17/18)
│   ├── display/GaugeDisplay.*      # Waveshare BSP + LVGL 9 gauge; layout switching
│   │   ├── ClassicLayout.cpp       # Classic layout
│   │   ├── SweepLayout.cpp         # Sweep layout, with its LED
│   │   ├── NeedleLayout.cpp        # Needle layout (analogue dial)
│   │   ├── TraceLayout.cpp         # Trace layout (the last 60 s)
│   │   ├── TapeLayout.cpp          # Tape layout (moving scale and rate of change)
│   │   ├── OemLayout.cpp           # OEM layout (styled on the Caterham factory gauge)
│   │   ├── WhiteLayout.cpp         # White layout (white face, status window)
│   │   ├── StatusLines.cpp         # shared widgets: status lines and the 6 o'clock LED
│   │   ├── Layouts.h               # shared layout interface
│   │   └── fonts/                  # generated Classic digit font + its licence
│   ├── Kconfig.projbuild           # explicitly opt-in simulation
│   └── idf_component.yml          # Waveshare BSP / LVGL dependencies
├── hardware/                       # wiring and build (WIRING.md), schematic (wiring.svg), BOM
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
| Display | 466×466 QSPI **CO5300** AMOLED, Waveshare BSP + LVGL 9; seven layouts (Classic, Sweep, Needle, Trace, Tape, OEM and White), cycled by a 1 s long-press; °C or °F set in menuconfig |
| ADC | **ADS122C04** 24-bit I²C ADC (Adafruit module 6432) at 0x40, internal 2.048 V reference, gain 1 with the PGA bypassed; SDA GPIO17, SCL GPIO18; DRDY not used (GPIO16 left open) |
| Measurement | The ADC's 250 µA current source drives Rref and the sender in series; Vref = A0 − A1, Vs = A1 − A2; `Rs = Rref × Vs / Vref` (ratiometric: cancels the current's accuracy, the reference, the rail and ground offset) |
| Sender interface | Signal wire to the spade, a **dedicated ground-sense wire** and a return wire to the tank bond stud |
| Integrity | Every register write read back with a CRC; every result checked by CRC16 and conversion counter; ADC offset self-test; excitation-current window; burn-out checks of both taps |
| Input protection | At least 2 kΩ between every harness wire and the ADC; no clamps or TVS. Not load-dump qualified; the short-to-battery bench test is owed |
| Interface hardware | Three resistors on the module's pins (option A), plus two capacitors across the pin pairs (option B, for the car). Build it without a board, on stripboard or on a two-layer PCB (hardware/option-a, hardware/option-b) |
| Power | USB-C into the display, on the bench and in the car (an ignition-switched charger); the module takes the display's 3.3 V. Never 12 V to the display |
| Isolation | No connection to MBE ECU / existing instruments |
| Radio | Not used. The internal ADC isn't needed, so Wi-Fi/BLE could be added later |
| Temperature calibration | Explicit sender measurements, no assumed Caterham curve |
| Safety | Fault detection, fail-closed calibration, no extrapolation; fault-current limiting still to be bench-tested |

## Important known limitations

* The **sender's resistance-versus-temperature curve is not verified**. The listed 1,158–44 Ω range is the 40 °C and 140 °C points of the likely factory sender's published curve (Caerbont TT6811-03). It is not a calibration of the fitted sender. A bench sender stamped 6811 didn't follow that curve: up to 89 °C it tracked the Bosch 0 280 130 026 curve (2.5 kΩ at 20 °C) instead. [docs/sender-curve.svg](docs/sender-curve.svg) plots both curves against the firmware's thresholds.
* The ADC cancels ground offset only while each input stays within 0.1 V of its GND range. Keep the tank's offset near 0 V. The harness's return wire gives the gauge a ground at the tank. The ADC's burn-out source makes a broken ground-sense wire show as `GROUND FAULT`.
* Readings over 5 kΩ show `SENSOR OPEN`. On the Bosch 0 280 130 026 curve, that's a sender colder than about 4 °C.
* Loose chassis grounds, alternator noise, load dumps and coolant/oil leaks are **vehicle integration hazards**. The I²C bus must stay short (a display lead of 20 cm or less) and never runs to the tank. The one-hour I²C soak test is still owed.
* An open tap at R1 or R2 is detected by the ADC's burn-out source (`SENSOR OPEN` or `GROUND FAULT`). The bench tap tests in WIRING.md are still owed.
* Option A has no input filter, so ignition or alternator interference near the ADC's sampling rate could make the reading noisy or shift it without a fault. Option B filters the difference between the inputs; interference common to every wire is left to the ADC's own rejection. The in-car noise check is still owed.
* There are no clamp diodes or TVS. The series resistors keep a short to battery voltage within the ADC's input rating, but don't assume the front end survives a load dump.
* The board and its exposed contacts need an enclosure rated for the chosen location. A dashboard instrument must not obscure any legally required indicators or the driver's view.
* Calibrate up to at least the red threshold. Readings hotter than the top calibration point show `ABOVE CAL RANGE` and are treated as **critical** (red), because the real temperature is unknown.
* Alarm levels (60/115/130 °C) are **illustrative configurable defaults**, not Caterham- or engine-approved limits. Review with engine builder/vehicle documentation before deployment.

## Sources

- [Waveshare board and product variants](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75)
- [Waveshare verified header pin mapping](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/HARDWARE_REFERENCE.md)
- [Waveshare ESP-IDF 02_lvgl_demo_v9 main](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/examples/esp-idf/02_lvgl_demo_v9/main/main.c)
- [TI ADS122C04 datasheet (SBAS751B)](https://www.ti.com/lit/ds/symlink/ads122c04.pdf)
- [Adafruit ADS122C04 guide](https://learn.adafruit.com/adafruit-ads122c04-24-bit-adc) and [PCB files](https://github.com/adafruit/Adafruit-ADS122C04-24-Bit-ADC-PCB)
- [Espressif ESP32-S3 I²C master driver](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/i2c.html)
- [Caterham part listing](https://caterhamparts.co.uk/senders/17-temperature-sender-water-all-variants.html)

## License

MIT, see [LICENSE](LICENSE). Classic's digits are a bitmap font generated from Montserrat Medium (SIL Open Font License 1.1); the licence and the generation record (in the header of the generated file) are in [main/display/fonts/](main/display/fonts/). Third-party libraries are **not vendored** and keep their own licenses when installed by the Espressif component manager.
