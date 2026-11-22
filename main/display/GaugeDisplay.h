#pragma once
#include "core/DisplayText.h"
#include "core/LayoutChoice.h"
#include "core/Temperature.h"
#include "display/Layouts.h"
#include "lvgl.h"
#include <atomic>
#include <stdint.h>

class GaugeDisplay {
 public:
  bool begin(gauge::Unit unit = gauge::Unit::Celsius, gauge::Layout layout = gauge::Layout::Classic);
  // trace: the history the Trace layout draws; it must outlive the display (main keeps it for the program's life).
  void render(const gauge::Reading& r, float max_temp_c, bool max_above_range, const gauge::TraceHistory& trace, bool demo);
  // True once per long-press switch, with the new layout. No LVGL call, no lock: safe from the main loop.
  bool takeLayoutChange(gauge::Layout& out);
 private:
  static void onScreenEvent(lv_event_t* e);
  void handleEvent(lv_event_code_t code);
  void showActive();  // with the LVGL mutex held
  ClassicLayout classic_;
  SweepLayout sweep_;
  NeedleLayout needle_;
  TraceLayout trace_;
  TapeLayout tape_;
  OemLayout oem_;
  bool built_ = false;
  gauge::Layout active_ = gauge::Layout::Classic;  // guarded by the LVGL mutex (render and the event callback)
  gauge::LongPress press_;
  std::atomic<uint8_t> pending_{0xFF};  // 0xFF = no switch; else the encoded new layout
  gauge::Unit unit_ = gauge::Unit::Celsius;
  char footer_text_[gauge::kFooterTextSize] = {};
};
