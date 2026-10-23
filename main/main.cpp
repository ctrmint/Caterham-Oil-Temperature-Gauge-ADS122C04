#include <cmath>
#include <cstdint>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "Config.h"
#include "core/Calibration.h"
#include "core/Temperature.h"
#include "hal/AnalogSource.h"
#include "display/GaugeDisplay.h"

namespace {
constexpr const char* TAG = "oil_gauge";
uint32_t ticks_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL); }
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG,"Caterham dry-sump oil gauge | Waveshare ESP32-S3 AMOLED 1.75");
  ESP_LOGW(TAG,"Experimental gauge; not a replacement for factory vehicle warnings");
  GaugeDisplay display;
  const bool display_ok = display.begin();
  if (!display_ok) ESP_LOGE(TAG,"Display initialization failed");
  gauge::TemperatureState state;
  gauge::Reading reading;
#ifndef CONFIG_OIL_GAUGE_DEMO
  AnalogSource source;
  if (!source.begin()) ESP_LOGE(TAG,"ADC startup failed; no live measurement");
#else
  ESP_LOGE(TAG,"DEMO MODE: SYNTHETIC DATA ONLY; DO NOT INSTALL IN VEHICLE");
#endif
  if (!gauge::calibrationValid()) ESP_LOGW(TAG,"No verified sender curve: degrees C disabled");
  ESP_LOGI(TAG,"CSV: uptime_ms,status,temp_c,resistance_ohms,sender_v,rail_v");
  uint32_t last_read=0,last_display=0,last_log=0;
  while (true) {
    const uint32_t now=ticks_ms();
    if (uint32_t(now-last_read)>=config::kSampleIntervalMs) {
      last_read=now;
#ifdef CONFIG_OIL_GAUGE_DEMO
      const float demo_temp = 60.0f + 45.0f * sinf(static_cast<float>(now)/25000.f);
      const float rs = gauge::resistanceFromTemperature(demo_temp);
      const gauge::Sample s = {true,3.30f*rs/(config::kPullUpOhms+rs),3.30f};
      reading=state.update(s,now);
#else
      reading=state.update(source.read(),now);
#endif
    }
    if (uint32_t(now-last_display)>=config::kDisplayIntervalMs) {
      last_display=now;
      if (display_ok) display.render(reading,state.maximumC(),
#ifdef CONFIG_OIL_GAUGE_DEMO
        true
#else
        false
#endif
      );
    }
    if (uint32_t(now-last_log)>=config::kLogIntervalMs) {
      last_log=now;
      if(reading.has_temperature)
        ESP_LOGI(TAG,"CSV,%lu,%s,%.1f,%.1f,%.4f,%.4f",(unsigned long)now,
                 gauge::statusText(reading.status),reading.temperature_c,reading.resistance_ohms,
                 reading.sensor_volts,reading.supply_volts);
      else
        ESP_LOGI(TAG,"CSV,%lu,%s,NA,%.1f,%.4f,%.4f",(unsigned long)now,
                 gauge::statusText(reading.status),reading.resistance_ohms,
                 reading.sensor_volts,reading.supply_volts);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
