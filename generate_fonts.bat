@echo off
setlocal enabledelayedexpansion

set "SOURCE_DIR=resources\source\fonts"
set "OUTPUT_DIR=resources\generated\fonts"
set "MSDF_ATLAS_GEN=tools\msdf-atlas-gen\msdf-atlas-gen.exe"

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

if not exist "%MSDF_ATLAS_GEN%" (
    echo Error: msdf-atlas-gen.exe not found at %MSDF_ATLAS_GEN%
    exit /b 1
)

if not exist "%SOURCE_DIR%" (
    echo Error: Source fonts directory not found at %SOURCE_DIR%
    exit /b 1
)

REM Generate atlas with default settings.
"%MSDF_ATLAS_GEN%" ^
    -font "%SOURCE_DIR%\MomoSignature-Regular.ttf" ^
    -and -font "%SOURCE_DIR%\Oswald-Regular.ttf"  ^
    -type msdf -imageout "%OUTPUT_DIR%\atlas_default.png" -json "%OUTPUT_DIR%\atlas_default.json"
if errorlevel 1 (
    echo Error generating font atlas
) else (
    echo Successfully generated font atlas
)

echo Done generating fonts.
pause