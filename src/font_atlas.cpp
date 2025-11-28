enum Font_Atlas_Kind {
  FONT_ATLAS_KIND_ROBOTO,
  FONT_ATLAS_KIND_SCIENCE_GOTHIC,
  FONT_ATLAS_KIND_LIMELIGHT,
  FONT_ATLAS_KIND_COUNT,
};

enum Font_Atlas_Roboto_Variant {
  FONT_ATLAS_ROBOTO_VARIANT_REGULAR,
  FONT_ATLAS_ROBOTO_VARIANT_BOLD,
  FONT_ATLAS_ROBOTO_VARIANT_ITALIC,
  FONT_ATLAS_ROBOTO_VARIANT_BOLD_ITALIC,
  FONT_ATLAS_ROBOTO_VARIANT_LIGHT,
  FONT_ATLAS_ROBOTO_VARIANT_COUNT,
};

enum Font_Atlas_Science_Gothic_Variant {
  FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_REGULAR,
  FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_BOLD,
  FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_LIGHT,
  FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_COUNT,
};

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
};

struct Font_Kerning {
  int   unicode1;
  int   unicode2;
  float advance;
};

struct Pair_Int_Hash {
  size_t operator()(const std::pair<int, int>& p) const {
    size_t h1 = std::hash<int> {}(p.first);
    size_t h2 = std::hash<int> {}(p.second);
    return h1 ^ (h2 << 1);
  }
};

struct Font_Variant {
  std::unordered_map<int, Font_Glyph>                           glyphs;
  std::unordered_map<std::pair<int, int>, float, Pair_Int_Hash> kernings;
  float                                                         line_height;
  float                                                         ascender;
  float                                                         descender;
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

void from_json(const nlohmann::json& j, Font_Kerning& kerning) {
  j.at("unicode1").get_to(kerning.unicode1);
  j.at("unicode2").get_to(kerning.unicode2);
  j.at("advance").get_to(kerning.advance);
}

void from_json(const nlohmann::json& j, Font_Variant& variant) {
  auto metrics_j = j.at("metrics");
  metrics_j.at("lineHeight").get_to(variant.line_height);
  metrics_j.at("ascender").get_to(variant.ascender);
  metrics_j.at("descender").get_to(variant.descender);

  std::vector<Font_Glyph> glyphs;
  j.at("glyphs").get_to(glyphs);
  for (const auto& glyph : glyphs) { variant.glyphs[glyph.unicode] = glyph; }

  std::vector<Font_Kerning> kernings;
  j.at("kerning").get_to(kernings);
  for (const auto& kerning : kernings) {
    variant.kernings[std::make_pair(kerning.unicode1, kerning.unicode2)] = kerning.advance;
  }
}

void from_json(const nlohmann::json& j, Font_Atlas& font_atlas) {
  auto atlas_j = j.at("atlas");
  atlas_j.at("distanceRange").get_to(font_atlas.distance_range);
  atlas_j.at("size").get_to(font_atlas.size);
  atlas_j.at("width").get_to(font_atlas.width);
  atlas_j.at("height").get_to(font_atlas.height);

  if (j.contains("variants")) {
    j.at("variants").get_to(font_atlas.variants);
  } else {
    auto& font_variant = font_atlas.variants.emplace_back();
    from_json(j, font_variant);
  }
}

static bool font_atlas_load(
    Font_Atlas*        font_atlas,
    Font_Atlas_Kind    kind,
    const std::string& base_path,
    SDL_GPUDevice*     device,
    SDL_GPUCopyPass*   copy_pass) {
  SDL_assert(font_atlas != nullptr);
  SDL_assert(device != nullptr);
  SDL_assert(copy_pass != nullptr);

  static constexpr const char* font_atlas_kind_names[FONT_ATLAS_KIND_COUNT] = {
      "roboto",
      "science_gothic",
      "limelight",
  };
  auto atlas_name = font_atlas_kind_names[kind];

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

static float
font_atlas_string_width(const Font_Variant& font_data, std::string_view text, float size) {
  float       width          = 0.0f;
  const char* ptr            = text.data();
  auto        str_size       = text.size();
  int         codepoint      = SDL_INVALID_UNICODE_CODEPOINT;
  int         prev_codepoint = 0;
  while (codepoint != 0) {
    codepoint = SDL_StepUTF8(&ptr, &str_size);
    if (codepoint == SDL_INVALID_UNICODE_CODEPOINT) { continue; }

    auto glyph_it = font_data.glyphs.find(codepoint);
    if (glyph_it == font_data.glyphs.end()) { continue; }

    if (prev_codepoint != 0) {
      auto kerning_it = font_data.kernings.find(std::make_pair(prev_codepoint, codepoint));
      if (kerning_it != font_data.kernings.end()) { width += kerning_it->second * size; }
    }
    prev_codepoint = codepoint;

    width += glyph_it->second.horizontal_advance * size;
  }
  return width;
}

static HMM_Vec2 font_atlas_string_multiline_block_size(
    const Font_Variant& font_data,
    std::string_view    text,
    float               size) {
  int         lines_count    = 1;
  float       max_line_width = 0.0f;
  const char* ptr            = text.data();
  auto        str_size       = text.size();
  const char* line_start     = ptr;
  int         codepoint      = SDL_INVALID_UNICODE_CODEPOINT;
  while (codepoint != 0) {
    codepoint = SDL_StepUTF8(&ptr, &str_size);
    if (codepoint == SDL_INVALID_UNICODE_CODEPOINT) { continue; }

    if (codepoint == 10) {
      std::string_view line(line_start, static_cast<size_t>(ptr - line_start - 1));
      float            line_width = font_atlas_string_width(font_data, line, size);
      max_line_width              = std::max(max_line_width, line_width);
      line_start                  = ptr;
      lines_count += 1;
    }
  }

  if (ptr > line_start) {
    std::string_view line(line_start, static_cast<size_t>(ptr - line_start));
    float            line_width = font_atlas_string_width(font_data, line, size);
    max_line_width              = std::max(max_line_width, line_width);
  }

  return HMM_V2(max_line_width, lines_count * font_data.line_height * size);
}
