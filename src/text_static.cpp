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

static bool text_static_create(
    Text_Static*         text_static,
    const std::string&   base_path,
    SDL_GPUDevice*       device,
    SDL_GPUTextureFormat swapchain_texture_format) {
  SDL_assert(text_static != nullptr);
  SDL_assert(device != nullptr);

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
      auto                 file_path = base_path + "/res/text_static.vert." + file_ext;
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
      auto                 file_path = base_path + "/res/text_static.frag." + file_ext;
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
    desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

    SDL_GPUGraphicsPipelineCreateInfo info     = {};
    info.target_info.num_color_targets         = 1;
    info.target_info.color_target_descriptions = &desc;
    info.primitive_type                        = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.vertex_shader                         = vertex_shader;
    info.fragment_shader                       = fragment_shader;
    text_static->pipeline                      = SDL_CreateGPUGraphicsPipeline(device, &info);
    if (text_static->pipeline == nullptr) {
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
    text_static->sampler          = SDL_CreateGPUSampler(device, &info);
    if (text_static->sampler == nullptr) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to create texture sampler: %s",
          SDL_GetError());
      return false;
    }
  }

  return true;
}

static void text_static_destroy(Text_Static* text_static, SDL_GPUDevice* device) {
  SDL_assert(text_static != nullptr);
  SDL_assert(device != nullptr);

  SDL_ReleaseGPUGraphicsPipeline(device, text_static->pipeline);
  SDL_ReleaseGPUSampler(device, text_static->sampler);
}

static bool text_static_data_create_multiline(
    Text_Static_Data* text_data,
    SDL_GPUDevice*    device,
    SDL_GPUCopyPass*  copy_pass,
    const Font_Atlas* font_atlas,
    int               font_variant,
    std::string_view  text,
    HMM_Vec3          position,
    float             size,
    const Text_Align& align = {}) {
  SDL_assert(device != nullptr);
  SDL_assert(copy_pass != nullptr);
  SDL_assert(font_variant >= 0 && font_variant < font_atlas->variants.size());

  const auto& font_data = font_atlas->variants[font_variant];

  int glyphs_count = 0;
  {
    const char* ptr       = text.data();
    auto        str_size  = text.size();
    int         codepoint = SDL_INVALID_UNICODE_CODEPOINT;
    while (codepoint != 0) {
      codepoint = SDL_StepUTF8(&ptr, &str_size);
      if (codepoint == SDL_INVALID_UNICODE_CODEPOINT) { continue; }
      if (codepoint == 32 || codepoint == 10) { continue; }

      auto font_glyph_it = font_data.glyphs.find(codepoint);
      if (font_glyph_it != font_data.glyphs.end()) { glyphs_count += 1; }
    }
  }

  text_data->glyphs_count = glyphs_count;
  text_data->font_atlas   = font_atlas;

  {
    SDL_GPUBufferCreateInfo info = {};
    info.size                    = sizeof(Text_Static_Glyph) * text_data->glyphs_count;
    info.usage                   = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    text_data->glyphs            = SDL_CreateGPUBuffer(device, &info);
    if (text_data->glyphs == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create buffer: %s", SDL_GetError());
      return false;
    }
  }

  SDL_GPUTransferBuffer* transfer_buffer;
  {
    SDL_GPUTransferBufferCreateInfo info = {};
    info.size                            = sizeof(Text_Static_Glyph) * text_data->glyphs_count;
    info.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_buffer                      = SDL_CreateGPUTransferBuffer(device, &info);
    if (transfer_buffer == nullptr) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to create transfer buffer: %s",
          SDL_GetError());
      return false;
    }
  }
  defer(SDL_ReleaseGPUTransferBuffer(device, transfer_buffer));

  {
    auto mapped_ptr =
        static_cast<Text_Static_Glyph*>(SDL_MapGPUTransferBuffer(device, transfer_buffer, false));

    if (mapped_ptr == nullptr) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to map transfer buffer: %s",
          SDL_GetError());
      return false;
    }
    defer(SDL_UnmapGPUTransferBuffer(device, transfer_buffer));

    HMM_Vec2 text_block_size = text_measure_string_size(font_data, text, size);

    float current_y = position.Y;
    switch (align.vertical) {
    case TEXT_V_ALIGN_TOP:
      current_y = position.Y - font_data.ascender * size;
      break;

    case TEXT_V_ALIGN_MIDDLE:
      current_y = position.Y + (text_block_size.Y * 0.5f) - font_data.ascender * size;
      break;

    case TEXT_V_ALIGN_BOTTOM:
      current_y = position.Y - font_data.descender * size +
                  (text_block_size.Y - font_data.line_height * size);
      break;

    case TEXT_V_ALIGN_BASELINE:
    default:
      break;
    }

    int glyph_index = 0;

    auto draw_line = [&](std::string_view line) {
      HMM_Vec3 line_position = HMM_V3(position.X, current_y, position.Z);

      switch (align.horizontal) {
      case TEXT_H_ALIGN_CENTER:
        line_position.X += -text_measure_width(font_data, line, size) * 0.5f;
        break;
      case TEXT_H_ALIGN_RIGHT:
        line_position.X += -text_measure_width(font_data, line, size);
        break;
      case TEXT_H_ALIGN_LEFT:
      default:
        break;
      }

      HMM_Vec3    current_position = line_position;
      const char* line_ptr         = line.data();
      auto        line_size        = line.size();
      int         codepoint        = SDL_INVALID_UNICODE_CODEPOINT;
      int         prev_codepoint   = 0;
      while (codepoint != 0) {
        codepoint = SDL_StepUTF8(&line_ptr, &line_size);
        if (codepoint == SDL_INVALID_UNICODE_CODEPOINT) { continue; }

        auto font_glyph_it = font_data.glyphs.find(codepoint);
        if (font_glyph_it == font_data.glyphs.end()) { continue; }

        if (prev_codepoint != 0) {
          auto kerning_it =
              font_data.kernings.find(font_atlas_pack_kerning(prev_codepoint, codepoint));
          if (kerning_it != font_data.kernings.end()) {
            current_position.X += kerning_it->second * size;
          }
        }
        prev_codepoint = codepoint;

        if (codepoint != 32) {
          auto glyph          = &mapped_ptr[glyph_index];
          glyph->position     = current_position;
          glyph->size         = size;
          glyph->plane_bounds = HMM_V4(
              font_glyph_it->second.plane_bounds.left,
              font_glyph_it->second.plane_bounds.top,
              font_glyph_it->second.plane_bounds.right,
              font_glyph_it->second.plane_bounds.bottom);
          float atlas_width   = static_cast<float>(font_atlas->width);
          float atlas_height  = static_cast<float>(font_atlas->height);
          glyph->atlas_bounds = HMM_V4(
              font_glyph_it->second.atlas_bounds.left / atlas_width,
              1.0f - font_glyph_it->second.atlas_bounds.top / atlas_height,
              font_glyph_it->second.atlas_bounds.right / atlas_width,
              1.0f - font_glyph_it->second.atlas_bounds.bottom / atlas_height);
          glyph_index += 1;
        }

        current_position.X += font_glyph_it->second.horizontal_advance * size;
      }

      current_y -= font_data.line_height * size;
    };

    const char* ptr        = text.data();
    auto        str_size   = text.size();
    const char* line_start = ptr;
    int         codepoint  = SDL_INVALID_UNICODE_CODEPOINT;
    while (codepoint != 0) {
      codepoint = SDL_StepUTF8(&ptr, &str_size);
      if (codepoint == SDL_INVALID_UNICODE_CODEPOINT) { continue; }

      if (codepoint == 10) {
        draw_line({line_start, static_cast<size_t>(ptr - line_start)});
        line_start = ptr;
      }
    }

    if (ptr > line_start) { draw_line({line_start, static_cast<size_t>(ptr - line_start)}); }
  }

  {
    SDL_GPUTransferBufferLocation source = {};
    source.transfer_buffer               = transfer_buffer;
    SDL_GPUBufferRegion dest             = {};
    dest.buffer                          = text_data->glyphs;
    dest.size                            = sizeof(Text_Static_Glyph) * text_data->glyphs_count;
    SDL_UploadToGPUBuffer(copy_pass, &source, &dest, true);
  }

  return true;
}

