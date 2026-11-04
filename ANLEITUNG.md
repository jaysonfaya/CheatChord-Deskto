# CheatChord 4.1 Desktop – Anleitung

CheatChord ist der Akkord- und Progressions-Generator von **JJ DA PLUG**, jetzt als Plugin für den Rechner:
**VST3** (Windows, Mac, Linux), **AU** (Mac), **VST2** (für MPC 2 Desktop) und als eigenständiges Programm.
Die Harmonie-Engine ist dieselbe wie auf der MPC (gleiche Styles, Artists, Producer, Presets). Neu ist die
Oberfläche: gemacht für Maus und grosse Bildschirme, mit ladbaren Layouts.

Installation: **INSTALL.txt**.

## Welches Format für welches Programm

| Programm | Format | Klang | Akkorde in andere Spuren |
|---|---|---|---|
| MPC 2.x Desktop (Windows) | **VST2** (`CheatChord.dll`) | eingebauter Synth | **DRAG MIDI** in die Sequenz |
| MPC 3 Desktop | VST3 | eingebauter Synth | DRAG MIDI |
| Ableton Live | VST3 | eingebauter Synth | **DRAG MIDI** in einen Clip |
| Reaper, Bitwig, Cubase, FL Studio, Studio One | VST3 | Synth | DRAG MIDI oder **MIDI OUT** an jedes Instrument |
| Logic, GarageBand | AU | Synth | DRAG MIDI |

Der eingebaute Synth klingt überall. MIDI OUT des Plugins nehmen nicht alle DAWs an. DRAG MIDI geht überall.

## Erste Schritte

1. **KEY** und **SCALE** wählen, dann **STYLE**, dazu ein **ARTIST** und ein **PRODUCER**. In den Listen kannst du
   tippen, um zu suchen (z. B. „dil“ für J Dilla).
2. **IDEAS:** Ein Klick spielt eine Idee an, ein zweiter Klick nimmt sie in die Progression.
   **REROLL** gibt acht neue Ideen.
3. **CRUISE** setzt den nächsten passenden Akkord (der Vorschlag steht auf dem Knopf). **GENERATE** schreibt eine
   ganze Progression. **UNDO** und **REDO** gehen jederzeit, und **HISTORY** behält jede Änderung.
4. **Akkord ändern:** Einen Akkord der Progression anklicken; die Ideen ersetzen dann genau diesen Akkord.
   - **Rechtsklick** auf einen Akkord: halbe oder doppelte Länge, Reverse, löschen.
   - Im Panel **CHORD** stellst du Grundton, Typ, Slash-Bass, Voicing und Oktave ein.
5. **Abspielen:**
   - **HOST**: CheatChord spielt mit dem Transport der DAW. **LOOP SYNC AUTO** füllt dabei den Loop der DAW.
   - **LISTEN** + **PLAY**: CheatChord spielt allein im Tempo der DAW.
6. **In die DAW bringen:**
   - **DRAG MIDI** mit gedrückter Maus auf eine Spur ziehen. Du bekommst einen MIDI-Clip mit Rhythmus, Strum und Voicing.
   - Oder **EXPORT .MID** (Rechtsklick öffnet den Ordner).
7. **MIDI-Keyboard / Pads:**
   - Die Noten 36–43 spielen die Progression, 44–51 die Ideen.
   - **LEARN PADS** (Panel PADS) lernt jedes andere Layout: erstes Pad, letztes Pad.

**Hilfe:** Fährst du mit der Maus über ein Element, steht unten eine Erklärung. Der Knopf **?** oben zeigt die Kurzanleitung.

**Tasten** (wenn das Plugin-Fenster den Fokus hat):

| Taste | Wirkung |
|---|---|
| G | Generate |
| C | Cruise |
| R | Reroll |
| A | Add |
| 1–8 | Akkord wählen |
| Entf | Akkord löschen |
| Strg+Z | Undo |
| Strg+Y | Redo |

## Layouts

Oben **LAYOUT** öffnet das Menü:

| Layout | Wofür |
|---|---|
| **Studio** (Standard) | Alles zum Schreiben gleichzeitig: Progression, Ideen, Browser, Tastatur, Rest in Tabs |
| **Wide** | Grosse und Ultrawide-Bildschirme (ab ca. 2400 px): jedes Panel offen |
| **Compact** | Laptops und kleine Fenster: Progression oben, alles andere in Tabs |
| **Perform** | Live spielen: riesige Akkorde und Ideen, grosser Transport |
| **Discover** | Styles, Artists und Producer erkunden: grosse Listen und das Profil des Artists oder Producers |

Weitere Einstellungen:

- **Zoom** (50–200 %) vergrössert alles. Das Fenster lässt sich zusätzlich an der Ecke ziehen, dann ordnen sich
  die Panels neu an.
- **Farben:** Velvet (rot und gold), Graphite oder Daylight (hell).
- **„Use this layout … for new instances“** merkt sich Layout, Zoom und Farben für neue Instanzen.
- Layout, Zoom, Tabs und Fenstergrösse werden pro Projekt gespeichert.

**Eigene Layouts:**

1. **„Save this layout as a file to edit…“** schreibt das aktuelle Layout als `.json` nach
   `Dokumente/JJ DA PLUG/CheatChord/Layouts`.
2. Die Datei mit einem Texteditor ändern. Panels werden auf ein Raster gesetzt; mehrere Panels an einem Platz
   werden zu Tabs. Die `README.txt` im Ordner erklärt das Format und listet alle Panel-Namen.
3. **„Reload layout files“** lädt die Änderung.

Jedes Layout aus dem Menü wird so ladbar.

## Automation

41 Einstellungen sind für die DAW automatisierbar, darunter Key, Scale, Style, Artist, Producer, Spice, Rhythmus,
Swing, Strum, Sound und Play Mode.

Dazu kommen 10 Aktionen: **Generate, Cruise, Reroll, Add, Undo, Redo, Delete, Clear, Stop und Panic**. Sie lösen
aus, wenn der Wert von 0 auf 1 springt. So lassen sie sich auf Q-Links, Pads oder Controller-Knöpfe legen.

Die 24 Werks-Presets und deine 16 eigenen stehen auch in der Preset-Liste der DAW.

## Grenzen

- **MIDI OUT:**
  - Ableton und MPC 2 nehmen den MIDI-Ausgang eines Plugins nicht an. Dort nimmst du den eingebauten Synth oder DRAG MIDI.
  - Das Reverse-Anschwellen (CC 11) geht nur über MIDI OUT; im MIDI-Export ist es nicht enthalten.
- **macOS:** Die Plugins sind nicht von Apple signiert (siehe INSTALL.txt).
- **VST2:** Die VST2-Version ist auf einer eigenen, frei geschriebenen Beschreibung der VST2-Schnittstelle gebaut.
  Steinberg vergibt keine VST2-Lizenzen mehr. Für deinen eigenen Gebrauch ist das kein Problem. Für einen Verkauf
  nimm VST3, AU und das eigenständige Programm.
