#include "display/GaugeDisplay.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "core/Calibration.h"
#include "esp_err.h"
#include <cmath>

using namespace layout;

bool GaugeDisplay::begin(gauge::Unit unit, gauge::Layout layout) {
  unit_ = unit;
  // Vendor BSP initializes QSPI CO5300 display, CST9217 touch and LVGL task.
  // BSP display_start is the exact API used by Waveshare's IDF v9 demo.
  if (bsp_display_start() == nullptr) return false;
  // Waveshare BSP 3.x reports esp_err_t, not a bool: ESP_OK == 0.
  if (bsp_display_lock(2000) != ESP_OK) return false;
  lv_obj_t* root = lv_screen_active();
  lv_obj_set_style_bg_color(root, lv_color_hex(kBg), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  classic_.build(root, unit_);
  sweep_.build(root, unit_);
  needle_.build(root, unit_);
  trace_.build(root, unit_);
  tape_.build(root, unit_);
  oem_.build(root, unit_);
  white_.build(root, unit_);
  night_.build(root, unit_);
  nixie_.build(root, unit_);
  heritage_.build(root, unit_);
  track_.build(root, unit_);
  active_ = layout;
  showActive();
  lv_obj_add_event_cb(root, &GaugeDisplay::onScreenEvent, LV_EVENT_ALL, this);
  built_ = true;
  bsp_display_unlock();
  return true;
}

// Runs in the LVGL task, which already holds the LVGL mutex: no bsp_display_lock, no NVS, no blocking, no allocation.
void GaugeDisplay::onScreenEvent(lv_event_t* e) {
  GaugeDisplay* self = static_cast<GaugeDisplay*>(lv_event_get_user_data(e));
  if (self) self->handleEvent(lv_event_get_code(e));
}

void GaugeDisplay::handleEvent(lv_event_code_t code) {
  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) { press_.release(); return; }
  if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) return;
  lv_point_t p = {0, 0};
  lv_indev_get_point(lv_indev_active(), &p);
  const uint32_t now = lv_tick_get();
  if (code == LV_EVENT_PRESSED) { press_.press(p.x, p.y, now); return; }
  if (!press_.hold(p.x, p.y, now)) return;
  active_ = gauge::nextLayout(active_);
  showActive();
  pending_.store(gauge::encodeLayout(active_));
}

void GaugeDisplay::showActive() {
  classic_.setVisible(active_ == gauge::Layout::Classic);
  sweep_.setVisible(active_ == gauge::Layout::Sweep);
  needle_.setVisible(active_ == gauge::Layout::Needle);
  trace_.setVisible(active_ == gauge::Layout::Trace);
  tape_.setVisible(active_ == gauge::Layout::Tape);
  oem_.setVisible(active_ == gauge::Layout::Oem);
  white_.setVisible(active_ == gauge::Layout::White);
  night_.setVisible(active_ == gauge::Layout::Night);
  nixie_.setVisible(active_ == gauge::Layout::Nixie);
  heritage_.setVisible(active_ == gauge::Layout::Heritage);
  track_.setVisible(active_ == gauge::Layout::Track);
}

bool GaugeDisplay::takeLayoutChange(gauge::Layout& out) {
  const uint8_t v = pending_.exchange(0xFF);
  if (v == 0xFF) return false;
  out = gauge::decodeLayout(true, v);
  return true;
}

void GaugeDisplay::render(const gauge::Reading& r, float max_temp_c, bool max_above_range,
                          const gauge::TraceHistory& trace, const gauge::BandTimes& bands, bool demo) {
  if (!built_ || bsp_display_lock(100) != ESP_OK) return;
  DisplayFrame f;
  f.colour = gauge::readingColour(r);  // graduated, never less severe than the latched alarm
  if (r.has_temperature) {
    f.classic_arc = static_cast<int>(fmaxf(0.0f,fminf(160.0f,r.temperature_c)));
  } else if (r.status == gauge::Status::AboveRange) {
    f.classic_arc = 160;
  } else {
    f.classic_arc = 0;
  }
  f.lit_segments = gauge::litSegments(r,unit_);
  f.scale_visible = gauge::scaleLabelsVisible();
  char text[gauge::kReadingTextSize];
  gauge::readingText(r,unit_,text,sizeof(text));
  gauge::readingCells(text,f.cells,sizeof(f.cells));
  gauge::sweepReading(text,f.sweep_sign,f.sweep_cells,sizeof(f.sweep_cells));
  gauge::footerText(r,max_temp_c,max_above_range,unit_,footer_text_,sizeof(footer_text_));
  f.title = gauge::titleText(unit_);
  f.unit = gauge::unitLabel(unit_);
  f.status = gauge::statusText(r.status);
  f.footer = footer_text_;
  f.demo = demo;
  f.led = gauge::ledState(r);
  f.needle = gauge::needleState(r, unit_);
  f.trace = &trace;
  f.tape = gauge::tapeState(r);
  f.rate = gauge::traceRate(trace);
  f.max_shown = gauge::calibrationValid() && (max_above_range || max_temp_c > gauge::kNoMaxYetC);
  f.max_c = max_above_range ? gauge::calibrationMaxC() : max_temp_c;
  f.oem_needle = gauge::oemNeedle(r);
  f.wordmark = gauge::oemWordmark(r, demo);
  f.white_needle = gauge::whiteNeedle(r);
  f.status_window = gauge::whiteWindow(r, demo);
  f.night_quiet = gauge::nightQuiet(r, demo);
  f.heritage_needle = gauge::heritageNeedle(r);
  f.heritage_legend = gauge::heritageLegend(r, demo);
  f.bands = &bands;
  gauge::maxText(max_temp_c, max_above_range, unit_, max_text_, sizeof(max_text_));
  f.max_text = max_text_;
  classic_.render(f);
  sweep_.render(f);
  needle_.render(f);
  trace_.render(f);
  tape_.render(f);
  oem_.render(f);
  white_.render(f);
  night_.render(f);
  nixie_.render(f);
  heritage_.render(f);
  track_.render(f);
  bsp_display_unlock();
}
