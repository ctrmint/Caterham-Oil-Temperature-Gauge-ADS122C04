#pragma once
#include "DisplayText.h"
#include "Temperature.h"
#include <stddef.h>
#include <stdint.h>
// UDP data stream (#33): the version 1 packet, as documented in docs/UDP_STREAM.md. Pure logic, so the host tests cover it;
// the network task only sends it.
namespace gauge {
constexpr size_t kUdpPacketSize = 56;
constexpr uint8_t kUdpVersion = 1;
// The flag bits at offset 6.
constexpr uint16_t kUdpCalibrated = 1u << 0, kUdpDemo = 1u << 1, kUdpHasTemperature = 1u << 2,
                   kUdpMaxAboveRange = 1u << 3, kUdpHasMax = 1u << 4;
// The frozen status codes 0..11 (the order of Status; static_asserts in UdpPacket.cpp keep it).
uint8_t udpStatusCode(Status s);
// CRC-32 (IEEE 802.3: reflected, polynomial 0x04C11DB7, initial and final XOR 0xFFFFFFFF), the same as zlib's crc32().
uint32_t crc32(const uint8_t *data, size_t n);
// What one packet carries.
struct UdpFields {
  uint32_t session = 0;     // random per power-up
  uint32_t sequence = 0;    // 0 at power-up, +1 per sample
  uint32_t uptime_ms = 0;
  Reading reading;
  float max_c = kNoMaxYetC;      // the session maximum in degC; at or below kNoMaxYetC when there's none yet
  bool max_above_range = false;  // the maximum went above the table
  bool demo = false;
};
// One packet, little-endian on any host. Fail closed: temp_c is NaN unless there's a verified calibration and the reading
// has a finite temperature (and only then is the has-temperature flag set); every other non-finite value goes out as NaN;
// with no verified calibration there's no maximum either. NaN is always the quiet NaN 0x7FC00000.
void encodeUdpPacket(const UdpFields &f, uint8_t (&out)[kUdpPacketSize]);
} // namespace gauge
