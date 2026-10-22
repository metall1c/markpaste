# Sourced by the image build steps (Dockerfile): fetch <url> <sha256> puts the file into the
# current directory. A file of the same name in /sources (tools/static/sources/, mounted only
# while a step runs) is used instead of downloading; the checksum is checked either way.
fetch() {
    f=$(basename "$1")
    if [ -f "/sources/$f" ]; then cp "/sources/$f" .; else curl -fsSL --retry 5 -o "$f" "$1"; fi
    echo "$2  $f" | sha256sum -c -
}
