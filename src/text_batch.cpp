#include "text_batch.hpp"

#include "font_atlas.hpp"

static constexpr int INDICES_PER_INSTANCE = 6;

struct Vertex_Uniform_Data {
  HMM_Mat4 world_to_clip_transform;
  int      first_instance;
};

bool text_batch_create(Text_Batch* text_batch, SDL_GPUDevice* device) {
  SDL_assert(text_batch != nullptr);
  SDL_assert(device != nullptr);

  {
    SDL_GPUBufferCreateInfo info = {};
    info.size                    = sizeof(Text_Batch_Instance) * TEXT_BATCH_MAX_INSTANCES;
    info.usage                   = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    text_batch->data_buffer      = SDL_CreateGPUBuffer(device, &info);
    if (text_batch->data_buffer == nullptr) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to create data buffer: %s",
          SDL_GetError());
      return false;
    }
  }

  {
    SDL_GPUTransferBufferCreateInfo info = {};
    info.size                            = sizeof(Text_Batch_Instance) * TEXT_BATCH_MAX_INSTANCES;
    info.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    text_batch->transfer_buffer          = SDL_CreateGPUTransferBuffer(device, &info);
    if (text_batch->data_buffer == nullptr) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to create transfer buffer: %s",
          SDL_GetError());
      return false;
    }
  }

  // TODO: Create shaders and pipeline.
  {}

  {
    SDL_GPUSamplerCreateInfo info = {};
    info.min_filter               = SDL_GPU_FILTER_LINEAR;
    info.mag_filter               = SDL_GPU_FILTER_LINEAR;
    info.address_mode_u           = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v           = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    text_batch->sampler           = SDL_CreateGPUSampler(device, &info);
    if (text_batch->sampler == nullptr) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to create texture sampler: %s",
          SDL_GetError());
      return false;
    }
  }

  return true;
}

void text_batch_destroy(Text_Batch* text_batch, SDL_GPUDevice* device) {
  SDL_assert(text_batch != nullptr);

  SDL_ReleaseGPUGraphicsPipeline(device, text_batch->pipeline);
  SDL_ReleaseGPUTransferBuffer(device, text_batch->transfer_buffer);
  SDL_ReleaseGPUBuffer(device, text_batch->data_buffer);

  *text_batch = {};
}

void text_batch_begin(
    Text_Batch*       text_batch,
    const HMM_Mat4&   world_to_clip_transform,
    const Font_Atlas* font_atlas,
    int               font_index) {
  SDL_assert(text_batch != nullptr);
  SDL_assert(font_atlas != nullptr);
  SDL_assert(font_index >= 0);
  SDL_assert(!text_batch->begin_called);
  SDL_assert(text_batch->draw_cmds_count < TEXT_BATCH_MAX_DRAW_CMDS);

  text_batch->begin_called = true;

  auto& draw_cmd = text_batch->draw_cmds[text_batch->draw_cmds_count];
  text_batch->draw_cmds_count += 1;

  draw_cmd.world_to_clip_transform = world_to_clip_transform;
  draw_cmd.font_atlas              = font_atlas;
  draw_cmd.font_index              = font_index;
  draw_cmd.first_instance  = text_batch->draw_cmds_count * TEXT_BATCH_MAX_INSTANCES_PER_DRAW_CMD;
  draw_cmd.instances_count = 0;
}

void text_batch_end(Text_Batch* text_batch) {
  SDL_assert(text_batch != nullptr);
  SDL_assert(text_batch->begin_called);

  text_batch->begin_called = false;
}

void text_batch_prepare_draw_cmds(
    Text_Batch*      text_batch,
    SDL_GPUDevice*   device,
    SDL_GPUCopyPass* copy_pass) {
  SDL_assert(text_batch != nullptr);
  SDL_assert(device != nullptr);
  SDL_assert(copy_pass != nullptr);
  SDL_assert(!text_batch->begin_called);

  if (text_batch->draw_cmds_count == 0) { return; }

  auto mapped_ptr = SDL_MapGPUTransferBuffer(device, text_batch->transfer_buffer, true);
  if (mapped_ptr == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to map transfer buffer: %s", SDL_GetError());
    return;
  }
  SDL_memcpy(
      mapped_ptr,
      text_batch->instances,
      sizeof(Text_Batch_Instance) * text_batch->total_instances_count);
  SDL_UnmapGPUTransferBuffer(device, text_batch->transfer_buffer);

  {
    SDL_GPUTransferBufferLocation source = {};
    source.transfer_buffer               = text_batch->transfer_buffer;
    SDL_GPUBufferRegion dest             = {};
    dest.buffer                          = text_batch->data_buffer;
    dest.size = sizeof(Text_Batch_Instance) * text_batch->total_instances_count;
    SDL_UploadToGPUBuffer(copy_pass, &source, &dest, true);
  }
}

void text_batch_render_draw_cmds(
    Text_Batch*           text_batch,
    SDL_GPUCommandBuffer* cmd_buf,
    SDL_GPURenderPass*    render_pass) {
  SDL_assert(text_batch != nullptr);
  SDL_assert(cmd_buf != nullptr);
  SDL_assert(render_pass != nullptr);
  SDL_assert(!text_batch->begin_called);

  if (text_batch->draw_cmds_count == 0) { return; }

  // TODO: If we have multiple pipelines in the future, we must move this into the loop below.
  SDL_BindGPUGraphicsPipeline(render_pass, text_batch->pipeline);

  SDL_BindGPUVertexStorageBuffers(render_pass, 0, &text_batch->data_buffer, 1);

  for (int i = 0; i < text_batch->draw_cmds_count; i++) {
    const auto& draw_cmd = text_batch->draw_cmds[i];

    {
      SDL_GPUTextureSamplerBinding binding = {};
      binding.texture                      = draw_cmd.font_atlas->texture;
      binding.sampler                      = text_batch->sampler;
      SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
    }

    {
      Vertex_Uniform_Data uniforms     = {};
      uniforms.world_to_clip_transform = draw_cmd.world_to_clip_transform;
      uniforms.first_instance          = draw_cmd.first_instance;
      SDL_PushGPUVertexUniformData(cmd_buf, 0, &uniforms, sizeof(uniforms));
    }

    SDL_DrawGPUPrimitives(render_pass, draw_cmd.instances_count * INDICES_PER_INSTANCE, 1, 0, 0);
  }
}
