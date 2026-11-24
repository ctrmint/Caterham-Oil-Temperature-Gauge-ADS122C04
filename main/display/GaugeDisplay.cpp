#include "display/GaugeDisplay.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "core/Calibration.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_lv_adapter_display.h"
#include "esp_lv_adapter_input.h"
#include "esp_memory_utils.h"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace layout;

namespace {
constexpr const char* TAG = "oil_display";
// LVGL's two draw buffers, each this many lines of the screen, in internal DMA-capable RAM: 2 x 466 x 20 x 2 bytes, about
// 37 KB, taken once at start-up before Wi-Fi. Even, for the CO5300's 2-pixel areas.
constexpr uint16_t kDrawLines = 20;
static_assert(kDrawLines % 2 == 0, "the CO5300 draws areas in whole pairs of lines");

// The CO5300 takes areas that start on an even pixel and end on an odd one (as the BSP's own rounder).
void roundArea(lv_event_t* e) {
  lv_area_t* a = static_cast<lv_area_t*>(lv_event_get_param(e));
  a->x1 = (a->x1 >> 1) << 1;
  a->y1 = (a->y1 >> 1) << 1;
  a->x2 = ((a->x2 >> 1) << 1) + 1;
  a->y2 = ((a->y2 >> 1) << 1) + 1;
}

// bsp_display_start(), with one change. The BSP puts LVGL's draw buffers in PSRAM, which the SPI bus can't send from, so
// for every frame the SPI driver borrows a ~46 KB block of internal DMA RAM to copy through. Once Wi-Fi (and the web
// server) had fragmented internal RAM, no such block was left: every frame failed with ESP_ERR_NO_MEM, the screen froze
// and touch stopped with it, until a reboot. Here the draw buffers are internal and DMA-capable, taken once before
// Wi-Fi starts, so drawing never allocates. The rest is as the BSP does it: panel, touch, brightness, the LVGL task.
lv_display_t* startDisplay() {
  bsp_display_cfg_t cfg = {};
  cfg.lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
  cfg.rotation = ESP_LV_ADAPTER_ROTATE_0;
  cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
  cfg.touch_flags.swap_xy = 0;
  cfg.touch_flags.mirror_x = 1;
  cfg.touch_flags.mirror_y = 1;
  if (esp_lv_adapter_init(&cfg.lv_adapter_cfg) != ESP_OK) return nullptr;
  bsp_display_config_t panel_cfg = {};
  panel_cfg.max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * BSP_LCD_BITS_PER_PIXEL / 8;
  esp_lcd_panel_handle_t panel = nullptr;
  esp_lcd_panel_io_handle_t io = nullptr;
  if (bsp_display_new(&panel_cfg, &panel, &io) != ESP_OK) return nullptr;
  esp_lv_adapter_display_config_t disp_cfg = {};
  disp_cfg.panel = panel;
  disp_cfg.panel_io = io;
  disp_cfg.profile.interface = ESP_LV_ADAPTER_PANEL_IF_OTHER;
  disp_cfg.profile.rotation = cfg.rotation;
  disp_cfg.profile.hor_res = BSP_LCD_H_RES;
  disp_cfg.profile.ver_res = BSP_LCD_V_RES;
  disp_cfg.profile.buffer_height = kDrawLines;
  disp_cfg.profile.use_psram = false;  // the change: internal RAM, so the SPI bus sends straight from it
  disp_cfg.profile.enable_ppa_accel = false;
  disp_cfg.profile.require_double_buffer = true;
  disp_cfg.tear_avoid_mode = cfg.tear_avoid_mode;
  lv_display_t* disp = esp_lv_adapter_register_display(&disp_cfg);
  if (!disp) return nullptr;
  lv_display_add_event_cb(disp, &roundArea, LV_EVENT_INVALIDATE_AREA, nullptr);
  const lv_draw_buf_t* buf = lv_display_get_buf_active(disp);
  if (!buf || !esp_ptr_dma_capable(buf->data)) {
    ESP_LOGW(TAG, "Draw buffer isn't DMA-capable: the SPI driver will copy each frame");
  }
  esp_lcd_touch_handle_t touch = nullptr;
  if (bsp_touch_new(&cfg, &touch) != ESP_OK || !touch) return nullptr;
  const esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, touch);
  if (!esp_lv_adapter_register_touch(&touch_cfg)) return nullptr;
  if (bsp_display_brightness_init() != ESP_OK) return nullptr;
  if (esp_lv_adapter_start() != ESP_OK) return nullptr;
  ESP_LOGI(TAG, "Draw buffers: 2 x %u lines in internal RAM; internal free %u, largest DMA block %u",
           static_cast<unsigned>(kDrawLines), static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
           static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));
  return disp;
}
} // namespace

