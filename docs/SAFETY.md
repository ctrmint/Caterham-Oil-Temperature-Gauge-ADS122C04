# Safety and limits

This is an **experimental supplemental display**. It is not automotive-certified or validated for driving, motorsport or engine protection. It makes no ECU intervention and must never supersede the factory warning lamps or oil-pressure instrumentation.

## Key hazards / mitigations

- **12 V/automotive transient fault into the 3.3 V ADC:** never apply car 12 V to the ADS1115 inputs or any H2 pin. ADS1115 inputs must stay within GND − 0.3 V to VDD + 0.3 V. The 1 kΩ series resistors and RC filtering provide only preliminary current limiting/noise control. Qualified ESD, surge, load-dump and overvoltage protection must be designed/verified for the signal and ground-sense wires before mounting in a vehicle.
- **Vehicle supply path:** use an ignition-switched fused supply, automotive converter rated for cranking/transients, with reverse polarity/ESD protection; never supply board from an unspecified 12 V lead.
- **Oil spills/hot oil:** install/maintain sensor connection only with the engine cold and oil system appropriately safe. Use correct seals and torque data obtained for the actual vehicle/sender port.
- **Ground/reference faults:** metal sender body is its circuit ground. The ground-sense wire cancels moderate offsets, but only while the tank stays within the ADS1115's input range relative to the gauge GND. Verify tank bonding, avoid loops, check laptop USB/vehicle power interaction. A 100 kΩ bias resistor pulls a broken ground-sense wire to 3.3 V so it is reported as `GROUND FAULT`, never as a temperature.
- **I²C bus:** keep the ADS1115 next to the Waveshare board inside the enclosure. A long or noisy I²C bus can hang or return corrupt data; the firmware treats I²C errors, timeouts and unexpected register read-backs as `ADC UNAVAILABLE`, resets the bus and retries.
- **Open-circuit reading:** an open sender stops current through the pull-up, so `Vp` falls to about 0 V and is reported as `SENSOR OPEN`. **No temperature** is shown.
- **Calibration missing/wrong:** no invented table in live firmware. Only measured sender data can enable °C, and no extrapolation is performed. Readings hotter than the table are shown as critical rather than as a fault.
- **Warning limits:** defaults are *illustrations* and not a definition of safe Caterham oil operation. High-temperature alerts provide no guarantee against engine damage.
- **Touch:** touch does one thing only: a press of at least 1 s (a long-press) switches the display layout. It cannot change the unit, the calibration or the alarms. Choose the layout before driving and do not change it while driving.
- **Display/software faults:** board resets or LVGL bugs can result in a blank screen. The primary vehicle monitoring remains mandatory.

## Acceptance gate

Before car use, verify (1) correct sender type, (2) protected circuit, (3) proper grounding, (4) clean power from vehicle supply, (5) full firmware hardware build and flash, (6) tested calibration over the intended temperature range, (7) fault-state tests, and (8) enclosure/installation vibration and thermal robustness.
