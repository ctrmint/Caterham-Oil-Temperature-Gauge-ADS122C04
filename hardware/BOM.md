# Hardware BOM

Required electronics for one prototype; prices and quantities are deliberately not guessed. See [WIRING.md](WIRING.md) for the circuit.

| Qty | Item | Notes |
|---|---|---|
| 1 | [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) | **Standard or -B**; not 1.75C, 2.1 LCD, or GNSS-G. Verify board revision and header before wiring. |
| 1 | **ADS1115 16-bit 4-channel I²C ADC module** (e.g. Adafruit ADS1115 STEMMA QT / Qwiic) | Powered from the board's 3.3 V. Needs differential inputs and on-board SDA/SCL pull-ups. Solder wires to the header pads for vehicle use; do not rely on the STEMMA QT / Qwiic friction connector. Grove and Gravity ADS1115 boards also work if run at 3.3 V. An ADS1015 (12-bit) is pin-compatible but about 16× coarser. |
| 1 | Existing fitted Caterham resistive temperature sender | Confirm part/thread/ohmic behaviour; dry-sump tank measurement, not engine gallery |
| 1 | 470 Ω **0.1 %, ≤25 ppm/°C** metal-film resistor, 0.25 W | Sensor pull-up. With the ratiometric circuit this resistor is the measurement reference, so its accuracy and drift set the accuracy of the resistance reading. Measure its actual value. |
| 3 | 1 kΩ 1 % resistors, 0.25 W | Series resistor on ADS1115 A0, A1 and A3 for current limiting and filtering |
| 3 | 100 nF ceramic capacitors (C0G or X7R) | From A0, A1 and A3 to GND at the ADS1115 |
| 1 | 100 kΩ 1 % resistor | Bias from 3V3 to the ground-sense wire at the connector, so a broken sense wire is detected |
| 1 set | Automotive-qualified low-leakage ESD/overvoltage input protection | For the signal and ground-sense wires, e.g. dual Schottky clamps to 3V3/GND after each 1 kΩ plus a harness TVS. Choose for the input transient profile and the ADS1115 absolute maximum (GND − 0.3 V to VDD + 0.3 V); **not fully specified by this prototype**. Verify leakage at hot temperatures and that the 3.3 V rail can absorb clamp current. |
| 1 | Automotive-rated input protection + 12 V to stable **5 V** converter | Ignition supply; fused feed, load-dump/cranking and reverse-polarity rated; do not feed 12 V to display |
| 1 | 2-core twisted or screened cable | Signal wire to the sender spade and dedicated **ground-sense** wire to the tank bond stud |
| 1 | Tank bond strap / ground lead and ring terminal | Low-resistance bond from tank to chassis; the ground-sense wire lands on the same stud |
| 1 | Proper cable/fuse/loom parts | Fused near pickup, strain relief, protective conduit, connector seals |
| 1 | Sender spade connector | Validate terminal fits sender; avoid loose or corroded connections |
| 1 | Insulated moisture-resistant enclosure and cockpit mount | Holds the Waveshare board and the ADS1115 together (short I²C leads); exposed electronics cannot be left loose |
| - | Multimeter; thermocouple/thermometer; known resistors (44 Ω–1.2 kΩ, plus 10 Ω for the ground-offset test) | Bench validation and calibration |

No longer needed compared with the original internal-ADC circuit: the 10 kΩ / 10 kΩ supply-reference divider and the 10 kΩ series resistors.

**Avoid mounting a breadboard in a vehicle.** The finished interface should use soldered components or a professionally assembled PCB, strain relief and appropriately selected protection. All connected ground potentials must be checked before applying power. Do not power the product from USB and the vehicle at the same time without verifying the board's USB/VBUS power path.

### Possible future PCB

The ADS1115 (as a bare chip), pull-up, input filters and protection can be combined into one small PCB fitted behind the gauge, with a locking connector for the sender and ground-sense wires. A custom automotive-tested analogue front end would be preferable for long-term operation. The exact protection diodes and supply converter should be selected after measuring possible fault/transient environments and reviewing board schematics; do not assume an unqualified generic 3.3 V TVS makes the circuit load-dump proof.
