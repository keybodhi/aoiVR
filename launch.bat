@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
rem Point awareness frame dir at the build's context/frames.
if not exist "context\frames" mkdir "context\frames" >nul 2>&1
set "AOI_FRAMES_DIR=%~dp0context\frames"

rem --- First run: create aoi_config.json interactively if missing (or keys empty). ---
set "NEED_CFG=0"
if not exist "aoi_config.json" set "NEED_CFG=1"
if exist "aoi_config.json" (
  findstr /b "\"apiKey\":" "aoi_config.json" >nul 2>&1 || set "NEED_CFG=1"
  findstr "\"apiKey\": \"\"" "aoi_config.json" >nul 2>&1 && set "NEED_CFG=1"
)
if "!NEED_CFG!"=="1" (
  echo.
  echo  First run: create aoi_config.json with your API keys.
  echo  Copy aoi_config.json.example and fill in:
  echo    llm.apiKey   - OpenCode: https://opencode.ai/auth
  echo    tts.apiKey   - MiMo:     https://platform.xiaomimimo.com/
  echo.
  if not exist "aoi_config.json" (
    copy /y "aoi_config.json.example" "aoi_config.json" >nul
    echo  Created aoi_config.json from the template. Edit it, then run again.
    echo.
    pause
    exit /b 1
  )
)

echo Starting AoiVR (SteamVR overlay)...
start "" "%~dp0AoiVR.exe"
endlocal