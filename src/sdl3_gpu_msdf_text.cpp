// -- External Header Includes ------------------------------------------------
#include <HandmadeMath.h>
#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <json.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// -- Std Header Includes -----------------------------------------------------
#include <unordered_map>
#include <vector>

// -- Local Source Includes ---------------------------------------------------
#include "common.cpp"
#include "imgui_font.cpp"
#include "demo_strings.cpp"
#include "font_atlas.cpp"
#include "text.cpp"
#include "text_batch.cpp"
#include "text_static.cpp"

// TODOs:
// - MSDFA atlases and a blur effect example.
// - Add on hover descriptions for demo kinds.

enum Demo_Kind {
  DEMO_KIND_TEXT_BATCH_SINGLELINE,
  DEMO_KIND_TEXT_BATCH_MULTILINE,
  DEMO_KIND_TEXT_BATCH_STARWARS,
  DEMO_KIND_TEXT_STATIC_SHAKESPEARE,
  DEMO_KIND_COUNT,
};

struct App_State {
  std::string          base_path;
  SDL_GPUDevice*       device;
  SDL_Window*          window;
  SDL_GPUTextureFormat swapchain_texture_format;
  float                content_scale;
  HMM_Vec2             window_size_pixels;
  bool                 window_minimized;
  int                  supported_sample_counts;
  uint64_t             count_per_second;
  uint64_t             last_counter;
  uint64_t             max_counter_delta;
  bool                 vsync      = true;
  bool                 fullscreen = false;

  ImFont* imgui_font;

  Font_Atlas_Kind font_atlas_kind;
  int             font_variant;
  Font_Atlas      font_atlases[FONT_ATLAS_KIND_COUNT];
  Text_Batch      text_batch;
  Text_Static     text_static;
  Demo_Kind       demo_kind;
  HMM_Vec2        text_block_size;
  HMM_Vec4        bg_color               = HMM_V4(0.078f, 0.076f, 0.069f, 1.0f);
  HMM_Mat4        view_to_clip_transform = HMM_M4D(1.0f);
  float           text_size;
  Text_Align      text_align;
  Text_Style      text_style;
  struct {
    std::string text;
  } demo_singleline;
  struct {
    HMM_Vec2 camera_position;
    float    camera_zoom = 1.0f;
  } demo_multiline;
  struct {
    float scroll_position;
    float scroll_speed;
    float fade_out_timer;
    float fade_out_duration;
  } demo_starwars;
  struct {
    Text_Static_Data text_static_data;
    HMM_Vec3         camera_position;
    float            camera_yaw;
    float            camera_pitch;
    bool             mouse_captured;
    float            mouse_sensitivity = 0.00085f;
    float            move_speed        = 500.0f;
    bool             fog_enabled       = true;
  } demo_static;
};

static void update_demo_view_to_clip_transform(App_State* as) {
  switch (as->demo_kind) {
  case DEMO_KIND_TEXT_BATCH_SINGLELINE:
  case DEMO_KIND_TEXT_BATCH_MULTILINE:
    as->view_to_clip_transform = HMM_Orthographic_RH_NO(
        0.0f,
        as->window_size_pixels.X,
        0.0f,
        as->window_size_pixels.Y,
        -1.0f,
        1.0f);
    break;
  case DEMO_KIND_TEXT_BATCH_STARWARS:
  case DEMO_KIND_TEXT_STATIC_SHAKESPEARE:
    as->view_to_clip_transform = HMM_Perspective_RH_NO(
        HMM_ToRad(45.0f),
        as->window_size_pixels.X / as->window_size_pixels.Y,
        0.1f,
        1000.0f);
    break;
  default:
    break;
  }
}

static void on_window_pixel_size_changed(App_State* as, int width, int height) {
  if (as->window_size_pixels.X == width && as->window_size_pixels.Y == height) { return; }
  as->window_size_pixels = HMM_V2(width, height);

  update_demo_view_to_clip_transform(as);
}

static void on_display_content_scale_changed(App_State* as, float content_scale) {
  as->content_scale = content_scale;

  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(as->content_scale);
  style.FontScaleDpi = as->content_scale;
}

static void on_vsync_changed(App_State* as, bool vsync) {
  as->vsync = vsync;

  SDL_GPUPresentMode present_mode =
      vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
  SDL_SetGPUSwapchainParameters(
      as->device,
      as->window,
      SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
      present_mode);
}

