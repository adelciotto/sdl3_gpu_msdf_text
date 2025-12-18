#pragma once

#include "font_atlas.h"
#include "util.h"
#include <HandmadeMath.h>
#include <SDL3/SDL.h>
#include <string_view>

static constexpr int TEXT_BATCH_MAX_DRAW_CMDS           = 8;
static constexpr int TEXT_BATCH_MAX_GLYPHS_PER_DRAW_CMD = 8192;
static constexpr int TEXT_BATCH_MAX_GLYPHS =
    TEXT_BATCH_MAX_DRAW_CMDS * TEXT_BATCH_MAX_GLYPHS_PER_DRAW_CMD;

struct Text_Batch_Glyph {
  HMM_Vec3 position;
  float    size;
  HMM_Vec4 plane_bounds;
  HMM_Vec4 atlas_bounds;
  HMM_Vec4 color;
  HMM_Vec4 outline_color;
  float    outline_thickness;
};

struct Text_Batch_Draw_Cmd {
  HMM_Mat4          world_to_clip_transform;
  const Font_Atlas* font_atlas;
  int               font_variant;
  int               first_glyph;
  int               glyphs_count;
};

struct Text_Batch {
  Text_Batch_Draw_Cmd      draw_cmds[TEXT_BATCH_MAX_DRAW_CMDS];
  int                      draw_cmds_count;
  Text_Batch_Glyph         glyphs[TEXT_BATCH_MAX_GLYPHS];
  int                      total_glyphs_count;
  bool                     begin_called;
  SDL_GPUBuffer*           data_buffer;
  SDL_GPUTransferBuffer*   transfer_buffer;
  SDL_GPUGraphicsPipeline* pipeline;
  SDL_GPUSampler*          sampler;
};

struct Text_Batch_Vertex_Uniforms {
  HMM_Mat4 world_to_clip_transform;
  uint32_t first_glyph;
};

struct Text_Batch_Fragment_Uniforms {
  float    font_size;
  HMM_Vec2 unit_range;
};

bool text_batch_create(
    Text_Batch*          text_batch,
    SDL_Storage*         storage,
    SDL_GPUDevice*       device,
    SDL_GPUTextureFormat swapchain_texture_format);
void text_batch_destroy(Text_Batch* text_batch, SDL_GPUDevice* device);
void text_batch_begin(
    Text_Batch*       text_batch,
    const HMM_Mat4&   world_to_clip_transform,
    const Font_Atlas* font_atlas,
    int               font_variant);
void text_batch_end(Text_Batch* text_batch);
void text_batch_draw(
    Text_Batch*       text_batch,
    std::string_view  text,
    HMM_Vec3          position,
    float             size,
    const Text_Align& align = {},
    const Text_Style& style = {});
void text_batch_draw_multiline(
    Text_Batch*       text_batch,
    std::string_view  text,
    HMM_Vec3          position,
    float             size,
    const Text_Align& align           = {},
    const Text_Style& style           = {},
    HMM_Vec2          text_block_size = HMM_V2(-1.0f, -1.0f));
void text_batch_prepare_draw_cmds(
    Text_Batch*           text_batch,
    SDL_GPUDevice*        device,
    SDL_GPUCommandBuffer* cmd_buf);
void text_batch_render_draw_cmds(
    Text_Batch*           text_batch,
    SDL_GPUCommandBuffer* cmd_buf,
    SDL_GPURenderPass*    render_pass);
