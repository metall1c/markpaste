# input.ps1 reads C:\mp\cmd.txt: "click X Y", "dclick X Y", "drag X1 Y1 X2 Y2"
$parts = (Get-Content C:\mp\cmd.txt -Raw).Trim() -split '\s+'
$Cmd = $parts[0]; $X1 = [int]$parts[1]; $Y1 = [int]$parts[2]
if ($parts.Count -ge 5) { $X2 = [int]$parts[3]; $Y2 = [int]$parts[4] }
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class U {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int x, int y, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[U]::SetProcessDPIAware() | Out-Null
function Down { [U]::mouse_event(2,0,0,0,[UIntPtr]::Zero) }
function Up { [U]::mouse_event(4,0,0,0,[UIntPtr]::Zero) }
switch ($Cmd) {
  "click" { [U]::SetCursorPos($X1,$Y1); Start-Sleep -m 100; Down; Start-Sleep -m 50; Up }
  "dclick" { [U]::SetCursorPos($X1,$Y1); Start-Sleep -m 100; Down; Up; Start-Sleep -m 80; Down; Up }
  "drag" { [U]::SetCursorPos($X1,$Y1); Start-Sleep -m 150; Down; Start-Sleep -m 150
           for ($i=1; $i -le 10; $i++) { [U]::SetCursorPos($X1 + ($X2-$X1)*$i/10, $Y1 + ($Y2-$Y1)*$i/10); Start-Sleep -m 40 }
           Start-Sleep -m 150; Up }
}