static void on_demo_kind_selection(App_State* as, Demo_Kind kind) {
  as->demo_kind = kind;

  switch (as->demo_kind) {
  case DEMO_KIND_TEXT_BATCH_SINGLELINE:
    as->font_atlas_kind       = FONT_ATLAS_KIND_ROBOTO;
    as->font_variant          = FONT_ATLAS_ROBOTO_VARIANT_BOLD_ITALIC;
    as->bg_color              = HMM_V4(0.97f, 0.95f, 0.86f, 1.0f);
    as->text_size             = as->font_atlases[as->font_atlas_kind].size;
    as->text_align.horizontal = TEXT_H_ALIGN_CENTER;
    as->text_align.vertical   = TEXT_V_ALIGN_BASELINE;
    as->text_style.color      = HMM_V4(0.024f, 0.02f, 0.019f, 1.0f);
    as->demo_singleline.text  = "Example Text!";
    break;
  case DEMO_KIND_TEXT_BATCH_MULTILINE:
    as->font_variant    = 0;
    as->font_atlas_kind = FONT_ATLAS_KIND_LIMELIGHT;
    as->bg_color        = HMM_V4(0.97f, 0.95f, 0.86f, 1.0f);
    as->text_size       = as->font_atlases[as->font_atlas_kind].size;
    as->text_block_size = text_measure_string_size(
        as->font_atlases[as->font_atlas_kind].variants[as->font_variant],
        demo_string_lorem_ipsum,
        as->text_size);
    as->text_align.horizontal          = TEXT_H_ALIGN_LEFT;
    as->text_align.vertical            = TEXT_V_ALIGN_MIDDLE;
    as->text_style.color               = HMM_V4(0.024f, 0.02f, 0.019f, 1.0f);
    as->demo_multiline.camera_position = as->window_size_pixels * 0.5f;
    as->demo_multiline.camera_zoom     = 1.0f;
    break;
  case DEMO_KIND_TEXT_BATCH_STARWARS:
    as->demo_starwars.scroll_position = 250.0f;
    as->demo_starwars.scroll_speed    = 60.0f;
    as->font_atlas_kind               = FONT_ATLAS_KIND_SCIENCE_GOTHIC;
    as->font_variant                  = FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_BOLD;
    as->bg_color                      = HMM_V4(0.0f, 0.0f, 0.0f, 1.0f);
    as->text_size                     = as->font_atlases[as->font_atlas_kind].size * 0.5f;
    as->text_block_size               = text_measure_string_size(
        as->font_atlases[as->font_atlas_kind].variants[as->font_variant],
        demo_string_star_wars,
        as->text_size);
    as->text_align.horizontal        = TEXT_H_ALIGN_CENTER;
    as->text_align.vertical          = TEXT_V_ALIGN_TOP;
    as->text_style.color             = HMM_V4(0.014f, 0.985f, 0.998f, 1.0f);
    as->text_style.outline_color     = HMM_V4(0.998f, 0.987f, 0.997f, 1.0f);
    as->text_style.outline_thickness = 0.4f;
    break;
  case DEMO_KIND_TEXT_STATIC_SHAKESPEARE:
    as->font_atlas_kind             = FONT_ATLAS_KIND_ROBOTO;
    as->font_variant                = FONT_ATLAS_ROBOTO_VARIANT_BOLD;
    as->bg_color                    = HMM_V4(0.97f, 0.95f, 0.86f, 1.0f);
    as->text_size                   = as->font_atlases[as->font_atlas_kind].size;
    as->text_align                  = {};
    as->text_style                  = {};
    as->text_style.color            = HMM_V4(0.024f, 0.02f, 0.019f, 1.0f);
    as->demo_static.camera_position = HMM_V3(0.0f, 0.0f, 600.0f);
    as->demo_static.camera_yaw      = 0.0f;
    as->demo_static.camera_pitch    = 0.0f;
    as->demo_static.mouse_captured  = false;
    break;
  default:
    break;
  }

  update_demo_view_to_clip_transform(as);
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to init SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  auto as = new (std::nothrow) App_State {};
  if (as == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate App_State");
    return SDL_APP_FAILURE;
  }
  *appstate = as;

  as->base_path = SDL_GetBasePath();

  SDL_GPUShaderFormat format_flags = 0;
#ifdef SDL_PLATFORM_WINDOWS
  format_flags |= SDL_GPU_SHADERFORMAT_DXIL;
#elif SDL_PLATFORM_LINUX
  format_flags |= SDL_GPU_SHADERFORMAT_SPIRV;
#else
#error "Platform not supported"
#endif
  bool debug = false;
#ifdef BUILD_DEBUG
  debug = true;
#endif
  as->device = SDL_CreateGPUDevice(format_flags, debug, nullptr);
  if (as->device == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create gpu device: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  int   window_width       = 800;
  int   window_height      = 600;
  float content_scale      = 1.0f;
  auto  primary_display_id = SDL_GetPrimaryDisplay();
  if (primary_display_id != 0) {
    auto display_mode = SDL_GetDesktopDisplayMode(primary_display_id);
    if (display_mode != nullptr) {
      window_width  = display_mode->w / 2;
      window_height = display_mode->h / 2;
    } else {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to get primary display mode: %s",
          SDL_GetError());
    }
    content_scale = SDL_GetDisplayContentScale(primary_display_id);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get primary display: %s", SDL_GetError());
  }

  as->window = SDL_CreateWindow(
      "SDL3 GPU MSDF TEXT",
      window_width,
      window_height,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (as->window == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_ClaimWindowForGPUDevice(as->device, as->window)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to claim window for gpu device: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }

  as->swapchain_texture_format = SDL_GetGPUSwapchainTextureFormat(as->device, as->window);

  {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    auto& style         = ImGui::GetStyle();
    style.ItemSpacing.y = 8.0f;

    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    as->imgui_font                = io.Fonts->AddFontFromMemoryCompressedTTF(
        const_cast<unsigned char*>(IMGUI_FONT_DATA),
        IMGUI_FONT_DATA_SIZE,
        18.0f,
        &font_cfg);

    on_display_content_scale_changed(as, content_scale);
  }

  {
    ImGui_ImplSDL3_InitForSDLGPU(as->window);

    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device                     = as->device;
    init_info.ColorTargetFormat          = SDL_GetGPUSwapchainTextureFormat(as->device, as->window);
    ImGui_ImplSDLGPU3_Init(&init_info);
  }

  if (!text_batch_create(
          &as->text_batch,
          as->base_path,
          as->device,
          as->swapchain_texture_format)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create text batch");
    return SDL_APP_FAILURE;
  }

  if (!text_static_create(
          &as->text_static,
          as->base_path,
          as->device,
          as->swapchain_texture_format)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create text static");
    return SDL_APP_FAILURE;
  }

  {
    auto cmd_buf = SDL_AcquireGPUCommandBuffer(as->device);
    if (cmd_buf == nullptr) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to acquire command buffer: %s",
          SDL_GetError());
      return SDL_APP_FAILURE;
    }
    defer(SDL_SubmitGPUCommandBuffer(cmd_buf));

    {
      auto copy_pass = SDL_BeginGPUCopyPass(cmd_buf);
      defer(SDL_EndGPUCopyPass(copy_pass));

      for (int i = 0; i < FONT_ATLAS_KIND_COUNT; i++) {
        if (!font_atlas_load(
                &as->font_atlases[i],
                static_cast<Font_Atlas_Kind>(i),
                as->base_path,
                as->device,
                copy_pass)) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load font atlas");
          return SDL_APP_FAILURE;
        }
      }

      std::string demo_string_shakespeare;
      {
        auto file_path = as->base_path + "/res/shakespeare.txt";
        if (!read_file_contents(file_path, &demo_string_shakespeare)) {
          SDL_LogError(
              SDL_LOG_CATEGORY_APPLICATION,
              "Failed to read file contents: %s",
              file_path.c_str());
          return SDL_APP_FAILURE;
        }
      }
      if (!text_static_data_create_multiline(
              &as->demo_static.text_static_data,
              as->device,
              copy_pass,
              &as->font_atlases[FONT_ATLAS_KIND_ROBOTO],
              FONT_ATLAS_ROBOTO_VARIANT_BOLD,
              demo_string_shakespeare,
              HMM_V3(0.0f, 0.0f, 0.0f),
              128.0f)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create text static data");
        return SDL_APP_FAILURE;
      }
    }
  }

  on_vsync_changed(as, as->vsync);
  {
    int w, h;
    SDL_GetWindowSizeInPixels(as->window, &w, &h);
    on_window_pixel_size_changed(as, w, h);
  }
  on_demo_kind_selection(as, DEMO_KIND_TEXT_BATCH_SINGLELINE);

  as->count_per_second  = SDL_GetPerformanceFrequency();
  as->last_counter      = SDL_GetPerformanceCounter();
  as->max_counter_delta = as->count_per_second / 60 * 8;

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  auto as = static_cast<App_State*>(appstate);

  auto& io                  = ImGui::GetIO();
  bool  process_imgui_event = true;

  switch (event->type) {
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    on_window_pixel_size_changed(as, event->window.data1, event->window.data2);
    break;
  case SDL_EVENT_WINDOW_MINIMIZED:
    as->window_minimized = true;
    break;
  case SDL_EVENT_WINDOW_RESTORED:
    as->window_minimized = false;
    break;
  case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
    on_display_content_scale_changed(as, SDL_GetDisplayContentScale(event->display.displayID));
    break;
  case SDL_EVENT_MOUSE_MOTION: {
    if (!io.WantCaptureMouse) {
      if (as->demo_kind == DEMO_KIND_TEXT_BATCH_MULTILINE) {
        if ((event->motion.state & SDL_BUTTON_LEFT) != 0) {
          as->demo_multiline.camera_position += HMM_V2(event->motion.xrel, -event->motion.yrel);
        }
      }
      if (as->demo_kind == DEMO_KIND_TEXT_STATIC_SHAKESPEARE) {
        if (as->demo_static.mouse_captured) {
          as->demo_static.camera_yaw += event->motion.xrel * as->demo_static.mouse_sensitivity;
          as->demo_static.camera_pitch -= event->motion.yrel * as->demo_static.mouse_sensitivity;
          as->demo_static.camera_pitch =
              HMM_Clamp(-HMM_PI32 * 0.49f, as->demo_static.camera_pitch, HMM_PI32 * 0.49f);
          process_imgui_event = false;
        }
      }
    }
  } break;
  case SDL_EVENT_MOUSE_WHEEL: {
    if (!io.WantCaptureMouse) {
      if (as->demo_kind == DEMO_KIND_TEXT_BATCH_MULTILINE) {
        HMM_Vec2 mouse_pos =
            HMM_V2(event->wheel.mouse_x, as->window_size_pixels.Y - event->wheel.mouse_y);
        HMM_Vec2 last_mouse_world =
            (mouse_pos - as->demo_multiline.camera_position) / as->demo_multiline.camera_zoom;

        as->demo_multiline.camera_zoom += event->wheel.y * 0.1f * as->demo_multiline.camera_zoom;
        as->demo_multiline.camera_zoom = HMM_Clamp(0.25f, as->demo_multiline.camera_zoom, 15.0f);

        HMM_Vec2 mouse_world =
            (mouse_pos - as->demo_multiline.camera_position) / as->demo_multiline.camera_zoom;
        as->demo_multiline.camera_position +=
            (mouse_world - last_mouse_world) * as->demo_multiline.camera_zoom;
      }
    }
  } break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    if (!io.WantCaptureMouse) {
      if (as->demo_kind == DEMO_KIND_TEXT_STATIC_SHAKESPEARE &&
          event->button.button == SDL_BUTTON_LEFT) {
        as->demo_static.mouse_captured = true;
        SDL_SetWindowRelativeMouseMode(as->window, true);
      }
    }
  } break;
  case SDL_EVENT_KEY_DOWN: {
    if (!io.WantCaptureKeyboard) {
      if (as->demo_kind == DEMO_KIND_TEXT_STATIC_SHAKESPEARE && event->key.key == SDLK_ESCAPE) {
        as->demo_static.mouse_captured = false;
        SDL_SetWindowRelativeMouseMode(as->window, false);
      }
    }
  } break;
  }

  if (process_imgui_event) { ImGui_ImplSDL3_ProcessEvent(event); }

  return SDL_APP_CONTINUE;
}

