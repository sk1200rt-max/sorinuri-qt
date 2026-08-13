#!/usr/bin/env bash
# Publish an already-built Sorinuri release to the public sorinuri.com origin.
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
  [[ -s "$file" ]] || { echo "Required release file missing: $file" >&2; exit 66; }
done

STAGING_DIR="$(mktemp -d)"
cleanup() { rm -rf "$STAGING_DIR"; }
trap cleanup EXIT

cp "$RELEASE_DIR/$INSTALLER" "$STAGING_DIR/$INSTALLER"
cp "$RELEASE_DIR/$PORTABLE" "$STAGING_DIR/$PORTABLE"
cp "$NOTES_FILE" "$STAGING_DIR/release-notes.txt"
python3 "$(dirname "$0")/create_update_manifest.py" \
  "$VERSION" "$STAGING_DIR/$INSTALLER" "$STAGING_DIR/$PORTABLE" \
  "$STAGING_DIR/release-notes.txt" "$STAGING_DIR/version.json"
(
  cd "$STAGING_DIR"
  sha256sum "$INSTALLER" "$PORTABLE" > "SHA256SUMS-${VERSION}.txt"
)

REMOTE_ROOT="/var/www/sorinuri/downloads"
REMOTE_STAGE="${REMOTE_ROOT}/.incoming/${VERSION}-${RANDOM}-${RANDOM}"
SSH_BASE=(sshpass -p "$SORINURI_DEPLOY_PASSWORD" ssh -o StrictHostKeyChecking=yes
  "${SORINURI_DEPLOY_USER}@${SORINURI_DEPLOY_HOST}")
SCP_BASE=(sshpass -p "$SORINURI_DEPLOY_PASSWORD" scp -o StrictHostKeyChecking=yes)

"${SSH_BASE[@]}" "mkdir -p '$REMOTE_ROOT/.incoming' '$REMOTE_ROOT/backups' '$REMOTE_STAGE'"
"${SCP_BASE[@]}" \
  "$STAGING_DIR/$INSTALLER" "$STAGING_DIR/$PORTABLE" \
  "$STAGING_DIR/version.json" "$STAGING_DIR/SHA256SUMS-${VERSION}.txt" \
  "${SORINURI_DEPLOY_USER}@${SORINURI_DEPLOY_HOST}:${REMOTE_STAGE}/"

"${SSH_BASE[@]}" "bash -s" -- "$VERSION" "$REMOTE_ROOT" "$REMOTE_STAGE" "$INSTALLER" "$PORTABLE" <<'REMOTE'
set -euo pipefail
VERSION="$1"
ROOT="$2"
STAGE="$3"
INSTALLER="$4"
PORTABLE="$5"

for file in "$INSTALLER" "$PORTABLE" version.json "SHA256SUMS-${VERSION}.txt"; do
  test -s "$STAGE/$file"
done

# Preserve the previous manifest for an immediate server-side rollback.
SITE_ROOT="${ROOT%/downloads}"
if test -f "$ROOT/version.json"; then
  cp -f "$ROOT/version.json" "$ROOT/backups/version-previous.json"
fi
if test -f "$SITE_ROOT/version.json"; then
  cp -f "$SITE_ROOT/version.json" "$ROOT/backups/root-version-previous.json"
fi

# Publish assets first; switch manifests last so clients only see complete files.
mv -f "$STAGE/$INSTALLER" "$ROOT/$INSTALLER"
mv -f "$STAGE/$PORTABLE" "$ROOT/$PORTABLE"
mv -f "$STAGE/SHA256SUMS-${VERSION}.txt" "$ROOT/SHA256SUMS-${VERSION}.txt"
mv -f "$STAGE/version.json" "$ROOT/version.json.new"
cp -f "$ROOT/version.json.new" "$SITE_ROOT/version.json.new"
mv -f "$ROOT/version.json.new" "$ROOT/version.json"
mv -f "$SITE_ROOT/version.json.new" "$SITE_ROOT/version.json"
rm -rf "$STAGE"
REMOTE

echo "Self-hosted release published: ${VERSION}"
