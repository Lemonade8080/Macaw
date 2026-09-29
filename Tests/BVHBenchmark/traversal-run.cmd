@echo off
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++20 /EHsc /O2 /Gy /MD /DNDEBUG /DNOMINMAX /utf-8 /I. /IExternals\Include Tests\BVHBenchmark\traversal-main.cpp Core\Memory\Memory.cpp Core\Stat\Stat.cpp Core\Base\FTransform.cpp Math\FVector.cpp Math\FMatrix.cpp Math\FQuat.cpp Math\FRotator.cpp Math\FVector4.cpp Math\FMatrixRegister.cpp Math\FMath.cpp /Fo:Tests\BVHBenchmark\ /Fe:Tests\BVHBenchmark\traversal-benchmark.exe /link /OPT:REF > Tests\BVHBenchmark\traversal-compile.log 2>&1
if errorlevel 1 exit /b 1
Tests\BVHBenchmark\traversal-benchmark.exe
