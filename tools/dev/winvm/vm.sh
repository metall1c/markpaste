#!/bin/bash
# Проверки Windows-версии в виртуальной машине VirtualBox (docs/build.md, «Проверка в VirtualBox»).
# В госте нужны Guest Additions, вход пользователя на консоли и каталог C:\mp с run.ps1 и input.ps1
# (vm.sh setup). Окна программ видны, только если их запустить в сеансе пользователя — поэтому
# запуск идёт через планировщик задач (run.ps1), а не напрямую через guestcontrol.
#
#   MP_VM=Win11-test MP_VM_USER=admin MP_VM_PASS=… tools/dev/winvm/vm.sh <команда>
#   setup                       скопировать помощники в C:\mp
#   deploy ZIP                  распаковать портативный ZIP в C:\mp
#   start [аргументы]           запустить markpaste.exe в сеансе пользователя (--capture, --quit…)
#   exec EXE                    запустить любую программу в сеансе пользователя
#   shot FILE.png               снимок экрана гостя
#   log [N]                     последние N строк журнала программы
#   key СКАНКОДЫ                нажатия как с настоящей клавиатуры (Print: e0 2a e0 37 e0 b7 e0 aa)
#   type ТЕКСТ                  набрать текст
#   click X Y | dclick X Y | drag X1 Y1 X2 Y2   мышь (через input.ps1, без окна консоли)
set -e
VM=${MP_VM:-Win11-test}
G="VBoxManage guestcontrol $VM --username ${MP_VM_USER:?MP_VM_USER} --password ${MP_VM_PASS:?MP_VM_PASS}"
PS='C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
DIR=$(cd "$(dirname "$0")" && pwd)
APP='C:\mp\Markpaste-portable\markpaste.exe'
runps() { $G run --exe "$PS" -- -NoProfile -ExecutionPolicy Bypass -File 'C:\mp\run.ps1' "$@"; }
cmd=$1; shift || true
case "$cmd" in
  setup) $G mkdir 'C:\mp' 2>/dev/null || true
         $G copyto "$DIR/run.ps1" 'C:\mp\run.ps1'; $G copyto "$DIR/input.ps1" 'C:\mp\input.ps1' ;;
  deploy) $G copyto "$(realpath "$1")" 'C:\mp\mp.zip'
          $G run --exe "$PS" -- -NoProfile -Command 'Remove-Item -Recurse -Force C:\mp\Markpaste-portable -ErrorAction SilentlyContinue; Expand-Archive -Force C:\mp\mp.zip C:\mp\unz; Move-Item (Get-ChildItem C:\mp\unz)[0].FullName C:\mp\Markpaste-portable; Remove-Item C:\mp\unz' ;;
  start) if [ $# -gt 0 ]; then runps -Exe "$APP" -Arguments "$*"; else runps -Exe "$APP"; fi ;;
  exec) runps -Exe "$1" ;;
  shot) VBoxManage controlvm "$VM" screenshotpng "$(realpath -m "$1")" ;;
  log) $G run --exe 'C:\Windows\System32\cmd.exe' -- /c 'type %LOCALAPPDATA%\markpaste\markpaste\logs\markpaste.log' | tail -"${1:-8}" ;;
  key) VBoxManage controlvm "$VM" keyboardputscancode "$@" ;;
  type) VBoxManage controlvm "$VM" keyboardputstring "$*" ;;
  click|dclick|drag)
         tmp=$(mktemp); echo "$cmd $*" > "$tmp"; $G copyto "$tmp" 'C:\mp\cmd.txt'; rm -f "$tmp"
         # conhost --headless: без окна консоли — иначе оно забирает фокус у окна, куда вставляем
         runps -Exe 'C:\Windows\System32\conhost.exe' \
               -Arguments '--headless powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\mp\input.ps1' -Wait 15 ;;
  *) sed -n '2,20p' "$0"; exit 1 ;;
esac
