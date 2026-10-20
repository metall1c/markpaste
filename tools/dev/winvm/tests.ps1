# tests.ps1 — наборы тестов в сеансе пользователя, каждый с таймаутом; итог — в C:\mp\tests.txt.
# Вывод в файл буферизуется и теряется, если процесс приходится убить, — поэтому только итог по набору.
Set-Location C:\mp\Markpaste-portable
$env:QT_PLUGIN_PATH = "C:\mp\Markpaste-portable\plugins"
$out = "C:\mp\tests.txt"
Set-Content $out ""
foreach ($t in @("markpaste_windows_tests", "markpaste_core_tests", "markpaste_ui_tests")) {
  $p = Start-Process -FilePath ".\$t.exe" -NoNewWindow -PassThru -RedirectStandardOutput "C:\mp\$t.txt" -RedirectStandardError "C:\mp\$t.err"
  $null = $p.Handle   # без этого ExitCode после WaitForExit пустой
  if (-not $p.WaitForExit(300000)) { Stop-Process -Id $p.Id -Force; Add-Content $out "$t : HANG" }
  else { Add-Content $out "$t : exit $($p.ExitCode)" }
  Get-Content "C:\mp\$t.txt" | Select-String -Pattern "PASSED|FAILED  \]|: Failure$" | ForEach-Object { Add-Content $out ("    " + $_.Line) }
}
