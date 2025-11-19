#pragma once

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
  float             kerning;
};

struct Font_Variant {
  std::vector<Font_Glyph> glyphs;
  float                   line_height;
  float                   ascender;
  float                   descender;
};

struct Font_Atlas {
  std::vector<Font_Variant> variants;
  float                     distance_range;
  int                       width;
  int                       height;
  SDL_GPUTexture*           texture;
};

bool font_atlas_load(
    Font_Atlas*      font_atlas,
    const char*      json_file_path,
    const char*      png_file_path,
    SDL_GPUDevice*   device,
    SDL_GPUCopyPass* copy_pass);
void font_atlas_destroy(Font_Atlas* font_atlas, SDL_GPUDevice* device);
