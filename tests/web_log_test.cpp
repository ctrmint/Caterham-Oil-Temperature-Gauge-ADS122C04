#include "core/Calibration.h"
#include "core/SweepModel.h"
#include "core/WebLog.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

using gauge::LogRow; using gauge::Status;

// A small JSON syntax check: one value, nothing after it. Enough to catch a stray comma, nan or unbalanced bracket.
struct Json {
  const char *p;
  void ws() { while (*p == ' ' || *p == '\n') ++p; }
  bool value() {
    ws();
    if (*p == '{') return object();
    if (*p == '[') return array();
    if (*p == '"') return string();
    if (!std::strncmp(p, "true", 4)) { p += 4; return true; }
    if (!std::strncmp(p, "false", 5)) { p += 5; return true; }
    if (!std::strncmp(p, "null", 4)) { p += 4; return true; }
    return number();
  }
  bool string() {
    for (++p; *p && *p != '"'; ++p) if (*p == '\\' || static_cast<unsigned char>(*p) < 0x20) return false;
    return *p++ == '"';
  }
  bool number() {
    const char *s = p;
    if (*p == '-') ++p;
    while ((*p >= '0' && *p <= '9') || *p == '.') ++p;
    return p > s && (p[-1] >= '0' && p[-1] <= '9');
  }
  bool array() {
    ++p; ws();
    if (*p == ']') { ++p; return true; }
    for (;;) { if (!value()) return false; ws(); if (*p == ']') { ++p; return true; } if (*p++ != ',') return false; }
  }
  bool object() {
    ++p; ws();
    if (*p == '}') { ++p; return true; }
    for (;;) {
      ws(); if (*p != '"' || !string()) return false;
      ws(); if (*p++ != ':') return false;
      if (!value()) return false;
      ws(); if (*p == '}') { ++p; return true; } if (*p++ != ',') return false;
    }
  }
};
static bool valid(const std::string &s) { Json j{s.c_str()}; if (!j.value()) return false; j.ws(); return *j.p == '\0'; }

static gauge::Reading make(Status s, bool has_t = false, float t = 0, float ohms = 123.4f, Status alarm = Status::Normal) {
  gauge::Reading r;
  r.status = s; r.alarm = alarm; r.has_temperature = has_t; r.temperature_c = t; r.resistance_ohms = ohms;
  r.reference_volts = 0.5f; r.sensor_volts = 0.0655f; r.supply_volts = 3.3f; r.ground_offset_volts = 0.012f;
  return r;
}
static std::string row(const LogRow &r, bool comma) {
  char b[160]; const size_t n = gauge::webLogRow(r, comma, b, sizeof(b)); assert(n == std::strlen(b)); return b;
}
static gauge::WebStatus status(const gauge::Reading &r) {
  gauge::WebStatus s = {};
  s.session = 0x2161ab7c; s.uptime_ms = 646145; s.reading = r; s.colour = gauge::readingColour(r);
  s.max_c = 117.5f; s.max_above_range = false; s.demo = false;
  const uint32_t bands[5] = {372000, 1105000, 2000, 0, 4500};
  std::memcpy(s.band_ms, bands, sizeof(bands));
  std::snprintf(s.warmup, sizeof(s.warmup), "WARM IN 6:12");
  return s;
}

