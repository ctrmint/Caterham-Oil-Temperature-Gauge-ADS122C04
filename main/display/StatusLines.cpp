// Widgets shared between layouts: the status lines, the LED at 6 o'clock and the main reading's digits.
#include "display/Layouts.h"
#include <cstdio>
#include <cstring>

extern "C" { LV_FONT_DECLARE(gauge_digits_148) }

using namespace layout;

namespace {
// Static one-character strings, so the digit cells never allocate. Index i is kCellChars[i].
constexpr char kCellChars[] = " 0123456789-<>";
const char* const kCellText[] = {" ", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "-", "<", ">"};
static_assert(sizeof(kCellChars) - 1 == sizeof(kCellText) / sizeof(kCellText[0]), "kCellChars and kCellText must match");
const char* cellText(char ch) {
  const char* p = strchr(kCellChars, ch);
  return (ch != '\0' && p) ? kCellText[p - kCellChars] : nullptr;
}
// The "--" form: right-aligned dashes, blanks to their left.
const char* dashForm(size_t i, size_t n) { return (i + 2 >= n) ? cellText('-') : cellText(' '); }

lv_obj_t* centred_line(lv_obj_t* parent, int w, int y, uint32_t colour) {
  lv_obj_t* l = lv_label_create(parent);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_obj_set_size(l, w, lines::kTextH);
  lv_obj_set_pos(l, lines::kCentre - w / 2, y);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(colour), 0);
  return l;
}
} // namespace

void StatusLines::build(lv_obj_t* parent, int status_y) {
  status_ = centred_line(parent, lines::kStatusW, status_y, kAmber);
  lv_label_set_text_static(status_, "SENSOR CHECK");
  footer_ = centred_line(parent, lines::kStatusW, status_y + lines::kGap, kMute);
  std::snprintf(footer_text_, sizeof(footer_text_), "NOT CALIBRATED");
  lv_label_set_text_static(footer_, footer_text_);
  demo_ = centred_line(parent, lines::kDemoW, status_y + 2 * lines::kGap, kRed);
  lv_label_set_text_static(demo_, "DEMO ONLY - NOT REAL");
  lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  status_text_ = nullptr;
  status_colour_ = kAmber;
  demo_shown_ = false;
}

