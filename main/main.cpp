#include <cmath>
#include <cstdint>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "Config.h"
#include "core/Calibration.h"
#include "core/Temperature.h"
#include "hal/Ads1115Source.h"
#include "display/GaugeDisplay.h"

namespace {
constexpr const char* TAG = "oil_gauge";
uint32_t ticks_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL); }

#ifdef CONFIG_OIL_GAUGE_UNIT_FAHRENHEIT
constexpr gauge::Unit kUnit = gauge::Unit::Fahrenheit;
#else
constexpr gauge::Unit kUnit = gauge::Unit::Celsius;
#endif

// Layout persistence. Returns false (after one warning) if NVS is unusable; the layout is then Classic and NVS is not used again.
bool nvsLoadLayout(nvs_handle_t& h, gauge::Layout& layout) {
  layout = gauge::Layout::Classic;
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    err = nvs_flash_erase();
    if (err == ESP_OK) err = nvs_flash_init();
  }
  if (err == ESP_OK) err = nvs_open("oil_gauge", NVS_READWRITE, &h);
  if (err != ESP_OK) {
    ESP_LOGW(TAG,"NVS unavailable (%s): layout will not be saved", esp_err_to_name(err));
    return false;
  }
  uint8_t v = 0;
  err = nvs_get_u8(h, "layout", &v);
  layout = gauge::decodeLayout(err == ESP_OK, v);
  return true;
}

bool nvsStoreLayout(nvs_handle_t h, gauge::Layout layout) {
  esp_err_t err = nvs_set_u8(h, "layout", gauge::encodeLayout(layout));
  if (err == ESP_OK) err = nvs_commit(h);
  if (err != ESP_OK) ESP_LOGW(TAG,"Saving layout failed (%s)", esp_err_to_name(err));
  return err == ESP_OK;
}
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG,"Caterham dry-sump oil gauge | Waveshare ESP32-S3 AMOLED 1.75 + ADS1115");
  ESP_LOGW(TAG,"Experimental gauge; not a replacement for factory vehicle warnings");
  nvs_handle_t nvs = 0;
  gauge::Layout layout = gauge::Layout::Classic;
  const bool nvs_ok = nvsLoadLayout(nvs, layout);
  uint8_t stored_layout = gauge::encodeLayout(layout);
  GaugeDisplay display;  // stays in internal RAM (main-task stack): its std::atomic hand-off must not live in PSRAM
  const bool display_ok = display.begin(kUnit, layout);
  if (!display_ok) ESP_LOGE(TAG,"Display initialization failed");
  gauge::TemperatureState state;
  gauge::Reading reading;
#ifndef CONFIG_OIL_GAUGE_DEMO
  Ads1115Source source;
  if (!source.begin()) ESP_LOGE(TAG,"I2C bus startup failed; no live measurement");
#else
  ESP_LOGE(TAG,"DEMO MODE: SYNTHETIC DATA ONLY; DO NOT INSTALL IN VEHICLE");
#endif
  if (!gauge::calibrationValid()) {
    ESP_LOGW(TAG,"No verified sender curve: degrees C disabled");
  } else if (gauge::calibrationMaxC() < config::kRedThresholdC) {
    ESP_LOGW(TAG,"Calibration ends at %.1f C, below the %.1f C red threshold: "
             "readings above the table are shown as CRITICAL",
             gauge::calibrationMaxC(),config::kRedThresholdC);
  }
  ESP_LOGI(TAG,"CSV: uptime_ms,status,alarm,temp_c,resistance_ohms,pullup_v,sender_v,supply_v,ground_offset_v");
  uint32_t last_read=0,last_display=0,last_log=0;
  while (true) {
    const uint32_t now=ticks_ms();
    if (uint32_t(now-last_read)>=config::kSampleIntervalMs) {
      last_read=now;
#ifdef CONFIG_OIL_GAUGE_DEMO
      // 25-135 C: stays inside the demo table and passes both alarm thresholds.
      const float demo_temp = 80.0f + 55.0f * sinf(static_cast<float>(now)/25000.f);
      const float rs = gauge::resistanceFromTemperature(demo_temp);
      const float amps = 3.30f/(config::kPullUpOhms+rs);
      const gauge::Sample s = {true,amps*config::kPullUpOhms,amps*rs,3.30f};
      reading=state.update(s,now);
#else
      reading=state.update(source.read(),now);
#endif
    }
    if (uint32_t(now-last_display)>=config::kDisplayIntervalMs) {
      last_display=now;
      if (display_ok) display.render(reading,state.maximumC(),state.maximumAboveRange(),
#ifdef CONFIG_OIL_GAUGE_DEMO
        true
#else
        false
#endif
      );
    }
    gauge::Layout next;
    if (display_ok && display.takeLayoutChange(next) && nvs_ok && gauge::encodeLayout(next) != stored_layout) {
      if (nvsStoreLayout(nvs, next)) stored_layout = gauge::encodeLayout(next);
    }
    if (uint32_t(now-last_log)>=config::kLogIntervalMs) {
      last_log=now;
      char temp[16] = "NA";
      if (reading.has_temperature) snprintf(temp,sizeof(temp),"%.1f",reading.temperature_c);
      ESP_LOGI(TAG,"CSV,%lu,%s,%s,%s,%.1f,%.4f,%.4f,%.4f,%.4f",(unsigned long)now,
               gauge::statusText(reading.status),gauge::statusText(reading.alarm),temp,
               reading.resistance_ohms,reading.pullup_volts,reading.sensor_volts,
               reading.supply_volts,reading.ground_offset_volts);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
