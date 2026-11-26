# 渲染 N 帧后存一张 PNG，用于无界面自检
$env:PATH = "D:\Qt\6.12.0\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;" + $env:PATH
$root = Split-Path -Parent $PSScriptRoot
$out = if ($args.Count -ge 1) { $args[0] } else { Join-Path $root "docs\snapshot.png" }
& (Join-Path $root "build\dusk-patrol-qt.exe") --snapshot $out --frames 90
Write-Host "saved: $out"
