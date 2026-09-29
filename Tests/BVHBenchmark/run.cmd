@echo off
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /EHsc /O2 /Gy /MD /DNDEBUG /DNOMINMAX /utf-8 /I. /IExternals\Include Tests\BVHBenchmark\main.cpp Core\Memory\Memory.cpp Core\Stat\Stat.cpp Math\FVector.cpp /Fo:Tests\BVHBenchmark\ /Fe:Tests\BVHBenchmark\benchmark.exe /link /OPT:REF > Tests\BVHBenchmark\compile.log 2>&1
if errorlevel 1 exit /b 1
Tests\BVHBenchmark\benchmark.exe %*
