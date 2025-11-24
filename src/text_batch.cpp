static constexpr int TEXT_BATCH_MAX_DRAW_CMDS              = 8;
static constexpr int TEXT_BATCH_MAX_INSTANCES_PER_DRAW_CMD = 8192;
static constexpr int TEXT_BATCH_MAX_INSTANCES =
    TEXT_BATCH_MAX_DRAW_CMDS * TEXT_BATCH_MAX_INSTANCES_PER_DRAW_CMD;
static constexpr int TEXT_BATCH_INDICES_PER_INSTANCE = 6;

struct Text_Batch_Instance {
  HMM_Vec3 position;
  float    size;
  HMM_Quat rotation;
  HMM_Vec4 color;
  HMM_Vec4 plane_bounds;
  HMM_Vec4 atlas_bounds;
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

struct Vertex_Uniform_Data {
  HMM_Mat4 world_to_clip_transform;
  uint32_t first_instance;
};

struct Fragment_Uniform_Data {
  float font_size;
  float pixel_range;
};

static bool text_batch_create(
    Text_Batch*          text_batch,
    const std::string&   base_path,
    SDL_GPUDevice*       device,
    SDL_GPUTextureFormat swapchain_texture_format) {
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

  {
    auto                shader_formats = SDL_GetGPUShaderFormats(device);
    const char*         file_ext;
    SDL_GPUShaderFormat format;
    if ((shader_formats & SDL_GPU_SHADERFORMAT_DXIL) != 0) {
      file_ext = "dxil";
      format   = SDL_GPU_SHADERFORMAT_DXIL;
    } else if ((shader_formats & SDL_GPU_SHADERFORMAT_MSL) != 0) {
      file_ext = "msl";
      format   = SDL_GPU_SHADERFORMAT_MSL;
    } else if ((shader_formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
      file_ext = "spv";
      format   = SDL_GPU_SHADERFORMAT_SPIRV;
    } else {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unsupported shader formats");
      return false;
    }

    SDL_GPUShader* vertex_shader;
    {
      auto                 file_path = base_path + "/text_batch.vert." + file_ext;
      std::vector<uint8_t> file_contents;
      if (!read_file_contents(file_path.c_str(), &file_contents)) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to read file contents: %s",
            file_path.c_str());
        return false;
      }

      SDL_GPUShaderCreateInfo info = {};
      info.code                    = file_contents.data();
      info.code_size               = file_contents.size();
      info.entrypoint              = "main";
      info.format                  = format;
      info.num_storage_buffers     = 1;
      info.num_uniform_buffers     = 1;
      info.stage                   = SDL_GPU_SHADERSTAGE_VERTEX;
      vertex_shader                = SDL_CreateGPUShader(device, &info);
      if (vertex_shader == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create vertex shader: %s",
            SDL_GetError());
        return false;
      }
    }
    defer(SDL_ReleaseGPUShader(device, vertex_shader));

    SDL_GPUShader* fragment_shader;
    {
      auto                 file_path = base_path + "/text_batch.frag." + file_ext;
      std::vector<uint8_t> file_contents;
      if (!read_file_contents(file_path.c_str(), &file_contents)) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to read file contents: %s",
            file_path.c_str());
        return false;
      }

      SDL_GPUShaderCreateInfo info = {};
      info.code                    = file_contents.data();
      info.code_size               = file_contents.size();
      info.entrypoint              = "main";
      info.format                  = format;
      info.num_samplers            = 1;
      info.num_uniform_buffers     = 1;
      info.stage                   = SDL_GPU_SHADERSTAGE_FRAGMENT;
      fragment_shader              = SDL_CreateGPUShader(device, &info);
      if (fragment_shader == nullptr) {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Failed to create fragment shader: %s",
            SDL_GetError());
        return false;
      }
    }
    defer(SDL_ReleaseGPUShader(device, fragment_shader));

    SDL_GPUColorTargetDescription desc     = {};
    desc.format                            = swapchain_texture_format;
    desc.blend_state.enable_blend          = true;
    desc.blend_state.color_blend_op        = SDL_GPU_BLENDOP_ADD;
    desc.blend_state.alpha_blend_op        = SDL_GPU_BLENDOP_ADD;
    desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

    SDL_GPUGraphicsPipelineCreateInfo info     = {};
    info.target_info.num_color_targets         = 1;
    info.target_info.color_target_descriptions = &desc;
    info.primitive_type                        = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.vertex_shader                         = vertex_shader;
    info.fragment_shader                       = fragment_shader;
    text_batch->pipeline                       = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (text_batch->pipeline == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create pipeline: %s", SDL_GetError());
      return false;
    }
  }

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

static void text_batch_destroy(Text_Batch* text_batch, SDL_GPUDevice* device) {
  SDL_assert(text_batch != nullptr);

  SDL_ReleaseGPUGraphicsPipeline(device, text_batch->pipeline);
  SDL_ReleaseGPUTransferBuffer(device, text_batch->transfer_buffer);
  SDL_ReleaseGPUBuffer(device, text_batch->data_buffer);
}

static void flush(
    Text_Batch*       text_batch,
    const HMM_Mat4&   world_to_clip_transform,
    const Font_Atlas* font_atlas,
    int               font_index) {
  SDL_assert(text_batch->draw_cmds_count < TEXT_BATCH_MAX_DRAW_CMDS);

  auto& draw_cmd                   = text_batch->draw_cmds[text_batch->draw_cmds_count];
  draw_cmd.world_to_clip_transform = world_to_clip_transform;
  draw_cmd.font_atlas              = font_atlas;
  draw_cmd.font_index              = font_index;
  draw_cmd.first_instance  = text_batch->draw_cmds_count * TEXT_BATCH_MAX_INSTANCES_PER_DRAW_CMD;
  draw_cmd.instances_count = 0;

  text_batch->draw_cmds_count += 1;
}

