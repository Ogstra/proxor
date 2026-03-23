@echo off
setlocal

set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set ROOT=%~dp0

echo [1/4] Setting up MSVC environment...
call %VCVARS% || (echo ERROR: vcvars64.bat not found && exit /b 1)

echo [2/4] Building C++ (proxor.exe)...
cmake --build "%ROOT%build" || (echo ERROR: cmake build failed && exit /b 1)

echo [3/4] Downloading geo assets (skipped if already present)...
if exist "%ROOT%deployment\public_res\geosite.db" (
    echo   geosite.db found — skipping download.
) else (
    bash libs/build_public_res.sh || (echo ERROR: build_public_res.sh failed && exit /b 1)
)

echo [4/4] Deploying...
cd /d "%ROOT%"
set PATH=%ROOT%qtsdk\Qt\bin;%PATH%
bash libs/deploy_windows64.sh || (echo ERROR: deploy script failed && exit /b 1)

echo.
echo Done. Output: deployment\windows64\
