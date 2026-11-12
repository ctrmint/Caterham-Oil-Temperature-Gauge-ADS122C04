#pragma once
#include "core/DisplayText.h"
#include "core/SweepModel.h"
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
