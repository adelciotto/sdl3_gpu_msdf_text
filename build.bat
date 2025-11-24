@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

:: --- Unpack Arguments -------------------------------------------------------
for %%a in (%*) do set "%%~a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1" set release=0 && echo [debug mode]
if "%release%"=="1" set release=0 && echo [release mode]

:: --- Unpack Command line Build Arguments ------------------------------------
:: None for now...

:: --- Compile/Link Definitions -----------------------------------------------
set cl_common=/nologo /MD /EHsc /std:c++17 ^
              /I..\src /I..\extern\HandmadeMath /I..\extern\SDL3\include /I..\extern\imgui /I..\extern\nlohmann /I..\extern\stb
set cl_debug=call cl /Zi /Od /DBUILD_DEBUG=1 %cl_common%
set cl_release=call cl /O2 /DBUILD_DEBUG=0 %cl_common%
set cl_link=/link ..\extern\SDL3\lib\x64\SDL3.lib shell32.lib /subsystem:console
if "%debug%"=="1" set cl_compile=%cl_debug%
if "%release%"=="1" set cl_compile=%cl_release%

:: --- Shader Compile Definitions ---------------------------------------------
set shadercross=call ..\tools\SDL3_shadercross\shadercross.exe
set shadercross_vertex=%shadercross% -t vertex -DVERTEX_SHADER
set shadercross_fragment=%shadercross% -t fragment -DFRAGMENT_SHADER

:: --- Font Atlas Build Definitions -------------------------------------------
set msdf_atlas_gen=call ..\tools\msdf_atlas_gen\msdf_atlas_gen.exe 

:: --- Prep Directories -------------------------------------------------------
if not exist build mkdir build

:: --- Build Everything -------------------------------------------------------
pushd build
%msdf_atlas_gen% -font ..\fonts\MomoSignature-Regular.ttf -and -font ..\fonts\Oswald-Regular.ttf ^
                 -type msdf -size 64 -pxrange 4 -coloringstrategy inktrap -errorcorrection auto-full ^
                 -imageout atlas_s64.png -json atlas_s64.json || exit /b 1
%shadercross_vertex% ..\src\text_batch.hlsl -o text_batch.vert.dxil || exit /b 1
%shadercross_fragment% ..\src\text_batch.hlsl -o text_batch.frag.dxil || exit /b 1
%cl_compile% ..\src\sdl3_gpu_msdf_text.cpp ^
             ..\extern\imgui\imgui.cpp ^
             ..\extern\imgui\imgui_demo.cpp ^
             ..\extern\imgui\imgui_draw.cpp ^
             ..\extern\imgui\imgui_impl_sdl3.cpp ^
             ..\extern\imgui\imgui_impl_sdlgpu3.cpp ^
             ..\extern\imgui\imgui_tables.cpp ^
             ..\extern\imgui\imgui_widgets.cpp ^
             %cl_link% /out:sdl3_gpu_msdf_text.exe || exit /b 1
popd

:: --- Copy DLL's -------------------------------------------------------------
if not exist build\SDL3.dll copy extern\SDL3\lib\x64\SDL3.dll build >nul
