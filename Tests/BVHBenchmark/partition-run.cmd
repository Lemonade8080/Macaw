@echo off
cd /d "%~dp0..\.."
powershell -NoProfile -File Tests\BVHBenchmark\partition-generate.ps1
if errorlevel 1 exit /b 1
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /EHsc /O2 /Gy /MD /DNDEBUG /DNOMINMAX /utf-8 /I. /IExternals\Include Tests\BVHBenchmark\partition-main.cpp Core\Memory\Memory.cpp Core\Stat\Stat.cpp Math\FVector.cpp /Fo:Tests\BVHBenchmark\ /Fe:Tests\BVHBenchmark\partition-benchmark.exe /link /OPT:REF > Tests\BVHBenchmark\partition-compile.log 2>&1
if errorlevel 1 exit /b 1
Tests\BVHBenchmark\partition-benchmark.exe
