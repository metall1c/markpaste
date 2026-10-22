#!/bin/sh
# Builds a static Qt (qtbase, qtsvg, qtwayland, linguist tools, translations) into $PREFIX.
# Runs inside the image from tools/static/Dockerfile (ADR 0028).
# Only the libraries every desktop has stay dynamic: glibc, libstdc++, libwayland-client/cursor,
# libxcb, libxkbcommon(-x11), fontconfig/freetype, libdbus. OpenGL is off (Widgets draw with QPainter; Wayland
# uses shm buffers), OpenSSL is loaded at run time (only the update check needs it).
# libdbus is linked: when Qt loaded it itself and it was missing, Qt's tray code crashed (docs/problems.md).
# A new Qt version: change QT and the five checksums below (from the .sha256 files next to the tarballs).
set -eu
. /usr/local/lib/markpaste/fetch.sh

QT=6.12.0
PREFIX=${PREFIX:-/opt/qt}
MIRROR=${QT_MIRROR:-https://download.qt.io/official_releases/qt}
src=/tmp/qt-src
mkdir -p "$src" && cd "$src"

qtmod() { # module sha256
    fetch "$MIRROR/${QT%.*}/$QT/submodules/$1-everywhere-src-$QT.tar.xz" "$2"
    tar xf "$1-everywhere-src-$QT.tar.xz" && rm "$1-everywhere-src-$QT.tar.xz"
}
qtmod qtbase         a951bd163c7b80fc6b8c88d7668fb56abf91c152373e13c10666763238131307
qtmod qtsvg          e4ab39534ec97987b1b9b60ef7f4d3253d5912a69f8c713a01753174a4029331
qtmod qtwayland      ae14d9bcf1bb3c300a3ab2e6a53f50284e5c6c966a4ac4cc5aedcb1bc74e4d02
qtmod qttools        8dab8f3611496486a470ad5f115ceea584f36bc22a2b8b6f6ebdbafbb8160693
qtmod qttranslations 85929c0c30d6f273f23bd879bb69803ea17b010ab73cab2f8abf95357f6f6bbb

# Since Qt 6.10 the Wayland features live in qtbase (QtGui): wayland-server (the compositor
# library in qtwayland) and the old shells are switched off here, not in the qtwayland step
cmake -S qtbase-everywhere-src-$QT -B b-qtbase -G Ninja \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF -DQT_BUILD_BENCHMARKS=OFF \
    -DINPUT_opengl=no -DFEATURE_vulkan=OFF -DFEATURE_egl=OFF \
    -DFEATURE_xcb=ON -DFEATURE_xlib=OFF -DFEATURE_xcb_xlib=OFF -DFEATURE_xcb_sm=OFF \
    -DFEATURE_system_xcb_xinput=OFF \
    -DFEATURE_wayland_server=OFF -DFEATURE_wayland_client_wl_shell=OFF \
    -DFEATURE_wayland_client_fullscreen_shell_v1=OFF \
    -DFEATURE_fontconfig=ON -DFEATURE_system_freetype=ON -DFEATURE_system_harfbuzz=OFF \
    -DFEATURE_system_png=OFF -DFEATURE_system_jpeg=OFF -DFEATURE_system_zlib=OFF \
    -DFEATURE_system_pcre2=OFF -DFEATURE_system_doubleconversion=OFF -DFEATURE_system_libb2=OFF \
    -DFEATURE_icu=OFF -DFEATURE_glib=OFF -DFEATURE_gtk3=OFF -DFEATURE_cups=OFF \
    -DFEATURE_libproxy=OFF -DFEATURE_gssapi=OFF -DFEATURE_brotli=OFF -DFEATURE_zstd=OFF \
    -DFEATURE_libudev=OFF -DFEATURE_libinput=OFF -DFEATURE_mtdev=OFF -DFEATURE_tslib=OFF \
    -DFEATURE_eglfs=OFF -DFEATURE_linuxfb=OFF -DFEATURE_vnc=OFF -DFEATURE_journald=OFF \
    -DFEATURE_sql=OFF -DFEATURE_printsupport=OFF \
    -DFEATURE_openssl=ON -DFEATURE_openssl_runtime=ON -DFEATURE_openssl_linked=OFF \
    -DFEATURE_dbus=ON -DFEATURE_dbus_linked=ON
cmake --build b-qtbase
cmake --install b-qtbase

mod() { # module dir, extra cmake args
    d=$1; shift
    "$PREFIX/bin/qt-cmake" -S $d-everywhere-src-$QT -B b-$d -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF "$@"
    cmake --build b-$d
    cmake --install b-$d
}
mod qtsvg
# The client part only: xdg-shell and the Adwaita decorations (GNOME draws no server-side ones)
mod qtwayland -DFEATURE_wayland_client_ivi_shell=OFF -DFEATURE_wayland_client_qt_shell=OFF \
    -DFEATURE_wayland_egl=OFF -DFEATURE_wayland_vulkan_server_buffer=OFF
# Only lupdate/lrelease are needed
mod qttools -DFEATURE_assistant=OFF -DFEATURE_designer=OFF -DFEATURE_linguist=ON \
    -DFEATURE_pixeltool=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF -DFEATURE_qdoc=OFF \
    -DFEATURE_clang=OFF -DFEATURE_distancefieldgenerator=OFF -DFEATURE_kmap2qmap=OFF \
    -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qev=OFF -DFEATURE_qdbus=OFF -DFEATURE_qtdbus=OFF
mod qttranslations

rm -rf "$src"
