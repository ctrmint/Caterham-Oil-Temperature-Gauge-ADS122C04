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

There are twelve layouts of the same readings. Every one is on a pure black background (`#000000`), apart from White's white face, so the AMOLED's unused pixels are switched off.

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
  * **Brightness:** a white face is much brighter than the dark layouts, especially at night. The firmware has no brightness control, so long-press to Night, or another dark layout, if it dazzles.
* **Night:** a dark layout for driving at night, with no ring, face or ticks. It's quiet while all is well and lights up the moment anything needs attention. The reading is at the centre of the screen in both states.
  * **Quiet:** while all is well (exactly when OEM shows `CATERHAM`), only the reading shows: whole degrees in a small font, in its usual colour at half brightness, with a dim unit under it. Under that, a small dim dot rises and falls every 2 s. It steps once per display update, so if the updates ever stopped the dot would stop too: a dark screen can't pass for a live one.
  * **Lit:** anything else (an alarm, a fault, a reading beyond the table, no calibration, and always in the demo build) shows Classic's large digits at full brightness with `OIL °C` above them, the status line and footer below, and the LED from Sweep and Trace under them. The status goes to `HIGH TEMPERATURE` at 115 °C, before the alarm's 2.5 s debounce, so the screen lights at the threshold.
* **Nixie:** the reading in four glowing nixie tubes, right-aligned in the same cells as Classic, so a 3-digit reading leaves the first tube dark. The numerals are drawn as thin glowing wires, always nixie orange, with the ten unlit numerals faint behind them as in a real tube. `<`, `>` and `-` are drawn the same way, and anything the tubes can't show is `--`. `OIL °C` sits above the tubes, with the status line, the footer and the LED from Sweep and Trace always shown below. Nixie orange is close to the alarm amber, so on this layout the status line (in the reading's colour) and the LED carry the colour, not the numerals.
* **Heritage:** a dial in the spirit of a 1960s instrument. A chrome-look bezel (lit from the upper left), a black face, and cream ticks and numbers on a short 90° sweep across the top: 50–130 °C, a tick every 5 °C (long at each 20), with 50, 90 and 130 numbered (122, 194, 266 in °F) and 90 at 12 o'clock. A slim cream needle with a short counterweight turns about a pivot below the centre, under a black boss with a chrome cap. It follows Needle's rules on this span, as OEM's does, and never points below the pivot. There's no digital readout.
  * **Under the pivot:** `OIL TEMP` and the unit while all is well, exactly as OEM shows `CATERHAM`, or the status line and footer in their place whenever anything needs attention.
  * **The lamp:** the LED from Sweep and Trace at 6 o'clock in a chrome surround, with a dark lens when it's off.
* **Track:** a data page for track days. At the top, the reading in its colour, with `MAX 121` (the session maximum, or `MAX >130` once it went above the table) and the rate of change (as on Tape) beneath it. Then a row for each band since power-up, each with a bar in the band's colour and the time spent there:
  * `<60`, `60-115`, `115-130`, `130+` (from the alarm thresholds; `<140`, `140-239`, `239-266`, `266+` in °F) and `--`, which is time with no temperature known: a fault, or a gap of more than 2 s between readings. A reading beyond the table counts towards the coldest or hottest band.
  * Each bar is its band's share of the time since power-up, so the bars add up to one full bar. Any time at all in a band shows as at least a sliver. Times read `m:ss`, then `1h05`, up to `99h59`. They start again from zero at each power-up and are counted whichever layout is showing.
  * The status line, the footer and the LED from Sweep and Trace sit below. Uncalibrated, only the ohms and the status show: no rows, no MAX and no rate.
* **Warm-up:** for the first minutes of a drive. A 270° ring like Classic's arc fills from 0 °C to the 60 °C warm threshold in the reading's colour, so it's full once the oil is warm and turns amber or red with the reading. Classic's large digits sit in the middle with `OIL °C` above, and the status line and footer below. There's no LED.
  * **The warm-up line:** while warming, the time since power-up and the rate of warming (as on Tape), such as `2:35    ▲ 3.1 °C/min`. At the first reading of 60 °C or more it becomes `WARM IN 6:12`, which stays for the rest of the power cycle even if the oil cools again. If the oil was already warm when the gauge started, it shows `WARM AT START`.
  * **On a fault the ring disappears altogether.** An empty ring would look like cold oil. Below the table the ring shows its empty track, because the oil is known to be cold. It never predicts when the oil will be warm. Uncalibrated, there's no ring and no warm-up line.

The status line and the footer are on every layout, so alarms and faults always show as text. On OEM they take the wordmark's place, on Heritage the legend's, on White they appear in a black window, and Night lights up with them, whenever anything needs attention.

