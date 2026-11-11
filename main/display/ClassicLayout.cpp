#include "display/Layouts.h"
#include <cstring>

using namespace layout;

namespace {
void center_text(lv_obj_t* label, int y, const lv_font_t* font, uint32_t color) {
  lv_obj_set_width(label, 360);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, y);
}

// Static one-character strings, so the cell labels never allocate. Index i is kCellChars[i].
constexpr char kCellChars[] = " 0123456789-<>";
const char* const kCellText[] = {" ", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "-", "<", ">"};
const char* cellText(char ch) {
  const char* p = strchr(kCellChars, ch);
  return (ch != '\0' && p) ? kCellText[p - kCellChars] : nullptr;
}
} // namespace

void ClassicLayout::build(lv_obj_t* screen, gauge::Unit unit) {
  root_ = lv_obj_create(screen);
  lv_obj_set_size(root_, 466, 466);
  lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_remove_flag(root_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

  arc_ = lv_arc_create(root_);
  lv_obj_set_size(arc_, 430, 430);
  lv_obj_align(arc_, LV_ALIGN_CENTER, 0, 0);
  lv_arc_set_range(arc_, 0, 160);
  lv_arc_set_rotation(arc_, 135);
  lv_arc_set_bg_angles(arc_, 0, 270);
  lv_arc_set_value(arc_, 0);
  lv_obj_remove_flag(arc_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_style_arc_width(arc_, 18, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_, 18, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(0x22313A), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(kGreen), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(arc_, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);

  title_ = lv_label_create(root_);
  lv_label_set_text_static(title_, gauge::titleText(unit));
  center_text(title_, -105, &lv_font_montserrat_24, kWhite);
  // Equal-width cells: the widest digit-font glyph of the reading characters.
  const lv_font_t* digit_font = &lv_font_montserrat_48;
  int32_t cell_w = 0;
  for (const char* p = kCellChars + 1; *p; ++p) {
    const int32_t w = static_cast<int32_t>(lv_font_get_glyph_width(digit_font, static_cast<uint32_t>(*p), 0));
    if (w > cell_w) cell_w = w;
  }
  const int32_t n = static_cast<int32_t>(gauge::kReadingCells);
  // Left offset of the group once, then exactly cell_w per cell, so an odd width still gives an even pitch.
  const int32_t left = -((n - 1) * cell_w) / 2;
  for (int32_t i = 0; i < n; ++i) {
    lv_obj_t* cell = lv_label_create(root_);
    lv_label_set_long_mode(cell, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(cell, cell_w);
    lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(cell, digit_font, 0);
    lv_obj_set_style_text_color(cell, lv_color_hex(kWhite), 0);
    lv_obj_align(cell, LV_ALIGN_CENTER, left + i * cell_w, -26);
    // Start as the right-aligned "--" form.
    lv_label_set_text_static(cell, i + 2 >= n ? kCellText[11] : kCellText[0]);
    cells_[i] = cell;
  }
  subtitle_ = lv_label_create(root_);
  lv_label_set_text(subtitle_, "SENSOR CHECK");
  center_text(subtitle_, 44, &lv_font_montserrat_16, kAmber);
  footer_ = lv_label_create(root_);
  lv_label_set_text(footer_, "NOT CALIBRATED");
  center_text(footer_, 110, &lv_font_montserrat_16, kMute);
  demo_ = lv_label_create(root_);
  lv_label_set_text(demo_, "DEMO ONLY - NOT REAL");
  center_text(demo_, -165, &lv_font_montserrat_16, kRed);
  lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  // Presses must reach the screen: no child may be clickable or scrollable.
  const uint32_t count = lv_obj_get_child_count(root_);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_remove_flag(lv_obj_get_child(root_, static_cast<int32_t>(i)),
                       static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  }
}

void ClassicLayout::render(const DisplayFrame& f) {
  if (!root_) return;
  lv_arc_set_value(arc_, f.classic_arc);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(f.colour), LV_PART_INDICATOR);
  lv_label_set_text_static(title_, f.title);
  // Fail closed: if any cell is not displayable, show the whole reading as "--".
  const char* text[gauge::kReadingCells];
  bool ok = true;
  for (size_t i = 0; i < gauge::kReadingCells; ++i) {
    text[i] = cellText(f.cells[i]);
    if (!text[i]) ok = false;
  }
  for (size_t i = 0; i < gauge::kReadingCells; ++i) {
    const char* t = text[i];
    if (!ok) t = (i + 2 >= gauge::kReadingCells) ? kCellText[11] : kCellText[0];
    lv_label_set_text_static(cells_[i], t);
    lv_obj_set_style_text_color(cells_[i], lv_color_hex(f.colour), 0);
  }
  lv_label_set_text_static(footer_, f.footer);
  lv_label_set_text_static(subtitle_, f.status);
  lv_obj_set_style_text_color(subtitle_, lv_color_hex(f.colour), 0);
  if (f.demo) lv_obj_remove_flag(demo_, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(demo_, LV_OBJ_FLAG_HIDDEN);
}
