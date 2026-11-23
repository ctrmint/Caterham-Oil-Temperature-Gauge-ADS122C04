#include "core/Calibration.h"
#include "core/UdpPacket.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using gauge::Status;

static uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24; }
[[maybe_unused]] static float getf(const uint8_t *p) { const uint32_t b = get32(p); float f; std::memcpy(&f, &b, 4); return f; }
static gauge::UdpFields base() {
  gauge::UdpFields f; f.session = 0x12345678u; f.sequence = 7; f.uptime_ms = 1750;
  f.reading.status = Status::Normal; f.reading.alarm = Status::Normal; f.reading.has_temperature = true;
  f.reading.temperature_c = 92.f; f.reading.resistance_ohms = 262.f; f.reading.reference_volts = 0.5f;
  f.reading.sensor_volts = 0.0655f; f.reading.supply_volts = 3.3f; f.reading.ground_offset_volts = 0.012f;
  f.max_c = 94.f;
  return f;
}
static bool isQuietNan(const uint8_t *p) { return get32(p) == 0x7FC00000u; }

int main() {
  // CRC-32 as zlib: the standard check value, and the empty input.
  assert(gauge::crc32(reinterpret_cast<const uint8_t *>("123456789"), 9) == 0xCBF43926u);
  assert(gauge::crc32(nullptr, 0) == 0u);
  // The frozen status codes.
  { const Status all[] = {Status::NoAdc, Status::BadSupply, Status::GroundFault, Status::ShortCircuit, Status::OpenCircuit,
                          Status::NeedsCalibration, Status::BelowRange, Status::AboveRange, Status::Cold, Status::Normal,
                          Status::Amber, Status::Red};
    for (int i = 0; i < 12; ++i) assert(gauge::udpStatusCode(all[i]) == i); }

  // The header and layout: magic, version, length, the counters little-endian, the reserved bytes 0, the CRC of 0..51.
  { uint8_t b[gauge::kUdpPacketSize]; std::memset(b, 0xAA, sizeof(b));
    gauge::UdpFields f = base(); f.session = 0xA1B2C3D4u; f.sequence = 0x01020304u; f.uptime_ms = 0xFFFFFFFEu;
    gauge::encodeUdpPacket(f, b);
    assert(gauge::kUdpPacketSize == 56 && std::memcmp(b, "OILG", 4) == 0 && b[4] == 1 && b[5] == 56);
    assert(b[8] == 0xD4 && b[9] == 0xC3 && b[10] == 0xB2 && b[11] == 0xA1 && get32(b + 12) == 0x01020304u);
    assert(get32(b + 16) == 0xFFFFFFFEu && b[20] == 9 && b[21] == 9 && b[22] == 0 && b[23] == 0);
    assert(get32(b + 52) == gauge::crc32(b, 52));
    b[30] ^= 1; assert(get32(b + 52) != gauge::crc32(b, 52)); }

  // Every non-finite value goes out as the one quiet NaN; the temperature only with a finite reading and a calibration.
  { uint8_t b[gauge::kUdpPacketSize]; gauge::UdpFields f = base();
    f.reading.resistance_ohms = INFINITY; f.reading.reference_volts = -INFINITY; f.reading.sensor_volts = NAN;
    f.reading.supply_volts = -NAN; f.reading.ground_offset_volts = INFINITY;
    gauge::encodeUdpPacket(f, b);
    for (int o = 28; o <= 44; o += 4) assert(isQuietNan(b + o));
    f = base(); f.reading.temperature_c = INFINITY; gauge::encodeUdpPacket(f, b);
    assert(isQuietNan(b + 24) && !(b[6] & gauge::kUdpHasTemperature));
    f = base(); f.reading.temperature_c = NAN; gauge::encodeUdpPacket(f, b);
    assert(isQuietNan(b + 24) && !(b[6] & gauge::kUdpHasTemperature));
    f = base(); f.reading.has_temperature = false; f.reading.temperature_c = 80.f; gauge::encodeUdpPacket(f, b);  // stale value
    assert(isQuietNan(b + 24) && !(b[6] & gauge::kUdpHasTemperature));
    f = base(); f.max_c = NAN; gauge::encodeUdpPacket(f, b);
    assert(isQuietNan(b + 48) && !(b[6] & gauge::kUdpHasMax));
    f = base(); f.max_c = gauge::kNoMaxYetC; gauge::encodeUdpPacket(f, b);
    assert(isQuietNan(b + 48) && !(b[6] & gauge::kUdpHasMax));
    f = base(); f.demo = true; gauge::encodeUdpPacket(f, b); assert(b[6] & gauge::kUdpDemo);
    f.demo = false; gauge::encodeUdpPacket(f, b); assert(!(b[6] & gauge::kUdpDemo) && b[7] == 0); }

#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());
  { uint8_t b[gauge::kUdpPacketSize]; gauge::UdpFields f = base(); gauge::encodeUdpPacket(f, b);
    assert(b[6] == (gauge::kUdpCalibrated | gauge::kUdpHasTemperature | gauge::kUdpHasMax) && getf(b + 24) == 92.f && getf(b + 48) == 94.f);
    f.max_above_range = true; f.max_c = 12.f; gauge::encodeUdpPacket(f, b);  // above the table: the table's maximum, flagged
    assert((b[6] & gauge::kUdpMaxAboveRange) && (b[6] & gauge::kUdpHasMax) && getf(b + 48) == gauge::calibrationMaxC()); }
  const char *build = "cal";
#else
  assert(!gauge::calibrationValid());
  { uint8_t b[gauge::kUdpPacketSize]; gauge::UdpFields f = base(); f.max_above_range = true;
    gauge::encodeUdpPacket(f, b);  // uncalibrated: no temperature and no maximum, whatever arrives
    assert(b[6] == 0 && isQuietNan(b + 24) && isQuietNan(b + 48)); }
  const char *build = "uncal";
#endif

  // The golden packets (tests/data/udp_packets.txt), shared with test_udp_listen.py: re-encode this build's lines.
  std::ifstream in("tests/data/udp_packets.txt");
  assert(in.good());
  std::string text;
  int checked = 0;
  while (std::getline(in, text)) {
    if (text.empty() || text[0] == '#') continue;
    std::istringstream s(text);
    std::string name, b, session, hex; uint32_t seq, uptime; unsigned flags, status, alarm; std::string v[7];
    s >> name >> b >> session >> seq >> uptime >> flags >> status >> alarm;
    for (auto &x : v) s >> x;
    s >> hex;
    assert(!s.fail() && hex.size() == 2 * gauge::kUdpPacketSize);
    if (b != build) continue;
    auto num = [](const std::string &x) { return x == "nan" ? NAN : std::strtof(x.c_str(), nullptr); };
    gauge::UdpFields f;
    f.session = static_cast<uint32_t>(std::strtoul(session.c_str(), nullptr, 16));
    f.sequence = seq; f.uptime_ms = uptime;
    f.reading.status = static_cast<Status>(status); f.reading.alarm = static_cast<Status>(alarm);
    f.reading.has_temperature = flags & gauge::kUdpHasTemperature;
    f.reading.temperature_c = f.reading.has_temperature ? num(v[0]) : 0.f;
    f.reading.resistance_ohms = num(v[1]); f.reading.reference_volts = num(v[2]); f.reading.sensor_volts = num(v[3]);
    f.reading.supply_volts = num(v[4]); f.reading.ground_offset_volts = num(v[5]);
    f.max_above_range = flags & gauge::kUdpMaxAboveRange;
    f.max_c = (flags & gauge::kUdpHasMax) && !f.max_above_range ? num(v[6]) : gauge::kNoMaxYetC;
    f.demo = flags & gauge::kUdpDemo;
    uint8_t out[gauge::kUdpPacketSize];
    gauge::encodeUdpPacket(f, out);
    char got[2 * gauge::kUdpPacketSize + 1];
    for (size_t i = 0; i < gauge::kUdpPacketSize; ++i) std::snprintf(got + 2 * i, 3, "%02x", out[i]);
    if (hex != got) { std::fprintf(stderr, "%s: %s\n   want %s\n", name.c_str(), got, hex.c_str()); assert(false); }
    assert((out[6] | out[7] << 8) == static_cast<int>(flags));
    ++checked;
  }
#ifdef GAUGE_DEMO
  assert(checked == 9);
  std::puts("UDP packet tests passed (demo)");
#else
  assert(checked == 2);
  std::puts("UDP packet tests passed (uncalibrated)");
#endif
  return 0;
}
