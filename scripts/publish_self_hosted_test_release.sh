#!/usr/bin/env bash
# Publish an already-built Sorinuri package to the isolated pre-production test path.
# This script NEVER writes /var/www/sorinuri/version.json, normal /downloads assets,
# index.html, or changelog.html. It only writes /downloads/testing/<version>/.
# Required environment variables:
#   SORINURI_DEPLOY_HOST, SORINURI_DEPLOY_USER, SORINURI_DEPLOY_PASSWORD
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "Usage: $0 <version> <release-dir> <notes-file>" >&2
  exit 64
fi

VERSION="$1"
RELEASE_DIR="$(cd "$2" && pwd)"
NOTES_FILE="$(cd "$(dirname "$3")" && pwd)/$(basename "$3")"

if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Invalid version: $VERSION" >&2
  exit 65
fi

: "${SORINURI_DEPLOY_HOST:?Missing SORINURI_DEPLOY_HOST}"
: "${SORINURI_DEPLOY_USER:?Missing SORINURI_DEPLOY_USER}"
: "${SORINURI_DEPLOY_PASSWORD:?Missing SORINURI_DEPLOY_PASSWORD}"

INSTALLER="Sorinuri-Setup-${VERSION}.exe"
PORTABLE="Sorinuri-Qt-${VERSION}-Portable.zip"
for file in "$RELEASE_DIR/$INSTALLER" "$RELEASE_DIR/$PORTABLE" "$NOTES_FILE"; do
  [[ -s "$file" ]] || { echo "Required test package file missing: $file" >&2; exit 66; }
done

STAGING_DIR="$(mktemp -d)"
cleanup() { rm -rf "$STAGING_DIR"; }
trap cleanup EXIT

cp "$RELEASE_DIR/$INSTALLER" "$STAGING_DIR/$INSTALLER"
cp "$RELEASE_DIR/$PORTABLE" "$STAGING_DIR/$PORTABLE"
cp "$NOTES_FILE" "$STAGING_DIR/release-notes.txt"
cat > "$STAGING_DIR/TESTING-ONLY.txt" <<EOF
Sorinuri v${VERSION} pre-production test package

This package is for hardware and regression testing before production publication.
It does not change the public automatic-update manifest, normal download links, or landing page.
EOF
(
  cd "$STAGING_DIR"
  sha256sum "$INSTALLER" "$PORTABLE" > "SHA256SUMS-${VERSION}.txt"
)

# The existing /downloads/ Nginx location returns static files as attachments and does
# not fall back to the single-page index for missing files.
TEST_ROOT="/var/www/sorinuri/downloads/testing"
REMOTE_STAGE="${TEST_ROOT}/.incoming/${VERSION}-${RANDOM}-${RANDOM}"
SSH_BASE=(sshpass -p "$SORINURI_DEPLOY_PASSWORD" ssh -o StrictHostKeyChecking=yes
  "${SORINURI_DEPLOY_USER}@${SORINURI_DEPLOY_HOST}")
SCP_BASE=(sshpass -p "$SORINURI_DEPLOY_PASSWORD" scp -o StrictHostKeyChecking=yes)

"${SSH_BASE[@]}" "mkdir -p '$TEST_ROOT/.incoming' '$REMOTE_STAGE'"
"${SCP_BASE[@]}" \
  "$STAGING_DIR/$INSTALLER" "$STAGING_DIR/$PORTABLE" \
  "$STAGING_DIR/release-notes.txt" "$STAGING_DIR/TESTING-ONLY.txt" \
  "$STAGING_DIR/SHA256SUMS-${VERSION}.txt" \
  "${SORINURI_DEPLOY_USER}@${SORINURI_DEPLOY_HOST}:${REMOTE_STAGE}/"

"${SSH_BASE[@]}" "bash -s" -- "$VERSION" "$TEST_ROOT" "$REMOTE_STAGE" "$INSTALLER" "$PORTABLE" <<'REMOTE'
set -euo pipefail
VERSION="$1"
ROOT="$2"
STAGE="$3"
INSTALLER="$4"
PORTABLE="$5"
DESTINATION="$ROOT/$VERSION"

# Do not overwrite a previously published test package: a version uniquely identifies its tested bytes.
test ! -e "$DESTINATION" || { echo "Test package already exists: $DESTINATION" >&2; exit 67; }
for file in "$INSTALLER" "$PORTABLE" release-notes.txt TESTING-ONLY.txt "SHA256SUMS-${VERSION}.txt"; do
  test -s "$STAGE/$file"
done
(
  cd "$STAGE"
  sha256sum --check "SHA256SUMS-${VERSION}.txt"
)
chmod -R a+rX "$STAGE"
# Same-filesystem rename publishes only a complete, checksum-verified package directory.
mv "$STAGE" "$DESTINATION"
REMOTE

BASE="https://sorinuri.com/downloads/testing/${VERSION}"
printf 'Test package published (no production manifest changed):\n'
printf '  Installer: %s/%s\n' "$BASE" "$INSTALLER"
printf '  Portable : %s/%s\n' "$BASE" "$PORTABLE"
printf '  Notes    : %s/release-notes.txt\n' "$BASE"
