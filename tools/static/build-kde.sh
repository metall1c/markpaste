#!/bin/sh
# Builds ECM and a static LayerShellQt against the static Qt in $PREFIX (tools/static/Dockerfile,
# ADR 0028). A separate step from build-qt.sh: changing it does not rebuild Qt.
set -eu
. /usr/local/lib/markpaste/fetch.sh

ECM=6.31.0
LSQ=6.8.1
PREFIX=${PREFIX:-/opt/qt}
QT=$(sed -n 's/^#define QT_VERSION_STR "\(.*\)"/\1/p' "$PREFIX/include/QtCore/qconfig.h")
KDE=${KDE_MIRROR:-https://download.kde.org/stable}
src=/tmp/kde-src
mkdir -p "$src" && cd "$src"

fetch "$KDE/frameworks/${ECM%.*}/extra-cmake-modules-$ECM.tar.xz" 8f74af329e4c918358bfe54316f8cf4d530c28444595a3e2fb15075f72877f17
fetch "$KDE/plasma/$LSQ/layer-shell-qt-$LSQ.tar.xz" c8141ac646fb8ae0f34e49c4c1d0051b9a90ae5bd7af1ef4585394d89f52b50e
tar xf extra-cmake-modules-$ECM.tar.xz && tar xf layer-shell-qt-$LSQ.tar.xz

cmake -S extra-cmake-modules-$ECM -B b-ecm -G Ninja -DCMAKE_INSTALL_PREFIX="$PREFIX" -DBUILD_TESTING=OFF -DBUILD_DOC=OFF
cmake --install b-ecm

# Qt generates the core Wayland protocol from its own newer wayland.xml (wl_fixes); the system
# wayland-client-protocol.h (Debian 12: 1.21) has the same include guard, so Qt's goes first
QT_WL_PROTOCOL_H=$PREFIX/include/QtWaylandClient/$QT/QtWaylandClient/private/wayland-wayland-client-protocol.h
[ -f "$QT_WL_PROTOCOL_H" ] || { echo "not found: $QT_WL_PROTOCOL_H" >&2; exit 1; }
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

rm -rf "$src"
