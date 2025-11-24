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
  std::unordered_map<int, Font_Glyph> glyphs;
  float                               line_height;
  float                               ascender;
  float                               descender;
};

struct Font_Atlas {
  std::vector<Font_Variant> variants;
  float                     distance_range;
  float                     size;
  int                       width;
  int                       height;
  SDL_GPUTexture*           texture;
};

void from_json(const nlohmann::json& j, Font_Glyph_Bounds& bounds) {
  j.at("left").get_to(bounds.left);
  j.at("bottom").get_to(bounds.bottom);
  j.at("right").get_to(bounds.right);
  j.at("top").get_to(bounds.top);
}

void from_json(const nlohmann::json& j, Font_Glyph& glyph) {
  j.at("unicode").get_to(glyph.unicode);
  j.at("advance").get_to(glyph.horizontal_advance);
  if (j.contains("planeBounds")) { j.at("planeBounds").get_to(glyph.plane_bounds); }
  if (j.contains("atlasBounds")) { j.at("atlasBounds").get_to(glyph.atlas_bounds); }
}

void from_json(const nlohmann::json& j, Font_Variant& variant) {
  auto metrics_j = j.at("metrics");
  metrics_j.at("lineHeight").get_to(variant.line_height);
  metrics_j.at("ascender").get_to(variant.ascender);
  metrics_j.at("descender").get_to(variant.descender);

  std::vector<Font_Glyph> glyphs;
  j.at("glyphs").get_to(glyphs);
  for (const auto& glyph : glyphs) { variant.glyphs[glyph.unicode] = glyph; }
}

void from_json(const nlohmann::json& j, Font_Atlas& font_atlas) {
  auto atlas_j = j.at("atlas");
  atlas_j.at("distanceRange").get_to(font_atlas.distance_range);
  atlas_j.at("size").get_to(font_atlas.size);
  atlas_j.at("width").get_to(font_atlas.width);
  atlas_j.at("height").get_to(font_atlas.height);

  j.at("variants").get_to(font_atlas.variants);
}

static bool font_atlas_load(
    Font_Atlas*        font_atlas,
    const std::string& base_path,
    const char*        atlas_name,
    SDL_GPUDevice*     device,
    SDL_GPUCopyPass*   copy_pass) {
  SDL_assert(font_atlas != nullptr);
  SDL_assert(atlas_name != nullptr);
  SDL_assert(device != nullptr);
  SDL_assert(copy_pass != nullptr);

  auto        json_file_path = base_path + "/" + atlas_name + ".json";
  std::string json_file_contents;
  if (!read_file_contents(json_file_path, &json_file_contents)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to read file contents: %s",
        json_file_path.c_str());
    return false;
  }

  try {
    auto json   = nlohmann::json::parse(json_file_contents);
    *font_atlas = json;
  } catch (const nlohmann::json::exception& e) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to parse json: %s", e.what());
    return false;
  }

  int  x, y, n;
  auto png_file_path = base_path + "/" + atlas_name + ".png";
  auto pixels        = stbi_load(png_file_path.c_str(), &x, &y, &n, 4);
  if (pixels == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load image data from: %s", png_file_path);
    return false;
  }
  defer(stbi_image_free(pixels));

  {
    SDL_GPUTextureCreateInfo info = {};
    info.type                     = SDL_GPU_TEXTURETYPE_2D;
    info.format                   = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.width                    = font_atlas->width;
    info.height                   = font_atlas->height;
    info.layer_count_or_depth     = 1;
    info.num_levels               = 1;
    info.usage                    = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    font_atlas->texture           = SDL_CreateGPUTexture(device, &info);
    if (font_atlas->texture == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture: %s", SDL_GetError());
      return nullptr;
    }
  }

  SDL_GPUTransferBuffer* transfer_buffer;
  {
    SDL_GPUTransferBufferCreateInfo info = {};
    info.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size                            = font_atlas->width * font_atlas->height * 4;
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

  auto pixels_ptr = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
  if (pixels_ptr == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to map transfer buffer: %s", SDL_GetError());
    return false;
  }
  SDL_memcpy(pixels_ptr, pixels, font_atlas->width * font_atlas->height * 4);
  SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

  {
    SDL_GPUTextureTransferInfo transfer_info = {};
    transfer_info.transfer_buffer            = transfer_buffer;
    transfer_info.offset                     = 0;
    SDL_GPUTextureRegion region              = {};
    region.texture                           = font_atlas->texture;
    region.w                                 = font_atlas->width;
    region.h                                 = font_atlas->height;
    region.d                                 = 1;
    SDL_UploadToGPUTexture(copy_pass, &transfer_info, &region, false);
  }

  return true;
}

static void font_atlas_destroy(Font_Atlas* font_atlas, SDL_GPUDevice* device) {
  SDL_assert(device != nullptr);

  SDL_ReleaseGPUTexture(device, font_atlas->texture);
}
