# Wiring (Waveshare ESP32-S3-Touch-AMOLED-1.75 standard / -B)

**All pin numbers are for Waveshare's connector `H2`**. Consult the revision-specific schematic and identify pin 1 orientation on the PCB before attaching anything.

| H2 pin | Signal | Use |
|---:|---|---|
| 1 | VBUS | Optional regulated **5 V** board power, only after reviewing the board power-path schematic; otherwise use a properly engineered USB 5 V input. Never 12 V. |
| 2 | GND | Electronics/sensor return to a single deliberate common-ground point |
| 3 | 3V3 | Excitation through pull-up and reference divider only; do not connect to vehicle 12 V |
| 4 | GPIO44 / UART RX | Not used |
| 5 | GPIO43 / UART TX | Not used |
| 6 | GPIO17 / **ADC2_CH6** | Half-rail reference input |
| 7 | GPIO18 | Spare; on -G may connect to GNSS |
| 8 | GPIO16 / **ADC2_CH5** | Sender voltage input |

## Circuit schematic

```text
                      WAVESHARE BOARD 3.3 V  (H2-3)
                                    |
               +--------------------+---------------------+
               |                    |                     |
        [470 Ω, precision]       [10 kΩ]                  |
               |                    |                     |
               o SENDER_NODE        o REF_NODE            |
               |                    |                     |
   sender spade o                [10 kΩ]                   |
               |                    |                     |
         [Sender resistor]          +--------- GND (H2-2)
               |
  grounded tank body ------ vehicle/electronics common ground
               |
           GND (H2-2)

  SENDER_NODE --- [10 kΩ series] --- o --- H2-8 / GPIO16 ADC2_CH5
                                     |
                                   100 nF
                                     |
                                    GND

  REF_NODE ------ [10 kΩ series] --- o --- H2-6 / GPIO17 ADC2_CH6
                                     |
                                   100 nF
                                     |
                                    GND

  [Vehicle-qualified ESD/overvoltage protection at harness entry/ADC input,
   part and clamp topology to be engineered; NOT omitted for in-car use.]
```

All capacitor ground connections return to the **electronics** common ground. Preserve the sender's expected **metal-body earth return**. Confirm the sump tank is grounded even if it is mounted on vibration isolators; ideally fit a dedicated bonding conductor.

## Engineering calculations

With pull-up `Rp` (470 Ω) and resistive sender `Rs` to ground:

`Vsender = V3v3 × Rs / (Rp + Rs)`

GPIO17 measures a precision midpoint: `Vmid = V3v3 × Rlower / (Rupper + Rlower)`.

Firmware calculates `V3v3 = Vmid × (Rupper+Rlower)/Rlower` and then

`Rs = Rp × Vsender / (V3v3 − Vsender)`.

This ratiometric two-channel approach reduces rail variation effects but **does not remove ADC absolute error, divider tolerance, or ground offsets**. Update `main/Config.h` with measured resistor values.

## Expected indicative raw voltages

The **voltages below are theoretical electrical examples**, not a calibrated oil-temperature table. For nominal 3.30 V and 470 Ω pull-up:

| Sender resistance | Expected GPIO16 DC voltage |
|---:|---:|
| 44 Ω | 0.282 V |
| 120 Ω | 0.671 V |
| 470 Ω | 1.650 V |
| 1,158 Ω | 2.347 V |
| Disconnected | Near 3.3 V **(ADC saturation expected)** |
| Signal shorted to ground | Near 0 V |

GPIO17 should read around **1.650 V**. Do not rely on ADC values near the supply voltage. GPIO16/17 are **3.3 V logic inputs, never 5 V tolerant**.

## Bench steps

1. With PCB completely disconnected from the ESP32, inspect continuity for unintended shorts to battery+ / chassis.
2. Power from a clean bench 5 V input. Check 3.3 V rail is stable.
3. Fit a **known** resistance of 120 Ω between SENDER_NODE and ground (do not use an unknown car harness at this point). Verify with a DMM: GPIO16 ≈0.67 V, GPIO17 ≈1.65 V.
4. Check firmware resistance reads close to 120 Ω (tolerance is not a declaration of calibration).
5. Remove resistor: expected `SENSOR OPEN`. Short sender node to ground through safe test leads: expected `SENSOR SHORT`.
6. Vary known resistors across the expected range; record readings and errors.
7. With ignition OFF, connect real sensor and ground; confirm no voltages above the protected ADC input limits before turning on gauge power.

**Never** probe or splice a powered automobile circuit assuming it is grounded at the same potential as a USB-connected development laptop. Keep the car and laptop disconnected from each other while validating grounds.
