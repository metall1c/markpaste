#!/bin/bash
# Checks of the Windows version in a VirtualBox VM (docs/build.md, "Testing in VirtualBox").
# The guest needs Guest Additions, a user logged in on the console and a C:\mp directory with run.ps1 and input.ps1
# (vm.sh setup). Program windows are visible only if they are started in the user's session, so
# they are launched via Task Scheduler (run.ps1) rather than directly via guestcontrol.
#
#   MP_VM=Win11-test MP_VM_USER=admin MP_VM_PASS=… tools/dev/winvm/vm.sh <command>
#   setup                       copy the helpers to C:\mp
#   deploy ZIP                  unpack the portable ZIP to C:\mp
#   start [arguments]           start markpaste.exe in the user's session (--capture, --quit…)
#   exec EXE                    start any program in the user's session
#   shot FILE.png               screenshot of the guest
#   log [N]                     the last N lines of the program log
#   tests                       tests from build-win (MP_BUILD) into the program directory, run, summary
#   key SCANCODES               presses as from a real keyboard (Print: e0 2a e0 37 e0 b7 e0 aa)
#   type TEXT                   type text
#   click X Y | dclick X Y | drag X1 Y1 X2 Y2   mouse (via input.ps1, without a console window)
set -e
VM=${MP_VM:-Win11-test}
G="VBoxManage guestcontrol $VM --username ${MP_VM_USER:?MP_VM_USER} --password ${MP_VM_PASS:?MP_VM_PASS}"
PS='C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
DIR=$(cd "$(dirname "$0")" && pwd)
APP='C:\mp\Markpaste-portable\markpaste.exe'
MINGW=/usr/x86_64-w64-mingw32/sys-root/mingw
runps() { $G run --exe "$PS" -- -NoProfile -ExecutionPolicy Bypass -File 'C:\mp\run.ps1' "$@"; }
cmd=$1; shift || true
case "$cmd" in
  setup) $G mkdir 'C:\mp' 2>/dev/null || true
         $G copyto "$DIR/run.ps1" 'C:\mp\run.ps1'; $G copyto "$DIR/input.ps1" 'C:\mp\input.ps1' ;;
  deploy) $G copyto "$(realpath "$1")" 'C:\mp\mp.zip'
          # Stop the running program: otherwise its directory is not deleted, and Move-Item silently
          # puts the new build inside the old one
          $G run --exe "$PS" -- -NoProfile -Command '$ErrorActionPreference = "Stop"; Get-Process markpaste -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep -m 500; if (Test-Path C:\mp\Markpaste-portable) { Remove-Item -Recurse -Force C:\mp\Markpaste-portable }; if (Test-Path C:\mp\unz) { Remove-Item -Recurse -Force C:\mp\unz }; Expand-Archive -Force C:\mp\mp.zip C:\mp\unz; Move-Item (Get-ChildItem C:\mp\unz)[0].FullName C:\mp\Markpaste-portable; Remove-Item C:\mp\unz' ;;
  start) if [ $# -gt 0 ]; then runps -Exe "$APP" -Arguments "$*"; else runps -Exe "$APP"; fi ;;
  exec) runps -Exe "$1" ;;
  tests) # The core and window tests enable offscreen themselves: without qoffscreen.dll (not in the ZIP) Qt crashes at
         # startup, and the process hangs on an invisible error window
         for t in markpaste_windows_tests markpaste_core_tests markpaste_ui_tests; do
           $G copyto "$(realpath "${MP_BUILD:-build-win}/tests/$t.exe")" "C:\\mp\\Markpaste-portable\\$t.exe"
         done
         $G copyto "$MINGW/bin/Qt6Test.dll" 'C:\mp\Markpaste-portable\Qt6Test.dll'
         $G copyto "$MINGW/lib/qt6/plugins/platforms/qoffscreen.dll" 'C:\mp\Markpaste-portable\plugins\platforms\qoffscreen.dll'
         $G copyto "$DIR/tests.ps1" 'C:\mp\tests.ps1'
         runps -Exe 'C:\Windows\System32\conhost.exe' \
               -Arguments '--headless powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\mp\tests.ps1' -Wait 1000
         $G run --exe "$PS" -- -NoProfile -Command '[Console]::OutputEncoding=[Text.Encoding]::UTF8; Get-Content C:\mp\tests.txt' ;;
  shot) VBoxManage controlvm "$VM" screenshotpng "$(realpath -m "$1")" ;;
  log) $G run --exe 'C:\Windows\System32\cmd.exe' -- /c 'type %LOCALAPPDATA%\markpaste\markpaste\logs\markpaste.log' | tail -"${1:-8}" ;;
  key) VBoxManage controlvm "$VM" keyboardputscancode "$@" ;;
  type) VBoxManage controlvm "$VM" keyboardputstring "$*" ;;
  click|dclick|drag)
         tmp=$(mktemp); echo "$cmd $*" > "$tmp"; $G copyto "$tmp" 'C:\mp\cmd.txt'; rm -f "$tmp"
         # conhost --headless: no console window, otherwise it takes focus from the window we paste into
         runps -Exe 'C:\Windows\System32\conhost.exe' \
               -Arguments '--headless powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\mp\input.ps1' -Wait 15 ;;
  *) sed -n '2,20p' "$0"; exit 1 ;;
esac
