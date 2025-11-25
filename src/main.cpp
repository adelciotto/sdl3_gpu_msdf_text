#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

// TODOs:
// - Skip whitespace char.
// - Add example and support for multi-line text. User can change line height.
// - Add example and support for effects. Outline, 3D bevel effect, blurred shadow.
// - Add example and support for 3D text.
// - Add star wars 3D scroll example.

enum Demo_Kind {
  DEMO_KIND_BASIC,
  DEMO_KIND_COUNT,
};

enum Font_Atlas_Kind {
  FONT_ATLAS_KIND_ROBOTO,
  FONT_ATLAS_KIND_SCIENCE_GOTHIC,
  FONT_ATLAS_KIND_PLAYWRITE,
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

struct App_State {
  std::string    base_path;
  SDL_GPUDevice* device;
  SDL_Window*    window;
  float          content_scale;
  HMM_Vec2       window_size_pixels;
  bool           window_minimized;

  ImFont* imgui_font;

  Font_Atlas_Kind font_atlas_kind;
  int             font_variant;
  Font_Atlas      font_atlases[FONT_ATLAS_KIND_COUNT];
  Text_Batch      text_batch;
  Demo_Kind       demo_kind;
  HMM_Vec4        bg_color                = HMM_V4(0.97f, 0.95f, 0.86f, 1.0f);
  HMM_Mat4        world_to_view_transform = HMM_M4D(1.0f);
  HMM_Mat4        view_to_clip_transform  = HMM_M4D(1.0f);
  struct {
    std::string        text               = "Example Text!";
    float              text_size          = 120.0f;
    Text_Batch_H_Align text_h_align       = TEXT_BATCH_H_ALIGN_CENTER;
    Text_Batch_V_Align text_v_align       = TEXT_BATCH_V_ALIGN_BASELINE;
    HMM_Vec4           text_color         = HMM_V4(0.024f, 0.02f, 0.019f, 1.0f);
    HMM_Vec4           text_outline_color = HMM_V4(0.998f, 0.9976f, 0.996f, 1.0f);
    float              text_outline_width = 0.0f;
  } demo_basic;
};

static void on_window_pixel_size_changed(App_State* as, int width, int height) {
  as->window_size_pixels = HMM_V2(width, height);

  as->view_to_clip_transform = HMM_Orthographic_RH_NO(
      0.0f,
      as->window_size_pixels.X,
      as->window_size_pixels.Y,
      0.0f,
      -1.0f,
      1.0f);
}

static void on_display_content_scale_changed(App_State* as, float content_scale) {
  as->content_scale = content_scale;

  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(as->content_scale);
  style.FontScaleDpi = as->content_scale;
}

static void on_demo_kind_selection(App_State* as, Demo_Kind kind) {
  as->demo_kind = kind;

  switch (as->demo_kind) {
  case DEMO_KIND_BASIC:
    as->world_to_view_transform = HMM_M4D(1.0f);
    break;
  default:
    break;
  }
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

  // Set the initial window size to be 50% of the primary displays desktop resolution.
  // Defaults to 800x600 if theres any issues getting the display id or display mode.
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
  SDL_SetGPUSwapchainParameters(
      as->device,
      as->window,
      SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
      SDL_GPU_PRESENTMODE_VSYNC);

  {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();

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
    init_info.MSAASamples                = SDL_GPU_SAMPLECOUNT_1;
    init_info.SwapchainComposition       = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    init_info.PresentMode                = SDL_GPU_PRESENTMODE_VSYNC;
    ImGui_ImplSDLGPU3_Init(&init_info);
  }

  auto cmd_buf = SDL_AcquireGPUCommandBuffer(as->device);
  if (cmd_buf == nullptr) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "Failed to acquire command buffer: %s",
        SDL_GetError());
    return SDL_APP_FAILURE;
  }
  auto                         copy_pass = SDL_BeginGPUCopyPass(cmd_buf);
  static constexpr const char* font_atlas_kind_names[FONT_ATLAS_KIND_COUNT] = {
      "roboto",
      "science_gothic",
      "playwrite",
  };
  for (int i = 0; i < FONT_ATLAS_KIND_COUNT; i++) {
    if (!font_atlas_load(
            &as->font_atlases[i],
            as->base_path,
            font_atlas_kind_names[i],
            as->device,
            copy_pass)) {
      SDL_LogError(
          SDL_LOG_CATEGORY_APPLICATION,
          "Failed to load font atlas: %s",
          font_atlas_kind_names[i]);
      return SDL_APP_FAILURE;
    }
  }
  SDL_EndGPUCopyPass(copy_pass);
  SDL_SubmitGPUCommandBuffer(cmd_buf);

  if (!text_batch_create(
          &as->text_batch,
          as->base_path,
          as->device,
          SDL_GetGPUSwapchainTextureFormat(as->device, as->window))) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create text batch");
    return SDL_APP_FAILURE;
  }

  {
    int w, h;
    SDL_GetWindowSizeInPixels(as->window, &w, &h);
    on_window_pixel_size_changed(as, w, h);
  }

  on_demo_kind_selection(as, DEMO_KIND_BASIC);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  auto as = static_cast<App_State*>(appstate);

  ImGui_ImplSDL3_ProcessEvent(event);

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
  }

  return SDL_APP_CONTINUE;
}

