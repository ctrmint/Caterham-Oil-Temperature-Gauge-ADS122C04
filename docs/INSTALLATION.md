# Dry-sump installation and first drive

1. Confirm dry-sump sender location and thread from the *actual vehicle*, not the 420R schematic alone. No new fitting should be removed or installed without accounting for oil quantity, thread sealing and leak risk.
2. Confirm the sender body and tank are electrically grounded through a measured low-resistance earth bond. Rubber isolators and hoses can prevent a reliable ground path. Fit a tank bond stud with a bond strap to chassis.
3. Run the **signal wire** to the sender spade, and the **dedicated ground-sense wire** and the **return wire** to the tank bond stud, twisted together or as a 3-core screened cable. The ground-sense wire must land on the tank side of the bond (not on a chassis point), otherwise it cannot cancel the offset. Use a correctly insulated connector on the spade and retain it against vibration. Route the cable away from ignition wiring, exhaust heat and moving components.
4. Build the front end as **option B** (with the two filter capacitors) for the car: without a board ([hardware/WIRING.md](../hardware/WIRING.md)), on stripboard or on the PCB ([hardware/option-b](../hardware/option-b/README.md)). Pass every bench step there first, including the tap checks, the pin-pair shorts and the short-to-battery test. Never fit the bench breadboard.
   - Mount the module and terminal strip in a small insulated box with strain-relieved cable entries, close to the display.
   - The display lead carries the I²C bus: keep it 20 cm or less and away from ignition wiring.
   - Only the analogue wires run to the tank; the I²C bus never does.
5. Power the display from its USB-C socket, using a USB-C charger on an ignition-switched, fused 12 V circuit, and secure the plug.
   - The module takes the display's 3.3 V.
   - The harness's return wire is the gauge's ground reference at the tank.
   - Never feed 12 V to the display, and never connect a laptop's USB while the car charger is connected.
6. **With the vehicle OFF:** measure the chassis-to-gauge ground difference and continuity, and make sure nothing is shorted to battery power.
   - **With the ignition ON:** check that `SENDER` and `SENSE` stay between the gauge's GND and 3.3 V.
   - **With the engine running and electrical loads on:** watch `ground_offset_v` in the serial CSV log, or with Wi-Fi on, under **Sensor readings** on the web page (as `Ground offset`, in mV) or on the UDP monitor (`ground`). It should stay close to 0 V (aim for within ±100 mV); if it doesn't, improve the tank bond.
   - **Noise check (WIRING.md step 14):** compare the reading's jitter with the engine off and running. It shouldn't be worse with the engine running.
   - **Expect a slightly negative offset:** the display's supply current returns through its own ground. The ADC is specified for inputs down to 0.1 V below its GND, so a negative offset beyond what bench step 8 in WIRING.md covered calls for a better ground.
7. Check the resistance reading cold, during warm-up, and after engine shutdown. Calibrated °C should agree with an independent reference within a documented tolerance; consider time lag in the tank.
8. Test open- and short-circuit faults, and a disconnected ground-sense wire, **with ignition off and sensor disconnected from tank**, using known-resistor fixtures. The system must visibly report faults after reconnection.
9. Secure the display away from airbags/controls/visibility and avoid driver distraction. Never rely on a prototype as the sole low-pressure or temperature protection for the engine.
10. After a short shakedown, inspect for oil leaks, cable chafe, vibrations, high board temperature and overvoltage resets. Capture logs only in a safe parked condition.

**Measurement semantics:** This is oil temperature in the dry-sump reservoir at its sender location, which is a slowly varying approximation of oil bulk temperature. It is *not* direct oil-film temperature, oil pressure or bearing temperature.
