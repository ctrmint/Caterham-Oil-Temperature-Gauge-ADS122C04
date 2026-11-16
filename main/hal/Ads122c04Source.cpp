#include "hal/Ads122c04Source.h"
#include "Config.h"
#include "core/AdcFrame.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

namespace {
// The Waveshare BSP owns I2C_NUM_1 on GPIO14/15 (touch, IO expander, codec).
constexpr i2c_port_num_t kPort = I2C_NUM_0;
constexpr gpio_num_t kSda = GPIO_NUM_17;   // display socket 17
constexpr gpio_num_t kScl = GPIO_NUM_18;   // display socket 18
constexpr uint16_t kAddress = 0x40;        // Adafruit module default: A0 and A1 to GND
constexpr uint32_t kSclHz = 400000;
constexpr int kTimeoutMs = 20;
// Commands, datasheet SBAS751B table 16.
constexpr uint8_t kCmdReset = 0x06, kCmdStart = 0x08, kCmdRdata = 0x10, kCmdRreg = 0x20, kCmdWreg = 0x40;
// Register 0: MUX[7:4], gain 1, PGA bypassed so inputs are valid down to AVSS - 0.1 V.
constexpr uint8_t kMuxShorted = 0xE;   // both inputs to mid-supply: the ADC's own offset
constexpr uint8_t kMuxA0A1 = 0x0;      // Vref, across Rref
constexpr uint8_t kMuxA1A2 = 0x4;      // Vs, across the sender
constexpr uint8_t kMuxA1Avss = 0x9;    // sender tap, single-ended
constexpr uint8_t kMuxA2Avss = 0xA;    // ground-sense wire, single-ended
constexpr uint8_t kMuxAvdd = 0xD;      // (AVDD - AVSS) / 4 monitor
constexpr uint8_t reg0(uint8_t mux) { return static_cast<uint8_t>((mux << 4) | 0x01); }
// Register 1: 45 SPS normal mode, single-shot, internal 2.048 V reference.
constexpr uint8_t kReg1 = 0x1 << 5;
// Register 2: conversion counter on, CRC16 on, both current sources set to 250 uA.
constexpr uint8_t kReg2 = 0x40 | (0x2 << 4) | 0x4;
constexpr uint8_t kBurnOut = 0x08;     // 10 uA burn-out sources, only for the wiring checks
constexpr uint8_t kDrdy = 0x80;        // register 2: result ready (read-only)
// Register 3: IDAC1 to AIN0, IDAC2 off.
constexpr uint8_t kReg3 = 0x1 << 5;
static_assert(config::kExcitationA == 0.000250f, "kReg2 sets the 250 uA current source");
constexpr float kVoltsPerCount = 2.048f / 8388608.0f;  // internal reference, gain 1
constexpr uint32_t kConversionMs = 23;  // 22.27 ms at 45 SPS, single-shot
constexpr int kMaxPolls = 5;
static const char* TAG = "oil_adc";
gauge::Sample failure() { return {false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; }
}

bool Ads122c04Source::begin() {
  i2c_master_bus_config_t bus_cfg = {};
  bus_cfg.i2c_port = kPort;
  bus_cfg.sda_io_num = kSda;
  bus_cfg.scl_io_num = kScl;
  bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_cfg.glitch_ignore_cnt = 7;
  bus_cfg.flags.enable_internal_pullup = true;  // the module also fits 10k pull-ups
  if (i2c_new_master_bus(&bus_cfg, &bus_) != ESP_OK) return false;
  i2c_device_config_t dev_cfg = {};
  dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev_cfg.device_address = kAddress;
  dev_cfg.scl_speed_hz = kSclHz;
  if (i2c_master_bus_add_device(bus_, &dev_cfg, &device_) != ESP_OK) return false;
  // A missing module is not fatal: read() reports ADC UNAVAILABLE and retries every cycle,
  // and every conversion writes the whole configuration, so no setup is lost here.
  const uint8_t reset = kCmdReset;
  if (i2c_master_probe(bus_, kAddress, kTimeoutMs) != ESP_OK)
    ESP_LOGE(TAG, "ADS122C04 not responding at 0x%02X on GPIO17/18", kAddress);
  else if (i2c_master_transmit(device_, &reset, 1, kTimeoutMs) != ESP_OK)
    ESP_LOGE(TAG, "ADS122C04 reset command failed");
  else
    ESP_LOGI(TAG, "ADS122C04 at 0x%02X, SDA GPIO17, SCL GPIO18, 250 uA on AIN0", kAddress);
  return true;
}

bool Ads122c04Source::readRegister(uint8_t address, uint8_t& value) {
  const uint8_t cmd = static_cast<uint8_t>(kCmdRreg | (address << 2));
  uint8_t rx[3] = {};
  if (i2c_master_transmit_receive(device_, &cmd, 1, rx, sizeof(rx), kTimeoutMs) != ESP_OK) return false;
  return gauge::decodeRegister(rx, value);
}

bool Ads122c04Source::readResult(uint8_t& counter, int32_t& code) {
  const uint8_t cmd = kCmdRdata;
  uint8_t rx[6] = {};
  if (i2c_master_transmit_receive(device_, &cmd, 1, rx, sizeof(rx), kTimeoutMs) != ESP_OK) return false;
  return gauge::decodeConversion(rx, counter, code);
}

bool Ads122c04Source::convert(uint8_t mux, bool burn_out, float& volts) {
  const uint8_t regs[4] = {reg0(mux), kReg1, static_cast<uint8_t>(kReg2 | (burn_out ? kBurnOut : 0)), kReg3};
  // Write the whole configuration every time, so a chip that reset itself is set up again,
  // then read each register back: a conversion only runs on a configuration known to be right.
  uint8_t tx[8];
  for (uint8_t i = 0; i < 4; ++i) {
    tx[2 * i] = static_cast<uint8_t>(kCmdWreg | (i << 2));
    tx[2 * i + 1] = regs[i];
  }
  if (i2c_master_transmit(device_, tx, sizeof(tx), kTimeoutMs) != ESP_OK) return false;
  for (uint8_t i = 0; i < 4; ++i) {
    uint8_t value = 0;
    if (!readRegister(i, value)) return false;
    if (i == 2 && (value & kDrdy)) {
      // An unread result (left by an earlier failed cycle) must not pass for this one.
      uint8_t counter = 0; int32_t code = 0;
      if (!readResult(counter, code)) return false;
      counter_ = counter;
      value &= static_cast<uint8_t>(~kDrdy);
    }
    if (value != regs[i]) return false;
  }
  const uint8_t start = kCmdStart;
  if (i2c_master_transmit(device_, &start, 1, kTimeoutMs) != ESP_OK) return false;
  vTaskDelay(pdMS_TO_TICKS(kConversionMs));
  for (int poll = 0; poll < kMaxPolls; ++poll) {
    uint8_t status = 0;
    if (!readRegister(2, status)) return false;
    if (status & kDrdy) {
      uint8_t counter = 0; int32_t code = 0;
      if (!readResult(counter, code)) return false;
      // The counter steps by one per conversion; anything else is a stale or missed result,
      // or a chip that reset itself (its counter restarts at zero).
      if (counter_known_ && counter != static_cast<uint8_t>(counter_ + 1)) return false;
      counter_ = counter;
      counter_known_ = true;
      volts = code * kVoltsPerCount;
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return false;
}

gauge::Sample Ads122c04Source::read() {
  if (!device_) return failure();
  float offset = 0.0f, vref = 0.0f, vs = 0.0f, ground = 0.0f, tap = 0.0f, avdd_quarter = 0.0f;
  // The burn-out source pulls a broken ground-sense wire or an open A1 tap towards AVDD.
  const bool ok = convert(kMuxShorted, false, offset) && convert(kMuxA0A1, false, vref) &&
                  convert(kMuxA1A2, false, vs) && convert(kMuxA2Avss, true, ground) &&
                  convert(kMuxA1Avss, true, tap) && convert(kMuxAvdd, false, avdd_quarter);
  if (!ok) {
    const bool first = !failing_;
    failing_ = true;
    counter_known_ = false;
    const esp_err_t reset = i2c_master_bus_reset(bus_);
    if (first)
      ESP_LOGW(TAG, "ADS122C04 read failed; I2C bus reset %s, retrying",
               reset == ESP_OK ? "done" : "failed");
    return failure();
  }
  if (failing_) ESP_LOGI(TAG, "ADS122C04 readings recovered");
  failing_ = false;
  return {true, offset, vref, vs, ground, tap, avdd_quarter * 4.0f};
}
