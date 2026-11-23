#include "core/Calibration.h"
#include "core/DisplayText.h"
#include "core/NixieModel.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using gauge::NixiePoint; using gauge::NixieStroke; using gauge::Status;

static bool tubes(const char *cells, const char *want) {
  char out[gauge::kNixieTubes];
  gauge::nixieTubes(cells, out);
  return std::memcmp(out, want, gauge::kNixieTubes) == 0;
}
static gauge::Reading make(Status s, bool has_t=false, float t_c=0, float ohms=123.4f) {
  gauge::Reading r;
  r.status=s; r.alarm=Status::Normal; r.has_temperature=has_t; r.temperature_c=t_c; r.resistance_ohms=ohms;
  return r;
}
// The tubes for a reading, through the same text and cells as Classic.
static bool shows(const gauge::Reading &r, gauge::Unit unit, const char *want) {
  char text[gauge::kReadingTextSize], cells[gauge::kReadingCells + 1];
  gauge::readingText(r, unit, text, sizeof(text));
  gauge::readingCells(text, cells, sizeof(cells));
  return tubes(cells, want);
}
static float dist(NixiePoint a, NixiePoint b) { return std::hypot(a.x - b.x, a.y - b.y); }

int main() {
  // What a tube can show.
  for (char c : {' ','0','1','2','3','4','5','6','7','8','9','-','<','>'}) assert(gauge::nixieShowable(c));
  for (char c : {'\0','.','+','A','o','/',':','*','\xC2'}) assert(!gauge::nixieShowable(c));

  // The tubes are Classic's cells, right-aligned, with a blank tube dark; anything unshowable fails closed to "  --".
  assert(tubes(" 118", " 118") && tubes("  92", "  92") && tubes("   7", "   7") && tubes(">130", ">130"));
  assert(tubes(" <20", " <20") && tubes("  --", "  --") && tubes("1234", "1234") && tubes("  -5", "  -5"));
  assert(tubes(" 1.5", "  --") && tubes("12A4", "  --") && tubes("A   ", "  --") && tubes("   +", "  --"));
  { const char shortCells[] = "92"; assert(tubes(shortCells, "  --")); }  // a NUL inside the four cells
  assert(tubes(nullptr, "  --"));

  // Every character a tube can show has a numeral, except the blank; nothing else has one.
  for (char c : {'0','1','2','3','4','5','6','7','8','9','-','<','>'}) {
    int n = -1; const NixieStroke *s = gauge::nixieStrokes(c, n); assert(s && n >= 1 && n <= 3);
  }
  for (char c : {' ','\0','.','A','+'}) { int n = -1; assert(!gauge::nixieStrokes(c, n) && n == 0); }

  // Every numeral: inside its box with room for the glow, arcs at most a full turn in steps of at most 20 degrees from
  // a0 to a1, and one connected wire (each stroke touches the rest at an end).
  const char all[] = "0123456789-<>";
  for (const char *p = all; *p; ++p) {
    int n = 0; const NixieStroke *s = gauge::nixieStrokes(*p, n);
    NixiePoint pts[3][gauge::kNixieMaxPoints]; int np[3] = {};
    for (int k = 0; k < n; ++k) {
      np[k] = gauge::nixiePolyline(s[k], pts[k]);
      if (s[k].arc) {
        const float span = s[k].a1 - s[k].a0;
        assert(span > 0.f && span <= 360.f);
        assert(np[k] == static_cast<int>(std::ceil(span / gauge::kNixieArcStepDeg - 1e-4f)) + 1);
        const float r = s[k].x1 > s[k].y1 ? s[k].x1 : s[k].y1;
        for (int i = 1; i < np[k]; ++i)
          assert(dist(pts[k][i], pts[k][i - 1]) <= 2.f * r * std::sin(gauge::kNixieArcStepDeg * 3.14159265f / 360.f) + 1e-3f);
        const float a0 = s[k].a0 * 3.14159265f / 180.f, a1 = s[k].a1 * 3.14159265f / 180.f;
        assert(dist(pts[k][0], {s[k].x0 + s[k].x1 * std::cos(a0), s[k].y0 + s[k].y1 * std::sin(a0)}) < 1e-3f);
        assert(dist(pts[k][np[k] - 1], {s[k].x0 + s[k].x1 * std::cos(a1), s[k].y0 + s[k].y1 * std::sin(a1)}) < 1e-3f);
      } else {
        assert(np[k] == 2 && pts[k][0].x == s[k].x0 && pts[k][0].y == s[k].y0 && pts[k][1].x == s[k].x1 && pts[k][1].y == s[k].y1);
        assert(dist(pts[k][0], pts[k][1]) > 4.f);
      }
      for (int i = 0; i < np[k]; ++i) {
        assert(pts[k][i].x >= gauge::kNixieMargin - 1e-3f && pts[k][i].x <= gauge::kNixieW - gauge::kNixieMargin + 1e-3f);
        assert(pts[k][i].y >= gauge::kNixieMargin - 1e-3f && pts[k][i].y <= gauge::kNixieH - gauge::kNixieMargin + 1e-3f);
      }
    }
    for (int k = 1; k < n; ++k) {  // stroke k touches an earlier stroke: one of its ends lies on it, or one of its ends on k
      bool touches = false;
      for (int j = 0; j < k && !touches; ++j)
        for (int e : {0, np[k] - 1}) for (int i = 0; i < np[j]; ++i) {
          if (dist(pts[k][e], pts[j][i]) < 1.f) touches = true;
        }
      for (int j = 0; j < k && !touches; ++j)
        for (int e : {0, np[j] - 1}) for (int i = 0; i < np[k]; ++i) {
          if (dist(pts[j][e], pts[k][i]) < 1.f) touches = true;
        }
      assert(touches);
    }
  }
  // The closed loops close: 0, 8, and the loops of 6 and 9.
  for (char c : {'0','6','8','9'}) {
    int n = 0; const NixieStroke *s = gauge::nixieStrokes(c, n); NixiePoint pts[gauge::kNixieMaxPoints];
    const int m = gauge::nixiePolyline(s[0], pts); assert(s[0].arc && dist(pts[0], pts[m - 1]) < 1e-3f);
  }
  // A '1' is tall and narrow; the numerals all differ from each other.
  { int n = 0; const NixieStroke *s = gauge::nixieStrokes('1', n); assert(!s[0].arc && s[0].x0 == s[0].x1 && s[0].y0 - s[0].y1 > 60.f); }
  for (const char *p = all; *p; ++p) for (const char *q = p + 1; *q; ++q) {
    int a = 0, b = 0; const NixieStroke *sa = gauge::nixieStrokes(*p, a), *sb = gauge::nixieStrokes(*q, b);
    assert(sa != sb);
    if (a == b) assert(std::memcmp(sa, sb, sizeof(NixieStroke) * a) != 0);
  }
  // Line strokes in steps: never more points than the buffer.
  { NixiePoint pts[gauge::kNixieMaxPoints];
    assert(gauge::nixiePolyline({true, 20, 40, 16, 36, 0, 360}, pts) == gauge::kNixieMaxPoints);
    assert(gauge::nixiePolyline({true, 20, 40, 16, 36, 0, 20}, pts) == 2);
    assert(gauge::nixiePolyline({true, 20, 40, 16, 36, 0, 21}, pts) == 3);
    assert(gauge::nixiePolyline({true, 20, 40, 16, 36, 0, 720}, pts) == gauge::kNixieMaxPoints); }

  // Readings through Classic's text: the tubes match Classic's cells in every state.
#ifdef GAUGE_DEMO
  assert(gauge::calibrationValid());
  assert(shows(make(Status::Normal, true, 92.4f), gauge::Unit::Celsius, "  92"));
  assert(shows(make(Status::Amber, true, 117.6f), gauge::Unit::Celsius, " 118"));
  assert(shows(make(Status::Amber, true, 117.6f), gauge::Unit::Fahrenheit, " 244"));
  assert(shows(make(Status::AboveRange), gauge::Unit::Celsius, ">140"));
  assert(shows(make(Status::AboveRange), gauge::Unit::Fahrenheit, ">284"));
  assert(shows(make(Status::BelowRange), gauge::Unit::Celsius, " <20"));
  assert(shows(make(Status::OpenCircuit), gauge::Unit::Celsius, "  --"));
  assert(shows(make(Status::Normal, true, NAN), gauge::Unit::Celsius, "  --"));
  std::puts("Nixie model tests passed (demo)");
#else
  assert(!gauge::calibrationValid());
  assert(shows(make(Status::NeedsCalibration, false, 0, 123.4f), gauge::Unit::Celsius, " 123"));
  assert(shows(make(Status::NeedsCalibration, false, 0, 1234.4f), gauge::Unit::Celsius, "1234"));
  assert(shows(make(Status::NeedsCalibration, false, 0, 12345.f), gauge::Unit::Celsius, "  --"));  // too long: fails closed
  assert(shows(make(Status::OpenCircuit, false, 0, NAN), gauge::Unit::Celsius, "  --"));
  std::puts("Nixie model tests passed (uncalibrated)");
#endif
  return 0;
}
