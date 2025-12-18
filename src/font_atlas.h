#pragma once

#include <SDL3/SDL.h>
#include <unordered_map>
#include <vector>

enum Font_Atlas_Kind {
  FONT_ATLAS_KIND_ROBOTO,
  FONT_ATLAS_KIND_SCIENCE_GOTHIC,
  FONT_ATLAS_KIND_LIMELIGHT,
  FONT_ATLAS_KIND_COUNT,
};

enum Font_Atlas_Roboto_Variant {
  FONT_ATLAS_ROBOTO_VARIANT_REGULAR,
  FONT_ATLAS_ROBOTO_VARIANT_BOLD,
  FONT_ATLAS_ROBOTO_VARIANT_ITALIC,
  FONT_ATLAS_ROBOTO_VARIANT_BOLD_ITALIC,
  FONT_ATLAS_ROBOTO_VARIANT_LIGHT,
  FONT_ATLAS_ROBOTO_VARIANT_COUNT,
};

enum Font_Atlas_Science_Gothic_Variant {
  FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_REGULAR,
  FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_BOLD,
  FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_LIGHT,
  FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_COUNT,
};

struct Font_Glyph_Bounds {
  float left;
  float bottom;
  float right;
  float top;
};

struct Font_Glyph {
  int               unicode;
  float             horizontal_advance;
  Font_Glyph_Bounds plane_bounds;
  Font_Glyph_Bounds atlas_bounds;
};

struct Font_Kerning {
  int   unicode1;
  int   unicode2;
  float advance;
};

struct Font_Atlas_Variant {
  std::unordered_map<int, Font_Glyph> glyphs;
  std::unordered_map<uint64_t, float> kernings;
  float                               line_height;
  float                               ascender;
  float                               descender;
};

struct Font_Atlas {
  std::vector<Font_Atlas_Variant> variants;
  float                           distance_range;
  float                           size;
  int                             width;
  int                             height;
  SDL_GPUTexture*                 texture;
};

uint64_t font_atlas_pack_kerning(int unicode1, int unicode2);
bool     font_atlas_load(
        Font_Atlas*      font_atlas,
        Font_Atlas_Kind  kind,
        SDL_Storage*     storage,
        SDL_GPUDevice*   device,
        SDL_GPUCopyPass* copy_pass);
void font_atlas_destroy(Font_Atlas* font_atlas, SDL_GPUDevice* device);