static void update_and_draw_demo(App_State* as, float dt) {
  switch (as->demo_kind) {
  case DEMO_KIND_TEXT_BATCH_SINGLELINE: {
    text_batch_begin(
        &as->text_batch,
        as->view_to_clip_transform,
        &as->font_atlases[as->font_atlas_kind],
        as->font_variant);
    text_batch_draw(
        &as->text_batch,
        as->demo_singleline.text,
        HMM_V3(as->window_size_pixels.X * 0.5f, as->window_size_pixels.Y * 0.5f, 0.0f),
        as->text_size,
        as->text_align,
        as->text_style);
    text_batch_end(&as->text_batch);
  } break;
  case DEMO_KIND_TEXT_BATCH_MULTILINE: {
    auto camera_pos  = as->demo_multiline.camera_position;
    auto translation = HMM_Translate(HMM_V3(camera_pos.X, camera_pos.Y, 0.0f));
    auto scale =
        HMM_Scale(HMM_V3(as->demo_multiline.camera_zoom, as->demo_multiline.camera_zoom, 1.0f));
    auto world_to_view_transform = translation * scale;
    auto world_to_clip_transform = as->view_to_clip_transform * world_to_view_transform;

    text_batch_begin(
        &as->text_batch,
        world_to_clip_transform,
        &as->font_atlases[as->font_atlas_kind],
        as->font_variant);
    text_batch_draw_multiline(
        &as->text_batch,
        demo_string_lorem_ipsum,
        HMM_V3(-as->text_block_size.X - 48.0f, 0.0f, 0.0f),
        as->text_size,
        as->text_align,
        as->text_style,
        as->text_block_size);
    text_batch_draw_multiline(
        &as->text_batch,
        demo_string_lorem_ipsum,
        HMM_V3(0.0f, 0.0f, 0.0f),
        as->text_size,
        as->text_align,
        as->text_style,
        as->text_block_size);
    text_batch_draw_multiline(
        &as->text_batch,
        demo_string_lorem_ipsum,
        HMM_V3(as->text_block_size.X + 48.0f, 0.0f, 0.0f),
        as->text_size,
        as->text_align,
        as->text_style,
        as->text_block_size);
    text_batch_end(&as->text_batch);
  } break;
  case DEMO_KIND_TEXT_BATCH_STARWARS: {
    float alpha = as->text_style.color.A;
    if (as->demo_starwars.scroll_position > 2500.0f) {
      if (as->demo_starwars.fade_out_timer > 0.0f) {
        float progress = as->demo_starwars.fade_out_timer / as->demo_starwars.fade_out_duration;
        alpha          = HMM_Lerp(as->text_style.color.A, 1.0f - progress, 0.0f);

        as->demo_starwars.fade_out_timer -= dt;
        if (as->demo_starwars.fade_out_timer <= 0.0f) {
          on_demo_kind_selection(as, DEMO_KIND_TEXT_BATCH_STARWARS);
        }
      } else {
        as->demo_starwars.fade_out_duration = 5.0f;
        as->demo_starwars.fade_out_timer += as->demo_starwars.fade_out_duration;
      }
    }

    as->demo_starwars.scroll_position += as->demo_starwars.scroll_speed * dt;

    static constexpr HMM_Vec3 camera_position = {0.0f, 0.0f, 600.0f};
    static constexpr HMM_Vec3 camera_target   = {0.0f, 800.0f, 0.0f};
    static constexpr HMM_Vec3 camera_up       = {0.0f, 1.0f, 0.0f};
    auto world_to_view_transform = HMM_LookAt_RH(camera_position, camera_target, camera_up);
    auto world_to_clip_transform = as->view_to_clip_transform * world_to_view_transform;

    auto style = as->text_style;
    style.color =
        HMM_V4(as->text_style.color.R, as->text_style.color.G, as->text_style.color.B, alpha);
    style.outline_color = HMM_V4(
        as->text_style.outline_color.R,
        as->text_style.outline_color.G,
        as->text_style.outline_color.B,
        alpha);

    text_batch_begin(
        &as->text_batch,
        world_to_clip_transform,
        &as->font_atlases[as->font_atlas_kind],
        as->font_variant);
    text_batch_draw_multiline(
        &as->text_batch,
        demo_string_star_wars,
        HMM_V3(0.0f, as->demo_starwars.scroll_position, 0.0f),
        as->text_size,
        as->text_align,
        style,
        as->text_block_size);
    text_batch_end(&as->text_batch);
  } break;
  case DEMO_KIND_TEXT_STATIC_SHAKESPEARE: {
    HMM_Vec3 forward = HMM_V3(
        HMM_SinF(as->demo_static.camera_yaw) * HMM_CosF(as->demo_static.camera_pitch),
        HMM_SinF(as->demo_static.camera_pitch),
        -HMM_CosF(as->demo_static.camera_yaw) * HMM_CosF(as->demo_static.camera_pitch));
    HMM_Vec3 right =
        HMM_V3(HMM_CosF(as->demo_static.camera_yaw), 0.0f, HMM_SinF(as->demo_static.camera_yaw));

    auto  key_state  = SDL_GetKeyboardState(nullptr);
    float move_speed = as->demo_static.move_speed * dt;

    if (key_state[SDL_SCANCODE_W]) { as->demo_static.camera_position += forward * move_speed; }
    if (key_state[SDL_SCANCODE_S]) { as->demo_static.camera_position -= forward * move_speed; }
    if (key_state[SDL_SCANCODE_D]) { as->demo_static.camera_position += right * move_speed; }
    if (key_state[SDL_SCANCODE_A]) { as->demo_static.camera_position -= right * move_speed; }
  } break;
  default:
    break;
  }
}

