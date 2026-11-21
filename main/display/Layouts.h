#pragma once
#include "core/DisplayText.h"
#include "core/SweepModel.h"
#include "core/TapeModel.h"
#include "core/TraceModel.h"
#include "lvgl.h"
#include <stdint.h>

namespace layout {
constexpr uint32_t kBg = 0x080E15;
constexpr uint32_t kWhite = 0xF2F6F8;
constexpr uint32_t kMute = 0x8597A3;
constexpr uint32_t kGreen = gauge::kColourGreen;  // the graduated palette (core/SweepModel.h)
constexpr uint32_t kAmber = gauge::kColourAmber;
constexpr uint32_t kRed = gauge::kColourRed;
} // namespace layout

// Everything a layout needs for one update. All pointers are static or owned by GaugeDisplay.
struct DisplayFrame {
  uint32_t colour = layout::kAmber;
  char cells[gauge::kReadingCells + 1] = {};        // Classic: four right-aligned cells
  char sweep_cells[gauge::kSweepCells + 1] = {};    // Sweep: three right-aligned cells
  char sweep_sign = 0;                              // Sweep: '<', '>' or 0
  const char* title = "";
  const char* unit = "";
  const char* status = "";
  const char* footer = "";
  int classic_arc = 0;
  int lit_segments = 0;
  bool scale_visible = false;
  bool demo = false;
  gauge::LedState led = {false, 0, gauge::LedPulse::None};  // the LED at 6 o'clock (Sweep and Trace)
  gauge::NeedleState needle = {false, 0.f};                 // Needle layout
  const gauge::TraceHistory* trace = nullptr;               // Trace layout; owned by main
  gauge::TapeState tape = {false, 0.f};                     // Tape layout: where the tape is centred
  gauge::Rate rate = {false, 0.f};                          // Tape layout: the rate of change
  bool max_shown = false;                                   // Tape layout: the session maximum's marker, in degC
  float max_c = 0.f;
};

// Every LayoutView method (build(), render() and setVisible() included) must be called with the display lock
// (bsp_display_lock) held, or from the LVGL task's own event callback.
class LayoutView {
 public:
  virtual ~LayoutView() = default;
  virtual void build(lv_obj_t* screen, gauge::Unit unit) = 0;
  virtual void render(const DisplayFrame& f) = 0;
  void setVisible(bool visible) {
    if (!root_) return;
    if (visible) lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
  }
 protected:
  lv_obj_t* root_ = nullptr;
};

// The LED at 6 o'clock (#10): a disc with a glow in the reading's colour, pulsing while an alarm is latched, as ledState()
// says. Shared by Sweep and Trace. Like LayoutView, call it with the display lock held.
namespace led {
constexpr int kD = 28, kGlow = 14;  // diameter and glow; the glow reaches kD / 2 + kGlow from the centre
constexpr uint32_t kTickMs = 40;    // pulse frame period, about 25 frames a second
} // namespace led

class AlarmLed {
 public:
  void build(lv_obj_t* parent, int centre_y);  // centred across the screen
  void render(const gauge::LedState& s);
 private:
  static void onTimer(lv_timer_t* t);  // LVGL task: the pulse's brightness
  lv_obj_t* obj_ = nullptr;
  lv_timer_t* timer_ = nullptr;
  bool lit_ = false;
  uint32_t colour_ = 0;
  gauge::LedPulse pulse_ = gauge::LedPulse::None;
};

class ClassicLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  void placeCells(bool wide);
  lv_obj_t* arc_ = nullptr;
  lv_obj_t* cells_[gauge::kReadingCells] = {};
  int32_t cell_w_ = 0;
  bool wide_ = false;  // true: all four cells centred; false: the right three centred
  lv_obj_t* title_ = nullptr;
  lv_obj_t* subtitle_ = nullptr;
  lv_obj_t* footer_ = nullptr;
  lv_obj_t* demo_ = nullptr;
};

// Sweep geometry on the 466 px screen, for the digit block, scale labels and small texts.
namespace sweep {
constexpr int kCentre = 233;
// Three seven-segment cells of kCellW px; each glyph is kGlyphW wide, kDigitH tall and kThick thick.
constexpr int kCellW = 64, kGlyphW = 52, kThick = 12, kGap = 2;
constexpr int kDigitW = static_cast<int>(gauge::kSweepCells) * kCellW, kDigitH = 116;
constexpr int kDigitCx = 233, kDigitCy = 195;
// '<' / '>' sign to the left of the cells: at most kSignW wide, ink at most kSignInkH tall, kSignGap from the first cell.
constexpr int kSignW = 32, kSignInkH = 36, kSignGap = 6;
constexpr int kMaxLabels = 6;  // 30..130 step 20 degC, 86..266 step 36 degF
} // namespace sweep

class SweepLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  AlarmLed led_;
  int label_count_ = 0;
  bool labels_shown_ = false;
  lv_obj_t* unit_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_obj_t* footer_ = nullptr;
  lv_obj_t* demo_ = nullptr;
  const char* unit_text_ = nullptr;
  const char* status_text_ = nullptr;
  uint32_t status_colour_ = 0;
  bool demo_shown_ = false;
};

// Status, footer and demo banner, as Classic and Sweep show them; shared by the Needle and Trace layouts. Each line is
// centred, kTextH tall and kGap below the one before.
namespace lines {
constexpr int kCentre = 233, kRadius = 233, kTextH = 16, kGap = 20, kStatusW = 220, kDemoW = 180;
constexpr int iabs(int v) { return v < 0 ? -v : v; }
// A point lies on the round screen.
constexpr bool onScreen(int x, int y) {
  return (x - kCentre) * (x - kCentre) + (y - kCentre) * (y - kCentre) <= kRadius * kRadius;
}
// A centred line w wide with its top at y lies on the round screen (its corners are the farthest points).
constexpr bool fits(int w, int y) {
  const int dy = iabs(y - kCentre) > iabs(y + kTextH - kCentre) ? iabs(y - kCentre) : iabs(y + kTextH - kCentre);
  return (w / 2) * (w / 2) + dy * dy <= kRadius * kRadius;
}
// The status, footer and demo lines starting at status_y all lie on the screen.
constexpr bool blockFits(int status_y) {
  return fits(kStatusW, status_y) && fits(kStatusW, status_y + kGap) && fits(kDemoW, status_y + 2 * kGap);
}
} // namespace lines

class StatusLines {
 public:
  void build(lv_obj_t* parent, int status_y);
  void render(const DisplayFrame& f);
 private:
  lv_obj_t* status_ = nullptr;
  lv_obj_t* footer_ = nullptr;
  lv_obj_t* demo_ = nullptr;
  const char* status_text_ = nullptr;
  uint32_t status_colour_ = 0;
  bool demo_shown_ = false;
  char footer_text_[gauge::kFooterTextSize] = {};
};

// Analogue dial: a white 270-degree scale with five numbers, a red needle and a hub. No digital readout.
class NeedleLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  static void onDrawFace(lv_event_t* e);  // LVGL task: the scale arc, ticks and pointers
  static void onTimer(lv_timer_t* t);     // LVGL task: eases the needle to its target, never past it
  void placeNeedle(float fraction);
  lv_obj_t* needle_ = nullptr;
  lv_obj_t* unit_ = nullptr;
  lv_timer_t* timer_ = nullptr;
  StatusLines lines_;
  float shown_ = 0.f;   // the needle's drawn place, 0..1 along the scale
  float target_ = 0.f;
  bool needle_shown_ = false;
  bool labels_shown_ = true;
  const char* unit_text_ = nullptr;
};

// Time-based trace: the last 60 s, one point a second, on a 30 degC window, with the LED at 6 o'clock. No digital readout.
class TraceLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  void drawTrace(const gauge::TraceHistory& h, bool scale_visible);
  gauge::Unit unit_ = gauge::Unit::Celsius;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* dot_ = nullptr;
  StatusLines lines_;
  AlarmLed led_;
  const char* title_text_ = nullptr;
  uint32_t version_ = 0;
  bool drawn_ = false;
  bool scale_shown_ = false;
};

// Moving tape (#15): a scale that scrolls past a fixed pointer, the reading in a window beside it, and the rate of change.
class TapeLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  static void onTimer(lv_timer_t* t);  // LVGL task: eases the tape to its target, never past it
  lv_obj_t* tape_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* value_ = nullptr;
  lv_obj_t* rate_ = nullptr;
  lv_timer_t* timer_ = nullptr;
  StatusLines lines_;
  gauge::Unit unit_ = gauge::Unit::Celsius;
  const char* title_text_ = nullptr;
  uint32_t value_colour_ = 0;
  bool tape_shown_ = false;
  char value_text_[gauge::kReadingCells + 1] = {};
  char rate_text_[gauge::kRateTextSize + 8] = {};  // with the arrow symbol in front
};
