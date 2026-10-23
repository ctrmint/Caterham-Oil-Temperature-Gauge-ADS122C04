# Dry-sump installation and first drive

1. Confirm dry-sump sender location and thread from the *actual vehicle*, not the 420R schematic alone. No new fitting should be removed or installed without accounting for oil quantity, thread sealing and leak risk.
2. Confirm the sender body and tank are electrically grounded through a measured low-resistance earth bond. Rubber isolators and hoses can prevent a reliable ground path. Use a separately routed return lead as appropriate.
3. Use a correctly insulated connector to the sender spade and retain it against vibration. Shield/route cable away from ignition wiring, exhaust heat and moving components.
4. Place the ADC input-conditioning/protection circuit **inside a sealed, strain-relieved gauge assembly**, not on a breadboard.
5. Supply a properly fused, ignition-controlled 5 V power rail with automotive transient suppression, undervoltage handling and reverse-polarity protection. Power down with ignition as designed; verify the USB/5 V VBUS path for safe operation.
6. With vehicle OFF, measure chassis/board ground difference and continuity; ensure no short to battery power. With ignition ON, verify input voltage range at both ADC pins.
7. Check the resistance reading cold, during warm-up, and after engine shutdown. Calibrated °C should agree with an independent reference within a documented tolerance; consider time lag in the tank.
8. Test open- and short-circuit faults **with ignition off and sensor disconnected from tank**, using known-resistor fixtures. The system must visibly report faults after reconnection.
9. Secure the display away from airbags/controls/visibility and avoid driver distraction. Never rely on a prototype as the sole low-pressure or temperature protection for the engine.
10. After a short shakedown, inspect for oil leaks, cable chafe, vibrations, high board temperature and overvoltage resets. Capture logs only in a safe parked condition.

**Measurement semantics:** This is oil temperature in the dry-sump reservoir at its sender location, which is a slowly varying approximation of oil bulk temperature. It is *not* direct oil-film temperature, oil pressure or bearing temperature.
