# Dry-sump installation and first drive

1. Confirm dry-sump sender location and thread from the *actual vehicle*, not the 420R schematic alone. No new fitting should be removed or installed without accounting for oil quantity, thread sealing and leak risk.
2. Confirm the sender body and tank are electrically grounded through a measured low-resistance earth bond. Rubber isolators and hoses can prevent a reliable ground path. Fit a tank bond stud with a bond strap to chassis.
3. Run the **signal wire** to the sender spade and the **dedicated ground-sense wire** to the tank bond stud, twisted together or as a 2-core screened cable. The ground-sense wire must land on the tank side of the bond (not on a chassis point), otherwise it cannot cancel the offset. Use a correctly insulated connector on the spade and retain it against vibration. Route the cable away from ignition wiring, exhaust heat and moving components.
4. Fit one of the interface boards in [hardware/pcb](../hardware/pcb/), never the breadboard or the bench veroboard (hardware/veroboard.svg), which have no input protection. Build it and pass its bench checks first (its README lists them). Mount it in a sealed, strain-relieved enclosure:
   - **surface-mount-v1** plugs into the display case's socket, so it shares the gauge enclosure.
   - **through-hole-v1**, **through-hole-usb-v1** and **veroboard-usb-v1** each sit in their own box, joined to the display by a cable of 30 cm or less. That cable carries the I²C bus: keep it as short as the mounting allows and away from ignition wiring.

   Only the analogue wires run to the tank; the I²C bus never does.
5. Power, from an ignition-switched, fused feed:
   - **surface-mount-v1** needs a separate automotive 12 V → 5 V converter with transient suppression, undervoltage handling and reverse-polarity protection; its 5 V goes into the harness connector (J2 pin 3).
   - **through-hole-v1** takes the car's 12 V on J1 and has its own resettable fuse, TVS, reverse-polarity diode and 5 V regulator. It still needs a fuse at the supply end. It needs about 7 V to hold 5 V, so the display may reset while cranking.
   - **through-hole-usb-v1** and **veroboard-usb-v1** have no supply input: the display runs from its own USB-C socket. Use a USB-C charger on an ignition-switched 12 V circuit, and secure the USB-C plug. Run the harness's return wire (J2 pin 3 on through-hole-usb-v1, pin 2 on veroboard-usb-v1) to the tank bond stud with the signal and ground-sense wires: it is the board's ground reference.

   Power down with ignition as designed. Never feed 12 V to the display, and verify the USB/5 V VBUS path before connecting USB while the car supply is on.
6. With vehicle OFF, measure chassis/board ground difference and continuity; ensure no short to battery power. With ignition ON, verify every ADS1115 input (A0, A1, A3) stays between its GND and 3.3 V. Measure the tank-stud voltage relative to the gauge GND with the engine running and electrical loads on; it should stay close to 0 V (aim for within ±100 mV). If it does not, improve the bond or move the gauge supply ground closer to the tank bond. Expect the tank to sit slightly *below* the gauge GND, because the board's own current returns through its ground lead. The ADS1115 is specified only for inputs at or above GND, so a negative offset larger than the bench negative-offset test (WIRING.md bench step 8) covered calls for a better ground.
7. Check the resistance reading cold, during warm-up, and after engine shutdown. Calibrated °C should agree with an independent reference within a documented tolerance; consider time lag in the tank.
8. Test open- and short-circuit faults, and a disconnected ground-sense wire, **with ignition off and sensor disconnected from tank**, using known-resistor fixtures. The system must visibly report faults after reconnection.
9. Secure the display away from airbags/controls/visibility and avoid driver distraction. Never rely on a prototype as the sole low-pressure or temperature protection for the engine.
10. After a short shakedown, inspect for oil leaks, cable chafe, vibrations, high board temperature and overvoltage resets. Capture logs only in a safe parked condition.

**Measurement semantics:** This is oil temperature in the dry-sump reservoir at its sender location, which is a slowly varying approximation of oil bulk temperature. It is *not* direct oil-film temperature, oil pressure or bearing temperature.
