# 用 Qt 6.12 + MinGW 13.1 构建（不在 Qt Creator 里时用这个）
$env:PATH = "D:\Qt\6.12.0\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not (Test-Path build)) {
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_PREFIX_PATH=D:/Qt/6.12.0/mingw_64 `
        -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe
}
cmake --build build
