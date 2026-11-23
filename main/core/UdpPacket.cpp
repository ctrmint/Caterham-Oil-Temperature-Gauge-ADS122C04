#include "UdpPacket.h"
#include "Calibration.h"
#include <cmath>
#include <cstring>
namespace gauge {
namespace {
// docs/UDP_STREAM.md freezes these codes: a reordered Status must not silently change the stream.
static_assert(static_cast<int>(Status::NoAdc) == 0 && static_cast<int>(Status::BadSupply) == 1 &&
              static_cast<int>(Status::GroundFault) == 2 && static_cast<int>(Status::ShortCircuit) == 3 &&
              static_cast<int>(Status::OpenCircuit) == 4 && static_cast<int>(Status::NeedsCalibration) == 5 &&
              static_cast<int>(Status::BelowRange) == 6 && static_cast<int>(Status::AboveRange) == 7 &&
              static_cast<int>(Status::Cold) == 8 && static_cast<int>(Status::Normal) == 9 &&
              static_cast<int>(Status::Amber) == 10 && static_cast<int>(Status::Red) == 11,
              "the UDP stream's status codes are frozen (docs/UDP_STREAM.md)");
static_assert(sizeof(float) == 4, "float32");
constexpr uint32_t kQuietNan = 0x7FC00000u;

void put16(uint8_t *p, uint16_t v) { p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8); }
void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
void putFloat(uint8_t *p, float v) {
  uint32_t bits = kQuietNan;
  if (std::isfinite(v)) std::memcpy(&bits, &v, sizeof(bits));
  put32(p, bits);
}
} // namespace

uint8_t udpStatusCode(Status s) { return static_cast<uint8_t>(s); }

uint32_t crc32(const uint8_t *data, size_t n) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < n; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return crc ^ 0xFFFFFFFFu;
}

void encodeUdpPacket(const UdpFields &f, uint8_t (&out)[kUdpPacketSize]) {
  const Reading &r = f.reading;
  const bool calibrated = calibrationValid();
  const bool has_t = calibrated && r.has_temperature && std::isfinite(r.temperature_c);
  const bool above = calibrated && f.max_above_range;
  const bool has_max = above || (calibrated && std::isfinite(f.max_c) && f.max_c > kNoMaxYetC);
  uint16_t flags = 0;
  if (calibrated) flags |= kUdpCalibrated;
  if (f.demo) flags |= kUdpDemo;
  if (has_t) flags |= kUdpHasTemperature;
  if (above) flags |= kUdpMaxAboveRange;
  if (has_max) flags |= kUdpHasMax;
  std::memset(out, 0, sizeof(out));
  std::memcpy(out, "OILG", 4);
  out[4] = kUdpVersion;
  out[5] = static_cast<uint8_t>(kUdpPacketSize);
  put16(out + 6, flags);
  put32(out + 8, f.session);
  put32(out + 12, f.sequence);
  put32(out + 16, f.uptime_ms);
  out[20] = udpStatusCode(r.status);
  out[21] = udpStatusCode(r.alarm);
  putFloat(out + 24, has_t ? r.temperature_c : NAN);
  putFloat(out + 28, r.resistance_ohms);
  putFloat(out + 32, r.reference_volts);
  putFloat(out + 36, r.sensor_volts);
  putFloat(out + 40, r.supply_volts);
  putFloat(out + 44, r.ground_offset_volts);
  putFloat(out + 48, above ? calibrationMaxC() : (has_max ? f.max_c : NAN));
  put32(out + 52, crc32(out, 52));
}
} // namespace gauge
