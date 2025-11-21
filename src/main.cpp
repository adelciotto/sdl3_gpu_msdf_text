#include "font_atlas.hpp"
#include "imgui_font.hpp"
#include "text_batch.hpp"

#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_sdlgpu3.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct App_State {
  std::string    base_path;
  SDL_GPUDevice* device;
  SDL_Window*    window;
  float          content_scale;

  ImFont* imgui_font;

  Font_Atlas font_atlas;
  Text_Batch text_batch;
};

static void on_display_content_scale_changed(App_State* as, float content_scale) {
  as->content_scale = content_scale;

  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(as->content_scale);
  style.FontScaleDpi = as->content_scale;
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

  as->device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, true, nullptr);
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
  auto copy_pass = SDL_BeginGPUCopyPass(cmd_buf);
  if (!font_atlas_load(&as->font_atlas, as->base_path, "atlas_default", as->device, copy_pass)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load font atlas");
    return SDL_APP_FAILURE;
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

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  auto as = static_cast<App_State*>(appstate);

  ImGui_ImplSDL3_ProcessEvent(event);

  switch (event->type) {
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;
  case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
    on_display_content_scale_changed(as, SDL_GetDisplayContentScale(event->display.displayID));
    break;
  }

  return SDL_APP_CONTINUE;
}

static void draw_imgui(App_State* as) {
  if (ImGui::Begin("SDL3 GPU MSDF Text Example")) {
    if (ImGui::CollapsingHeader("Font Atlas", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::LabelText("Width", "%d", as->font_atlas.width);
      ImGui::LabelText("Height", "%d", as->font_atlas.width);
      if (ImGui::TreeNode("Texture")) {
        ImGui::Image(
            static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(as->font_atlas.texture)),
            ImVec2(as->font_atlas.width, as->font_atlas.height));
        ImGui::TreePop();
      }
    }
  }
  ImGui::End();
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  auto as = static_cast<App_State*>(appstate);

  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  draw_imgui(as);
  ImGui::Render();

  ImDrawData* draw_data    = ImGui::GetDrawData();
  bool        is_minimized = draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f;

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

  if (swapchain_texture != nullptr && !is_minimized) {
    text_batch_prepare_draw_cmds(&as->text_batch, as->device, cmd_buf);

    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd_buf);

    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture                = swapchain_texture;
    target_info.clear_color            = SDL_FColor {0.212f, 0.2f, 0.2f};
    target_info.load_op                = SDL_GPU_LOADOP_CLEAR;
    target_info.store_op               = SDL_GPU_STOREOP_STORE;
    target_info.mip_level              = 0;
    target_info.layer_or_depth_plane   = 0;
    target_info.cycle                  = false;
    SDL_GPURenderPass* render_pass     = SDL_BeginGPURenderPass(cmd_buf, &target_info, 1, nullptr);

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
  font_atlas_destroy(&as->font_atlas, as->device);

  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();

  SDL_ReleaseWindowFromGPUDevice(as->device, as->window);
  SDL_DestroyWindow(as->window);
  SDL_DestroyGPUDevice(as->device);

  delete as;

  SDL_Quit();
}
