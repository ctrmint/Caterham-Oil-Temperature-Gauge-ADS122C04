#pragma once
#include "core/DisplayText.h"
#include "core/SweepModel.h"
#include "lvgl.h"
#include <stdint.h>

namespace layout {
constexpr uint32_t kBg = 0x080E15;
constexpr uint32_t kWhite = 0xF2F6F8;
constexpr uint32_t kMute = 0x8597A3;
constexpr uint32_t kGreen = 0x24CA89;
constexpr uint32_t kAmber = 0xF4B246;
constexpr uint32_t kRed = 0xF04B4B;
} // namespace layout

// Everything a layout needs for one update. All pointers are static or owned by GaugeDisplay.
struct DisplayFrame {
  uint32_t colour = layout::kGreen;
  char cells[gauge::kReadingCells + 1] = {};
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
  lv_obj_t* arc_ = nullptr;
  lv_obj_t* cells_[gauge::kReadingCells] = {};
  lv_obj_t* title_ = nullptr;
  lv_obj_t* subtitle_ = nullptr;
  lv_obj_t* footer_ = nullptr;
  lv_obj_t* demo_ = nullptr;
};

// Sweep geometry on the 466 px screen, shared with the digit renderer. The digit area is reserved and empty here.
namespace sweep {
constexpr int kCentre = 233;
constexpr int kDigitW = 224, kDigitH = 80;
constexpr int kDigitCx = 233, kDigitCy = 215;
constexpr int kMaxLabels = 11;  // 100..300 step 20 in Fahrenheit
} // namespace sweep

class SweepLayout : public LayoutView {
 public:
  void build(lv_obj_t* screen, gauge::Unit unit) override;
  void render(const DisplayFrame& f) override;
 private:
  lv_point_precise_t pts_[gauge::kSweepSegments][2] = {};
  lv_obj_t* seg_[gauge::kSweepSegments] = {};
  uint32_t seg_colour_[gauge::kSweepSegments] = {};
  lv_obj_t* label_[sweep::kMaxLabels] = {};
  char label_text_[sweep::kMaxLabels][8] = {};
  int label_count_ = 0;
  bool labels_shown_ = false;
  lv_obj_t* unit_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_obj_t* footer_ = nullptr;
  lv_obj_t* demo_ = nullptr;
  const char* unit_text_ = nullptr;
  const char* title_text_ = nullptr;
  const char* status_text_ = nullptr;
  uint32_t status_colour_ = 0;
  bool demo_shown_ = false;
  char footer_text_[gauge::kFooterTextSize] = {};
};