static void text_static_data_destroy(Text_Static_Data* text_static_data, SDL_GPUDevice* device) {
  SDL_assert(text_static_data != nullptr);
  SDL_assert(device != nullptr);

  SDL_ReleaseGPUBuffer(device, text_static_data->glyphs);
}

static void text_static_draw(
    Text_Static*            text_static,
    SDL_GPUCommandBuffer*   cmd_buf,
    SDL_GPURenderPass*      render_pass,
    const Text_Static_Data& text_data,
    const HMM_Mat4&         view_to_clip_transform,
    HMM_Vec3                camera_position,
    bool                    fog_enabled,
    HMM_Vec4                fog_color,
    const Text_Style&       style = {}) {
  SDL_assert(text_static != nullptr);
  SDL_assert(cmd_buf != nullptr);
  SDL_assert(render_pass != nullptr);

  SDL_BindGPUGraphicsPipeline(render_pass, text_static->pipeline);
  SDL_BindGPUVertexStorageBuffers(render_pass, 0, &text_data.glyphs, 1);

  {
    SDL_GPUTextureSamplerBinding binding = {};
    binding.texture                      = text_data.font_atlas->texture;
    binding.sampler                      = text_static->sampler;
    SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
  }

  {
    Text_Static_Vertex_Uniforms uniforms = {};
    uniforms.view_to_clip_transform      = view_to_clip_transform;
    uniforms.camera_position             = camera_position;
    SDL_PushGPUVertexUniformData(cmd_buf, 0, &uniforms, sizeof(uniforms));
  }

  {
    Text_Static_Fragment_Uniforms uniforms = {};
    uniforms.font_size                     = text_data.font_atlas->size;
    uniforms.unit_range =
        HMM_V2(text_data.font_atlas->distance_range, text_data.font_atlas->distance_range) /
        HMM_V2(text_data.font_atlas->width, text_data.font_atlas->height);
    uniforms.color             = style.color;
    uniforms.outline_color     = style.outline_color;
    uniforms.outline_thickness = style.outline_thickness;
    uniforms.fog_color         = fog_color;
    uniforms.fog_enabled       = static_cast<uint32_t>(fog_enabled);
    SDL_PushGPUFragmentUniformData(cmd_buf, 0, &uniforms, sizeof(uniforms));
  }

  SDL_DrawGPUPrimitives(render_pass, text_data.glyphs_count * TEXT_INDICES_PER_GLYPH, 1, 0, 0);
}
