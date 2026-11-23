idf_component_register(
    SRCS
        "main.cpp"
        "core/Calibration.cpp"
        "core/DisplayText.cpp"
        "core/HeritageModel.cpp"
        "core/LayoutChoice.cpp"
        "core/NightModel.cpp"
        "core/NixieModel.cpp"
        "core/OemModel.cpp"
        "core/SweepModel.cpp"
        "core/TapeModel.cpp"
        "core/Temperature.cpp"
        "core/TraceModel.cpp"
        "core/TrackModel.cpp"
        "core/UdpPacket.cpp"
        "core/WarmupModel.cpp"
        "core/WebLog.cpp"
        "core/WifiChoice.cpp"
        "core/WhiteModel.cpp"
        "hal/Ads122c04Source.cpp"
        "net/WebServer.cpp"
        "net/WifiNet.cpp"
        "display/ClassicLayout.cpp"
        "display/fonts/gauge_digits_148.c"
        "display/GaugeDisplay.cpp"
        "display/HeritageLayout.cpp"
        "display/NeedleLayout.cpp"
        "display/NightLayout.cpp"
        "display/NixieLayout.cpp"
        "display/OemLayout.cpp"
        "display/StatusLines.cpp"
        "display/SweepLayout.cpp"
        "display/TapeLayout.cpp"
        "display/TraceLayout.cpp"
        "display/TrackLayout.cpp"
        "display/WarmupLayout.cpp"
        "display/WhiteLayout.cpp"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "web/index.html"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_event esp_http_server esp_netif esp_timer esp_wifi freertos lwip nvs_flash
             lvgl__lvgl
             waveshare__esp32_s3_touch_amoled_1_75
)

# Wi-Fi settings (#33, #34): main/wifi_secrets.h, which git ignores. Re-configure when it appears or goes (a change to this
# directory), so a first build after creating it picks it up; edits to it are tracked as an ordinary include.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/wifi_secrets.h")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE OIL_GAUGE_WIFI_SECRETS=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE OIL_GAUGE_WIFI_SECRETS=0)
endif()
