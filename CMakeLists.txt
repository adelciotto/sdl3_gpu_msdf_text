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

# Run msdf-atlas-gen to generate a single font atlas from multiple fonts.
set(FONT_ATLAS_PNG "${CMAKE_CURRENT_BINARY_DIR}/resources/font_atlas.png")
set(FONT_ATLAS_JSON "${CMAKE_CURRENT_BINARY_DIR}/resources/font_atlas.json")
set(FONT_FILES
    "${CMAKE_SOURCE_DIR}/resources/fonts/Oswald-Regular.ttf"
    "${CMAKE_SOURCE_DIR}/resources/fonts/MomoSignature-Regular.ttf"
)

list(JOIN FONT_FILES ";-and;-font;" FONT_ARGS)
set(FONT_ARGS "-font;${FONT_ARGS}")

if(WIN32)
    set(MSDF_ATLAS_GEN "${CMAKE_SOURCE_DIR}/tools/msdf-atlas-gen/msdf-atlas-gen.exe")
else()
endif()

add_custom_command(
    OUTPUT ${FONT_ATLAS_PNG} ${FONT_ATLAS_CSV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/resources"
    COMMAND ${MSDF_ATLAS_GEN} ${FONT_ARGS} -type msdf -imageout ${FONT_ATLAS_PNG} -json ${FONT_ATLAS_JSON}
    DEPENDS ${FONT_FILES}
    COMMENT "Generating font atlas from TTF files"
    VERBATIM
)

add_custom_target(generate_font_atlas ALL DEPENDS ${FONT_ATLAS_PNG} ${FONT_ATLAS_JSON})
add_dependencies(${PROJECT_NAME} generate_font_atlas)

# Copy generated resources to the directory next to the executable.
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_BINARY_DIR}/resources/"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    COMMENT "Copying resources to executable directory"
)