static void draw_demo(App_State* as) {
  switch (as->demo_kind) {
  case DEMO_KIND_BASIC: {
    auto world_to_clip_transform = as->view_to_clip_transform * as->world_to_view_transform;
    text_batch_begin(
        &as->text_batch,
        world_to_clip_transform,
        &as->font_atlases[as->font_atlas_kind],
        as->font_variant);
    text_batch_draw(
        &as->text_batch,
        as->demo_basic.text,
        as->window_size_pixels * 0.5f,
        as->demo_basic.text_size,
        as->demo_basic.text_h_align,
        as->demo_basic.text_v_align,
        as->demo_basic.text_color,
        as->demo_basic.text_outline_color,
        as->demo_basic.text_outline_width);
    text_batch_end(&as->text_batch);
  } break;
  }
}

static void draw_imgui(App_State* as) {
  if (ImGui::Begin("SDL3 GPU MSDF Text Demo", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {
    static constexpr const char* demo_kind_strings[DEMO_KIND_COUNT] = {
        "Basic",
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
    ImGui::ColorEdit4("Background Color", &as->bg_color.X);
    ImGui::Separator();

    auto& io = ImGui::GetIO();
    ImGui::Text(
        "Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / io.Framerate,
        io.Framerate);

    if (ImGui::CollapsingHeader("Demo Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
      const auto& font_atlas = as->font_atlases[as->font_atlas_kind];

      switch (as->demo_kind) {
      case DEMO_KIND_BASIC: {
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
            as->demo_basic.text.data(),
            as->demo_basic.text.capacity() + 1,
            ImGuiInputTextFlags_CallbackResize,
            resize_callback,
            &as->demo_basic.text);

        ImGui::SliderFloat(
            "Text Size",
            &as->demo_basic.text_size,
            font_atlas.size * 0.5f,
            font_atlas.size * 4.0f,
            "%.0f");

        static constexpr const char* text_h_align_strings[TEXT_BATCH_H_ALIGN_COUNT] = {
            "Left",
            "Center",
            "Right",
        };
        if (ImGui::BeginCombo(
                "Text Horizontal Align",
                text_h_align_strings[as->demo_basic.text_h_align])) {
          for (int i = 0; i < TEXT_BATCH_H_ALIGN_COUNT; i++) {
            bool is_selected = as->demo_basic.text_h_align == i;
            if (ImGui::Selectable(text_h_align_strings[i], is_selected)) {
              as->demo_basic.text_h_align = static_cast<Text_Batch_H_Align>(i);
            }
            if (is_selected) { ImGui::SetItemDefaultFocus(); }
          }
          ImGui::EndCombo();
        }

        static constexpr const char* text_v_align_strings[TEXT_BATCH_V_ALIGN_COUNT] = {
            "Top",
            "Middle",
            "Baseline",
            "Bottom",
        };
        if (ImGui::BeginCombo(
                "Text Vertical Align",
                text_v_align_strings[as->demo_basic.text_v_align])) {
          for (int i = 0; i < TEXT_BATCH_V_ALIGN_COUNT; i++) {
            bool is_selected = as->demo_basic.text_v_align == i;
            if (ImGui::Selectable(text_v_align_strings[i], is_selected)) {
              as->demo_basic.text_v_align = static_cast<Text_Batch_V_Align>(i);
            }
            if (is_selected) { ImGui::SetItemDefaultFocus(); }
          }
          ImGui::EndCombo();
        }

        ImGui::ColorEdit4("Text Color", &as->demo_basic.text_color.X);
        ImGui::ColorEdit4("Text Outline Color", &as->demo_basic.text_outline_color.X);
        ImGui::SliderFloat(
            "Text Outline Width",
            &as->demo_basic.text_outline_width,
            0.0f,
            10.0f,
            "%.0f");

        ImGui::LabelText(
            "Screen Pixel Range",
            "%f",
            as->demo_basic.text_size / font_atlas.size * font_atlas.distance_range);
      } break;
      default:
        break;
      }
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Font Atlas", ImGuiTreeNodeFlags_DefaultOpen)) {
      static constexpr const char* font_atlas_kind_strings[FONT_ATLAS_KIND_COUNT] = {
          "Roboto",
          "Science Gothic",
          "Playwrite",
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
    draw_demo(as);

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    draw_imgui(as);
    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();
    text_batch_prepare_draw_cmds(&as->text_batch, as->device, cmd_buf);

    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd_buf);

    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture                = swapchain_texture;
    target_info.clear_color = {as->bg_color.R, as->bg_color.G, as->bg_color.B, as->bg_color.A};
    target_info.load_op     = SDL_GPU_LOADOP_CLEAR;
    target_info.store_op    = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(cmd_buf, &target_info, 1, nullptr);

    text_batch_render_draw_cmds(&as->text_batch, cmd_buf, render_pass);

    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd_buf, render_pass);

    SDL_EndGPURenderPass(render_pass);
  }

  SDL_SubmitGPUCommandBuffer(cmd_buf);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  auto as = static_cast<App_State*>(appstate);

  SDL_WaitForGPUIdle(as->device);

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
