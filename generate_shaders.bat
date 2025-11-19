@echo off
setlocal enabledelayedexpansion

set "SOURCE_DIR=resources\source\shaders"
set "OUTPUT_DIR=resources\generated\shaders"
set "SHADERCROSS=tools\SDL3_shadercross\shadercross.exe"

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

if not exist "%SHADERCROSS%" (
    echo Error: shadercross.exe not found at %SHADERCROSS%
    exit /b 1
)

if not exist "%SOURCE_DIR%" (
    echo Error: Source shaders directory not found at %SOURCE_DIR%
    exit /b 1
)

REM Process each shader file
for %%f in ("%SOURCE_DIR%\*.hlsl") do (
    set "SHADER_FILE=%%f"
    set "SHADER_NAME=%%~nf"
    
    echo Processing: !SHADER_NAME!
    
    REM Generate SPIR-V
    echo   - Generating SPIR-V...
    "%SHADERCROSS%" "!SHADER_FILE!" ^
        -o "%OUTPUT_DIR%\!SHADER_NAME!.spv"
    
    REM Generate MSL
    echo   - Generating MSL...
    "%SHADERCROSS%" "!SHADER_FILE!" ^
        -o "%OUTPUT_DIR%\!SHADER_NAME!.msl"
    
    REM Generate DXIL
    echo   - Generating DXIL...
    "%SHADERCROSS%" "!SHADER_FILE!" ^
        -o "%OUTPUT_DIR%\!SHADER_NAME!.dxil"
    
    if errorlevel 1 (
        echo Error processing !SHADER_NAME!
    ) else (
        echo Successfully compiled !SHADER_NAME!
    )
    echo.
)

echo Done generating shaders.
pause