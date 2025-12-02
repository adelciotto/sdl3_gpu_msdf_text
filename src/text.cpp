static constexpr int TEXT_INDICES_PER_GLYPH = 6;

enum Text_H_Align {
  TEXT_H_ALIGN_LEFT,
  TEXT_H_ALIGN_CENTER,
  TEXT_H_ALIGN_RIGHT,
  TEXT_H_ALIGN_COUNT,
};

enum Text_V_Align {
  TEXT_V_ALIGN_TOP,
  TEXT_V_ALIGN_MIDDLE,
  TEXT_V_ALIGN_BASELINE,
  TEXT_V_ALIGN_BOTTOM,
  TEXT_V_ALIGN_COUNT,
};

struct Text_Align {
  Text_H_Align horizontal = TEXT_H_ALIGN_LEFT;
  Text_V_Align vertical   = TEXT_V_ALIGN_TOP;
};

struct Text_Style {
  HMM_Vec4 color             = HMM_V4(0.0f, 0.0f, 0.0f, 1.0f);
  HMM_Vec4 outline_color     = HMM_V4(1.0f, 1.0f, 1.0f, 1.0f);
  float    outline_thickness = 0.0f;
};

static float text_measure_width(
    const Font_Atlas_Variant& font_atlas_variant,
    std::string_view          text,
    float                     size) {
  float       width          = 0.0f;
  const char* ptr            = text.data();
  auto        str_size       = text.size();
  int         codepoint      = SDL_INVALID_UNICODE_CODEPOINT;
  int         prev_codepoint = 0;
  while (codepoint != 0) {
    codepoint = SDL_StepUTF8(&ptr, &str_size);
    if (codepoint == SDL_INVALID_UNICODE_CODEPOINT) { continue; }

    auto glyph_it = font_atlas_variant.glyphs.find(codepoint);
    if (glyph_it == font_atlas_variant.glyphs.end()) { continue; }

    if (prev_codepoint != 0) {
      auto kerning_it =
          font_atlas_variant.kernings.find(font_atlas_pack_kerning(prev_codepoint, codepoint));
      if (kerning_it != font_atlas_variant.kernings.end()) { width += kerning_it->second * size; }
    }
    prev_codepoint = codepoint;

    width += glyph_it->second.horizontal_advance * size;
  }
  return width;
}

static HMM_Vec2 text_measure_string_size(
    const Font_Atlas_Variant& font_atlas_variant,
    std::string_view          text,
    float                     size) {
  int         lines_count    = 1;
  float       max_line_width = 0.0f;
  const char* ptr            = text.data();
  auto        str_size       = text.size();
  const char* line_start     = ptr;
  int         codepoint      = SDL_INVALID_UNICODE_CODEPOINT;
  while (codepoint != 0) {
    codepoint = SDL_StepUTF8(&ptr, &str_size);
    if (codepoint == SDL_INVALID_UNICODE_CODEPOINT) { continue; }

    if (codepoint == 10) {
      std::string_view line(line_start, static_cast<size_t>(ptr - line_start - 1));
      float            line_width = text_measure_width(font_atlas_variant, line, size);
      max_line_width              = std::max(max_line_width, line_width);
      line_start                  = ptr;
      lines_count += 1;
    }
  }

  if (ptr > line_start) {
    std::string_view line(line_start, static_cast<size_t>(ptr - line_start));
    float            line_width = text_measure_width(font_atlas_variant, line, size);
    max_line_width              = std::max(max_line_width, line_width);
  }

  return HMM_V2(max_line_width, lines_count * font_atlas_variant.line_height * size);
}
