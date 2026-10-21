#!/bin/sh
# Prints the CHANGELOG.md section of a version, the release notes for GitHub (docs/release.md).
#   tools/package/release-notes.sh 0.2.0
set -eu
version=${1:?usage: $0 <version>}
root=$(cd "$(dirname "$0")/../.." && pwd)
notes=$(awk -v v="$version" '
    index($0, "## [" v "]") == 1 { on = 1; next }
    on && /^## \[/ { exit }
    on && /^\[[^]]*\]: / { next }
    on { print }
' "$root/CHANGELOG.md")
[ -n "$(echo "$notes" | tr -d '[:space:]')" ] || { echo "no CHANGELOG.md section for $version" >&2; exit 1; }
echo "$notes" | sed -e '/./,$!d'
printf '\nThe user guide with screenshots: [README](https://github.com/metall1c/markpaste#readme) ([по-русски](https://github.com/metall1c/markpaste/blob/main/README.ru.md)). Checksums: `SHA256SUMS`.\n'
