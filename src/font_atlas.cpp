#include "font_atlas.h"
#include "util.h"
#include <json.hpp>
#include <stb_image.h>

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

void from_json(const nlohmann::json& j, Font_Atlas_Variant& variant) {
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
    variant.kernings[font_atlas_pack_kerning(kerning.unicode1, kerning.unicode2)] = kerning.advance;
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

uint64_t font_atlas_pack_kerning(int unicode1, int unicode2) {
  return static_cast<uint64_t>(static_cast<uint32_t>(unicode1)) << 32 |
         static_cast<uint32_t>(unicode2);
}

bool font_atlas_load(
    Font_Atlas*      font_atlas,
    Font_Atlas_Kind  kind,
    SDL_Storage*     storage,
    SDL_GPUDevice*   device,
    SDL_GPUCopyPass* copy_pass) {
  SDL_assert(font_atlas != nullptr);
  SDL_assert(device != nullptr);
  SDL_assert(copy_pass != nullptr);

  static constexpr const char* font_atlas_kind_names[FONT_ATLAS_KIND_COUNT] = {
      "roboto",
      "science_gothic",
      "limelight",
  };
  auto atlas_name = font_atlas_kind_names[kind];

  std::string json_file_path = std::string("res/") + atlas_name + ".json";
  std::string json_file_contents;
  if (!read_storage_file(storage, json_file_path.c_str(), &json_file_contents)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to read json file contents: %s",
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

  int                  x, y, n;
  std::string          png_file_path = std::string("res/") + atlas_name + ".png";
  std::vector<uint8_t> png_file_contents;
  if (!read_storage_file(storage, png_file_path.c_str(), &png_file_contents)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to read png file contents: %s",
        json_file_path.c_str());
    return false;
  }
  auto pixels =
      stbi_load_from_memory(png_file_contents.data(), png_file_contents.size(), &x, &y, &n, 4);
  if (pixels == nullptr) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to load image data from: %s",
        png_file_path.c_str());
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
      return false;
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

void font_atlas_destroy(Font_Atlas* font_atlas, SDL_GPUDevice* device) {
  SDL_assert(device != nullptr);

  SDL_ReleaseGPUTexture(device, font_atlas->texture);
}