void StatusLines::render(const DisplayFrame& f) {
  if (!status_) return;
  if (f.status != status_text_) { status_text_ = f.status; lv_label_set_text_static(status_, f.status); }
  if (f.colour != status_colour_) {
    status_colour_ = f.colour;
    lv_obj_set_style_text_color(status_, lv_color_hex(f.colour), 0);
  }
  if (std::strncmp(footer_text_, f.footer, sizeof(footer_text_)) != 0) {
    std::snprintf(footer_text_, sizeof(footer_text_), "%s", f.footer);
    lv_label_set_text_static(footer_, footer_text_);
  }
  if (f.demo != demo_shown_) {
    demo_shown_ = f.demo;
    if (f.demo) lv_obj_remove_flag(demo_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  }
}

void AlarmLed::build(lv_obj_t* parent, int centre_y) {
  obj_ = lv_obj_create(parent);
  lv_obj_set_size(obj_, led::kD, led::kD);
  lv_obj_set_pos(obj_, lines::kCentre - led::kD / 2, centre_y - led::kD / 2);
  lv_obj_set_style_radius(obj_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(obj_, 0, 0);
  lv_obj_set_style_pad_all(obj_, 0, 0);
  lv_obj_set_style_bg_opa(obj_, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(obj_, led::kGlow, 0);
  lv_obj_set_style_shadow_spread(obj_, 2, 0);
  lv_obj_set_style_shadow_opa(obj_, LV_OPA_COVER, 0);
  lv_obj_add_flag(obj_, LV_OBJ_FLAG_HIDDEN);
  lit_ = false;
  colour_ = 0;
  pulse_ = gauge::LedPulse::None;
  timer_ = lv_timer_create(&AlarmLed::onTimer, led::kTickMs, this);
  lv_timer_pause(timer_);
}

// Shown and coloured as ledState() says; the timer animates the pulse.
void AlarmLed::render(const gauge::LedState& s) {
  if (!obj_) return;
  if (s.lit != lit_) {
    lit_ = s.lit;
    if (lit_) lv_obj_remove_flag(obj_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj_, LV_OBJ_FLAG_HIDDEN);
  }
  if (s.lit && s.colour != colour_) {
    colour_ = s.colour;
    lv_obj_set_style_bg_color(obj_, lv_color_hex(colour_), 0);
    lv_obj_set_style_shadow_color(obj_, lv_color_hex(colour_), 0);
  }
  const gauge::LedPulse pulse = s.lit ? s.pulse : gauge::LedPulse::None;
  if (pulse != pulse_) {
    pulse_ = pulse;
    if (pulse == gauge::LedPulse::None) {
      lv_timer_pause(timer_);
      lv_obj_set_style_opa(obj_, LV_OPA_COVER, 0);
    } else {
      lv_timer_resume(timer_);
    }
  }
}

void AlarmLed::onTimer(lv_timer_t* t) {
  AlarmLed* self = static_cast<AlarmLed*>(lv_timer_get_user_data(t));
  if (!self || !self->obj_) return;
  lv_obj_set_style_opa(self->obj_, gauge::ledOpacity(self->pulse_, lv_tick_get()), 0);
}

void ReadingDigits::build(lv_obj_t* parent) {
  // Equal-width cells: the widest digit-font glyph of the reading characters.
  const lv_font_t* digit_font = &gauge_digits_148;
  int32_t cell_w = 0;
  for (const char* p = kCellChars + 1; *p; ++p) {
    const int32_t w = static_cast<int32_t>(lv_font_get_glyph_width(digit_font, static_cast<uint32_t>(*p), 0));
    if (w > cell_w) cell_w = w;
  }
  const int32_t n = static_cast<int32_t>(gauge::kReadingCells);
  cell_w_ = cell_w;
  for (int32_t i = 0; i < n; ++i) {
    lv_obj_t* cell = lv_label_create(parent);
    lv_label_set_long_mode(cell, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(cell, cell_w);
    lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(cell, digit_font, 0);
    lv_obj_set_style_text_color(cell, lv_color_hex(kWhite), 0);
    // Start as the right-aligned "--" form.
    lv_label_set_text_static(cell, dashForm(static_cast<size_t>(i), static_cast<size_t>(n)));
    cells_[i] = cell;
  }
  place(false);
}

// Up to three characters: the right three cells are centred, so a 3-digit reading sits in the middle of the screen and the
// ones digit stays put. Four characters (>284, 4-digit ohms) use all four cells, centred, which moves the ones digit half
// a cell to the right. The offsets are whole cells from one left edge, so an odd cell width still gives an even pitch.
void ReadingDigits::place(bool wide) {
  const int32_t n = static_cast<int32_t>(gauge::kReadingCells);
  const int32_t left = wide ? -((n - 1) * cell_w_) / 2 : -(n * cell_w_) / 2;
  for (int32_t i = 0; i < n; ++i) lv_obj_align(cells_[i], LV_ALIGN_CENTER, left + i * cell_w_, 0);
  wide_ = wide;
}

void ReadingDigits::render(const char* cells, uint32_t colour) {
  if (!cells_[0]) return;
  // Fail closed: if any cell is not displayable, show the whole reading as "--".
  const char* text[gauge::kReadingCells];
  bool ok = true;
  for (size_t i = 0; i < gauge::kReadingCells; ++i) {
    text[i] = cellText(cells[i]);
    if (!text[i]) ok = false;
  }
  const bool wide = ok && cells[0] != ' ';
  if (wide != wide_) place(wide);
  for (size_t i = 0; i < gauge::kReadingCells; ++i) {
    const char* t = text[i];
    if (!ok) t = dashForm(i, gauge::kReadingCells);
    lv_label_set_text_static(cells_[i], t);
    lv_obj_set_style_text_color(cells_[i], lv_color_hex(colour), 0);
  }
}
