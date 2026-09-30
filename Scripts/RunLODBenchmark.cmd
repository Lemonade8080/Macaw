@echo off
setlocal
pushd "%~dp0.."
set "LODVSVars=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%I in (`"%LODVSVars%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "LODVSRoot=%%I"
if not defined LODVSRoot exit /b 1
call "%LODVSRoot%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
if not exist Build mkdir Build
cl /nologo /std:c++20 /EHsc /MD /O2 /utf-8 /DNOMINMAX /I. /IExternals\Include /Ivcpkg_installed\x64-windows\x64-windows\include Tests\LODBenchmark.cpp /FoBuild\LODBenchmark.obj /FeBuild\LODBenchmark.exe /link /LIBPATH:bin\Release\x64 /LIBPATH:Externals\bin\release /LIBPATH:vcpkg_installed\x64-windows\x64-windows\lib World.lib Render.lib Asset.lib Serialization.lib Core.lib Math.lib DirectXTex.lib freetype.lib d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib shell32.lib ole32.lib imm32.lib dwmapi.lib windowscodecs.lib
if errorlevel 1 exit /b 1
set "PATH=%CD%\vcpkg_installed\x64-windows\x64-windows\bin;%PATH%"
Build\LODBenchmark.exe %*
set "BenchmarkResult=%ERRORLEVEL%"
popd
exit /b %BenchmarkResult%
