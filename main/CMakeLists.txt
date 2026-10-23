idf_component_register(
    SRCS
        "main.cpp"
        "core/Calibration.cpp"
        "core/Temperature.cpp"
        "hal/AnalogSource.cpp"
        "display/GaugeDisplay.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_adc esp_timer freertos lvgl__lvgl waveshare__esp32_s3_touch_amoled_1_75
)
