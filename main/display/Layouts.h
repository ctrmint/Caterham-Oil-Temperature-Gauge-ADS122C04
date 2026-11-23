#pragma once
#include "core/DisplayText.h"
#include "core/NightModel.h"
#include "core/NixieModel.h"
#include "core/OemModel.h"
#include "core/SweepModel.h"
#include "core/TapeModel.h"
#include "core/TraceModel.h"
#include "core/WhiteModel.h"
#include "lvgl.h"
#include <stdint.h>

namespace layout {
// Pure black, so the AMOLED's unused pixels are switched off. The background of every layout except White's face.
constexpr uint32_t kBg = 0x000000;
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
  gauge::NeedleState oem_needle = {false, 0.f};             // OEM layout: the needle on its 50-130 degC span
  bool wordmark = false;                                    // OEM layout: CATERHAM, or the status lines in its place
  gauge::NeedleState white_needle = {false, 0.f};           // White layout: the needle on its 40-140 degC span
  bool status_window = true;                                // White layout: the status window, shown unless all is well
  bool night_quiet = false;                                 // Night layout: dark while all is well, fully lit otherwise
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
// says. Shared by Sweep, Trace, White, Night and Nixie. Like LayoutView, call it with the display lock held.
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

// The main reading in the 148 px digit font, as Classic shows it; shared by Classic and Night. kReadingCells equal-width
// cells, each as wide as the widest reading character. Up to three characters fill the right three cells, centred, so a
// 3-digit reading sits in the middle and the ones digit stays put; four use all four cells, centred. Fails closed: a
// reading with any character it can't show becomes "--". Like LayoutView, call it with the display lock held.
namespace digits {
// The widest glyph of the 148 px font (99 px), and its glyph box height (box_h 104-106, line_height 106, base_line 1).
constexpr int kCellMax = 99, kGlyphH = 106;
constexpr int kBlockW = static_cast<int>(gauge::kReadingCells) * kCellMax;
} // namespace digits

class ReadingDigits {
 public:
  void build(lv_obj_t* parent);  // centred on parent, starting as "--"
  void render(const char* cells, uint32_t colour);  // cells: a readingCells() string, kReadingCells characters
 private:
  void place(bool wide);
  lv_obj_t* cells_[gauge::kReadingCells] = {};
  int32_t cell_w_ = 0;
  bool wide_ = false;  // true: all four cells centred; false: the right three centred
};

class ClassicLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  lv_obj_t* arc_ = nullptr;
  ReadingDigits digits_;
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

// Status, footer and demo banner, as Classic and Sweep show them; shared by the Needle, Trace, Tape, OEM, White, Night and
// Nixie layouts.
// Each line is centred, kTextH tall and kGap below the one before.
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

// OEM dial (#17), styled on the Caterham factory gauge: a black face in a dark rim, a tick every 10 degC from 50 to 130,
// four numbers, an off-white needle with a counterweight under a black hub, the oil-temperature symbol as the 6 o'clock
// LED, and CATERHAM, which gives way to the status lines whenever anything needs attention. No digital readout.
class OemLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  static void onDrawFace(lv_event_t* e);     // LVGL task: rim, face and ticks
  static void onDrawNeedle(lv_event_t* e);   // LVGL task: the counterweight and the needle
  static void onDrawSymbol(lv_event_t* e);   // LVGL task: the oil can and thermometer, in the LED's colour
  static void onNeedleTimer(lv_timer_t* t);  // LVGL task: eases the needle to its target, never past it
  static void onPulseTimer(lv_timer_t* t);   // LVGL task: the symbol's pulse
  void placeNeedle(float fraction);
  void renderSymbol(const gauge::LedState& s);
  lv_obj_t* needle_ = nullptr;
  lv_obj_t* symbol_ = nullptr;
  lv_obj_t* unit_ = nullptr;
  lv_obj_t* word_ = nullptr;  // the wordmark
  lv_obj_t* text_ = nullptr;  // the status lines, shown in the wordmark's place
  lv_obj_t* numbers_[gauge::kOemNumbers] = {};
  lv_timer_t* needle_timer_ = nullptr;
  lv_timer_t* pulse_timer_ = nullptr;
  StatusLines lines_;
  char number_text_[gauge::kOemNumbers][8] = {};
  lv_point_precise_t needle_pts_[3] = {};  // the needle's triangle, relative to needle_: two base corners and the tip
  lv_point_precise_t tail_pts_[2] = {};    // the counterweight's bar, relative to needle_
  int needle_key_ = -1;                    // the tip's pixel, to skip redundant redraws
  float shown_ = 0.f;                      // the needle's drawn place, 0..1 along the scale
  float target_ = 0.f;
  bool needle_shown_ = false;
  bool labels_shown_ = true;
  bool word_shown_ = true;
  const char* unit_text_ = nullptr;
  uint32_t symbol_colour_ = 0;
  uint8_t symbol_opa_ = 255;
  gauge::LedPulse symbol_pulse_ = gauge::LedPulse::None;
};

