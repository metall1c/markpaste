#!/bin/sh
# Builds Markpaste against the static Qt from tools/static/Dockerfile (ADR 0028), runs the tests
# and packs the archive and the packages for any distribution:
#   tools/static/build.sh              # → dist/: markpaste-<version>-linux-x86_64.tar.gz, markpaste_<version>-1_amd64.deb,
#                                      #   markpaste-<version>-1.x86_64.rpm; build/static/markpaste
#   tools/static/build.sh --image-name # prints the image name and exits (CI builds the image itself, with a cache)
# The image is markpaste-static:<hash of the files it is built from>: a change to them builds a new
# image (Qt from source: ~6 min on 16 cores) instead of silently reusing an old one.
# MARKPASTE_STATIC_IMAGE=<name> uses that image as it is.
# CONTAINER_ENGINE=docker|podman picks the engine (by default podman, then docker).
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
hash=$(cd "$root/tools/static" && cat Dockerfile fetch.sh build-qt.sh build-kde.sh | sha256sum | cut -c1-12)
image=${MARKPASTE_STATIC_IMAGE:-markpaste-static:$hash}
[ "${1:-}" = --image-name ] && { echo "$image"; exit 0; }
version=$(sed -n 's/^project(Markpaste VERSION \([0-9.]*\).*/\1/p' "$root/CMakeLists.txt")
name=markpaste-$version-linux-x86_64
dist="$root/dist"
out="$root/build/static"
mkdir -p "$dist" "$out"
engine=${CONTAINER_ENGINE:-$(command -v podman || command -v docker || true)}
[ -n "$engine" ] || { echo "podman or docker is needed" >&2; exit 1; }

if ! "$engine" image inspect "$image" > /dev/null 2>&1; then
    echo "Building the image $image (old markpaste-static images can be removed: $engine image ls markpaste-static)"
    "$engine" build -t "$image" "$root/tools/static"
fi

"$engine" run --rm -v "$root:/src:ro,z" -v "$out:/out:z" -v "$dist:/dist:z" "$image" sh -ec "
    cmake -S /src -B /tmp/build -G Ninja -DCMAKE_PREFIX_PATH=/opt/qt -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release -DMARKPASTE_BUILD_TESTS=ON -DMARKPASTE_BUILD_TOOLS=OFF \
        -DCPACK_DEBIAN_PACKAGE_RELEASE=1
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
    # One .deb for Debian and Ubuntu, one .rpm for Fedora, openSUSE and the like
    cpack -G 'DEB;RPM' > /dev/null
    cp -v markpaste_*.deb markpaste-*.rpm /dist/
    chown $(id -u):$(id -g) /out/markpaste /dist/$name.tar.gz /dist/markpaste_*.deb /dist/markpaste-*.rpm
"
echo "Archive: $dist/$name.tar.gz"