**Colours:** the digits (except Nixie's, which are always nixie orange), the status text and Classic's arc are graduated by temperature, blending smoothly between bands: dark blue below 50 °C, light blue 50–69, green 70–95, yellow 96–105, amber 106–119 and red from 120 °C. The colour is never less severe than a latched alarm (amber at 115 °C, red at 130 °C, unchanged). A fault or an uncalibrated reading is amber.

Beyond the calibration table the reading shows `<min` or `>max` (for example `<20` or `>140`) and no temperature reading, and the unit stays in the label. Uncalibrated firmware shows `ohm` in every layout. There are no scale numbers, no lit segments, no LED (OEM's symbol stays dark grey, and White's socket and Heritage's lamp show their dark lens), no needle, no trace, no tape or rate, no Track rows, and no Warm-up ring or warm-up line.

**Changing layout:** press anywhere on the screen for at least 1 s (a long-press). Each press moves to the next layout: Classic, Sweep, Needle, Trace, Tape, OEM, White, Night, Nixie, Heritage, Track, Warm-up, then Classic again. A short tap or a swipe does nothing. The choice is stored in flash and restored at power-up; it is Classic on first boot, or if the stored choice is missing or can't be read.

**Choosing °F:** `idf.py menuconfig` → **Caterham oil temperature gauge** → **Display temperature unit**, then rebuild. °C is the default. The unit applies to the display only; the serial CSV log stays in °C.

## Wi-Fi

The gauge can join a Wi-Fi network or host it. It's off until you ask for it, and it can't delay a reading or touch the alarms: everything Wi-Fi runs in its own task below the measurement loop's priority.

* **One network, two ways:** when Wi-Fi is switched on, the gauge looks for the network named in `main/wifi_secrets.h`.
  * **Found:** it joins as a client. The panel shows `Joined <network>` and the address it was given.
  * **Not found:** it hosts that network itself, with the same name and password, at `192.168.4.1`. The panel shows `Hosting <network>`. A laptop set up for that network connects either way.
  * **Found but it can't join** (a wrong password, say): `Wifi failed` and `Can't join <network>`. It never hosts a second network with the same name.
  * **A joined network that drops** (driving away from home, say): it rejoins if it can, and hosts the network once it's gone.

* **The BOOT button** (the top side button) switches it. Each short press moves to the next state, and a black panel across the top of the screen confirms it for 3 s:
  * `Wifi Off` → `Wifi - Web On, UDP Off` → `Wifi - Web On, UDP On` → `Wifi - Web Off, UDP On` → `Wifi Off`.
  * States for features that aren't built in are skipped. With only the UDP stream built, presses go `Wifi Off` ↔ `Wifi - Web Off, UDP On`; with only the web page, `Wifi Off` ↔ `Wifi - Web On, UDP Off`.
  * While starting, the panel says `Wifi starting...` and `Looking for <network>`. Once up, it says `Joined` or `Hosting`, with the network and the gauge's address. `Wifi failed` shows in amber if it can't start.
  * The panel never covers the status lines or the LED on any layout, and the button never touches an alarm.
  * The state is saved and restored at power-up. A long press does nothing. Don't hold BOOT while powering up, or the board starts in flashing mode.
* **The web log page** (#32): open `http://<the address on the panel>/` in a phone's browser on the same network (`http://192.168.4.1/` when the gauge hosts it). It shows the live reading on a copy of the Sweep face, a chart of the session with the alarm thresholds, the time in each band, and a **Download CSV** button. It loads nothing from the internet, can't change anything on the gauge, and keeps its copy of the session on the phone through reloads and gauge restarts. The gauge keeps the last 8 hours for it from power-up. See [docs/WEB_PAGE.md](docs/WEB_PAGE.md).
* **The UDP data stream** (#33): four packets a second to the multicast group `239.10.10.10:5005`, send-only, for a laptop or Raspberry Pi on the network. Watch it with `python3 scripts/udp_listen.py`, a live terminal dashboard (add `--csv FILE` to log, or `--plain` for one line per packet). The packet format is in [docs/UDP_STREAM.md](docs/UDP_STREAM.md).
* **Setting it up:**
  1. Copy `main/wifi_secrets.example.h` to `main/wifi_secrets.h` and put in the network name (1–32 characters) and password (8–63, WPA2). Git ignores that file: never commit it.
  2. `idf.py menuconfig` → **Caterham oil temperature gauge** → **Wi-Fi (#32, #33, #34)** → switch on **Web log page**, **UDP data stream**, or both.
  3. Rebuild. A first build after creating the file picks it up by itself.

  Without the file, or with both off, a build never starts Wi-Fi. The button then says `Wifi failed - no settings` or `Wifi not built`.

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
│   │                               # (OemModel.*, WhiteModel.*), Night's quiet
│   │                               # rule and heartbeat (NightModel.*), Nixie's
│   │                               # tubes and numerals (NixieModel.*), the
│   │                               # Heritage dial (HeritageModel.*), Track's
│   │                               # band times and bars (TrackModel.*), the
│   │                               # warm-up ring and clock (WarmupModel.*), the
│   │                               # UDP packet (UdpPacket.*), the web page's
│   │                               # log and JSON (WebLog.*), the Wi-Fi button
│   │                               # and panel (WifiChoice.*)
│   │                               # and layout choice/long-press
│   │                               # (LayoutChoice.*)
│   ├── hal/Ads122c04Source.*       # ADS122C04 over I²C (port 0, GPIO17/18)
│   ├── net/WifiNet.*               # the Wi-Fi network and UDP stream task (#33, #34)
│   ├── net/WebServer.*             # the web page's log, status and server (#32)
│   ├── web/index.html              # the web page, built into the firmware (#32)
│   ├── display/GaugeDisplay.*      # Waveshare BSP + LVGL 9 gauge; layout switching
│   │   ├── ClassicLayout.cpp       # Classic layout
│   │   ├── SweepLayout.cpp         # Sweep layout, with its LED
│   │   ├── NeedleLayout.cpp        # Needle layout (analogue dial)
│   │   ├── TraceLayout.cpp         # Trace layout (the last 60 s)
│   │   ├── TapeLayout.cpp          # Tape layout (moving scale and rate of change)
│   │   ├── OemLayout.cpp           # OEM layout (styled on the Caterham factory gauge)
│   │   ├── WhiteLayout.cpp         # White layout (white face, status window)
│   │   ├── NightLayout.cpp         # Night layout (dark while all is well)
│   │   ├── NixieLayout.cpp         # Nixie layout (four glowing tubes)
│   │   ├── HeritageLayout.cpp      # Heritage layout (1960s-style dial)
│   │   ├── TrackLayout.cpp         # Track layout (time in each band)
│   │   ├── WarmupLayout.cpp        # Warm-up layout (ring to 60 °C, warm-up time)
│   │   ├── StatusLines.cpp         # shared widgets: status lines, the 6 o'clock LED
│   │   │                           # and Classic's digits (Classic, Night, Warm-up)
│   │   ├── Layouts.h               # shared layout interface
│   │   └── fonts/                  # generated Classic digit font + its licence
│   ├── Kconfig.projbuild           # explicitly opt-in simulation; unit; Wi-Fi options
│   └── idf_component.yml          # Waveshare BSP / LVGL dependencies
├── hardware/                       # wiring and build (WIRING.md), schematic (wiring.svg), BOM
├── docs/                           # architecture, Fedora setup, calibration,
│                                   # installation, validation, safety, porting,
│                                   # sender R/T graph (sender-curve.svg) and its data
├── tests/                          # native C++ and Python tests
└── scripts/                        # calibration-header and sender-graph generators, test runner,
                                    # UDP stream monitor and decoder (udp_listen.py)
```

## Design decisions

| Area | Choice |
| --- | --- |
| Board | Waveshare ESP32-S3-Touch-AMOLED-1.75, **standard or -B** |
| Display | 466×466 QSPI **CO5300** AMOLED, Waveshare BSP + LVGL 9; twelve layouts (Classic, Sweep, Needle, Trace, Tape, OEM, White, Night, Nixie, Heritage, Track and Warm-up), cycled by a 1 s long-press; °C or °F set in menuconfig |
| ADC | **ADS122C04** 24-bit I²C ADC (Adafruit module 6432) at 0x40, internal 2.048 V reference, gain 1 with the PGA bypassed; SDA GPIO17, SCL GPIO18; DRDY not used (GPIO16 left open) |
| Measurement | The ADC's 250 µA current source drives Rref and the sender in series; Vref = A0 − A1, Vs = A1 − A2; `Rs = Rref × Vs / Vref` (ratiometric: cancels the current's accuracy, the reference, the rail and ground offset) |
| Sender interface | Signal wire to the spade, a **dedicated ground-sense wire** and a return wire to the tank bond stud |
| Integrity | Every register write read back with a CRC; every result checked by CRC16 and conversion counter; ADC offset self-test; excitation-current window; burn-out checks of both taps |
| Input protection | At least 2 kΩ between every harness wire and the ADC; no clamps or TVS. Not load-dump qualified; the short-to-battery bench test is owed |
| Interface hardware | Three resistors on the module's pins (option A), plus two capacitors across the pin pairs (option B, for the car). Build it without a board, on stripboard or on a two-layer PCB (hardware/option-a, hardware/option-b) |
| Power | USB-C into the display, on the bench and in the car (an ignition-switched charger); the module takes the display's 3.3 V. Never 12 V to the display |
| Isolation | No connection to MBE ECU / existing instruments |
| Radio | Wi-Fi, off by default: the web log page (#32) and the UDP stream (#33), each built in from menuconfig and switched by the BOOT button (#34), in a task below the measurement loop. The ADS122C04 does the measuring, so no internal ADC is lost to Wi-Fi |
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
