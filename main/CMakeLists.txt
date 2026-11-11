idf_component_register(
    SRCS
        "main.cpp"
        "core/Calibration.cpp"
        "core/DisplayText.cpp"
        "core/LayoutChoice.cpp"
        "core/SweepModel.cpp"
        "core/Temperature.cpp"
        "hal/Ads1115Source.cpp"
        "display/GaugeDisplay.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c esp_timer freertos lvgl__lvgl waveshare__esp32_s3_touch_amoled_1_75
)