// White dial (#19), styled on a white-faced aftermarket gauge: a white face in a black ring, a tick every 2 degC from 40 to
// 140, six numbers, a red needle with a counterweight under a black hub, OIL and the unit under the hub, the 6 o'clock LED
// in a black socket, and a black status window that shows only when something needs attention. No digital readout.
class WhiteLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  static void onDrawFace(lv_event_t* e);    // LVGL task: ring, face and ticks
  static void onDrawNeedle(lv_event_t* e);  // LVGL task: the counterweight and the needle
  static void onTimer(lv_timer_t* t);       // LVGL task: eases the needle to its target, never past it
  void placeNeedle(float fraction);
  lv_obj_t* needle_ = nullptr;
  lv_obj_t* unit_ = nullptr;
  lv_obj_t* window_ = nullptr;  // the black window and the status lines in it
  lv_obj_t* numbers_[gauge::kWhiteNumbers] = {};
  lv_timer_t* timer_ = nullptr;
  AlarmLed led_;
  StatusLines lines_;
  char number_text_[gauge::kWhiteNumbers][8] = {};
  lv_point_precise_t needle_pts_[3] = {};  // the needle's triangle, relative to needle_: two base corners and the tip
  lv_point_precise_t tail_pts_[2] = {};    // the counterweight's bar, relative to needle_
  int needle_key_ = -1;                    // the tip's pixel, to skip redundant redraws
  float shown_ = 0.f;                      // the needle's drawn place, 0..1 along the scale
  float target_ = 0.f;
  bool needle_shown_ = false;
  bool labels_shown_ = true;
  bool window_shown_ = true;
  const char* unit_text_ = nullptr;
};

// Night (#21): a dark layout on pure black. While all is well (nightQuiet()) it shows only a small, dim reading, the unit
// and a heartbeat dot that steps once per update (nightBeat()); anything else lights the screen with Classic's digits,
// OIL and the unit above them, the status lines and the LED. The reading is at the screen's centre in both states.
class NightLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  lv_obj_t* quiet_ = nullptr;    // the quiet state's group
  lv_obj_t* alert_ = nullptr;    // the alert state's group
  lv_obj_t* reading_ = nullptr;  // quiet: the dim reading
  lv_obj_t* unit_ = nullptr;     // quiet: the dim unit
  lv_obj_t* beat_ = nullptr;     // quiet: the heartbeat dot
  lv_obj_t* title_ = nullptr;    // alert: OIL and the unit
  ReadingDigits digits_;
  StatusLines lines_;
  AlarmLed led_;
  uint32_t frame_ = 0;  // display updates so far: the heartbeat's phase
  uint32_t colour_ = 0;
  bool quiet_shown_ = false;
  const char* unit_text_ = nullptr;
  const char* title_text_ = nullptr;
  char reading_text_[gauge::kReadingCells + 1] = {};
};

// Nixie (#26): the reading in four fixed nixie tubes, right-aligned as Classic's cells (nixieTubes()). The numerals are drawn
// from strokes (nixieStrokes()) and always glow nixie orange, with the ten unlit numerals faint behind them. OIL and the
// unit above, and the status lines and the LED below, are always shown: on this layout they carry the reading's colour.
class NixieLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  static void onDrawTube(lv_event_t* e);  // LVGL task: the glass, the mesh, the unlit numerals and the lit one
  lv_obj_t* tubes_[gauge::kNixieTubes] = {};
  lv_obj_t* title_ = nullptr;
  StatusLines lines_;
  AlarmLed led_;
  char shown_[gauge::kNixieTubes] = {' ', ' ', ' ', ' '};  // each tube's character, read by onDrawTube
  const char* title_text_ = nullptr;
};
