struct Text_Static_Glyph {
  HMM_Vec3 position;
  float    size;
  HMM_Vec4 plane_bounds;
  HMM_Vec4 atlas_bounds;
};

struct Text_Static_Data {
  SDL_GPUBuffer* glyphs;
};

struct Text_Static {
  SDL_GPUGraphicsPipeline* pipeline;
  SDL_GPUSampler*          sampler;
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
      auto                 file_path = base_path + "/text_static.vert." + file_ext;
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
      auto                 file_path = base_path + "/text_static.frag." + file_ext;
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
