@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

:: --- Unpack Arguments -------------------------------------------------------
for %%a in (%*) do set "%%~a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1" set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]
if not "%skipfonts%"=="1" set buildfonts=1
if "%skipfonts%"=="1" echo [skipping font atlas generation]

:: --- Unpack Command line Build Arguments ------------------------------------
:: None for now...

:: --- Compile/Link Definitions -----------------------------------------------
set cl_common=/nologo /EHsc /std:c++17 ^
              /I..\src /I..\extern\HandmadeMath /I..\extern\SDL3\include /I..\extern\imgui /I..\extern\nlohmann /I..\extern\stb
set cl_debug=call cl /MDd /Zi /Od /DBUILD_DEBUG %cl_common%
set cl_release=call cl /MD /O2 %cl_common%
set cl_link=/link ..\extern\SDL3\lib\x64\SDL3.lib shell32.lib /subsystem:console
if "%debug%"=="1" set cl_compile=%cl_debug%
if "%release%"=="1" set cl_compile=%cl_release%

:: --- Shader Compile Definitions ---------------------------------------------
set shadercross=call ..\tools\SDL3_shadercross\shadercross.exe
set shadercross_vertex=%shadercross% -t vertex -DVERTEX_SHADER
set shadercross_fragment=%shadercross% -t fragment -DFRAGMENT_SHADER

:: --- Font Atlas Build Definitions -------------------------------------------
set msdf_atlas_gen=call ..\tools\msdf_atlas_gen\msdf_atlas_gen.exe
set msdf_common=-type msdf -size 128 -pxrange 8 -coloringstrategy inktrap -errorcorrection auto-full

:: --- Prep Directories -------------------------------------------------------
if not exist build mkdir build
if not exist build\res mkdir build\res

:: --- Build Everything -------------------------------------------------------
pushd build

if "%buildfonts%"=="1" (
  %msdf_atlas_gen% -font ..\res\Roboto-Regular.ttf ^
                   -and -font ..\res\Roboto-Bold.ttf ^
                   -and -font ..\res\Roboto-Italic.ttf ^
                   -and -font ..\res\Roboto-BoldItalic.ttf ^
                   -and -font ..\res\Roboto-Light.ttf ^
                   %msdf_common% ^
                   -imageout res\roboto.png -json res\roboto.json || exit /b 1
  %msdf_atlas_gen% -font ..\res\ScienceGothic-Regular.ttf ^
                   -and -font ..\res\ScienceGothic-Bold.ttf ^
                   -and -font ..\res\ScienceGothic-Light.ttf ^
                   %msdf_common% ^
                   -imageout res\science_gothic.png -json res\science_gothic.json || exit /b 1
  %msdf_atlas_gen% -font ..\res\Limelight-Regular.ttf ^
                   %msdf_common% ^
                   -imageout res\limelight.png -json res\limelight.json || exit /b 1
)
%shadercross_vertex% ..\src\text_batch.hlsl -o res\text_batch.vert.dxil || exit /b 1
%shadercross_fragment% ..\src\text_batch.hlsl -o res\text_batch.frag.dxil || exit /b 1
%shadercross_vertex% ..\src\text_static.hlsl -o res\text_static.vert.dxil || exit /b 1
%shadercross_fragment% ..\src\text_static.hlsl -o res\text_static.frag.dxil || exit /b 1
%cl_compile% ..\src\font_atlas.cpp ^
             ..\src\main.cpp ^
             ..\src\text_batch.cpp ^
             ..\src\text_static.cpp ^
             ..\src\util.cpp ^
             ..\extern\imgui\imgui.cpp ^
             ..\extern\imgui\imgui_demo.cpp ^
             ..\extern\imgui\imgui_draw.cpp ^
             ..\extern\imgui\imgui_impl_sdl3.cpp ^
             ..\extern\imgui\imgui_impl_sdlgpu3.cpp ^
             ..\extern\imgui\imgui_tables.cpp ^
             ..\extern\imgui\imgui_widgets.cpp ^
             %cl_link% /out:sdl3_gpu_msdf_text.exe || exit /b 1
popd

:: --- Copy Resources ---------------------------------------------------------
if not exist build\res\shakespeare.txt copy res\shakespeare.txt build\res >nul

:: --- Copy DLL's -------------------------------------------------------------
if not exist build\SDL3.dll copy extern\SDL3\lib\x64\SDL3.dll build >nul
