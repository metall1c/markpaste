#!/bin/sh
# Installs Markpaste from this archive (the static build: it carries its own Qt, ADR 0028).
#   ./install.sh               # into ~/.local, for the current user
#   sudo ./install.sh --system # into /usr/local, for everyone
#   ./install.sh --uninstall   # (with --system: from /usr/local)
# The .desktop file gets the absolute path of the program: KWin lets a program into
# ScreenShot2 (capture without a dialog) only when Exec matches its executable.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
id=su._525.Markpaste
prefix=$HOME/.local
uninstall=
for a in "$@"; do
    case "$a" in
    --system) prefix=/usr/local ;;
    --uninstall) uninstall=1 ;;
    *) sed -n '2,7s/^# \{0,1\}//p' "$0"; exit 1 ;;
    esac
done
bin=$prefix/bin/markpaste
apps=$prefix/share/applications
icons=$prefix/share/icons/hicolor/scalable/apps

# A running copy keeps the old file open; KWin then refuses it ScreenShot2 until a restart
running=
if [ -x "$bin" ] && pgrep -u "$(id -u)" -x markpaste > /dev/null 2>&1; then
    "$bin" --quit > /dev/null 2>&1 || true
    running=1
fi

if [ -n "$uninstall" ]; then
    rm -f "$bin" "$apps/$id.desktop" "$icons/$id.svg" "$prefix/share/metainfo/$id.metainfo.xml"
    echo "Markpaste removed from $prefix"
    exit 0
fi

install -Dm755 "$here/markpaste" "$bin"
mkdir -p "$apps"
sed "s|^Exec=.*|Exec=$bin|" "$here/share/$id.desktop" > "$apps/$id.desktop"
install -Dm644 "$here/share/$id.svg" "$icons/$id.svg"
install -Dm644 "$here/share/$id.metainfo.xml" "$prefix/share/metainfo/$id.metainfo.xml"
command -v update-desktop-database > /dev/null && update-desktop-database -q "$apps" || true
command -v gtk-update-icon-cache > /dev/null && gtk-update-icon-cache -q "$prefix/share/icons/hicolor" 2> /dev/null || true

echo "Markpaste installed: $bin"
case ":$PATH:" in *":$prefix/bin:"*) ;; *) echo "Note: $prefix/bin is not in PATH; the menu entry works anyway" ;; esac
if [ -n "$running" ] && [ "$(id -u)" != 0 ]; then
    setsid "$bin" > /dev/null 2>&1 < /dev/null &
    echo "Restarted the running copy"
fi
