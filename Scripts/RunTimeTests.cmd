@echo off
setlocal
pushd "%~dp0.."
set "TimerVsWhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%TimerVsWhere%" exit /b 1
for /f "usebackq tokens=*" %%I in (`"%TimerVsWhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "TimerVsRoot=%%I"
if not defined TimerVsRoot exit /b 1
call "%TimerVsRoot%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
if not exist Build mkdir Build
cl /nologo /std:c++20 /EHsc /MDd /Zi /utf-8 /D_DEBUG /DNOMINMAX /I. /IExternals\Include /Ivcpkg_installed\x64-windows\x64-windows\include Tests\TimeTests.cpp /FoBuild\TimeTests.obj /FdBuild\TimeTests.pdb /FeBuild\TimeTests.exe /link /LIBPATH:bin\Debug\x64 /LIBPATH:Externals\bin\debug /LIBPATH:vcpkg_installed\x64-windows\x64-windows\debug\lib World.lib Render.lib Asset.lib Serialization.lib Core.lib Math.lib DirectXTex.lib freetyped.lib d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib shell32.lib ole32.lib imm32.lib dwmapi.lib
if errorlevel 1 exit /b 1
set "PATH=%CD%\vcpkg_installed\x64-windows\x64-windows\debug\bin;%PATH%"
Build\TimeTests.exe
set "TimerTestResult=%ERRORLEVEL%"
popd
exit /b %TimerTestResult%
