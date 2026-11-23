#include <cmath>
#include <cstdint>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "Config.h"
#include "core/Calibration.h"
#include "core/SweepModel.h"
#include "core/Temperature.h"
#include "core/TraceModel.h"
#include "core/TrackModel.h"
#include "core/UdpPacket.h"
#include "core/WarmupModel.h"
#include "core/WifiChoice.h"
#include "hal/Ads122c04Source.h"
#include "display/GaugeDisplay.h"
#include "net/WebServer.h"
#include "net/WifiNet.h"
// The Wi-Fi network name and password (#33, #34): main/wifi_secrets.h, which git ignores (see wifi_secrets.example.h).
// CMakeLists.txt defines OIL_GAUGE_WIFI_SECRETS when the file exists. Without it, Wi-Fi never starts.
#if OIL_GAUGE_WIFI_SECRETS
#include "wifi_secrets.h"
#endif
#ifndef OIL_GAUGE_WIFI_SSID
#define OIL_GAUGE_WIFI_SSID ""
#endif
#ifndef OIL_GAUGE_WIFI_PASSWORD
#define OIL_GAUGE_WIFI_PASSWORD ""
#endif

namespace {
constexpr const char* TAG = "oil_gauge";
uint32_t ticks_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL); }

#ifdef CONFIG_OIL_GAUGE_DEMO
constexpr bool kDemo = true;
#else
constexpr bool kDemo = false;
#endif

// Wi-Fi (#32, #33, #34): the web page and the UDP stream are each built in when menuconfig says so.
#ifdef CONFIG_OIL_GAUGE_WEB_PAGE
constexpr bool kWebBuilt = true;
#else
constexpr bool kWebBuilt = false;
#endif
#ifdef CONFIG_OIL_GAUGE_UDP_STREAM
constexpr bool kUdpBuilt = true;
constexpr const char* kUdpGroup = CONFIG_OIL_GAUGE_UDP_GROUP;
constexpr uint16_t kUdpPort = CONFIG_OIL_GAUGE_UDP_PORT;
#else
constexpr bool kUdpBuilt = false;
constexpr const char* kUdpGroup = "";
constexpr uint16_t kUdpPort = 0;
#endif
constexpr gpio_num_t kBootButton = GPIO_NUM_0;  // the top side button (BOOT), low while pressed

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

bool nvsStoreWifi(nvs_handle_t h, gauge::WifiState s) {
  esp_err_t err = nvs_set_u8(h, "wifi", gauge::encodeWifiState(s));
  if (err == ESP_OK) err = nvs_commit(h);
  if (err != ESP_OK) ESP_LOGW(TAG,"Saving the Wi-Fi state failed (%s)", esp_err_to_name(err));
  return err == ESP_OK;
}

bool nvsStoreLayout(nvs_handle_t h, gauge::Layout layout) {
  esp_err_t err = nvs_set_u8(h, "layout", gauge::encodeLayout(layout));
  if (err == ESP_OK) err = nvs_commit(h);
  if (err != ESP_OK) ESP_LOGW(TAG,"Saving layout failed (%s)", esp_err_to_name(err));
  return err == ESP_OK;
}
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG,"Caterham dry-sump oil gauge | Waveshare ESP32-S3 AMOLED 1.75 + ADS122C04");
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
  static gauge::TraceHistory trace;  // static: off the main-task stack; lives as long as the display
  static gauge::BandTimes bands;     // Track: the time per band since power-up; static for the same reasons
  static gauge::WarmupClock warmup;  // Warm-up: the time since power-up and to the first warm reading
