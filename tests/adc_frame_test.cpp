#include "core/AdcFrame.h"
#include <cassert>
#include <cstdio>
#include <cstring>

// Builds a frame with a correct CRC appended after n data bytes.
template <size_t N> static void seal(uint8_t (&frame)[N], size_t n) {
  const uint16_t crc = gauge::adcCrc16(frame, n);
  frame[n] = static_cast<uint8_t>(crc >> 8);
  frame[n + 1] = static_cast<uint8_t>(crc & 0xFF);
}

int main() {
  // CRC-16/CCITT-FALSE check value for "123456789".
  const char *check = "123456789";
  assert(gauge::adcCrc16(reinterpret_cast<const uint8_t *>(check), std::strlen(check)) == 0x29B1);
  assert(gauge::adcCrc16(nullptr, 0) == 0xFFFF);
  const uint8_t one[] = {0x00};
  assert(gauge::adcCrc16(one, 1) == 0xE1F0);

  // Register reply.
  uint8_t reg[3] = {0x6C, 0, 0};
  seal(reg, 1);
  uint8_t value = 0;
  assert(gauge::decodeRegister(reg, value) && value == 0x6C);
  for (int byte = 0; byte < 3; ++byte)
    for (int bit = 0; bit < 8; ++bit) {
      uint8_t bad[3]; std::memcpy(bad, reg, sizeof(bad));
      bad[byte] ^= static_cast<uint8_t>(1 << bit);
      value = 0x55;
      assert(!gauge::decodeRegister(bad, value) && value == 0x55);  // any single-bit error
    }
  // A chip that reset itself has its CRC off and answers with one byte; the rest reads FFh.
  uint8_t unsealed[3] = {0x6C, 0xFF, 0xFF};
  assert(!gauge::decodeRegister(unsealed, value));

  // Conversion replies: counter, then 24-bit two's complement.
  struct { uint8_t b[3]; int32_t code; } cases[] = {
    {{0x00, 0x00, 0x00}, 0},
    {{0x00, 0x00, 0x01}, 1},
    {{0x7F, 0xFF, 0xFF}, 8388607},     // positive full scale
    {{0x80, 0x00, 0x00}, -8388608},    // negative full scale
    {{0xFF, 0xFF, 0xFF}, -1},
    {{0x12, 0x34, 0x56}, 0x123456},
    {{0xC0, 0x00, 0x00}, -4194304},
  };
  for (auto &c : cases) {
    uint8_t frame[6] = {0xA5, c.b[0], c.b[1], c.b[2], 0, 0};
    seal(frame, 4);
    uint8_t counter = 0; int32_t code = 12345;
    assert(gauge::decodeConversion(frame, counter, code));
    assert(counter == 0xA5 && code == c.code);
  }
  uint8_t frame[6] = {0x07, 0x01, 0x02, 0x03, 0, 0};
  seal(frame, 4);
  for (int byte = 0; byte < 6; ++byte)
    for (int bit = 0; bit < 8; ++bit) {
      uint8_t bad[6]; std::memcpy(bad, frame, sizeof(bad));
      bad[byte] ^= static_cast<uint8_t>(1 << bit);
      uint8_t counter = 0x11; int32_t code = 777;
      assert(!gauge::decodeConversion(bad, counter, code) && counter == 0x11 && code == 777);
    }
  std::puts("ADC frame tests passed");
}
