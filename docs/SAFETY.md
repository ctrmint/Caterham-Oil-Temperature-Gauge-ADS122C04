# Safety and limits

This is an **experimental supplemental display**. It is not automotive-certified or validated for driving, motorsport or engine protection. It makes no ECU intervention and must never supersede the factory warning lamps or oil-pressure instrumentation.

## Key hazards / mitigations

- **12 V/automotive transient fault into the 3.3 V ADC:** never apply car 12 V to the ADS122C04's pins or any display socket pin. The ADC's inputs must stay within GND − 0.3 V to VDD + 0.3 V, with at most 10 mA into any pin.
  - **What the design does:** every harness wire reaches the ADC only through at least 2 kΩ: Rref and the 2.2 kΩ R1 on the signal wire, and the 2.2 kΩ R2 on the sense wire.
  - **What that covers:** a wire shorted to 14.4 V pushes about 5 mA into each pin, through the chip's own clamps into the display's 3.3 V rail; a 24 V jump start pushes about 10 mA.
  - **What it doesn't:** there are no clamp diodes or TVS, so this is **not** ESD, surge or load-dump qualification.
  - **Owed:** pass the short-to-battery bench test (hardware/WIRING.md, step 12) before fitting, and check the installed system in the car. The bench breadboard is never fitted in the car.
- **Vehicle supply path:** the display runs from USB-C. In the car, use a quality USB-C charger on an ignition-switched, fused socket, rated for cranking and transients, and secure the plug. Never supply the display from an unspecified 12 V lead.
- **Oil spills/hot oil:** install/maintain sensor connection only with the engine cold and oil system appropriately safe. Use correct seals and torque data obtained for the actual vehicle/sender port.
- **Ground/reference faults:** the metal sender body is its circuit ground.
  - **Offsets:** the ground-sense wire cancels moderate offsets, but only while the tank stays within the ADC's input range (from 0.1 V below the gauge GND). Verify tank bonding, avoid loops, and check how laptop USB and vehicle power interact.
  - **Broken sense wire:** each cycle the ADC's 10 µA burn-out source pulls the sense input up. A broken ground-sense wire, or an open R2, is then reported as `GROUND FAULT`, never as a temperature. An open R1 is reported as `SENSOR OPEN` the same way.
  - **Return wire:** with the display on USB-C, the harness's return wire is the gauge's ground at the tank. If it breaks while the supply floats, no excitation current flows and the gauge shows a fault.
- **Electrical interference:** option A has no input filter. Ignition or alternator interference near the ADC's 256 kHz sampling rate could make readings noisy, or shift them, without any fault.
  - Use option B in the car, and do the in-car noise check (WIRING.md step 14).
  - Option B's capacitors go only across pin pairs, so a shorted one is a fault, never a reading. A capacitor from a pin to ground could short into a plausible wrong reading, so the design has none.
- **I²C bus:** keep it short (a display lead of 20 cm or less), and never run it to the tank. The one-hour soak test is still owed.
  - A long or noisy I²C bus can hang or return corrupt data.
  - Before every conversion the firmware writes the ADC's configuration and reads it back with a CRC, and every result carries a CRC16 and a conversion counter.
  - Any I²C error, timeout, CRC failure, register mismatch or out-of-sequence counter is `ADC UNAVAILABLE`, never a stale value. The firmware then resets the bus and retries.
- **Open-circuit reading:** an open sender stops the excitation current, so the voltage across Rref falls to about 0 V and is reported as `SENSOR OPEN`. **No temperature** is shown.
- **Wrong reference resistor:** the reading is only as good as Rref and its configured value. The firmware checks the measured excitation current (Vref / Rref) against the current source's setting, so a missing or grossly wrong Rref is a `SUPPLY FAULT`, not a plausible wrong reading. Small errors aren't caught, so measure Rref and enter its value.
- **Calibration missing/wrong:** no invented table in live firmware. Only measured sender data can enable °C or °F, and no extrapolation is performed. Readings hotter than the table are shown as critical rather than as a fault.
- **Warning limits:** defaults are *illustrations* and not a definition of safe Caterham oil operation. High-temperature alerts provide no guarantee against engine damage.
- **Colour bands:** the reading's colour is graduated by temperature (red from 120 °C) and is never less severe than a latched alarm. The colour bands are a visual guide, separate from the alarm thresholds, which are unchanged (amber 115 °C, red 130 °C). The status text (`HIGH TEMPERATURE`, `CRITICAL TEMP`) is the alarm. A fault or an uncalibrated reading is always amber, never a temperature colour.
- **Touch:** touch does one thing only: a press of at least 1 s (a long-press) switches the display layout. It cannot change the unit, the calibration or the alarms. Choose the layout before driving and do not change it while driving.
- **Display/software faults:** board resets or LVGL bugs can result in a blank screen. The primary vehicle monitoring remains mandatory.
- **Network outputs (the web page #32 and the UDP stream #33):** they're as supplementary as the display. Wi-Fi is off unless it's built in and switched on with the BOOT button.
  - **Never the alarm:** the alarm is the gauge's own status text and LED. A phone or laptop can lose the network, stall, sleep or show an old page. Don't watch a phone while driving.
  - **Stale data is marked, never hidden:**
    - The page turns its update age amber after 3 s with no answer. After 10 s it shows `NO DATA` and a grey `--`.
    - The UDP monitor shows `NO DATA` after 2 s, then `STREAM LOST` and `--` after 10 s.
    - Wi-Fi drops the odd packet, and nothing is ever filled in.
  - **Fail closed, as the gauge:** neither output shows a temperature the gauge didn't have. With no temperature (a fault, beyond the table, or uncalibrated) the JSON has `null`, the packet's has-temperature flag is clear, and both CSV files leave `temp_c` empty.
  - **Read-only:** nothing on the network can change the gauge. The server answers three `GET` requests only, and the stream is send-only. Neither can touch the measurement, the calibration, the alarms or the display.
  - **Not encrypted:** anyone on the same network can read the page and the stream. When the gauge hosts the network, that's only people with its WPA2 password.
  - **Demo data:** a demo build says so on the page (`DEMO ONLY - NOT REAL`), in the page's CSV name (`DEMO-`), and on the monitor (`DEMO`). The monitor's CSV has no demo column, so name a file recorded from a demo build yourself.
  - **Radio beside the front end:** the ratiometric, filtered measurement shouldn't notice the 2.4 GHz radio. The bench check comparing the ohms' noise with Wi-Fi on and off is still owed (`docs/VALIDATION.md`).

## Acceptance gate

Before car use, verify (1) correct sender type, (2) protected circuit, (3) proper grounding, (4) clean power from vehicle supply, (5) full firmware hardware build and flash, (6) tested calibration over the intended temperature range, (7) fault-state tests, and (8) enclosure/installation vibration and thermal robustness.
