#!/bin/bash
#
# Build a self-contained, distributable QuickImport.app.
#
# The normal development build resolves Qt through @rpath into ~/Qt and links
# LibRaw by absolute path from ~/devel/LibRaw, so it only runs on this machine.
# This script produces a bundle that carries everything it needs.
#
#   ./package-macos.sh              build, deploy and ad-hoc sign
#   ./package-macos.sh --dmg        also wrap the result in a DMG
#
# Override the dependency locations with QT_DIR / LIBRAW_ROOT / EXIV2_ROOT.

set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SRC_DIR/build-release}"
QT_DIR="${QT_DIR:-$HOME/Qt/6.11.0/macos}"
LIBRAW_ROOT="${LIBRAW_ROOT:-$HOME/devel/LibRaw}"
EXIV2_ROOT="${EXIV2_ROOT:-$HOME/devel/exiv2}"
APP="$BUILD_DIR/QuickImport.app"
MAKE_DMG=0

for arg in "$@"; do
    case "$arg" in
        --dmg) MAKE_DMG=1 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

step() { printf '\n\033[1m==> %s\033[0m\n' "$1"; }
fail() { printf '\033[31mERROR: %s\033[0m\n' "$1" >&2; exit 1; }

[ -x "$QT_DIR/bin/macdeployqt" ] || fail "macdeployqt not found at $QT_DIR/bin (set QT_DIR)"

# The deployment target the bundle promises to support. Every binary that ends
# up inside it must be built for this version or older, otherwise the app dies
# on launch on older macOS with no useful error.
DEPLOYMENT_TARGET=$(sed -n 's/.*CMAKE_OSX_DEPLOYMENT_TARGET *"\([0-9.]*\)".*/\1/p' "$SRC_DIR/CMakeLists.txt" | head -1)
DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-14.0}"

step "Configuring release build (deployment target $DEPLOYMENT_TARGET)"
rm -rf "$BUILD_DIR"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_DIR" \
    -DLIBRAW_ROOT="$LIBRAW_ROOT" \
    -DEXIV2_ROOT="$EXIV2_ROOT"

step "Building"
cmake --build "$BUILD_DIR" -j "$(sysctl -n hw.ncpu)"
[ -d "$APP" ] || fail "no app bundle at $APP"

step "Deploying Qt frameworks and plugins"
# -always-overwrite so a rerun cannot leave a stale framework behind.
"$QT_DIR/bin/macdeployqt" "$APP" -always-overwrite -no-strip

step "Bundling LibRaw"
# CMake points the executable at the dylib's build-tree path so the dev build
# runs; for distribution it has to come from inside the bundle.
LIBRAW_DYLIB=$(otool -L "$APP/Contents/MacOS/QuickImport" | awk '/libraw/ {print $1}' | head -1 || true)
if [ -n "$LIBRAW_DYLIB" ] && [[ "$LIBRAW_DYLIB" != @* ]]; then
    mkdir -p "$APP/Contents/Frameworks"
    cp -f "$LIBRAW_DYLIB" "$APP/Contents/Frameworks/"
    NAME=$(basename "$LIBRAW_DYLIB")
    chmod u+w "$APP/Contents/Frameworks/$NAME"
    install_name_tool -id "@rpath/$NAME" "$APP/Contents/Frameworks/$NAME"
    install_name_tool -change "$LIBRAW_DYLIB" "@rpath/$NAME" "$APP/Contents/MacOS/QuickImport"
    echo "    bundled $NAME"
else
    echo "    already bundled ($LIBRAW_DYLIB)"
fi

step "Checking for dependencies outside the bundle"
# Anything that is not a system library and not @rpath/@executable_path would
# be resolved from this machine only.
BAD=0
while IFS= read -r bin; do
    # Dependency lines are the tab-indented ones; the untabbed lines are
    # per-architecture headers (Qt ships universal binaries). A library's own
    # install name also shows up as a dependency, so drop that too.
    SELF=$(otool -D "$bin" 2>/dev/null | grep -v ':$' | head -1 || true)
    while IFS= read -r dep; do
        if [ "$dep" = "$SELF" ]; then continue; fi
        case "$dep" in
            /usr/lib/*|/System/*|@rpath/*|@executable_path/*|@loader_path/*) ;;
            *) echo "    $(basename "$bin"): $dep"; BAD=1 ;;
        esac
    done < <(otool -L "$bin" 2>/dev/null | grep $'^\t' | awk '{print $1}' || true)
done < <(find "$APP" -type f \( -perm -u+x -o -name '*.dylib' \) ! -name '*.qm' ! -name '*.plist')
if [ "$BAD" -eq 0 ]; then
    echo "    OK: only system libraries and bundle-relative paths"
else
    fail "the bundle references libraries outside itself (see above)"
fi

step "Checking deployment targets"
# A library built for a newer macOS than the bundle claims makes the app refuse
# to launch on the versions it advertises support for.
BAD=0
while IFS= read -r bin; do
    MINOS=$(otool -l "$bin" 2>/dev/null | awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; exit}' || true)
    if [ -z "$MINOS" ]; then continue; fi
    if [ "$(printf '%s\n%s\n' "$MINOS" "$DEPLOYMENT_TARGET" | sort -V | tail -1)" != "$DEPLOYMENT_TARGET" ]; then
        echo "    $(basename "$bin"): built for macOS $MINOS"
        BAD=1
    fi
done < <(find "$APP" -type f \( -perm -u+x -o -name '*.dylib' \) ! -name '*.qm' ! -name '*.plist')
if [ "$BAD" -eq 0 ]; then
    echo "    OK: everything runs on macOS $DEPLOYMENT_TARGET"
else
    fail "the bundle contains libraries that need a newer macOS than $DEPLOYMENT_TARGET"
fi

step "Signing (ad-hoc)"
# Ad-hoc: good enough for your own machines. Distributing to others needs a
# "Developer ID Application" certificate plus notarisation.
codesign --force --deep --sign - --timestamp=none "$APP"
codesign --verify --deep --strict --verbose=2 "$APP" 2>&1 | sed 's/^/    /'

if [ "$MAKE_DMG" -eq 1 ]; then
    step "Building DMG"
    VERSION=$(defaults read "$APP/Contents/Info.plist" CFBundleShortVersionString)
    DMG="$BUILD_DIR/QuickImport-$VERSION.dmg"
    STAGE=$(mktemp -d)
    cp -R "$APP" "$STAGE/"
    ln -s /Applications "$STAGE/Applications"
    rm -f "$DMG"
    hdiutil create -volname "QuickImport $VERSION" -srcfolder "$STAGE" \
        -ov -format UDZO "$DMG" >/dev/null
    rm -rf "$STAGE"
    echo "    $DMG"
fi

step "Done"
echo "    $APP"
du -sh "$APP" | awk '{print "    size: " $1}'