static void text_batch_begin(
    Text_Batch*       text_batch,
    const HMM_Mat4&   world_to_clip_transform,
    const Font_Atlas* font_atlas,
    int               font_index) {
  SDL_assert(text_batch != nullptr);
  SDL_assert(font_atlas != nullptr);
  SDL_assert(font_index >= 0 && font_index < font_atlas->variants.size());
  SDL_assert(!text_batch->begin_called);
  SDL_assert(text_batch->draw_cmds_count < TEXT_BATCH_MAX_DRAW_CMDS);

  text_batch->begin_called = true;

  flush(text_batch, world_to_clip_transform, font_atlas, font_index);
}

static void text_batch_end(Text_Batch* text_batch) {
  SDL_assert(text_batch != nullptr);
  SDL_assert(text_batch->begin_called);

  text_batch->begin_called = false;
}

static void text_batch_draw(
    Text_Batch*        text_batch,
    const std::string& text,
    HMM_Vec2           position,
    float              size,
    HMM_Vec4           color) {
  SDL_assert(text_batch != nullptr);
  SDL_assert(text_batch->begin_called);

  const char* ptr              = text.c_str();
  HMM_Vec2    current_position = position;
  while (*ptr) {
    Uint32 codepoint = SDL_StepUTF8(&ptr, nullptr);
    if (codepoint == 0) { break; }
    if (codepoint == SDL_INVALID_UNICODE_CODEPOINT) { continue; }

    auto& draw_cmd = text_batch->draw_cmds[text_batch->draw_cmds_count - 1];

    const auto& font_data = draw_cmd.font_atlas->variants[draw_cmd.font_index];
    auto        glyph_it  = font_data.glyphs.find(codepoint);
    if (glyph_it == font_data.glyphs.end()) { return; }

    if (draw_cmd.instances_count >= TEXT_BATCH_MAX_INSTANCES_PER_DRAW_CMD) {
      flush(text_batch, draw_cmd.world_to_clip_transform, draw_cmd.font_atlas, draw_cmd.font_index);
      draw_cmd = text_batch->draw_cmds[text_batch->draw_cmds_count - 1];
    }

    auto& instance = text_batch->instances[text_batch->total_instances_count];
    text_batch->total_instances_count += 1;
    draw_cmd.instances_count += 1;

    instance.position     = HMM_V3(current_position.X, current_position.Y, 0.0f);
    instance.size         = size;
    instance.rotation     = HMM_Q(0.0f, 0.0f, 0.0f, 1.0f);
    instance.color        = color;
    instance.plane_bounds = HMM_V4(
        glyph_it->second.plane_bounds.left,
        glyph_it->second.plane_bounds.top,
        glyph_it->second.plane_bounds.right,
        glyph_it->second.plane_bounds.bottom);

    float atlas_width     = static_cast<float>(draw_cmd.font_atlas->width);
    float atlas_height    = static_cast<float>(draw_cmd.font_atlas->height);
    instance.atlas_bounds = HMM_V4(
        glyph_it->second.atlas_bounds.left / atlas_width,
        1.0f - (glyph_it->second.atlas_bounds.top / atlas_height),
        glyph_it->second.atlas_bounds.right / atlas_width,
        1.0f - (glyph_it->second.atlas_bounds.bottom / atlas_height));

    current_position.X += (glyph_it->second.horizontal_advance * size);
  }
}

static void text_batch_prepare_draw_cmds(
    Text_Batch*           text_batch,
    SDL_GPUDevice*        device,
    SDL_GPUCommandBuffer* cmd_buf) {
  SDL_assert(text_batch != nullptr);
  SDL_assert(device != nullptr);
  SDL_assert(cmd_buf != nullptr);
  SDL_assert(!text_batch->begin_called);

  if (text_batch->draw_cmds_count == 0) { return; }

  Text_Batch_Instance* mapped_ptr = static_cast<Text_Batch_Instance*>(
      SDL_MapGPUTransferBuffer(device, text_batch->transfer_buffer, true));
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
    auto copy_pass = SDL_BeginGPUCopyPass(cmd_buf);
    defer(SDL_EndGPUCopyPass(copy_pass));

    SDL_GPUTransferBufferLocation source = {};
    source.transfer_buffer               = text_batch->transfer_buffer;
    SDL_GPUBufferRegion dest             = {};
    dest.buffer                          = text_batch->data_buffer;
    dest.size = sizeof(Text_Batch_Instance) * text_batch->total_instances_count;
    SDL_UploadToGPUBuffer(copy_pass, &source, &dest, true);
  }
}

static void text_batch_render_draw_cmds(
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
      uniforms.first_instance          = static_cast<uint32_t>(draw_cmd.first_instance);
      SDL_PushGPUVertexUniformData(cmd_buf, 0, &uniforms, sizeof(uniforms));
    }

    {
      Fragment_Uniform_Data uniforms = {};
      uniforms.font_size             = draw_cmd.font_atlas->size;
      uniforms.pixel_range           = draw_cmd.font_atlas->distance_range;
      SDL_PushGPUFragmentUniformData(cmd_buf, 0, &uniforms, sizeof(uniforms));
    }

    SDL_DrawGPUPrimitives(
        render_pass,
        draw_cmd.instances_count * TEXT_BATCH_INDICES_PER_INSTANCE,
        1,
        0,
        0);
  }

  text_batch->draw_cmds_count       = 0;
  text_batch->total_instances_count = 0;
}
