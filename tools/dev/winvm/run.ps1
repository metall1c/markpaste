# Запустить программу в интерактивном сеансе пользователя (планировщик задач) и дождаться конца
param([string]$Exe, [string]$Arguments = "", [int]$Wait = 0)
$a = if ($Arguments) { New-ScheduledTaskAction -Execute $Exe -Argument $Arguments } else { New-ScheduledTaskAction -Execute $Exe }
$p = New-ScheduledTaskPrincipal -UserId "admin" -LogonType Interactive -RunLevel Limited
$s = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit 0
$name = "mp-" + [guid]::NewGuid().ToString("N").Substring(0,8)
Register-ScheduledTask -TaskName $name -Action $a -Principal $p -Settings $s -Force | Out-Null
Start-ScheduledTask -TaskName $name
for ($i = 0; $i -lt $Wait * 10; $i++) { if ((Get-ScheduledTask -TaskName $name).State -ne "Running") { break }; Start-Sleep -m 100 }
Start-Sleep -m 300
Unregister-ScheduledTask -TaskName $name -Confirm:$false
