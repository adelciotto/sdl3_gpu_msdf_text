#include "util.h"
#include "font_atlas.h"
#include <string>
#include <vector>

// -- Storage -----------------------------------------------------------------

template<typename Container>
bool read_storage_file(SDL_Storage* storage, const char* file_path, Container* out_container) {
  uint64_t file_size = 0;
  if (!SDL_GetStorageFileSize(storage, file_path, &file_size)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to get storage file size: %s",
        SDL_GetError());
    return false;
  }
  if (file_size == 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get storage file size: file size is 0");
    return false;
  }

  out_container->resize(file_size);
  if (!SDL_ReadStorageFile(storage, file_path, out_container->data(), file_size)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read storage file: %s", SDL_GetError());
    return false;
  }

  return true;
}

template bool read_storage_file<std::string>(SDL_Storage*, const char*, std::string*);
template bool
read_storage_file<std::vector<uint8_t>>(SDL_Storage*, const char*, std::vector<uint8_t>*);

// -- Text --------------------------------------------------------------------

float text_measure_width(
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

HMM_Vec2 text_measure_string_size(
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
