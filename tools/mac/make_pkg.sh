#!/bin/bash
# Builds "CheatChord Installer.pkg" from the built, ad-hoc-signed bundles in $1 (a folder with CheatChord.vst3,
# .component, .vst and .app). Output: $2/CheatChord Installer.pkg
set -euo pipefail
SRC="$1"; OUT="$2"; HERE="$(cd "$(dirname "$0")" && pwd)"
W="$(mktemp -d)"
R="$W/root"
mkdir -p "$R/Library/Audio/Plug-Ins/VST3" "$R/Library/Audio/Plug-Ins/Components" "$R/Library/Audio/Plug-Ins/VST" "$R/Applications"
ditto "$SRC/CheatChord.vst3" "$R/Library/Audio/Plug-Ins/VST3/CheatChord.vst3"
ditto "$SRC/CheatChord.component" "$R/Library/Audio/Plug-Ins/Components/CheatChord.component"
ditto "$SRC/CheatChord.vst" "$R/Library/Audio/Plug-Ins/VST/CheatChord.vst"
ditto "$SRC/CheatChord.app" "$R/Applications/CheatChord.app"
# the bundles always go exactly where they belong (not "relocated" to an older copy somewhere else)
pkgbuild --analyze --root "$R" "$W/comp.plist"
i=0
while /usr/libexec/PlistBuddy -c "Print :$i" "$W/comp.plist" >/dev/null 2>&1; do
  /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$W/comp.plist" 2>/dev/null || true
  /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$W/comp.plist"
  /usr/libexec/PlistBuddy -c "Set :$i:BundleOverwriteAction upgrade" "$W/comp.plist" 2>/dev/null || true
  i=$((i+1))
done
echo "bundles in the package: $i"; /usr/libexec/PlistBuddy -c "Print" "$W/comp.plist" | grep -E "RootRelativeBundlePath|Relocatable"
pkgbuild --root "$R" --component-plist "$W/comp.plist" --scripts "$HERE/pkg/scripts" \
  --identifier com.jjdaplug.cheatchord --version 4.1.0 --install-location / "$W/CheatChord-core.pkg"
pkgutil --payload-files "$W/CheatChord-core.pkg" | grep -E "CheatChord\.(vst3|component|vst|app)$"
mkdir -p "$OUT"
productbuild --distribution "$HERE/pkg/distribution.xml" --resources "$HERE/pkg/resources" --package-path "$W" \
  "$OUT/CheatChord Installer.pkg"
rm -rf "$W"
