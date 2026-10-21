# tests.ps1 runs the test suites in the user's session, each with a timeout; the summary goes to C:\mp\tests.txt.
# Output to a file is buffered and lost if the process has to be killed, so there is only a per-suite summary.
Set-Location C:\mp\Markpaste-portable
$env:QT_PLUGIN_PATH = "C:\mp\Markpaste-portable\plugins"
$out = "C:\mp\tests.txt"
Set-Content $out ""
foreach ($t in @("markpaste_windows_tests", "markpaste_core_tests", "markpaste_ui_tests")) {
  $p = Start-Process -FilePath ".\$t.exe" -NoNewWindow -PassThru -RedirectStandardOutput "C:\mp\$t.txt" -RedirectStandardError "C:\mp\$t.err"
  $null = $p.Handle   # without this ExitCode is empty after WaitForExit
  if (-not $p.WaitForExit(300000)) { Stop-Process -Id $p.Id -Force; Add-Content $out "$t : HANG" }
  else { Add-Content $out "$t : exit $($p.ExitCode)" }
  Get-Content "C:\mp\$t.txt" | Select-String -Pattern "PASSED|FAILED  \]|: Failure$" | ForEach-Object { Add-Content $out ("    " + $_.Line) }
}
