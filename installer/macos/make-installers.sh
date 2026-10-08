#!/bin/bash
# Builds MikMap.app and, from it, MikMap-Setup.pkg and MikMap.dmg (the file names the store's Download tab expects).
#
#   installer/macos/make-installers.sh <build dir with mikmap + assets/> <version> <output dir>
#
# macOS only (uses pkgbuild, hdiutil, codesign). Nothing here is Developer-ID signed or notarised: the bundle gets an
# ad-hoc signature (required for arm64 code to run at all) and Gatekeeper will still warn on first launch.
set -euo pipefail

BUILD="$1"; VERSION="${2#v}"; OUT="$3"
ID="io.github.duyminh-bostrap.mikmap"      # TODO(owner): replace with the bundle identifier you own
ROOT="$(cd "$(dirname "$0")" && pwd)"
STAGE="$(mktemp -d)"
APP="$STAGE/MikMap.app"
mkdir -p "$OUT" "$APP/Contents/MacOS"

# The program looks for "assets" next to its own executable, so the folder goes beside it.
cp "$BUILD/mikmap" "$APP/Contents/MacOS/mikmap"
cp -R "$BUILD/assets" "$APP/Contents/MacOS/assets"
chmod +x "$APP/Contents/MacOS/mikmap"

# CFBundleVersion must be dotted numbers; "0.0.0-dev" style versions keep only the numeric part.
NUM="${VERSION%%-*}"
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>MikMap</string>
  <key>CFBundleDisplayName</key><string>MikMap</string>
  <key>CFBundleIdentifier</key><string>$ID</string>
  <key>CFBundleExecutable</key><string>mikmap</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$NUM</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSApplicationCategoryType</key><string>public.app-category.graphics-design</string>
</dict>
</plist>
PLIST

codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"

# --- MikMap-Setup.pkg: installs MikMap.app into /Applications ---------------------------------------------------
pkgbuild --component "$APP" --install-location /Applications --identifier "$ID" --version "$NUM" "$OUT/MikMap-Setup.pkg"

# --- MikMap.dmg: the app next to a shortcut to /Applications, for drag-and-drop install -------------------------
DMG="$STAGE/dmg"; mkdir -p "$DMG"
cp -R "$APP" "$DMG/MikMap.app"
ln -s /Applications "$DMG/Applications"
rm -f "$OUT/MikMap.dmg"
hdiutil create -volname "MikMap" -srcfolder "$DMG" -ov -format UDZO "$OUT/MikMap.dmg"

rm -rf "$STAGE"
ls -la "$OUT"/MikMap-Setup.pkg "$OUT"/MikMap.dmg
