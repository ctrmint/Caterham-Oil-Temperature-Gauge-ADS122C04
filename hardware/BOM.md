# Hardware BOM

Required electronics for one prototype; prices and quantities are deliberately not guessed.

| Qty | Item | Notes |
|---|---|---|
| 1 | [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) | **Standard or -B**; not 1.75C, 2.1 LCD, or GNSS-G. Verify board revision and header before wiring. |
| 1 | Existing fitted Caterham resistive temperature sender | Confirm part/thread/ohmic behaviour; dry-sump tank measurement, not engine gallery |
| 1 | 470 Ω 1% metal-film resistor, 0.25 W | Sensor pull-up; measure its actual resistance |
| 2 | 10 kΩ 0.1–1% resistors, 0.25 W | Supply-reference divider GPIO17; matched pair preferred |
| 2 | 10 kΩ 1% resistors, 0.25 W | One series resistor for each ADC input for fault-current limiting and filtering |
| 2 | 100 nF ceramic capacitors (X7R) | Each ADC input to signal ground |
| 1 set | Automotive-qualified low-leakage ESD/overvoltage input protection | Choose for input transient profile and 3.3 V ADC absolute max; **not fully specified by this prototype**. Verify leakage at hot temperatures and available 3.3 V rail fault-clamp energy. |
| 1 | Automotive-rated input protection + 12 V to stable **5 V** converter | Ignition supply; fused feed, load-dump/cranking and reverse-polarity rated; do not feed 12 V to display |
| 1 | Proper cable/fuse/loom parts | Fused near pickup, strain relief, protective conduit, connector seals |
| 1 | Sender spade connector and dedicated tank ground return | Validate terminal fits sender; avoid loose or corroded connections |
| 1 | Insulated moisture-resistant enclosure and cockpit mount | Grounded where appropriate and mechanically secure; exposed electronics cannot be left loose |
| - | Multimeter; thermocouple/thermometer; known resistors | Bench validation and calibration |

**Avoid mounting a breadboard in a vehicle.** The finished interface should use soldered components or a professionally assembled PCB, strain relief and appropriately selected protection. All connected ground potentials must be checked before applying power. Do not power the product from USB and the vehicle at the same time without verifying the board's USB/VBUS power path.

### Possible future PCB

The two resistor dividers, protection, and noise filters can be combined into one small PCB fitted behind the gauge. A custom automotive-tested analogue front end would be preferable for long-term operation. The exact protection diode and supply converter should be selected after measuring possible fault/transient environments and reviewing board schematics; do not assume an unqualified generic 3.3 V TVS makes the circuit load-dump proof.
