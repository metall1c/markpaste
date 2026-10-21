#!/bin/sh
# Regenerates the README screenshots in docs/images/{en,ru} with markpaste-uipreview
# (offscreen, the Breeze Dark palette, a demo set). Run after UI changes:
#   cmake --build build --target markpaste-uipreview && tools/dev/readme-screenshots.sh
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
preview="$root/build/tools/uipreview/markpaste-uipreview"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for lang in en ru; do
    "$preview" --dark --lang "$lang" "$tmp/$lang"
    dest="$root/docs/images/$lang"
    mkdir -p "$dest"
    for f in overlay editor editor-comment strip-set strip-empty preview settings-1 pen-menu-2 pen-menu-3 captions; do
        cp "$tmp/$lang/$f.png" "$dest/$f.png"
    done
done
if command -v optipng >/dev/null; then
    optipng -quiet -o2 "$root"/docs/images/*/*.png
fi
echo "Screenshots updated in docs/images/"
