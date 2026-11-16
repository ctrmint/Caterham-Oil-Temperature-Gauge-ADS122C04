#pragma once
#include <stddef.h>
#include <stdint.h>

// ADS122C04 I2C frames with its data-integrity mode on (CRC[1:0] = 10, DCNT = 1).
// Datasheet SBAS751B 8.3.12-8.3.13. Pure logic, so the host tests cover it.
namespace gauge {
// CRC-16-CCITT: x^16 + x^12 + x^5 + 1, seed FFFFh, MSB first, no final XOR.
inline uint16_t adcCrc16(const uint8_t *data, size_t n) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; ++i) {
    crc ^= static_cast<uint16_t>(data[i] << 8);
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
  }
  return crc;
}
// n data bytes followed by their CRC, MSB first.
inline bool adcCrcOk(const uint8_t *frame, size_t n) {
  return adcCrc16(frame, n) == static_cast<uint16_t>((frame[n] << 8) | frame[n + 1]);
}
// RREG reply: [value, CRC MSB, CRC LSB].
inline bool decodeRegister(const uint8_t (&frame)[3], uint8_t &value) {
  if (!adcCrcOk(frame, 1)) return false;
  value = frame[0];
  return true;
}
// RDATA reply: [counter, data MSB, data, data LSB, CRC MSB, CRC LSB]; the CRC covers the
// counter and the data. The 24-bit result is two's complement.
inline bool decodeConversion(const uint8_t (&frame)[6], uint8_t &counter, int32_t &code) {
  if (!adcCrcOk(frame, 4)) return false;
  counter = frame[0];
  const uint32_t raw = (uint32_t(frame[1]) << 16) | (uint32_t(frame[2]) << 8) | frame[3];
  code = (raw & 0x800000u) ? static_cast<int32_t>(raw) - 0x1000000 : static_cast<int32_t>(raw);
  return true;
}
}  // namespace gauge