bool GaugeDisplay::begin(gauge::Unit unit, gauge::Layout layout) {
  unit_ = unit;
  // The QSPI CO5300 display, the CST9217 touch and the LVGL task, as the vendor BSP starts them, but with the draw
  // buffers in internal RAM (startDisplay, above).
  if (startDisplay() == nullptr) return false;
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
  warmup_.build(root, unit_);
  buildPanel();
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
  warmup_.setVisible(active_ == gauge::Layout::Warmup);
}

bool GaugeDisplay::takeLayoutChange(gauge::Layout& out) {
  const uint8_t v = pending_.exchange(0xFF);
  if (v == 0xFF) return false;
  out = gauge::decodeLayout(true, v);
  return true;
}

void GaugeDisplay::render(const gauge::Reading& r, float max_temp_c, bool max_above_range,
                          const gauge::TraceHistory& trace, const gauge::BandTimes& bands,
                          const gauge::WarmupClock& warmup, const WifiPanel& panel, bool demo) {
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
  f.warmup_ring = gauge::warmupRing(r);
  gauge::warmupText(warmup, warmup_text_, sizeof(warmup_text_));
  f.warmup_text = warmup_text_;
  f.warm = warmup.warm();
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
  warmup_.render(f);
  renderPanel(panel);
  bsp_display_unlock();
}

// The Wi-Fi panel (#34), on LVGL's top layer above every layout, in the band panel:: reserves across the top of the screen.
void GaugeDisplay::buildPanel() {
  panel_ = lv_obj_create(lv_layer_top());
  lv_obj_set_size(panel_, panel::kW, panel::kH);
  lv_obj_set_pos(panel_, (466 - panel::kW) / 2, panel::kTop);
  lv_obj_set_style_bg_color(panel_, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(panel_, panel::kRadius, 0);
  lv_obj_set_style_border_width(panel_, 1, 0);
  lv_obj_set_style_border_color(panel_, lv_color_hex(0x2A3640), 0);
  lv_obj_set_style_pad_all(panel_, 0, 0);
  const lv_obj_flag_t off = static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(panel_, off);
  auto line = [&](int y, int h, const lv_font_t* font, uint32_t colour, char* text) {
    lv_obj_t* l = lv_label_create(panel_);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_size(l, panel::kW - 16, h);
    lv_obj_set_pos(l, 8, y);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
    lv_label_set_text_static(l, text);
    lv_obj_remove_flag(l, off);
    return l;
  };
  panel_line1_ = line(7, 18, &lv_font_montserrat_16, kWhite, panel_text1_);
  panel_line2_ = line(29, 16, &lv_font_montserrat_14, kMute, panel_text2_);
  lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
  panel_shown_ = false;
  panel_warn_ = false;
}

void GaugeDisplay::renderPanel(const WifiPanel& p) {
  if (!panel_) return;
  if (p.visible != panel_shown_) {
    panel_shown_ = p.visible;
    if (panel_shown_) lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
  }
  if (!panel_shown_) return;
  const char* l1 = p.line1 ? p.line1 : "";
  const char* l2 = p.line2 ? p.line2 : "";
  if (std::strncmp(panel_text1_, l1, sizeof(panel_text1_)) != 0) {
    std::snprintf(panel_text1_, sizeof(panel_text1_), "%s", l1);
    lv_label_set_text_static(panel_line1_, panel_text1_);
  }
  if (std::strncmp(panel_text2_, l2, sizeof(panel_text2_)) != 0) {
    std::snprintf(panel_text2_, sizeof(panel_text2_), "%s", l2);
    lv_label_set_text_static(panel_line2_, panel_text2_);
  }
  if (p.warn != panel_warn_) {
    panel_warn_ = p.warn;
    lv_obj_set_style_text_color(panel_line1_, lv_color_hex(p.warn ? kAmber : kWhite), 0);
  }
}
