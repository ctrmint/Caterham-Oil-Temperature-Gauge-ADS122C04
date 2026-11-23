#pragma once
#include "DisplayText.h"
// Nixie (#26): the reading in four fixed nixie tubes, with numerals drawn from strokes. Pure logic, so the host tests cover
// it; the display only draws it.
namespace gauge {
constexpr int kNixieTubes = 4;
static_assert(kNixieTubes == static_cast<int>(kReadingCells), "one tube per reading cell");
// A character a tube can show: a blank (a dark tube), a digit, '-', '<' or '>'.
bool nixieShowable(char c);
// The character each tube shows, left to right, from a readingCells() string (kReadingCells characters, right-aligned),
// so the tubes match Classic's cells. Fails closed: if any character can't be shown, the reading is "  --", as Classic.
void nixieTubes(const char *cells, char (&out)[kNixieTubes]);

// A numeral's box, in units with y down: every stroke stays at least kNixieMargin inside it, room for the glow.
constexpr float kNixieW = 40.f, kNixieH = 80.f, kNixieMargin = 4.f;
// A stroke: a line from (x0, y0) to (x1, y1), or an elliptical arc about (x0, y0) with radii x1 and y1, from angle a0 to
// a1 (a1 > a0, at most a full turn), in degrees clockwise from 3 o'clock.
struct NixieStroke { bool arc; float x0, y0, x1, y1, a0, a1; };
// The strokes of a character's numeral, as one connected wire like a real cathode: count is 0 (and the result nullptr)
// for a blank or a character without a numeral.
const NixieStroke *nixieStrokes(char c, int &count);
// A stroke as a polyline: a line's two ends, or an arc in equal steps of at most kNixieArcStepDeg, from a0 to a1.
constexpr float kNixieArcStepDeg = 20.f;
constexpr int kNixieMaxPoints = 19;  // a full turn: 360 / kNixieArcStepDeg steps, plus one
struct NixiePoint { float x, y; };
int nixiePolyline(const NixieStroke &s, NixiePoint (&out)[kNixieMaxPoints]);
} // namespace gauge
