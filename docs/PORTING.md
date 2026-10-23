# Porting to other ESP32 round-display hardware

The sensor core deliberately has no ESP-IDF/LVGL dependency. Porting consists of replacing only the **hardware acquisition** and **display** adapters.

1. Validate target ESP32 MCU/flash/PSRAM, physical board connector and analogue-capable exposed pins **before purchasing**.
2. Update `main/hal/AnalogSource.cpp` to the target chip's ADC unit/channels and voltage calibration implementation. If Wi-Fi is required use exposed ADC1 pins or an external ADC; this board uses ADC2 only.
3. Validate sensor node, rail reference and ground. The sender/pull-up equation in `main/core/Temperature.cpp` remains unchanged **only when the circuit still implements the same topology**.
4. Replace `main/display/GaugeDisplay.cpp` vendor BSP dependency and LVGL widgets for the new display driver/board. The target may need different initialization, power management, orientation, screen resolution, touch driver and fonts.
5. Update `main/idf_component.yml`, `sdkconfig.defaults` and board build instructions.
6. Rerun `scripts/test_host.sh`, then real-hardware ADC and display tests, then temperature calibration checks.

### Current Waveshare-specific elements

| Element | File |
|---|---|
| GPIO16 & GPIO17; ADC2 channels 5 & 6 | `main/hal/AnalogSource.cpp` |
| LCD CO5300, power/touch init | Waveshare managed BSP, `main/display/GaugeDisplay.cpp` |
| 466×466 ring/typography | `main/display/GaugeDisplay.cpp` |
| Panel, PSRAM, flash settings | `sdkconfig.defaults` |
| Reference dividers, thresholds | `main/Config.h` |

Do not assume the Waveshare `-G` model offers free GPIO17/18: these are connected to GNSS in some configurations. The `1.75C` board is a *different hardware SKU* and must be validated separately.