#ifndef CONFIG_OIL_GAUGE_DEMO
  Ads122c04Source source;
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
  // Wi-Fi (#33, #34): off until the BOOT button asks for it (or a saved state does), and run by its own task below this
  // loop's priority, so it can never delay a reading. It joins the network in main/wifi_secrets.h if that's there, and
  // hosts it otherwise. Without usable settings or a feature built in, it never starts.
  // The web page (#32) keeps its own copy of the 1 s log from power-up, so a phone that connects later gets the whole
  // session. Without PSRAM for it, the page is left out of the cycle.
  const bool wifi_credentials = gauge::wifiCredentialsOk(OIL_GAUGE_WIFI_SSID, OIL_GAUGE_WIFI_PASSWORD);
  static WebSource web;  // static: internal RAM (its spinlock), and it lives as long as the server
  const bool web_ok = kWebBuilt && wifi_credentials && web.begin(config::kWebLogRows);
  static gauge::WebStatus web_status;  // static: off the main-task stack
  const gauge::WifiBuild wifi_build = {web_ok, kUdpBuilt, wifi_credentials};
  static WifiNet net;  // static: internal RAM (std::atomic hand-off), and it lives as long as its task
  const bool net_ok = (wifi_build.web || wifi_build.udp) && wifi_build.credentials &&
                      net.begin(OIL_GAUGE_WIFI_SSID, OIL_GAUGE_WIFI_PASSWORD, kUdpGroup, kUdpPort, web_ok ? &web : nullptr, kUnit);
  gauge::WifiState wifi = gauge::WifiState::Off;
  uint8_t stored_wifi = gauge::encodeWifiState(wifi);
  if (nvs_ok) {
    uint8_t v = 0;
    const bool found = nvs_get_u8(nvs, "wifi", &v) == ESP_OK;
    if (found) stored_wifi = v;
    if (net_ok) wifi = gauge::decodeWifiState(found, v, wifi_build);
  }
  gpio_config_t io = {};
  io.pin_bit_mask = 1ULL << kBootButton;
  io.mode = GPIO_MODE_INPUT;
  io.pull_up_en = GPIO_PULLUP_ENABLE;
  io.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io.intr_type = GPIO_INTR_DISABLE;
  const esp_err_t io_err = gpio_config(&io);
  const bool button_ok = io_err == ESP_OK;
  if (!button_ok) ESP_LOGW(TAG,"BOOT button unavailable (%s): Wi-Fi stays as it is", esp_err_to_name(io_err));
  gauge::ShortPress button;
  gauge::PanelTimer wifi_panel;
  if (wifi != gauge::WifiState::Off) { net.request(wifi); wifi_panel.start(ticks_ms()); }  // restored: say so
  bool save_wifi = false;  // save once the network has followed the button
  gauge::WifiPhase last_phase = gauge::WifiPhase::Off;
  gauge::WifiLink last_link = gauge::WifiLink::None;
  const uint32_t session = esp_random();
  uint32_t sequence = 0;
  ESP_LOGI(TAG,"CSV: uptime_ms,status,alarm,temp_c,resistance_ohms,ref_v,sender_v,supply_v,ground_offset_v");
  uint32_t last_read=0,last_display=0,last_log=0;
  while (true) {
    const uint32_t now=ticks_ms();
    if (uint32_t(now-last_read)>=config::kSampleIntervalMs) {
      last_read=now;
#ifdef CONFIG_OIL_GAUGE_DEMO
      // 25-135 C: stays inside the demo table and passes both alarm thresholds.
      const float demo_temp = 80.0f + 55.0f * sinf(static_cast<float>(now)/25000.f);
      const float rs = gauge::resistanceFromTemperature(demo_temp);
      const float amps = config::kExcitationA;
      const gauge::Sample s = {true,0.0f,amps*config::kReferenceOhms,amps*rs,
                               config::kGroundSenseBiasV,config::kGroundSenseBiasV+amps*rs,3.30f};
      reading=state.update(s,now);
#else
      reading=state.update(source.read(),now);
#endif
      trace.update(reading,now);
      bands.update(reading,now);
      warmup.update(reading,now);
      if (net_ok && gauge::wifiUdp(wifi)) {  // the stream: one packet per sample, numbered from 0
        gauge::UdpFields f;
        f.session = session;
        f.sequence = sequence++;
        f.uptime_ms = now;
        f.reading = reading;
        f.max_c = state.maximumC();
        f.max_above_range = state.maximumAboveRange();
        f.demo = kDemo;
        net.post(f);
      }
      if (web_ok) {  // the page's live status: what this sample shows, in the gauge's own words and colour
        web_status.session = session;
        web_status.uptime_ms = now;
        web_status.reading = reading;
        web_status.colour = gauge::readingColour(reading);
        web_status.max_c = state.maximumC();
        web_status.max_above_range = state.maximumAboveRange();
        web_status.demo = kDemo;
        web_status.unit = kUnit;
        for (int b = 0; b < gauge::kBands; ++b) web_status.band_ms[b] = bands.ms(static_cast<gauge::Band>(b));
        gauge::warmupText(warmup, web_status.warmup, sizeof(web_status.warmup));
        web.setStatus(web_status);
      }
    }
    // The BOOT button: each short press moves to the next Wi-Fi state; the panel confirms it for 3 s.
    if (button_ok && button.update(gpio_get_level(kBootButton) == 0, now)) {
      if (net_ok) {
        wifi = gauge::nextWifiState(wifi, wifi_build);
        net.request(wifi);
        save_wifi = true;
      }
      wifi_panel.start(now);
    }
    if (net_ok) {
      const gauge::WifiPhase phase = net.phase();
      if (phase != last_phase) {
        last_phase = phase;
        if (phase == gauge::WifiPhase::Failed) {  // back to off, not saved, and shown
          wifi = gauge::WifiState::Off;
          save_wifi = false;
          wifi_panel.start(now);
        }
      }
      const gauge::WifiLink link = net.link();
      if (link != last_link) {  // joined, hosting, or a joined network gone and now hosted: say so
        last_link = link;
        if (link != gauge::WifiLink::None) wifi_panel.start(now);
      }
      const bool settled = wifi == gauge::WifiState::Off ? phase != gauge::WifiPhase::Starting : phase == gauge::WifiPhase::On;
      if (save_wifi && settled) {
        save_wifi = false;
        if (nvs_ok && gauge::encodeWifiState(wifi) != stored_wifi && nvsStoreWifi(nvs, wifi)) stored_wifi = gauge::encodeWifiState(wifi);
      }
    }
    if (uint32_t(now-last_display)>=config::kDisplayIntervalMs) {
      last_display=now;
      if (display_ok) {
        char line1[gauge::kWifiPanelTextSize], line2[gauge::kWifiPanelTextSize], ip[16] = "";
        bool warn = false;
        if (net_ok) net.address(ip, sizeof(ip));
        const gauge::WifiStatus st = {net_ok ? net.phase() : gauge::WifiPhase::Off,
                                      net_ok ? net.link() : gauge::WifiLink::None,
                                      net_ok ? net.failure() : gauge::WifiFailure::None, OIL_GAUGE_WIFI_SSID, ip};
        gauge::wifiPanelText(wifi, st, wifi_build, line1, sizeof(line1), line2, sizeof(line2), warn);
        const WifiPanel shown = {wifi_panel.visible(now), warn, line1, line2};
        display.render(reading,state.maximumC(),state.maximumAboveRange(),trace,bands,warmup,shown,kDemo);
      }
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
               reading.resistance_ohms,reading.reference_volts,reading.sensor_volts,
               reading.supply_volts,reading.ground_offset_volts);
      if (web_ok) web.append(reading, now);  // the same row, for the web page
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
