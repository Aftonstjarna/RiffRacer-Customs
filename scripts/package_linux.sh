#!/usr/bin/env bash
# Run in an Arch Linux build container; see packaging/Containerfile.anylinux.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
version=$(cat VERSION)
export VERSION="$version"
[[ $(uname -m) == x86_64 ]]
mkdir -p build/anylinux dist
make -C LiveryMaker OUT=../build/anylinux APP=RiffRacer-Customs all test
install -Dm755 build/anylinux/RiffRacer-Customs /usr/bin/RiffRacer-Customs
revision=20be1aecb02be7dbb13fbf1ddef95fc93fc40b7e
curl -fL --retry 3 "https://raw.githubusercontent.com/pkgforge-dev/Anylinux-AppImages/$revision/useful-tools/quick-sharun.sh" -o build/anylinux/quick-sharun
printf '%s  %s\n' 6fc4f4561288297a7f8237f92b872656be1b7a4ba534f2b6cb9cfecabcddfd9d build/anylinux/quick-sharun | sha256sum -c -
chmod +x build/anylinux/quick-sharun
export APPDIR="$root/build/anylinux/AppDir"
export DESKTOP="$root/packaging/customs.desktop" ICON="$root/packaging/customs.svg"
export OUTPATH="$root/dist" OUTNAME="RiffRacer-Customs-$version-anylinux-x86_64.AppImage"
# GTK3/Cairo editor has no GL viewport. Keep software rendering portable.
export ALWAYS_SOFTWARE=1 GTK_FIX_NONSENSE=1
export STRACE_FLAGS="--smoke-package $root/build/anylinux/deploy-smoke.png"
rm -rf "$APPDIR"
xvfb-run -a build/anylinux/quick-sharun /usr/bin/RiffRacer-Customs
mkdir -p "$APPDIR/share/doc/riffracer-customs" "$APPDIR/share/fonts/truetype"
cp LICENSE README.md RELEASE-NOTES.md "$APPDIR/share/doc/riffracer-customs/"
cp NativeLinux/third_party/nlohmann/LICENSE.MIT "$APPDIR/share/doc/riffracer-customs/JSON-LICENSE.MIT"
cp -a /usr/share/licenses "$APPDIR/share/"
cp -a /usr/share/fonts/TTF/DejaVuSans*.ttf "$APPDIR/share/fonts/truetype/"
# Include a bundled fallback even when the host has no fonts installed.
python3 - "$APPDIR/etc/fonts/fonts.conf" <<'PYFONT'
from pathlib import Path
import sys
p = Path(sys.argv[1])
p.write_text(p.read_text().replace('<fontconfig>', '<fontconfig>\n<dir prefix="relative">../../share/fonts</dir>', 1))
PYFONT
pacman -Qi > "$APPDIR/share/doc/riffracer-customs/DEPENDENCY-DETAILS.txt"
cat > "$APPDIR/share/doc/riffracer-customs/SOURCES.txt" <<'SOURCES'
Arch Linux package recipes and upstream source URLs:
https://gitlab.archlinux.org/archlinux/packaging/packages
Exact installed package versions: DEPENDENCIES.txt
Historical binary packages: https://archive.archlinux.org/packages/
AnyLinux runtime and packaging sources:
https://github.com/pkgforge-dev/Anylinux-sharun/tree/3.9.0
https://github.com/pkgforge-dev/appimagetool/tree/0.7.0
https://github.com/pkgforge-dev/cross-libc-dlopen/tree/v0.2.11
RiffRacer Customs source: https://github.com/Aftonstjarna/RiffRacer-Customs
SOURCES
pacman -Q > "$APPDIR/share/doc/riffracer-customs/DEPENDENCIES.txt"
printf 'quick-sharun revision: %s\n' "$revision" > "$APPDIR/share/doc/riffracer-customs/BUILD.txt"
build/anylinux/quick-sharun --make-appimage
(cd dist; sha256sum "$OUTNAME" > SHA256SUMS-linux)
xvfb-run -a "dist/$OUTNAME" --smoke-package "$root/build/anylinux/package-smoke.png"
