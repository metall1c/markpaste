#!/bin/sh
# Builds the release packages in a clean container, so that each Linux package is linked
# against its distribution's Qt (docs/build.md, "Release packages in containers").
#   tools/package/build-in-container.sh fedora:44      # → dist/*.rpm, dist/*.tar.gz
#   tools/package/build-in-container.sh ubuntu:24.04   # → dist/*~ubuntu24.04_amd64.deb (Qt 6.4: also checks the API minimum)
#   tools/package/build-in-container.sh ubuntu:26.04   # → dist/*~ubuntu26.04_amd64.deb; debian:13 the same way
#   tools/package/build-in-container.sh windows        # → dist/*.zip, dist/*-setup.exe (MinGW on fedora:44)
# The sources are mounted read-only; the core and UI tests run before packaging (Linux).
# CONTAINER_ENGINE=docker|podman picks the engine (by default podman, then docker).
set -eu

target=${1:?usage: $0 <fedora:44|ubuntu:24.04|ubuntu:26.04|debian:13|windows>}
image=$target
root=$(cd "$(dirname "$0")/../.." && pwd)
dist="$root/dist"
mkdir -p "$dist"
engine=${CONTAINER_ENGINE:-$(command -v podman || command -v docker || true)}
[ -n "$engine" ] || { echo "podman or docker is needed" >&2; exit 1; }

configure='-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DMARKPASTE_BUILD_TESTS=ON -DMARKPASTE_BUILD_TOOLS=OFF'
tests='QT_QPA_PLATFORM=offscreen ./tests/markpaste_core_tests --gtest_brief=1
    QT_QPA_PLATFORM=offscreen ./tests/markpaste_ui_tests --gtest_brief=1'
targets='package'
outdir=/tmp/build
case "$target" in
windows)
    # Cross-compiling with MinGW (ADR 0017). The tests are .exe files: they run under Wine
    # on the developer machine (docs/build.md), not here.
    image=fedora:44
    install='dnf install -y --setopt=install_weak_deps=False --setopt=retries=10 --disablerepo="*openh264*" \
        cmake ninja-build mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-qt6-qttools mingw64-qt6-qtsvg \
        mingw64-qt6-qtimageformats mingw64-qt6-qttranslations mingw32-nsis qt6-qttools qt6-linguist git'
    configure='-DCMAKE_TOOLCHAIN_FILE=/src/cmake/mingw64-fedora.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DMARKPASTE_BUILD_TESTS=OFF -DMARKPASTE_BUILD_TOOLS=OFF'
    tests=':'
    targets='package installer'
    outdir=/tmp/build/package
    keep='*.zip *-setup.exe'
    ;;
fedora:*)
    install='dnf install -y --setopt=install_weak_deps=False --setopt=retries=10 --disablerepo="*openh264*" gcc-c++ cmake ninja-build rpm-build \
        qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtwayland-devel qt6-qttools-devel qt6-linguist \
        qt6-qtsvg-devel layer-shell-qt-devel wayland-devel wayland-protocols-devel libxkbcommon-devel \
        gtest-devel gmock-devel libxcb-devel xcb-util-keysyms-devel libXtst-devel'
    keep='*.rpm *.tar.gz'
    ;;
ubuntu:*|debian:*)
    # Library package names differ between releases (the t64 transition), so a .deb fits only the
    # release it was built on: the distribution goes into the package version, 0.2.0-1~ubuntu24.04.
    distro=$(echo "$image" | tr -d ':')
    # Ubuntu 24.04 has only the Qt 5 LayerShellQt; linking it into a Qt 6 program would break it
    layershell=liblayershellqtinterface-dev
    [ "$image" = ubuntu:24.04 ] && layershell=
    install="export DEBIAN_FRONTEND=noninteractive && apt-get update -q && apt-get install -y -q --no-install-recommends \
        g++ cmake ninja-build pkg-config dpkg-dev file \
        qt6-base-dev qt6-base-private-dev qt6-wayland-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
        qt6-svg-dev libgl-dev libwayland-dev wayland-protocols libxkbcommon-dev $layershell \
        libgtest-dev libgmock-dev libxcb1-dev libxcb-xtest0-dev libxcb-keysyms1-dev libxtst-dev"
    configure="$configure -DCPACK_DEBIAN_PACKAGE_RELEASE=1~$distro"
    keep='*.deb'
    ;;
*)
    echo "unsupported image: $image" >&2
    exit 1
    ;;
esac

"$engine" run --rm -v "$root:/src:ro,z" -v "$dist:/out:z" "$image" sh -ec "
    $install
    cmake -S /src -B /tmp/build -G Ninja $configure
    cmake --build /tmp/build
    cd /tmp/build
    $tests
    for t in $targets; do cmake --build /tmp/build --target \$t; done
    cd $outdir
    for f in $keep; do cp -v \"\$f\" /out/; done
    chown $(id -u):$(id -g) /out/*
"
echo "Packages for $target are in $dist"
