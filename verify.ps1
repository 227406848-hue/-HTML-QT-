Set-Location "D:\workspace\dusk-patrol-qt"
Remove-Item "build\dusk-patrol.log" -ErrorAction SilentlyContinue
& ".\build\dusk-patrol-qt.exe" --snapshot "D:\workspace\dusk-patrol-qt\verify.png" --frames 200 2>&1 | Out-Null
"exit=$LASTEXITCODE"
Get-Content "build\dusk-patrol.log" -Encoding UTF8 | Out-String
