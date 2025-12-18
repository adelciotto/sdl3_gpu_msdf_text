#pragma once

#include <HandmadeMath.h>
#include <SDL3/SDL.h>
#include <string_view>

// -- Defer -------------------------------------------------------------------

template<typename F> struct Priv_Defer {
  F f;
  Priv_Defer(F f) : f(f) {
  }
  ~Priv_Defer() {
    f();
  }
};

template<typename F> Priv_Defer<F> defer_func(F f) {
  return Priv_Defer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&]() { code; })

// -- Storage -----------------------------------------------------------------

template<typename Container>
bool read_storage_file(SDL_Storage* storage, const char* file_path, Container* out_container);

// -- Text --------------------------------------------------------------------

struct Font_Atlas_Variant;

inline constexpr int TEXT_INDICES_PER_GLYPH = 6;

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

float text_measure_width(
    const Font_Atlas_Variant& font_atlas_variant,
    std::string_view          text,
    float                     size);
HMM_Vec2 text_measure_string_size(
    const Font_Atlas_Variant& font_atlas_variant,
    std::string_view          text,
    float                     size);