static void draw_imgui(App_State* as) {
  if (ImGui::Begin("SDL3 GPU MSDF Text Demo", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {
    static constexpr const char* demo_kind_strings[DEMO_KIND_COUNT] = {
        "Text Batch Single-Line",
        "Text Batch Multi-Line",
        "Text Batch Star Wars",
        "Text Static Shakespeare",
    };
    if (ImGui::BeginCombo("Demo Selection", demo_kind_strings[as->demo_kind])) {
      for (int i = 0; i < DEMO_KIND_COUNT; i++) {
        bool is_selected = as->demo_kind == i;
        if (ImGui::Selectable(demo_kind_strings[i], is_selected)) {
          on_demo_kind_selection(as, static_cast<Demo_Kind>(i));
        }
        if (is_selected) { ImGui::SetItemDefaultFocus(); }
      }
      ImGui::EndCombo();
    }

    bool vsync = as->vsync;
    if (ImGui::Checkbox("VSync", &vsync)) { on_vsync_changed(as, vsync); }
    if (ImGui::Button("Toggle Fullscreen")) {
      as->fullscreen = !as->fullscreen;
      SDL_SetWindowFullscreen(as->window, as->fullscreen);
    }

    auto& io = ImGui::GetIO();
    ImGui::Text(
        "Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / io.Framerate,
        io.Framerate);

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Demo Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
      const auto& font_atlas = as->font_atlases[as->font_atlas_kind];

      switch (as->demo_kind) {
      case DEMO_KIND_TEXT_BATCH_SINGLELINE: {
        auto resize_callback = [](ImGuiInputTextCallbackData* data) {
          if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            auto str = static_cast<std::string*>(data->UserData);
            str->resize(data->BufTextLen);
            data->Buf = str->data();
          }
          return 0;
        };
        ImGui::InputText(
            "Text",
            as->demo_singleline.text.data(),
            as->demo_singleline.text.capacity() + 1,
            ImGuiInputTextFlags_CallbackResize,
            resize_callback,
            &as->demo_singleline.text);

        ImGui::SliderFloat(
            "Text Size",
            &as->text_size,
            font_atlas.size * 0.5f,
            font_atlas.size * 4.0f,
            "%.0f");

        ImGui::LabelText(
            "Screen Pixel Range",
            "%f",
            as->text_size / font_atlas.size * font_atlas.distance_range);
      } break;
      case DEMO_KIND_TEXT_BATCH_MULTILINE: {
        ImGui::Text("Controls: Scroll to zoom in/out, Left mouse-down and move to pan.");
      } break;
      case DEMO_KIND_TEXT_BATCH_STARWARS: {
        ImGui::SliderFloat("Scroll Speed", &as->demo_starwars.scroll_speed, 0.0f, 200.0f, "%.0f");
      } break;
      case DEMO_KIND_TEXT_STATIC_SHAKESPEARE: {
        ImGui::Text("Controls: WSAD to move, Mouse move to aim.");
        ImGui::Text(
            "Click on the screen to capture mouse. Press escape to uncapture and use the UI.");

        ImGui::LabelText(
            "Camera Position",
            "[%.3f, %.3f, %.3f]",
            as->demo_static.camera_position.X,
            as->demo_static.camera_position.Y,
            as->demo_static.camera_position.Z);

        ImGui::SliderFloat(
            "Aim Sensitivity",
            &as->demo_static.mouse_sensitivity,
            0.0001f,
            0.005f,
            "%.5f");
        ImGui::SliderFloat("Move Speed", &as->demo_static.move_speed, 100.0f, 10000.0f);

        ImGui::Checkbox("Fog Enabled", &as->demo_static.fog_enabled);
      } break;
      default:
        break;
      }
    }

    if (ImGui::Button("Reset")) { on_demo_kind_selection(as, as->demo_kind); }
    ImGui::ColorEdit4("Text Color", &as->text_style.color.X);
    ImGui::ColorEdit4("Text Outline Color", &as->text_style.outline_color.X);
    ImGui::SliderFloat(
        "Text Outline Thickness",
        &as->text_style.outline_thickness,
        0.0f,
        0.4f,
        "%.1f");

    ImGui::BeginDisabled(as->demo_kind == DEMO_KIND_TEXT_STATIC_SHAKESPEARE);
    static constexpr const char* text_h_align_strings[TEXT_H_ALIGN_COUNT] = {
        "Left",
        "Center",
        "Right",
    };
    static constexpr const char* text_v_align_strings[TEXT_V_ALIGN_COUNT] = {
        "Top",
        "Middle",
        "Baseline",
        "Bottom",
    };
    if (ImGui::BeginCombo(
            "Text Horizontal Align",
            text_h_align_strings[as->text_align.horizontal])) {
      for (int i = 0; i < TEXT_H_ALIGN_COUNT; i++) {
        bool is_selected = as->text_align.horizontal == i;
        if (ImGui::Selectable(text_h_align_strings[i], is_selected)) {
          as->text_align.horizontal = static_cast<Text_H_Align>(i);
        }
        if (is_selected) { ImGui::SetItemDefaultFocus(); }
      }
      ImGui::EndCombo();
    }
    if (ImGui::BeginCombo("Text Vertical Align", text_v_align_strings[as->text_align.vertical])) {
      for (int i = 0; i < TEXT_V_ALIGN_COUNT; i++) {
        bool is_selected = as->text_align.vertical == i;
        if (ImGui::Selectable(text_v_align_strings[i], is_selected)) {
          as->text_align.vertical = static_cast<Text_V_Align>(i);
        }
        if (is_selected) { ImGui::SetItemDefaultFocus(); }
      }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Font Atlas", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::BeginDisabled(as->demo_kind != DEMO_KIND_TEXT_BATCH_SINGLELINE);
      static constexpr const char* font_atlas_kind_strings[FONT_ATLAS_KIND_COUNT] = {
          "Roboto",
          "Science Gothic",
          "Limelight",
      };
      if (ImGui::BeginCombo("Font Selection", font_atlas_kind_strings[as->font_atlas_kind])) {
        for (int i = 0; i < FONT_ATLAS_KIND_COUNT; i++) {
          bool is_selected = as->font_atlas_kind == i;
          if (ImGui::Selectable(font_atlas_kind_strings[i], is_selected)) {
            as->font_atlas_kind = static_cast<Font_Atlas_Kind>(i);
            as->font_variant    = 0;
          }
          if (is_selected) { ImGui::SetItemDefaultFocus(); }
        }
        ImGui::EndCombo();
      }

      const auto& font_atlas = as->font_atlases[as->font_atlas_kind];

      static constexpr const char* font_roboto_variant_strings[FONT_ATLAS_ROBOTO_VARIANT_COUNT] = {
          "Regular",
          "Bold",
          "Italic",
          "Bold Italic",
          "Light",
      };
      static constexpr const char*
          font_science_gothic_variant_strings[FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_COUNT] = {
              "Regular",
              "Bold",
              "Light",
          };
      switch (as->font_atlas_kind) {
      case FONT_ATLAS_KIND_ROBOTO: {
        if (ImGui::BeginCombo(
                "Font Variant Selection",
                font_roboto_variant_strings[as->font_variant])) {
          for (int i = 0; i < FONT_ATLAS_ROBOTO_VARIANT_COUNT; i++) {
            bool is_selected = as->font_variant == i;
            if (ImGui::Selectable(font_roboto_variant_strings[i], is_selected)) {
              as->font_variant = i;
            }
            if (is_selected) { ImGui::SetItemDefaultFocus(); }
          }
          ImGui::EndCombo();
        }
      } break;
      case FONT_ATLAS_KIND_SCIENCE_GOTHIC:
        if (ImGui::BeginCombo(
                "Font Variant Selection",
                font_science_gothic_variant_strings[as->font_variant])) {
          for (int i = 0; i < FONT_ATLAS_SCIENCE_GOTHIC_VARIANT_COUNT; i++) {
            bool is_selected = as->font_variant == i;
            if (ImGui::Selectable(font_science_gothic_variant_strings[i], is_selected)) {
              as->font_variant = i;
            }
            if (is_selected) { ImGui::SetItemDefaultFocus(); }
          }
          ImGui::EndCombo();
        }
      default:
        break;
      }
      ImGui::EndDisabled();

      ImGui::LabelText("Width", "%d", font_atlas.width);
      ImGui::LabelText("Height", "%d", font_atlas.width);
      if (ImGui::TreeNode("Texture")) {
        ImGui::Image(
            static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(font_atlas.texture)),
            ImVec2(font_atlas.width, font_atlas.height));
        ImGui::TreePop();
      }
    }
  }
  ImGui::End();
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto as = static_cast<App_State*>(appstate);

  auto counter       = SDL_GetPerformanceCounter();
  auto counter_delta = counter - as->last_counter;
  as->last_counter   = counter;
  if (counter_delta > as->max_counter_delta) { counter_delta = as->count_per_second / 60; }

  auto delta_time = static_cast<double>(counter_delta) / static_cast<double>(as->count_per_second);

  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  draw_imgui(as);
  ImGui::Render();

  SDL_GPUCommandBuffer* cmd_buf = SDL_AcquireGPUCommandBuffer(as->device);
  if (cmd_buf == nullptr) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to acquire command buffer: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUTexture* swapchain_texture;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(
          cmd_buf,
          as->window,
          &swapchain_texture,
          nullptr,
          nullptr)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to acquire swapchain texture: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (swapchain_texture != nullptr && !as->window_minimized) {
    update_and_draw_demo(as, static_cast<float>(delta_time));

    ImDrawData* draw_data = ImGui::GetDrawData();

    text_batch_prepare_draw_cmds(&as->text_batch, as->device, cmd_buf);

    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd_buf);

    {
      SDL_GPUColorTargetInfo target_info = {};
      target_info.texture                = swapchain_texture;
      target_info.clear_color = {as->bg_color.R, as->bg_color.G, as->bg_color.B, as->bg_color.A};
      target_info.load_op     = SDL_GPU_LOADOP_CLEAR;
      target_info.store_op    = SDL_GPU_STOREOP_STORE;
      SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmd_buf, &target_info, 1, nullptr);
      defer(SDL_EndGPURenderPass(render_pass));

      text_batch_render_draw_cmds(&as->text_batch, cmd_buf, render_pass);

      if (as->demo_kind == DEMO_KIND_TEXT_STATIC_SHAKESPEARE) {
        auto camera_forward = HMM_V3(
            HMM_SinF(as->demo_static.camera_yaw) * HMM_CosF(as->demo_static.camera_pitch),
            HMM_SinF(as->demo_static.camera_pitch),
            -HMM_CosF(as->demo_static.camera_yaw) * HMM_CosF(as->demo_static.camera_pitch));
        auto camera_up = HMM_V3(0.0f, 1.0f, 0.0f);

        // Create view matrix with rotation only (camera at origin looking in the direction).
        auto rotation_view_transform = HMM_LookAt_RH(HMM_V3(0, 0, 0), camera_forward, camera_up);
        auto view_to_clip_transform  = as->view_to_clip_transform * rotation_view_transform;

        text_static_draw(
            &as->text_static,
            cmd_buf,
            render_pass,
            as->demo_static.text_static_data,
            view_to_clip_transform,
            as->demo_static.camera_position,
            as->demo_static.fog_enabled,
            as->bg_color,
            as->text_style);
      }

      ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd_buf, render_pass);
    }
  }

  SDL_SubmitGPUCommandBuffer(cmd_buf);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  auto as = static_cast<App_State*>(appstate);

  SDL_WaitForGPUIdle(as->device);

  text_static_data_destroy(&as->demo_static.text_static_data, as->device);
  text_static_destroy(&as->text_static, as->device);
  text_batch_destroy(&as->text_batch, as->device);

  for (int i = 0; i < FONT_ATLAS_KIND_COUNT; i++) {
    font_atlas_destroy(&as->font_atlases[i], as->device);
  }

  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();

  SDL_ReleaseWindowFromGPUDevice(as->device, as->window);
  SDL_DestroyWindow(as->window);
  SDL_DestroyGPUDevice(as->device);

  delete as;

  SDL_Quit();
}
