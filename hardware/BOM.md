# Hardware BOM

There's no interface board: the front end is three resistors soldered to the ADC module, plus a terminal strip for the harness. See [WIRING.md](WIRING.md) for the circuit and the build. Prices and quantities are deliberately not guessed.

| Qty | Item | Notes |
|---|---|---|
| 1 | [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) | **Standard or -B**; not 1.75C, 2.1 LCD, or GNSS-G. Verify the board revision and header before wiring. |
| 1 | **[Adafruit ADS122C04 24-bit ADC module](https://www.adafruit.com/product/6432)** (product 6432; The Pi Hut SKU ADA6432) | Runs from the display's 3.3 V. It has I²C pull-ups and no regulator, and the address is 0x40 with its jumpers open. Solder every connection; don't rely on its STEMMA QT / Qwiic connectors in the car. It comes with a 12-pin header strip: 6 pins go on the module, and 8 make the display plug. |
| 1 | **Rref: 2.00 kΩ, 0.1 %, ≤ 25 ppm/°C** metal-film resistor, 0.25 W | The measurement reference. Its accuracy and drift set the accuracy of the resistance reading. Measure its actual value and put it in `kReferenceOhms`. |
| 2 | 2.2 kΩ, 1 % metal-film resistors, 0.25 W | Series resistors on A1 (sender tap) and A2 (ground sense). They limit fault current, and the firmware's ground reading assumes 2.2 kΩ. |
| 1 | 3-way screw terminal strip ("chocolate block") | The harness connector: `SENDER`, `SENSE`, `RETURN` |
| 1 | 5-core flexible cable, 20 cm or less | Display lead: 3.3 V, GND, SDA, SCL to the module, and GND to `RETURN` |
| 1 | Small insulated box, with cable strain reliefs or glands | Holds the module and the terminal strip. Mount the module by its four holes, and glue the resistor bodies down |
| 1 | Existing fitted Caterham resistive temperature sender | Confirm part/thread/ohmic behaviour; dry-sump tank measurement, not engine gallery |
| 1 | 3-core twisted or screened cable | Signal wire to the sender spade, **ground-sense** wire and **return** wire to the tank bond stud |
| 1 | Ring terminal for the tank bond stud | The ground-sense and return wires land on the same stud |
| - | Bootlace ferrules, heat-shrink, sleeving | For the harness wires in the terminal strip and the resistor leads |
| 1 | Ignition-switched USB-C charger, and a secured USB-C lead | Powers the display. Use a quality charger rated for the car's transients |
| 1 | Proper cable/loom parts | Strain relief, protective conduit, connector seals |
| 1 | Sender spade connector | Validate that the terminal fits the sender; avoid loose or corroded connections |
| - | Multimeter; thermocouple/thermometer; known resistors (44 Ω–5 kΩ, plus 10 Ω, and 1 Ω, 27 Ω and 12 Ω with a 1.5 V AA cell for the ground-offset test); a current-limited bench supply for the short-to-battery test | Bench validation and calibration |

**What's no longer needed** compared with the earlier ADC's circuit:
- the 470 Ω pull-up;
- the 1 kΩ / 100 nF input filters;
- the 100 kΩ ground-sense bias (the ADC's burn-out source does that job);
- the clamp diodes and TVS;
- every interface PCB and stripboard.

**Protection:** the series resistors keep a harness wire shorted to battery voltage within the ADC's input-current rating, but they are **not** load-dump or surge qualification. Bench test the short to battery (WIRING.md step 12), then check the installed system in the car.

**Avoid mounting a breadboard in a vehicle.** The car build uses soldered joints, strain relief and an insulated box. Check every connected ground potential before applying power, and never power the display from USB and another supply at the same time.
