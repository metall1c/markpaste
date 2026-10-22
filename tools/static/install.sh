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
    *) sed -n '2,5s/^# \{0,1\}//p' "$0"; exit 1 ;;
    esac
done
bin=$prefix/bin/markpaste
apps=$prefix/share/applications
icons=$prefix/share/icons/hicolor
me=$(id -u)

# Copies of this program file that are running now: "pid uid" lines. A copy keeps the old file
# open; once the file is replaced, KWin refuses it ScreenShot2 until a restart. Matched by the
# executable, so a build elsewhere is left alone. As root this sees every user's copies.
running() {
    for pid in $(pgrep -x markpaste 2> /dev/null || true); do
        exe=$(readlink "/proc/$pid/exe" 2> /dev/null) || continue
        case "$exe" in "$bin" | "$bin (deleted)") echo "$pid $(stat -c %u "/proc/$pid")" ;; esac
    done
}

# Asks a copy to quit through its single-instance socket (in the owner's $XDG_RUNTIME_DIR) and
# waits for it to exit. offscreen: --quit works without a display too (SSH, a text console).
quit_copy() { # pid uid
    rt=/run/user/$2
    [ "$2" = "$me" ] && [ -n "${XDG_RUNTIME_DIR:-}" ] && rt=$XDG_RUNTIME_DIR
    if [ "$2" = "$me" ]; then
        # The new binary from the archive speaks the same protocol and is there even if the old file is gone
        cli=$here/markpaste
        [ -x "$cli" ] || cli=$bin
        XDG_RUNTIME_DIR=$rt QT_QPA_PLATFORM=offscreen "$cli" --quit > /dev/null 2>&1 || true
    elif command -v runuser > /dev/null; then
        user=$(getent passwd "$2" | cut -d: -f1)
        runuser -u "$user" -- env XDG_RUNTIME_DIR="$rt" QT_QPA_PLATFORM=offscreen "$bin" --quit > /dev/null 2>&1 || true
    fi
    i=0
    while kill -0 "$1" 2> /dev/null; do
        [ $i -ge 50 ] && return 1
        sleep 0.1
        i=$((i + 1))
    done
}

# Quit the running copies before their file changes; the user's own copy starts again at the end
mine=
closed=
while read -r pid uid; do
    [ -n "$pid" ] || continue
    if quit_copy "$pid" "$uid"; then
        if [ "$uid" = "$me" ]; then mine=1; else closed=1; fi
    else
        echo "Markpaste (pid $pid) did not quit: quit it from its tray menu and start it again" >&2
    fi
done << EOF
$(running)
EOF

# Exec value for the program path (Desktop Entry spec, "The Exec key"): quoted when the path has
# anything but plain characters (a space in $HOME); inside the quotes \ " ` $ are escaped, then
# every \ is doubled again as in any string value, and % becomes %%.
exec_value() {
    if printf '%s' "$1" | LC_ALL=C grep -q '[^A-Za-z0-9/._+@,=:-]'; then
        printf '"%s"' "$(printf '%s' "$1" | sed -e 's/[\\"`$]/\\&/g' -e 's/\\/\\\\/g' -e 's/%/%%/g')"
    else
        printf '%s' "$1"
    fi
}

refresh_caches() {
    if command -v update-desktop-database > /dev/null; then update-desktop-database -q "$apps" 2> /dev/null || true; fi
    # -t: these hicolor directories have no index.theme of their own
    if [ -d "$icons" ] && command -v gtk-update-icon-cache > /dev/null; then
        gtk-update-icon-cache -q -t -f "$icons" 2> /dev/null || true
    fi
    # KDE reads .desktop files through its sycoca cache; KWin looks up the ScreenShot2 permission there
    if [ "$me" != 0 ] && command -v kbuildsycoca6 > /dev/null; then kbuildsycoca6 > /dev/null 2>&1 || true; fi
}

if [ -n "$uninstall" ]; then
    rm -f "$bin" "$apps/$id.desktop" "$icons/scalable/apps/$id.svg" "$prefix/share/metainfo/$id.metainfo.xml"
    refresh_caches
    echo "Markpaste removed from $prefix"
    exit 0
fi

install -Dm755 "$here/markpaste" "$bin"
mkdir -p "$apps"
EXEC_VALUE=$(exec_value "$bin") awk '/^Exec=/ { print "Exec=" ENVIRON["EXEC_VALUE"]; next } { print }' \
    "$here/share/$id.desktop" > "$apps/$id.desktop"
install -Dm644 "$here/share/$id.svg" "$icons/scalable/apps/$id.svg"
install -Dm644 "$here/share/$id.metainfo.xml" "$prefix/share/metainfo/$id.metainfo.xml"
refresh_caches

echo "Markpaste installed: $bin"
case ":$PATH:" in *":$prefix/bin:"*) ;; *) echo "Note: $prefix/bin is not in PATH; the menu entry works anyway" ;; esac
# Only where there is a desktop to show it on (not over SSH)
if [ -n "$mine" ] && [ "$me" != 0 ] && [ -n "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ]; then
    setsid "$bin" > /dev/null 2>&1 < /dev/null &
    echo "Restarted the running copy"
elif [ -n "$mine$closed" ]; then
    echo "The running Markpaste was closed: start it again from the menu"
fi
