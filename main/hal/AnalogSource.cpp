#include "hal/AnalogSource.h"
#include "Config.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_err.h"

namespace {
constexpr adc_channel_t kSensorChannel = ADC_CHANNEL_5;    // GPIO16 / ADC2_CH5
constexpr adc_channel_t kSupplyChannel = ADC_CHANNEL_6;    // GPIO17 / ADC2_CH6
constexpr int kSamplesPerChannel = 24;
constexpr adc_atten_t kAtten = ADC_ATTEN_DB_12;
static const char* TAG = "oil_adc";
}

bool AnalogSource::begin() {
  adc_oneshot_unit_init_cfg_t unit_cfg = {};
  unit_cfg.unit_id = ADC_UNIT_2;
  if (adc_oneshot_new_unit(&unit_cfg, &unit_) != ESP_OK) return false;
  adc_oneshot_chan_cfg_t chan_cfg = {};
  chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
  chan_cfg.atten = kAtten;
  if (adc_oneshot_config_channel(unit_, kSensorChannel, &chan_cfg) != ESP_OK ||
      adc_oneshot_config_channel(unit_, kSupplyChannel, &chan_cfg) != ESP_OK) return false;
  adc_cali_curve_fitting_config_t cal_cfg = {};
  cal_cfg.unit_id = ADC_UNIT_2;
  cal_cfg.chan = kSensorChannel;
  cal_cfg.atten = kAtten;
  cal_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
  // Never silently convert raw ADC codes using a fictitious voltage scale.
  esp_err_t err = adc_cali_create_scheme_curve_fitting(&cal_cfg, &sensor_calibration_);
  if (err == ESP_OK) {
    cal_cfg.chan = kSupplyChannel;
    err = adc_cali_create_scheme_curve_fitting(&cal_cfg, &supply_calibration_);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ADC calibration unavailable: %s", esp_err_to_name(err));
    return false;
  }
  available_ = true;
  ESP_LOGI(TAG, "ADC2 GPIO16/17, curve calibration enabled; Wi-Fi disabled by design");
  return true;
}

bool AnalogSource::readVolts(adc_channel_t channel, adc_cali_handle_t calibration, float& volts) {
  int64_t total_mv = 0;
  for (int n = 0; n < kSamplesPerChannel; ++n) {
    int raw = 0, calibrated_mv = 0;
    if (adc_oneshot_read(unit_, channel, &raw) != ESP_OK ||
        adc_cali_raw_to_voltage(calibration, raw, &calibrated_mv) != ESP_OK) return false;
    total_mv += calibrated_mv;
  }
  volts = (static_cast<float>(total_mv) / kSamplesPerChannel) / 1000.0f;
  return true;
}

gauge::Sample AnalogSource::read() {
  if (!available_) return {false, 0.0f, 0.0f};
  float sensor = 0.0f, midpoint = 0.0f;
  if (!readVolts(kSensorChannel, sensor_calibration_, sensor) ||
      !readVolts(kSupplyChannel, supply_calibration_, midpoint))
    return {false, 0.0f, 0.0f};
  const float supply = midpoint * (config::kReferenceUpperOhms +
                                   config::kReferenceLowerOhms) /
                                   config::kReferenceLowerOhms;
  return {true, sensor, supply};
}
