#!/bin/bash
# CheatChord for macOS: copies the plugins into your user's plug-in folders and removes the download block
# (Gatekeeper's quarantine), so Ableton, Logic and other hosts can load them. Double-click; if macOS blocks this file
# too: System Settings > Privacy & Security > "Open Anyway".
cd "$(dirname "$0")" || exit 1
echo "CheatChord installieren ..."
xattr -dr com.apple.quarantine . 2>/dev/null
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST
copy() {   # $1 = bundle, $2 = target folder
  [ -e "$1" ] || return
  rm -rf "$2/$1"
  cp -R "$1" "$2/"
  xattr -dr com.apple.quarantine "$2/$1" 2>/dev/null
  codesign --force --deep -s - "$2/$1" >/dev/null 2>&1
  echo "  ok: $2/$1"
}
copy CheatChord.vst3 ~/Library/Audio/Plug-Ins/VST3
copy CheatChord.component ~/Library/Audio/Plug-Ins/Components
copy CheatChord.vst ~/Library/Audio/Plug-Ins/VST
copy CheatChord.app ~/Applications 2>/dev/null || true
killall -9 AudioComponentRegistrar >/dev/null 2>&1
echo
echo "Fertig. DAW neu starten und die Plugins neu scannen (Ableton: Einstellungen > Plug-Ins > mit Alt-Taste 'Neu scannen')."
read -n 1 -s -r -p "Taste druecken zum Schliessen ..."
