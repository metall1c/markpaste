#!/bin/sh
# Builds Markpaste against the static Qt from tools/static/Dockerfile (ADR 0028), runs the tests
# and packs the archive for any distribution:
#   tools/static/build.sh   # → dist/markpaste-<version>-linux-x86_64.tar.gz, build/static/markpaste
# The image is built on first use (Qt from source: ~6 min on 16 cores, then cached by the engine).
# CONTAINER_ENGINE=docker|podman picks the engine (by default podman, then docker).
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
version=$(sed -n 's/^project(Markpaste VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")
name=markpaste-$version-linux-x86_64
dist="$root/dist"
out="$root/build/static"
mkdir -p "$dist" "$out"
engine=${CONTAINER_ENGINE:-$(command -v podman || command -v docker || true)}
[ -n "$engine" ] || { echo "podman or docker is needed" >&2; exit 1; }
image=${MARKPASTE_STATIC_IMAGE:-markpaste-static}

"$engine" image inspect "$image" > /dev/null 2>&1 || "$engine" build -t "$image" "$root/tools/static"

"$engine" run --rm -v "$root:/src:ro,z" -v "$out:/out:z" -v "$dist:/dist:z" "$image" sh -ec "
    cmake -S /src -B /tmp/build -G Ninja -DCMAKE_PREFIX_PATH=/opt/qt -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release -DMARKPASTE_BUILD_TESTS=ON -DMARKPASTE_BUILD_TOOLS=OFF
    cmake --build /tmp/build
    cd /tmp/build
    QT_QPA_PLATFORM=offscreen ./tests/markpaste_core_tests --gtest_brief=1
    QT_QPA_PLATFORM=offscreen ./tests/markpaste_ui_tests --gtest_brief=1
    DESTDIR=/tmp/stage cmake --install /tmp/build --strip > /dev/null
    p=/tmp/$name && mkdir -p \$p/share
    cp /tmp/stage/usr/bin/markpaste /src/tools/static/install.sh /src/LICENSE \$p/
    cp /tmp/stage/usr/share/applications/*.desktop /tmp/stage/usr/share/metainfo/*.xml \$p/share/
    cp /tmp/stage/usr/share/icons/hicolor/scalable/apps/*.svg \$p/share/
    cp \$p/markpaste /out/markpaste
    tar -C /tmp -czf /dist/$name.tar.gz $name
    chown $(id -u):$(id -g) /out/markpaste /dist/$name.tar.gz
"
echo "Archive: $dist/$name.tar.gz"
