#!/bin/sh
# Builds a static Qt (qtbase, qtsvg, qtwayland, linguist tools, translations) and a static
# LayerShellQt into $PREFIX. Runs inside the image from tools/static/Dockerfile (ADR 0028).
# Only the libraries every desktop has stay dynamic: glibc, libstdc++, libwayland-client/cursor,
# libxcb, libxkbcommon(-x11), fontconfig/freetype, libdbus. OpenGL is off (Widgets draw with QPainter; Wayland
# uses shm buffers), OpenSSL is loaded at run time (only the update check needs it).
# libdbus is linked: when Qt loaded it itself and it was missing, Qt's tray code crashed (docs/problems.md).
set -eu
# Stage: qt, kde (ECM and LayerShellQt) or all

QT=${QT_VERSION:?}
PREFIX=${PREFIX:-/opt/qt}
MIRROR=${QT_MIRROR:-https://download.qt.io/official_releases/qt}
KDE=${KDE_MIRROR:-https://download.kde.org/stable}
ECM=${ECM_VERSION:?}
LSQ=${LSQ_VERSION:?}
src=/tmp/qt-src
mkdir -p "$src" && cd "$src"

fetch() { # url sha256
    f=$(basename "$1")
    [ -f "/sources/$f" ] && cp "/sources/$f" . || curl -fsSL --retry 5 -o "$f" "$1"
    echo "$2  $f" | sha256sum -c -
    tar xJf "$f"
}
qtmod() { fetch "$MIRROR/${QT%.*}/$QT/submodules/$1-everywhere-src-$QT.tar.xz" "$2"; }

stage=${1:-all}
if [ "$stage" != kde ]; then
qtmod qtbase         a951bd163c7b80fc6b8c88d7668fb56abf91c152373e13c10666763238131307
qtmod qtsvg          e4ab39534ec97987b1b9b60ef7f4d3253d5912a69f8c713a01753174a4029331
qtmod qtwayland      ae14d9bcf1bb3c300a3ab2e6a53f50284e5c6c966a4ac4cc5aedcb1bc74e4d02
qtmod qttools        8dab8f3611496486a470ad5f115ceea584f36bc22a2b8b6f6ebdbafbb8160693
qtmod qttranslations 85929c0c30d6f273f23bd879bb69803ea17b010ab73cab2f8abf95357f6f6bbb
fi
if [ "$stage" != qt ]; then
fetch "$KDE/frameworks/${ECM%.*}/extra-cmake-modules-$ECM.tar.xz" 8f74af329e4c918358bfe54316f8cf4d530c28444595a3e2fb15075f72877f17
fetch "$KDE/plasma/$LSQ/layer-shell-qt-$LSQ.tar.xz" c8141ac646fb8ae0f34e49c4c1d0051b9a90ae5bd7af1ef4585394d89f52b50e

fi

if [ "$stage" != kde ]; then
cmake -S qtbase-everywhere-src-$QT -B b-qtbase -G Ninja \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF -DQT_BUILD_BENCHMARKS=OFF \
    -DINPUT_opengl=no -DFEATURE_vulkan=OFF -DFEATURE_egl=OFF \
    -DFEATURE_xcb=ON -DFEATURE_xlib=OFF -DFEATURE_xcb_xlib=OFF -DFEATURE_xcb_sm=OFF \
    -DFEATURE_system_xcb_xinput=OFF \
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
mod qtwayland -DFEATURE_wayland_server=OFF -DFEATURE_wayland_egl=OFF -DFEATURE_wayland_vulkan_server_buffer=OFF
# Only lupdate/lrelease are needed
mod qttools -DFEATURE_assistant=OFF -DFEATURE_designer=OFF -DFEATURE_linguist=ON \
    -DFEATURE_pixeltool=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF -DFEATURE_qdoc=OFF \
    -DFEATURE_clang=OFF -DFEATURE_distancefieldgenerator=OFF -DFEATURE_kmap2qmap=OFF \
    -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qev=OFF -DFEATURE_qdbus=OFF -DFEATURE_qtdbus=OFF
mod qttranslations
fi

if [ "$stage" != qt ]; then

cmake -S extra-cmake-modules-$ECM -B b-ecm -G Ninja -DCMAKE_INSTALL_PREFIX="$PREFIX" -DBUILD_TESTING=OFF -DBUILD_DOC=OFF
cmake --install b-ecm

# Qt generates the core Wayland protocol from its own newer wayland.xml (wl_fixes); the system
# wayland-client-protocol.h (Debian 12: 1.21) has the same include guard, so Qt's goes first
QT_WL_PROTOCOL_H=$PREFIX/include/QtWaylandClient/$QT/QtWaylandClient/private/wayland-wayland-client-protocol.h
# LayerShellQt: only the static Interface library. Window::get() installs the shell
# integration itself, so the plugin is not needed; the QML part would need QtQml.
cd layer-shell-qt-$LSQ
sed -i 's/ Qml)/)/; /add_subdirectory(declarative)/d; /add_subdirectory(tests)/d; /include(ECMQmlModule)/d' CMakeLists.txt src/CMakeLists.txt
sed -i '/^add_library(layer-shell SHARED/,/^endif()/d; /^install(TARGETS layer-shell$/,/wayland-shell-integration)/d' src/CMakeLists.txt
cd ..
"$PREFIX/bin/qt-cmake" -S layer-shell-qt-$LSQ -B b-lsq -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$PREFIX" -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF \
    -DCMAKE_CXX_FLAGS="-include $QT_WL_PROTOCOL_H"
cmake --build b-lsq
cmake --install b-lsq
fi

rm -rf "$src"
