# CheatChord 4.1 Desktop (JJ DA PLUG)

CheatChord, der Akkord-Generator aus der MPC-Version, als Desktop-Plugin:

| System | Formate |
|---|---|
| Windows | VST3, VST2 (für MPC 2), Standalone |
| macOS | VST3, AU, VST2, Standalone (Universal: Apple Silicon und Intel) |
| Linux | VST3, Standalone |

Bedienung: **ANLEITUNG.md**. Installation: **INSTALL.txt**.

## Aufbau

| Ordner | Inhalt |
|---|---|
| `engine/` | Die C-Engine der MPC-Version 4.1.0 (Harmonie, Voicing, Sequencer, Synth). Desktop-Änderungen: Samplerate des Hosts, MIDI an den Host statt an einen ALSA-Port, kein Echo-Schutz, Ordner unter Dokumente, `compat.c/h` für Windows (Threads, Atomics, UTF-8-Pfade), `cc_desktop.h` (zusätzliche Funktionen für die Oberfläche). |
| `plugin/` | JUCE-Hülle: `PluginProcessor` (Transport, Loop, MIDI ein/aus sample-genau in 128er-Blöcken, 51 Host-Parameter, Programme, State), `PluginEditor` (Zoom, Layouts, Tastenkürzel), `ui/` (Theme, Widgets, Panels, Layouts). |
| `vst2abi/` | Eine frei geschriebene Beschreibung der VST2-Schnittstelle, damit JUCE ohne Steinberg-SDK ein VST2 baut (`-DCC_VST2=ON`). |
| `tests/` | `host_test.cpp` lädt das VST3 wie eine DAW. `vst2_test.c` lädt das VST2 mit eigenen, unabhängigen Strukturen. `ui_shots.cpp` rendert jedes Layout als PNG. |
| `tests_engine/` | Der Engine-Test der MPC-Version (116 Prüfungen, ASan/UBSan). |
| `tools/` | `make_meta.py` erzeugt `plugin/ParamsMeta.h` aus `tools/params.json`. |

## Bauen

**GitHub (Windows, macOS, Linux):**

1. Alles in ein Repository hochladen, auch den Ordner `.github`.
2. Der Workflow „Build CheatChord“ läuft dann von selbst (oder per *Run workflow*).
3. Unter **Actions → der Lauf → Artifacts** liegen die fertigen ZIPs.

Der Workflow testet auch:

- den Engine-Test;
- den Host-Test bei 44,1, 48 und 96 kHz;
- den VST2-Test;
- `pluginval` (Strictness 5);
- `auval` (nur macOS).

**Lokal:**

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release [-DCC_VST2=ON] [-DCC_BUILD_HOST_TEST=ON]
cmake --build build --config Release
```

JUCE 8.0.10 wird automatisch geladen. Mit `-DCC_JUCE_DIR=<pfad>` nimmt der Build eine vorhandene Kopie.

## Lizenzen

- CheatChord: MIT.
- JUCE: AGPLv3 oder kommerzielle Lizenz. Für den eigenen Gebrauch reicht das. Wer verkaufen will, prüft die
  JUCE-Lizenz.
- Titillium Web: SIL OFL (`assets/fonts/OFL.txt`).
- VST ist eine Marke der Steinberg Media Technologies GmbH.
