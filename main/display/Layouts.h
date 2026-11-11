#pragma once
#include "core/DisplayText.h"
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
  bool demo = false;
};

// build() and render() must be called with the display lock (bsp_display_lock) held.
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