int main() {
  // since: a plain decimal number in the query, anything else is 0 (from the start).
  assert(gauge::parseSince("since=123") == 123 && gauge::parseSince("a=1&since=45") == 45 && gauge::parseSince("since=5&b=2") == 5);
  assert(gauge::parseSince("since=4294967295") == 4294967295u && gauge::parseSince("since=0") == 0);
  for (const char *bad : {"", "since=", "since=abc", "since=-1", "since=12x", "since=4294967296", "since=4294967297", "xsince=5", "since=99999999999",
                          "since= 5", "a=since=5"})
    assert(gauge::parseSince(bad) == 0);
  assert(gauge::parseSince(nullptr) == 0);

  // The ring: rows numbered from 0, the oldest dropped once full, copies from since (or the oldest held), at most max.
  { LogRow store[5]; gauge::LogBuffer b(store, 5); LogRow out[10];
    assert(b.first() == 0 && b.next() == 0 && b.copy(0, out, 10) == 0);
    for (int i = 0; i < 3; ++i) b.append(make(Status::Normal, true, 80.f + i), 1000u * i);
    assert(b.first() == 0 && b.next() == 3 && b.copy(0, out, 10) == 3 && out[0].seq == 0 && out[2].seq == 2 && out[2].uptime_ms == 2000);
    for (int i = 3; i < 7; ++i) b.append(make(Status::Normal, true, 80.f + i), 1000u * i);
    assert(b.first() == 2 && b.next() == 7);
    assert(b.copy(0, out, 10) == 5 && out[0].seq == 2 && out[4].seq == 6);
    assert(gauge::calibrationValid() ? out[4].temp_c == 86.f : std::isnan(out[4].temp_c));  // never degrees uncalibrated
    assert(b.copy(5, out, 10) == 2 && out[0].seq == 5 && out[1].seq == 6);
    assert(b.copy(7, out, 10) == 0 && b.copy(100, out, 10) == 0);
    assert(b.copy(2, out, 2) == 2 && out[0].seq == 2 && out[1].seq == 3);
    for (int i = 7; i < 1007; ++i) b.append(make(Status::Normal, true, 90.f), 1000u * i);
    assert(b.first() == 1002 && b.next() == 1007 && b.copy(1004, out, 10) == 3 && out[0].seq == 1004 && out[0].uptime_ms == 1004000);
    gauge::LogBuffer none(nullptr, 0); none.append(make(Status::Normal), 0); assert(none.copy(0, out, 10) == 0); }

  // A row: the serial log's values, fixed decimals, null for anything not finite.
  { LogRow r = gauge::makeLogRow(make(Status::Normal, true, 92.44f, 262.13f), 12, 3456);
    assert(r.seq == 12 && r.uptime_ms == 3456 && r.status == 9 && r.alarm == 9);
    if (gauge::calibrationValid()) assert(row(r, false) == "[12,3456,9,9,92.4,262.1,0.5000,0.0655,3.3000,0.0120]");
    else assert(row(r, false) == "[12,3456,9,9,null,262.1,0.5000,0.0655,3.3000,0.0120]");  // no degrees uncalibrated
    assert(row(r, true)[0] == ',' && valid(row(r, false)));
    r = gauge::makeLogRow(make(Status::OpenCircuit, false, 0.f, INFINITY), 13, 4000);
    r.ref_v = NAN; r.supply_v = -INFINITY;
    assert(row(r, false) == "[13,4000,4,9,null,null,null,0.0655,null,0.0120]");
    r = gauge::makeLogRow(make(Status::Normal, true, NAN), 14, 4250);
    assert(std::isnan(r.temp_c));
    r = gauge::makeLogRow(make(Status::Normal, false, 80.f), 15, 4500);  // a stale value with no flag
    assert(std::isnan(r.temp_c));
    char small[10]; assert(gauge::webLogRow(r, false, small, sizeof(small)) == 0 && small[0] == '\0');
    // Exactly full: the text and its terminator fit, one byte less doesn't.
    const std::string whole = row(r, false); char exact[160];
    assert(gauge::webLogRow(r, false, exact, whole.size() + 1) == whole.size() && whole == exact);
    assert(gauge::webLogRow(r, false, exact, whole.size()) == 0 && exact[0] == '\0'); }

  // The head, then rows, then the close: valid JSON in every state.
  { for (Status st : {Status::Normal, Status::Cold, Status::Amber, Status::OpenCircuit, Status::AboveRange, Status::NeedsCalibration}) {
      gauge::WebStatus s = status(make(st, st == Status::Normal || st == Status::Cold || st == Status::Amber, 117.5f, 120.8f,
                                       st == Status::Amber ? Status::Amber : Status::Normal));
      s.max_above_range = st == Status::AboveRange;
      char head[640]; const size_t n = gauge::webLogHead(s, 2, 1843, head, sizeof(head));
      assert(n > 0 && n == std::strlen(head));
      std::string json = head;
      LogRow store[3]; gauge::LogBuffer b(store, 3);
      for (int i = 0; i < 3; ++i) b.append(s.reading, 1000u * i);
      LogRow out[3]; const uint32_t k = b.copy(0, out, 3);
      for (uint32_t i = 0; i < k; ++i) json += row(out[i], i > 0);
      json += "]}";
      assert(valid(json));
      assert(json.find("\"session\":\"2161ab7c\"") != std::string::npos && json.find("\"first\":2,\"next\":1843") != std::string::npos);
      assert(json.find("\"bands\":[372000,1105000,2000,0,4500]") != std::string::npos);
      assert(json.find("nan") == std::string::npos && json.find("inf") == std::string::npos);
      char colour[16]; std::snprintf(colour, sizeof(colour), "\"#%06lX\"", static_cast<unsigned long>(gauge::readingColour(s.reading)));
      assert(json.find(colour) != std::string::npos);
      for (gauge::Unit u : {gauge::Unit::Celsius, gauge::Unit::Fahrenheit}) {  // Sweep's lit segments, in the gauge's unit
        s.unit = u; char h[640], lit[24]; assert(gauge::webLogHead(s, 0, 0, h, sizeof(h)) > 0);
        std::snprintf(lit, sizeof(lit), "\"lit\":%d,", gauge::litSegments(s.reading, u));
        assert(std::string(h).find(lit) != std::string::npos);
      }
    }
    gauge::WebStatus s = status(make(Status::Normal, true, 92.4f));
    std::snprintf(s.warmup, sizeof(s.warmup), "WARM\" IN\\ 6:12<b>");  // never anything that could break the JSON
    char head[640]; assert(gauge::webLogHead(s, 0, 0, head, sizeof(head)) > 0);
    assert(std::string(head).find("\"warmup\":\"WARM IN 6:12b\"") != std::string::npos && valid(std::string(head) + "]}"));
    assert(gauge::webLogHead(s, 0, 0, head, 40) == 0 && head[0] == '\0');
    // The maximum: as the gauge keeps it, the table's top when it went beyond, none before the first or uncalibrated.
    const bool cal = gauge::calibrationValid();
    auto has = [&](const gauge::WebStatus &st, const char *text) {
      char h[640]; assert(gauge::webLogHead(st, 0, 0, h, sizeof(h)) > 0); return std::string(h).find(text) != std::string::npos; };
    s.max_c = 117.46f; s.max_above_range = false;
    assert(has(s, cal ? "\"max\":117.5,\"maxAbove\":false" : "\"max\":null,\"maxAbove\":false"));
    s.max_c = gauge::kNoMaxYetC; assert(has(s, "\"max\":null,\"maxAbove\":false"));
    s.max_c = gauge::kNoMaxYetC + 1.f; assert(has(s, cal ? "\"max\":-199.0" : "\"max\":null"));
    s.max_c = NAN; assert(has(s, "\"max\":null"));
    s.max_c = 100.f; s.max_above_range = true;
    char top[48]; std::snprintf(top, sizeof(top), "\"max\":%.1f,\"maxAbove\":true", static_cast<double>(gauge::calibrationMaxC()));
    assert(has(s, cal ? top : "\"max\":null,\"maxAbove\":false"));
    // The reading's own values: degrees only calibrated, with a temperature, finite; the colour as the gauge's.
    s = status(make(Status::Normal, true, 92.44f)); assert(has(s, cal ? "\"temp\":92.4," : "\"temp\":null,"));
    s = status(make(Status::Normal, false, 92.44f)); assert(has(s, "\"temp\":null,"));
    s = status(make(Status::Normal, true, INFINITY)); assert(has(s, "\"temp\":null,"));
    s.colour = 0xFF2ED158u; assert(has(s, "\"colour\":\"#2ED158\""));
    s.demo = true; assert(has(s, "\"demo\":true,\"calibrated\":") && has(s, cal ? "\"calibrated\":true" : "\"calibrated\":false"));
    assert(has(s, "\"uptime\":646145,\"status\":9,\"alarm\":9,"));
    // Sweep's lit segments, as the gauge counts them.
    s = status(make(Status::Normal, true, 81.f)); assert(has(s, cal ? "\"lit\":20," : "\"lit\":0,"));
    s.unit = gauge::Unit::Fahrenheit; assert(has(s, cal ? "\"lit\":20," : "\"lit\":0,"));
    s = status(make(Status::AboveRange)); assert(has(s, cal ? "\"lit\":40," : "\"lit\":0,"));
    s = status(make(Status::OpenCircuit, false, 0.f, INFINITY, Status::Red)); assert(has(s, "\"lit\":0,"));
    // The LED, as Sweep's: off uncalibrated, and on a fault unless an alarm is latched; pulsing with the alarm.
    s = status(make(Status::Normal, true, 92.4f)); assert(has(s, cal ? "\"led\":\"steady\"" : "\"led\":\"off\""));
    s = status(make(Status::Amber, true, 117.f, 120.f, Status::Amber)); assert(has(s, cal ? "\"led\":\"slow\"" : "\"led\":\"off\""));
    s = status(make(Status::Red, true, 131.f, 90.f, Status::Red)); assert(has(s, cal ? "\"led\":\"fast\"" : "\"led\":\"off\""));
    s = status(make(Status::OpenCircuit)); assert(has(s, "\"led\":\"off\""));
    s = status(make(Status::OpenCircuit, false, 0.f, INFINITY, Status::Amber)); assert(has(s, cal ? "\"led\":\"slow\"" : "\"led\":\"off\""));
    s = status(make(Status::OpenCircuit, false, 0.f, INFINITY, Status::Red)); assert(has(s, cal ? "\"led\":\"fast\"" : "\"led\":\"off\"")); }

  // The scale: thresholds, unit, dial, the 40 segment colours as Sweep draws them, the gradient every 2 degC.
  for (gauge::Unit u : {gauge::Unit::Celsius, gauge::Unit::Fahrenheit}) {
    char scale[2048]; const size_t n = gauge::webScale(u, scale, sizeof(scale));
    assert(n > 0 && valid(scale));
    const std::string s = scale;
    assert(s.find(u == gauge::Unit::Celsius ? "\"unit\":\"C\"" : "\"unit\":\"F\"") != std::string::npos);
    assert(s.find("\"warm\":60.0,\"amber\":115.0,\"red\":130.0") != std::string::npos);
    assert(s.find(u == gauge::Unit::Celsius ? "\"dial\":[30.0,130.0]" : "\"dial\":[86.0,266.0]") != std::string::npos);
    if (gauge::calibrationValid())  // Sweep's labels, only calibrated
      assert(s.find(u == gauge::Unit::Celsius ? "\"labels\":[30,50,70,90,110,130]," : "\"labels\":[86,122,158,194,230,266],") != std::string::npos);
    else assert(s.find("\"labels\":[],") != std::string::npos);
    for (int i = 0; i < gauge::kSweepSegments; ++i) {
      char c[12]; std::snprintf(c, sizeof(c), "\"#%06lX\"", static_cast<unsigned long>(gauge::segmentColour(i, u)));
      assert(s.find(c) != std::string::npos);
    }
    const size_t g = s.find("\"gradient\":[");
    assert(g != std::string::npos);
    size_t count = 0;
    for (size_t at = g; (at = s.find("\"#", at + 1)) != std::string::npos; ++count) {}
    assert(count == 81);
    char c80[12]; std::snprintf(c80, sizeof(c80), "\"#%06lX\"", static_cast<unsigned long>(gauge::gradientColour(80.f)));
    assert(s.find(c80, g) != std::string::npos);
    assert(gauge::webScale(u, scale, 100) == 0);
  }
  std::puts(gauge::calibrationValid() ? "Web log tests passed (demo)" : "Web log tests passed (uncalibrated)");
  return 0;
}
