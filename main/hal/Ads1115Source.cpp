#include "hal/Ads1115Source.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

namespace {
// The Waveshare BSP owns I2C_NUM_1 on GPIO14/15 (touch, IO expander, codec).
constexpr i2c_port_num_t kPort = I2C_NUM_0;
constexpr gpio_num_t kSda = GPIO_NUM_17;   // H2-6
constexpr gpio_num_t kScl = GPIO_NUM_18;   // H2-7
constexpr uint16_t kAddress = 0x48;        // ADDR tied to GND
constexpr uint32_t kSclHz = 400000;
constexpr int kTimeoutMs = 20;
constexpr uint8_t kRegConversion = 0x00;
constexpr uint8_t kRegConfig = 0x01;
// Config register: OS=1 start, PGA ±4.096 V, single-shot, 128 SPS, comparator off.
// Both differential readings use the same PGA so the ADC gain error cancels in Vs/Vp.
constexpr uint16_t kConfigBase = 0x8000 | (0x1 << 9) | 0x0100 | (0x4 << 5) | 0x0003;
constexpr uint16_t kMuxA0A3 = 0x1 << 12;   // Vp = A0 - A3, across the pull-up
constexpr uint16_t kMuxA1A3 = 0x2 << 12;   // A1 - A3 = -Vs (no A3 - A1 setting exists)
constexpr uint16_t kMuxA0Gnd = 0x4 << 12;  // excitation rail, single-ended
constexpr float kVoltsPerCount = 4.096f / 32768.0f;
constexpr int kPairsPerRead = 4;
constexpr uint32_t kConversionMs = 8;      // 1/128 s = 7.8 ms
constexpr int kMaxPolls = 5;
static const char* TAG = "oil_adc";
gauge::Sample failure() { return {false, 0.0f, 0.0f, 0.0f}; }
}

bool Ads1115Source::begin() {
  i2c_master_bus_config_t bus_cfg = {};
  bus_cfg.i2c_port = kPort;
  bus_cfg.sda_io_num = kSda;
  bus_cfg.scl_io_num = kScl;
  bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_cfg.glitch_ignore_cnt = 7;
  bus_cfg.flags.enable_internal_pullup = true;  // module also fits 10k pull-ups
  if (i2c_new_master_bus(&bus_cfg, &bus_) != ESP_OK) return false;
  i2c_device_config_t dev_cfg = {};
  dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev_cfg.device_address = kAddress;
  dev_cfg.scl_speed_hz = kSclHz;
  if (i2c_master_bus_add_device(bus_, &dev_cfg, &device_) != ESP_OK) return false;
  // A missing module is not fatal: read() reports ADC UNAVAILABLE and retries every cycle.
  if (i2c_master_probe(bus_, kAddress, kTimeoutMs) != ESP_OK)
    ESP_LOGE(TAG, "ADS1115 not responding at 0x%02X on GPIO17/18", kAddress);
  else
    ESP_LOGI(TAG, "ADS1115 at 0x%02X, SDA GPIO17, SCL GPIO18, +/-4.096 V", kAddress);
  return true;
}

bool Ads1115Source::convert(uint16_t mux, float& volts) {
  const uint16_t cfg = kConfigBase | mux;
  const uint8_t tx[3] = {kRegConfig, static_cast<uint8_t>(cfg >> 8), static_cast<uint8_t>(cfg & 0xFF)};
  if (i2c_master_transmit(device_, tx, sizeof(tx), kTimeoutMs) != ESP_OK) return false;
  vTaskDelay(pdMS_TO_TICKS(kConversionMs));
  for (int poll = 0; poll < kMaxPolls; ++poll) {
    uint8_t reg = kRegConfig, rx[2] = {};
    if (i2c_master_transmit_receive(device_, &reg, 1, rx, 2, kTimeoutMs) != ESP_OK) return false;
    // Read-back must match what was written (guards against a wrong device or a reset chip).
    if ((rx[0] & 0x7F) != ((cfg >> 8) & 0x7F)) return false;
    if (rx[0] & 0x80) {  // OS=1: conversion complete
      reg = kRegConversion;
      if (i2c_master_transmit_receive(device_, &reg, 1, rx, 2, kTimeoutMs) != ESP_OK) return false;
      const int16_t code = static_cast<int16_t>((rx[0] << 8) | rx[1]);
      volts = code * kVoltsPerCount;
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return false;
}

gauge::Sample Ads1115Source::read() {
  if (!device_) return failure();
  float pullup = 0.0f, sender = 0.0f, supply = 0.0f;
  bool ok = true;
  // Interleave the two differential readings so both see the same moment.
  for (int n = 0; ok && n < kPairsPerRead; ++n) {
    float vp = 0.0f, a1_minus_a3 = 0.0f;
    ok = convert(kMuxA0A3, vp) && convert(kMuxA1A3, a1_minus_a3);
    pullup += vp;
    sender -= a1_minus_a3;
  }
  ok = ok && convert(kMuxA0Gnd, supply);
  if (!ok) {
    if (!failing_) ESP_LOGW(TAG, "ADS1115 read failed; resetting I2C bus and retrying");
    failing_ = true;
    i2c_master_bus_reset(bus_);
    return failure();
  }
  if (failing_) ESP_LOGI(TAG, "ADS1115 readings recovered");
  failing_ = false;
  return {true, pullup / kPairsPerRead, sender / kPairsPerRead, supply};
}
