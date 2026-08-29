#!/bin/bash
#
# Walk through a QuickImport release, one confirmed step at a time.
#
#   ./release.sh            release the version in CMakeLists.txt
#   ./release.sh --dry-run  do everything except tagging and publishing
#
# Nothing leaves this machine until the step that says so, and every step
# asks first. Answer "n" to stop; you can rerun the script afterwards.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

DRY_RUN=0
[ "${1:-}" = "--dry-run" ] && DRY_RUN=1

bold()  { printf '\n\033[1m%s\033[0m\n' "$1"; }
info()  { printf '    %s\n' "$1"; }
warn()  { printf '\033[33m    %s\033[0m\n' "$1"; }
fail()  { printf '\033[31mERROR: %s\033[0m\n' "$1" >&2; exit 1; }

confirm() {
    printf '\033[36m?\033[0m %s [j/N] ' "$1"
    read -r reply || reply=""
    case "$reply" in [jJyY]) return 0 ;; *) echo "Gestopt."; exit 0 ;; esac
}

# ---------------------------------------------------------------- 1. version
VERSION=$(sed -n 's/^project(QuickImport VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
[ -n "$VERSION" ] || fail "could not read the version from CMakeLists.txt"
TAG="v$VERSION"
DMG="build-release/QuickImport-$VERSION.dmg"
NOTES="release-notes/$VERSION.md"

bold "1. Version"
info "CMakeLists.txt says $VERSION, so this release is tagged $TAG."

LATEST=$(git tag -l 'v*' | sort -V | tail -1)
if [ -n "$LATEST" ]; then
    info "Highest existing tag: $LATEST"
    HIGHEST=$(printf '%s\n%s\n' "${LATEST#v}" "$VERSION" | sort -V | tail -1)
    if [ "$HIGHEST" != "$VERSION" ] || [ "${LATEST#v}" = "$VERSION" ]; then
        fail "$VERSION is not higher than $LATEST — bump the version in CMakeLists.txt first"
    fi
fi
git rev-parse "$TAG" >/dev/null 2>&1 && fail "tag $TAG already exists"
[ -f "$NOTES" ] || fail "no release notes at $NOTES"
info "Release notes: $NOTES ($(grep -c '' "$NOTES") lines)"
grep -q "^## \[$VERSION\]" CHANGELOG.md \
    || fail "CHANGELOG.md has no '## [$VERSION]' section — rename [Unreleased] first"
info "CHANGELOG.md has a section for $VERSION."

# ------------------------------------------------------------- 2. work tree
bold "2. Working tree"
BRANCH=$(git rev-parse --abbrev-ref HEAD)
info "Branch: $BRANCH"
[ -n "$(git status --porcelain)" ] && fail "uncommitted changes — commit or stash them first"
info "No uncommitted changes."
git fetch --quiet origin
LOCAL=$(git rev-parse @)
REMOTE=$(git rev-parse "@{u}" 2>/dev/null || echo none)
[ "$LOCAL" = "$REMOTE" ] || fail "local and origin/$BRANCH differ — push or pull first"
info "In sync with origin/$BRANCH."

# ------------------------------------------------------------------ 3. build
bold "3. Build the release bundle and DMG"
info "Runs package-macos.sh --dmg: clean release build, macdeployqt, LibRaw"
info "bundled, then it verifies the bundle is self-contained and that nothing"
info "in it needs a newer macOS than the deployment target."
confirm "Build now? (a few minutes)"
./package-macos.sh --dmg
[ -f "$DMG" ] || fail "expected $DMG but it is not there"
info "Built $DMG ($(du -h "$DMG" | cut -f1))"

# ------------------------------------------------------------------ 4. check
bold "4. Check the app before publishing"
warn "The app is ad-hoc signed, not notarised: everyone who downloads it will"
warn "have to approve it under System Settings > Privacy & Security."
info "Opening the packaged app so you can look at it yourself."
confirm "Open build-release/QuickImport.app?"
open build-release/QuickImport.app
echo
info "Check at least: the app starts, the language picker works, and the"
info "version in the title bar reads $VERSION."
confirm "Does it all look right?"
osascript -e 'quit app "QuickImport"' >/dev/null 2>&1 || true

# ------------------------------------------------------------------ 5. notes
bold "5. Release notes"
echo
sed 's/^/    /' "$NOTES"
echo
confirm "Publish these notes as they are?"

# -------------------------------------------------------------------- 6. tag
bold "6. Tag and publish"
if [ "$DRY_RUN" -eq 1 ]; then
    warn "--dry-run: stopping here. Nothing was tagged or published."
    info "Would have run:"
    info "  git tag -a $TAG -m \"QuickImport $VERSION\" && git push origin $TAG"
    info "  gh release create $TAG $DMG --title \"QuickImport $VERSION\" --notes-file $NOTES"
    exit 0
fi

command -v gh >/dev/null || fail "gh is not installed (brew install gh)"
gh auth status >/dev/null 2>&1 || fail "not logged in to GitHub — run: gh auth login"

warn "This is the point of no return: the tag and the release become public."
confirm "Create tag $TAG, push it, and publish the GitHub release?"

git tag -a "$TAG" -m "QuickImport $VERSION"
git push origin "$TAG"
info "Tag $TAG pushed."

gh release create "$TAG" "$DMG" \
    --title "QuickImport $VERSION" \
    --notes-file "$NOTES"

bold "Done"
gh release view "$TAG" --json url --jq .url | sed 's/^/    /'
