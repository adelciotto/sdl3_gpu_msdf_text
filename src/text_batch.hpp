#pragma once

inline constexpr int TEXT_BATCH_MAX_DRAW_CMDS              = 8;
inline constexpr int TEXT_BATCH_MAX_INSTANCES_PER_DRAW_CMD = 8192;
inline constexpr int TEXT_BATCH_MAX_INSTANCES =
    TEXT_BATCH_MAX_DRAW_CMDS * TEXT_BATCH_MAX_INSTANCES_PER_DRAW_CMD;

struct Font_Atlas;

struct Text_Batch_Instance {
  HMM_Vec3 position;
  float    padding_0;
  HMM_Quat rotation;
  HMM_Vec2 scale;
  HMM_Vec2 padding_1;
  HMM_Vec4 atlas_quad;
  uint32_t color;
};

struct Text_Batch_Draw_Cmd {
  HMM_Mat4          world_to_clip_transform;
  const Font_Atlas* font_atlas;
  int               font_index;
  int               first_instance;
  int               instances_count;
};

struct Text_Batch {
  Text_Batch_Draw_Cmd      draw_cmds[TEXT_BATCH_MAX_DRAW_CMDS];
  int                      draw_cmds_count;
  Text_Batch_Instance      instances[TEXT_BATCH_MAX_INSTANCES];
  int                      total_instances_count;
  bool                     begin_called;
  SDL_GPUBuffer*           data_buffer;
  SDL_GPUTransferBuffer*   transfer_buffer;
  SDL_GPUGraphicsPipeline* pipeline;
  SDL_GPUSampler*          sampler;
};

bool text_batch_create(Text_Batch* text_batch, SDL_GPUDevice* device);
void text_batch_destroy(Text_Batch* text_batch, SDL_GPUDevice* device);
void text_batch_begin(
    Text_Batch*       text_batch,
    const HMM_Mat4&   world_to_clip_transform,
    const Font_Atlas* font_atlas,
    int               font_index = 0);
void text_batch_end(Text_Batch* text_batch);
void text_batch_prepare_draw_cmds(
    Text_Batch*      text_batch,
    SDL_GPUDevice*   device,
    SDL_GPUCopyPass* copy_pass);
void text_batch_render_draw_cmds(
    Text_Batch*           text_batch,
    SDL_GPUCommandBuffer* cmd_buf,
    SDL_GPURenderPass*    render_pass);
