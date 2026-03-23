@echo off
setlocal

set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set ROOT=%~dp0

echo [1/3] Setting up MSVC environment...
call %VCVARS% || (echo ERROR: vcvars64.bat not found && exit /b 1)

echo [2/3] Building C++ (proxor.exe)...
cmake --build "%ROOT%build" || (echo ERROR: cmake build failed && exit /b 1)

echo [3/3] Deploying...
cd /d "%ROOT%"
set PATH=%ROOT%qtsdk\Qt\bin;%PATH%
bash libs/deploy_windows64.sh || (echo ERROR: deploy script failed && exit /b 1)

echo.
echo Done. Output: deployment\windows64\
