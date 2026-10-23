# Safety and limits

This is an **experimental supplemental display**. It is not automotive-certified or validated for driving, motorsport or engine protection. It makes no ECU intervention and must never supersede the factory warning lamps or oil-pressure instrumentation.

## Key hazards / mitigations

- **12 V/automotive transient fault into 3.3 V ADC:** do not apply car 12 V to GPIO16/GPIO17. The 10k series resistance and RC filtering provide only preliminary current limiting/noise control. Qualified ESD, surge, load-dump and overvoltage protection must be designed/verified for the harness before mounting in a vehicle.
- **Vehicle supply path:** use an ignition-switched fused supply, automotive converter rated for cranking/transients, with reverse polarity/ESD protection; never supply board from an unspecified 12 V lead.
- **Oil spills/hot oil:** install/maintain sensor connection only with the engine cold and oil system appropriately safe. Use correct seals and torque data obtained for the actual vehicle/sender port.
- **Ground/reference faults:** metal sender body is its circuit ground. Verify tank bonding, avoid loops, check laptop USB/vehicle power interaction.
- **ADC2 versus radio:** ADC2 may conflict with Wi-Fi; do not initialize Wi-Fi/BLE on this target. For networking use an ADC1-capable board or external ADC.
- **Open-circuit reading:** analogue input near 3.3 V is beyond the S3's reliably calibrated ADC range; firmware has a pre-saturation open guard. In some exotic failure modes it may show `OUT OF CAL RANGE` instead of `SENSOR OPEN`; **no temperature** is shown in either case.
- **Calibration missing/wrong:** no invented table in live firmware. Only measured sender data can enable °C, and no extrapolation is performed.
- **Warning limits:** defaults are *illustrations* and not a definition of safe Caterham oil operation. High-temperature alerts provide no guarantee against engine damage.
- **Display/software faults:** board resets or LVGL bugs can result in a blank screen. The primary vehicle monitoring remains mandatory.

## Acceptance gate

Before car use, verify (1) correct sender type, (2) protected circuit, (3) proper grounding, (4) clean power from vehicle supply, (5) full firmware hardware build and flash, (6) tested calibration over the intended temperature range, (7) fault-state tests, and (8) enclosure/installation vibration and thermal robustness.
