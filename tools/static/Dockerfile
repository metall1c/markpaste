# The image for the static Linux build (ADR 0028): Debian 12 (glibc 2.36, so the binary runs on
# Debian 12+, Ubuntu 23.04+, Fedora 37+) with a static Qt in /opt/qt.
#   docker build -t markpaste-static tools/static
FROM debian:12

ARG QT_VERSION=6.12.0
ARG ECM_VERSION=6.31.0
ARG LSQ_VERSION=6.8.1
ARG QT_MIRROR=https://download.qt.io/official_releases/qt

RUN export DEBIAN_FRONTEND=noninteractive && apt-get update -q && apt-get install -y -q --no-install-recommends \
        build-essential ninja-build meson pkg-config python3 perl curl ca-certificates xz-utils file \
        libfontconfig-dev libfreetype-dev libxkbcommon-dev libxkbcommon-x11-dev \
        libwayland-dev wayland-protocols libdbus-1-dev libssl-dev \
        libxcb1-dev libxcb-cursor-dev libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev \
        libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev libxcb-shm0-dev libxcb-sync-dev \
        libxcb-xfixes0-dev libxcb-xkb-dev libxcb-xtest0-dev libxcb-util-dev \
        libgtest-dev libgmock-dev \
    && rm -rf /var/lib/apt/lists/*

# ECM 6.31 needs CMake 3.30+, Debian 12 has 3.25: the official Kitware build
ARG CMAKE_VERSION=3.31.8
COPY sources/ /sources/
RUN f=cmake-$CMAKE_VERSION-linux-x86_64.tar.gz && cd /tmp \
    && { [ -f /sources/$f ] && cp /sources/$f . || curl -fsSLO https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/$f; } \
    && echo "630615d8e98ac33eba7fbe472626dff5c899c85af3c024585ae109166a6909d0  $f" | sha256sum -c - \
    && tar xzf $f -C /opt && ln -s /opt/cmake-$CMAKE_VERSION-linux-x86_64/bin/* /usr/local/bin/ && rm $f

# Debian 12 has wayland-protocols 1.31, without ext-data-control-v1 (spec PST-1)
ARG WL_PROTOCOLS_VERSION=1.47
RUN f=wayland-protocols-$WL_PROTOCOLS_VERSION.tar.xz && cd /tmp \
    && { [ -f /sources/$f ] && cp /sources/$f . || curl -fsSLO https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/$WL_PROTOCOLS_VERSION/downloads/$f; } \
    && echo "5fd4349bcbc9bab9a46f8cf77d1f434296a7a052c87440a094f63fcf62a58e20  $f" | sha256sum -c - \
    && tar xJf $f && meson setup --prefix=/usr -Dtests=false wp wayland-protocols-$WL_PROTOCOLS_VERSION \
    && meson install -C wp && rm -rf wp wayland-protocols-* $f

# Tarballs placed in tools/static/sources/ are used instead of downloading (same checksums)
COPY build-qt.sh /usr/local/bin/
ENV QT_VERSION=$QT_VERSION ECM_VERSION=$ECM_VERSION LSQ_VERSION=$LSQ_VERSION QT_MIRROR=$QT_MIRROR
RUN sh /usr/local/bin/build-qt.sh qt
RUN sh /usr/local/bin/build-qt.sh kde && rm -rf /sources

# The xcb extensions and xcb-util go into the binary: libxcb-cursor0 and friends are missing on
# many Wayland desktops (Ubuntu). Without the .so links the linker takes the .a archives.
# libxcb.so.1 itself stays shared: libxkbcommon-x11 uses it too, two copies would clash.
RUN cd /usr/lib/x86_64-linux-gnu && for l in libxcb-*.so; do rm "$l"; done && ls libxcb*.so
