# Hardware BOM

The table below is for the bench prototype, on a breadboard or the veroboard; prices and quantities are deliberately not guessed. See [WIRING.md](WIRING.md) for the circuit. For the car, build one of the two PCBs at the end of this page instead: each has its own complete parts list.

| Qty | Item | Notes |
|---|---|---|
| 1 | [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) | **Standard or -B**; not 1.75C, 2.1 LCD, or GNSS-G. Verify board revision and header before wiring. |
| 1 | **ADS1115 16-bit 4-channel I²C ADC module** (e.g. Adafruit ADS1115 STEMMA QT / Qwiic) | Powered from the board's 3.3 V. Needs differential inputs and on-board SDA/SCL pull-ups. Solder wires to the header pads for vehicle use; do not rely on the STEMMA QT / Qwiic friction connector. Grove and Gravity ADS1115 boards also work if run at 3.3 V. An ADS1015 (12-bit) is pin-compatible but about 16× coarser. |
| 1 | Existing fitted Caterham resistive temperature sender | Confirm part/thread/ohmic behaviour; dry-sump tank measurement, not engine gallery |
| 1 | 470 Ω **0.1 %, ≤25 ppm/°C** metal-film resistor, 0.25 W | Sensor pull-up. With the ratiometric circuit this resistor is the measurement reference, so its accuracy and drift set the accuracy of the resistance reading. Measure its actual value. |
| 3 | 1 kΩ 1 % resistors, 0.25 W | Series resistor on ADS1115 A0, A1 and A3 for current limiting and filtering |
| 3 | 100 nF ceramic capacitors (C0G or X7R) | From A0, A1 and A3 to GND at the ADS1115 |
| 1 | 100 kΩ 1 % resistor | Bias from 3V3 to the ground-sense wire at the connector, so a broken sense wire is detected |
| 1 set | Automotive-qualified low-leakage ESD/overvoltage input protection | For the signal and ground-sense wires, e.g. dual Schottky clamps to 3V3/GND after each 1 kΩ plus a harness TVS. Choose for the input transient profile and the ADS1115 absolute maximum (GND − 0.3 V to VDD + 0.3 V); **not fully specified by this prototype**. Verify leakage at hot temperatures and that the 3.3 V rail can absorb clamp current. The signal wire also reaches the 3.3 V rail directly through the 470 Ω pull-up, so its protection must cover that path too (see WIRING.md, Input protection). |
| 1 | Automotive-rated input protection + 12 V to stable **5 V** converter | Ignition supply; fused feed, load-dump/cranking and reverse-polarity rated; do not feed 12 V to display |
| 1 | 2-core twisted or screened cable | Signal wire to the sender spade and dedicated **ground-sense** wire to the tank bond stud |
| 1 | Tank bond strap / ground lead and ring terminal | Low-resistance bond from tank to chassis; the ground-sense wire lands on the same stud |
| 1 | Proper cable/fuse/loom parts | Fused near pickup, strain relief, protective conduit, connector seals |
| 1 | Sender spade connector | Validate terminal fits sender; avoid loose or corroded connections |
| 1 | Insulated moisture-resistant enclosure and cockpit mount | Holds the Waveshare board and the ADS1115 together (short I²C leads); exposed electronics cannot be left loose |
| - | Multimeter; thermocouple/thermometer; known resistors (44 Ω–1.2 kΩ, plus 10 Ω for the ground-offset test, and 1 Ω, 27 Ω and 12 Ω with a 1.5 V AA cell for the negative-offset test) | Bench validation and calibration |

No longer needed compared with the original internal-ADC circuit: the 10 kΩ / 10 kΩ supply-reference divider and the 10 kΩ series resistors.

**Avoid mounting a breadboard in a vehicle.** The finished interface should use soldered components or a professionally assembled PCB, strain relief and appropriately selected protection. All connected ground potentials must be checked before applying power. Do not power the product from USB and the vehicle at the same time without verifying the board's USB/VBUS power path.

### surface-mount-v1 PCB

There are four interface boards for the car: three PCBs and a stripboard. The smallest is [surface-mount-v1](pcb/surface-mount-v1/README.md), which plugs onto the display. [Its parts list](pcb/surface-mount-v1/BOM.md) gives its parts and connectors, with manufacturer and LCSC part numbers; its README covers ordering, assembly and the checks. It combines everything above except the display, the sender, the harness, the converter, the enclosure and the test equipment. It has a bare ADS1115 instead of the module, a 1206 Rp (Vishay TNPW1206470RBEEA), and the input protection: SMBJ15A, BAV99 clamps and 1206 first-stage resistors.

The protection still has to pass its bench fault tests and qualification in the car; do not assume it makes the circuit load-dump proof.

### through-hole-v1 PCB

[through-hole-v1](pcb/through-hole-v1/README.md) is the hand-soldered alternative:
- one copper layer, through-hole parts only;
- its own box and display cable;
- a 12 V input with its own regulator, so no separate converter.

[Its parts list](pcb/through-hole-v1/BOM.md) covers the board, the plugs and terminals, the display cable, the module standoffs and the box screws. It uses the Adafruit ADS1115 module (soldered in, never socketed), a 0.1 % 0207 Rp (Vishay MBB0207VD4700BC100), 1N4148 clamps and a P6KE18A on the signal wire.

### through-hole-usb-v1 PCB

[through-hole-usb-v1](pcb/through-hole-usb-v1/README.md) is through-hole-v1 for a display powered from its own USB-C socket:
- the same front end, module and protection, in through-hole parts on one copper layer;
- no 12 V input and no regulator: the board takes the display's 3.3 V;
- a harness connector with a dedicated ground return to the tank.

[Its parts list](pcb/through-hole-usb-v1/BOM.md) covers the board, the plugs and terminals, the display cable, the module standoffs and the box screws. The USB-C charger for the display isn't part of it.

### veroboard-usb-v1 stripboard

[veroboard-usb-v1](pcb/veroboard-usb-v1/README.md) is through-hole-usb-v1's circuit on stripboard, with the same parts. Instead of Micro-Fit connectors it has Phoenix MPT 0,5 screw terminal blocks. [Its parts list](pcb/veroboard-usb-v1/BOM.md) also covers the stripboard, link wire, cable and ferrules.
