# The image for the static Linux build (ADR 0028): Debian 12 (glibc 2.36, so the binary runs on
# Debian 12+, Ubuntu 23.04+, Fedora 37+) with a static Qt in /opt/qt.
#   tools/static/build.sh builds it as markpaste-static:<hash of these files>
# Versions and checksums are in the steps below and in build-qt.sh / build-kde.sh, each next to
# its download. The steps go from the slowest to change to the quickest, so that a change
# rebuilds only what follows it. Tarballs placed in tools/static/sources/ are used instead of
# downloading (same checksums); they are mounted while a step runs and do not stay in the image.
FROM debian:12

# rpm: the .rpm comes from the same static build as the .deb
RUN export DEBIAN_FRONTEND=noninteractive && apt-get update -q && apt-get install -y -q --no-install-recommends \
        build-essential ninja-build meson pkg-config python3 perl curl ca-certificates xz-utils file rpm \
        libfontconfig-dev libfreetype-dev libxkbcommon-dev libxkbcommon-x11-dev \
        libwayland-dev wayland-protocols libdbus-1-dev libssl-dev \
        libxcb1-dev libxcb-cursor-dev libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev \
        libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev libxcb-shm0-dev libxcb-sync-dev \
        libxcb-xfixes0-dev libxcb-xkb-dev libxcb-xtest0-dev libxcb-util-dev \
        libgtest-dev libgmock-dev \
    && rm -rf /var/lib/apt/lists/*

COPY fetch.sh /usr/local/lib/markpaste/

# ECM 6.31 needs CMake 3.30+, Debian 12 has 3.25: the official Kitware build
RUN --mount=type=bind,source=sources,target=/sources . /usr/local/lib/markpaste/fetch.sh && cd /tmp \
    && v=3.31.8 && f=cmake-$v-linux-x86_64.tar.gz \
    && fetch https://github.com/Kitware/CMake/releases/download/v$v/$f 630615d8e98ac33eba7fbe472626dff5c899c85af3c024585ae109166a6909d0 \
    && tar xf $f -C /opt && ln -s /opt/cmake-$v-linux-x86_64/bin/* /usr/local/bin/ && rm $f

# Debian 12 has wayland-protocols 1.31, without ext-data-control-v1 (spec PST-1)
RUN --mount=type=bind,source=sources,target=/sources . /usr/local/lib/markpaste/fetch.sh && cd /tmp \
    && v=1.47 && f=wayland-protocols-$v.tar.xz \
    && fetch https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/$v/downloads/$f 5fd4349bcbc9bab9a46f8cf77d1f434296a7a052c87440a094f63fcf62a58e20 \
    && tar xf $f && meson setup --prefix=/usr -Dtests=false wp wayland-protocols-$v \
    && meson install -C wp && rm -rf wp wayland-protocols-* $f

# Another mirror when download.qt.io does not answer: --build-arg QT_MIRROR=https://…/qt
ARG QT_MIRROR=https://download.qt.io/official_releases/qt
COPY build-qt.sh /usr/local/lib/markpaste/
RUN --mount=type=bind,source=sources,target=/sources QT_MIRROR=$QT_MIRROR sh /usr/local/lib/markpaste/build-qt.sh

COPY build-kde.sh /usr/local/lib/markpaste/
RUN --mount=type=bind,source=sources,target=/sources sh /usr/local/lib/markpaste/build-kde.sh

# The xcb extensions and xcb-util go into the binary: libxcb-cursor0 and friends are missing on
# many Wayland desktops (Ubuntu). Without the .so links the linker takes the .a archives.
# libxcb.so.1 itself stays shared: libxkbcommon-x11 uses it too, two copies would clash.
RUN cd /usr/lib/x86_64-linux-gnu && for l in libxcb-*.so; do rm "$l"; done && ls libxcb*.so
