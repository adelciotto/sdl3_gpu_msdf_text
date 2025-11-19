cmake_minimum_required(VERSION 3.11...4.0)

project(sdl3_gpu_msdf_text
        VERSION 1.0
        LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

include(FetchContent)

set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3_test library" FORCE)
FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG release-3.2.26
)

FetchContent_MakeAvailable(SDL)

add_executable(${PROJECT_NAME}
  src/defer.hpp
  src/font_atlas.cpp
  src/font_atlas.hpp
  src/text_batch.cpp
  src/text_batch.hpp
  src/main.cpp

  extern/imgui/imgui_demo.cpp
  extern/imgui/imgui_draw.cpp
  extern/imgui/imgui_impl_sdl3.cpp
  extern/imgui/imgui_impl_sdlgpu3.cpp
  extern/imgui/imgui_tables.cpp
  extern/imgui/imgui_widgets.cpp
  extern/imgui/imgui.cpp
)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.hpp)
target_include_directories(${PROJECT_NAME} PRIVATE extern)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)

# Copy generated resources to the directory next to the executable.
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/resources/generated"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    COMMENT "Copying resources to executable directory"
)
