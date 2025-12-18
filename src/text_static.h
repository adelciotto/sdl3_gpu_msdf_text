#pragma once

#include "font_atlas.h"
#include "util.h"
#include <HandmadeMath.h>
#include <SDL3/SDL.h>
#include <string_view>

struct Text_Static_Glyph {
  HMM_Vec3 position;
  float    size;
  HMM_Vec4 plane_bounds;
  HMM_Vec4 atlas_bounds;
};

struct Text_Static_Data {
  SDL_GPUBuffer*    glyphs;
  int               glyphs_count;
  const Font_Atlas* font_atlas;
};

struct Text_Static {
  SDL_GPUGraphicsPipeline* pipeline;
  SDL_GPUSampler*          sampler;
};

struct Text_Static_Vertex_Uniforms {
  HMM_Mat4 view_to_clip_transform;
  HMM_Vec3 camera_position;
};

struct Text_Static_Fragment_Uniforms {
  float    font_size;
  HMM_Vec2 unit_range;
  HMM_Vec4 fog_color;
  HMM_Vec4 color;
  HMM_Vec4 outline_color;
  float    outline_thickness;
  uint32_t fog_enabled;
};

bool text_static_create(
    Text_Static*         text_static,
    SDL_Storage*         storage,
    SDL_GPUDevice*       device,
    SDL_GPUTextureFormat swapchain_texture_format);
void text_static_destroy(Text_Static* text_static, SDL_GPUDevice* device);
bool text_static_data_create_multiline(
    Text_Static_Data* text_data,
    SDL_GPUDevice*    device,
    SDL_GPUCopyPass*  copy_pass,
    const Font_Atlas* font_atlas,
    int               font_variant,
    std::string_view  text,
    HMM_Vec3          position,
    float             size,
    const Text_Align& align = {});
void text_static_data_destroy(Text_Static_Data* text_static_data, SDL_GPUDevice* device);
void text_static_draw(
    Text_Static*            text_static,
    SDL_GPUCommandBuffer*   cmd_buf,
    SDL_GPURenderPass*      render_pass,
    const Text_Static_Data& text_data,
    const HMM_Mat4&         view_to_clip_transform,
    HMM_Vec3                camera_position,
    bool                    fog_enabled,
    HMM_Vec4                fog_color,
    const Text_Style&       style = {});
