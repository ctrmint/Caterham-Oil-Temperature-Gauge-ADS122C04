# Porting to other ESP32 round-display hardware

The sensor core deliberately has no ESP-IDF/LVGL dependency. Porting consists of replacing only the **hardware acquisition** and **display** adapters.

1. Validate target ESP32 MCU/flash/PSRAM, physical board connector and two free GPIOs for I²C plus a 3.3 V rail **before purchasing**. Because the ADS1115 does the measurement, no analogue-capable pins are required, and Wi-Fi can be used.
2. Update the I²C pins and port number at the top of `main/hal/Ads1115Source.cpp`. Pick a controller the new board's BSP does not already use, and check that address 0x48 does not clash with an on-board device (change ADDR if it does: 0x49 to VDD, 0x4A to SDA, 0x4B to SCL).
3. Keep the same analogue front end (470 Ω pull-up, ground-sense wire, A0/A1/A3 assignment). The equation `Rs = Rp × Vs / Vp` in `main/core/` remains valid **only when the circuit still implements the same topology**. If the new board runs the ADS1115 from a different supply, check that every input stays between its GND and VDD.
4. Replace `main/display/GaugeDisplay.cpp` vendor BSP dependency and LVGL widgets for the new display driver/board. The target may need different initialization, power management, orientation, screen resolution, touch driver and fonts.
5. Update `main/idf_component.yml`, `sdkconfig.defaults` and board build instructions.
6. Rerun `scripts/test_host.sh`, then real-hardware ADC and display tests, then temperature calibration checks.

### Current Waveshare-specific elements

| Element | File |
|---|---|
| I²C port 0, SDA GPIO17, SCL GPIO18 (ALERT/RDY not used; GPIO16 left open) | `main/hal/Ads1115Source.cpp` |
| LCD CO5300, power/touch init | Waveshare managed BSP, `main/display/GaugeDisplay.cpp` |
| 466×466 ring/typography | `main/display/GaugeDisplay.cpp` |
| Panel, PSRAM, flash settings | `sdkconfig.defaults` |
| Pull-up value, thresholds | `main/Config.h` |

Do not assume the Waveshare `-G` model offers free GPIO17/18: these are connected to GNSS in some configurations. The `1.75C` board is a *different hardware SKU* and must be validated separately.
