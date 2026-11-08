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
| H | Helper ein/aus |
| Esc | EDIT beenden |

## Klang: 16 Instrumente

Der eingebaute Synth ist neu: Electric Piano, Warm Pad, Organ, Synth Keys, Strings, Bell Keys, **Grand Piano**,
**Wurli**, **Lo-Fi Keys**, **Nylon Guitar**, **Harp**, **Vibraphone**, **Clav**, **Choir**, **Brass**, **Glass Pad**.

- **Ton wechseln ist jetzt sicher:** Eine klingende Note behält ihren Klang bis zum Ende. Neue Noten nehmen den neuen
  Klang. Am Ausgang sitzt ein Limiter, so kann nichts mehr laut knallen.
- Die Regler: **TONE** (heller/dunkler), **RELEASE** (wie lange es nachklingt), **SPACE** (Hall).

## Layouts

Oben **LAYOUT** öffnet das Menü:

| Layout | Wofür |
|---|---|
| **Studio** | Alles zum Schreiben gleichzeitig: Progression, Ideen, Browser, Tastatur, Rest in Tabs |
| **Wide** | Grosse und Ultrawide-Bildschirme: fast jedes Panel offen |
| **Compact** | Laptops und kleine Fenster: Progression oben, alles andere in Tabs (Standard auf kleinen Bildschirmen) |
| **Perform** | Live spielen: riesige Akkorde und Ideen, grosser Transport |
| **Discover** | Styles, Artists und Producer erkunden |

**In jedem Layout ist alles erreichbar.** Was ein Layout nicht offen zeigt, steht als Tab da. Jedes Panel hat seine eigene
Farbe, im Tab und im Titel.

**Selbst anordnen: EDIT** (oben):

- Ein Panel ziehen verschiebt es, die Ecke unten rechts ändert die Grösse.
- Ein Panel auf ein anderes ziehen macht sie zu Tabs.
- **Rechtsklick** (oder **MENU**) auf ein Panel oder einen Tab:
  - „Give it its own place“ trennt einen Tab heraus.
  - „Move it as a tab to …“ hängt einen Tab woanders hin.
  - „Swap places with …“ tauscht zwei Plätze.
  - „Open as its own window“ öffnet das Panel als Fenster.
- **DONE** beendet. **RESET LAYOUT** stellt das Original wieder her. **SAVE AS FILE** speichert es als eigenes Layout im
  LAYOUT-Menü.
- Dein angepasstes Layout wird mit dem Projekt gespeichert.

**Eigene Fenster:**

- Der kleine Pfeil-Knopf oben rechts in jedem Panel (oder Doppelklick auf einen Tab) öffnet das Panel als eigenes
  kleines Fenster.
- Verschieben am Titel, Grösse an der Ecke, **BACK INTO LAYOUT** schliesst es wieder.
- So siehst du zum Beispiel SOUND und CHORD gleichzeitig, auch wenn sie im selben Tab-Bereich liegen.

**Grösse:**

- **Zoom** (50–200 %) vergrössert alles.
- **Text size** (100–180 %, im Zoom- und im LAYOUT-Menü) vergrössert die Schrift und die Zeilen.
- Das Fenster lässt sich an der Ecke ziehen, die Panels ordnen sich neu.
- **Farben:** Velvet, Graphite oder Daylight (LAYOUT → Colour theme).
- „Use this layout … for new instances“ merkt sich alles für neue Instanzen.

## Eigener MIDI-Port: die Akkorde als MIDI-Eingang einer Spur

CheatChord kann seine Akkorde als **eigenes MIDI-Gerät** ausgeben. Dann wählst du in jeder DAW bei einer anderen Spur
**MIDI-Eingang: CheatChord**, und diese Spur spielt und nimmt jede Note der Akkorde auf. Das geht auch in der MPC und
in Ableton, die den MIDI-Ausgang eines Plugins sonst nicht annehmen.

1. **Nur Windows, einmalig:** das kostenlose **loopMIDI** von Tobias Erichsen installieren (tobias-erichsen.de). Es
   bringt den Treiber mit, über den CheatChord seinen Port anlegt. Einen Port in loopMIDI selbst brauchst du nicht.
   Auf dem Mac geht es ohne Installation.
2. In CheatChord: **SOUND → MIDI PORT… → CheatChord (own port)**. Die Zeile daneben zeigt „Open“. Statt des eigenen
   Ports kannst du auch einen vorhandenen MIDI-Ausgang wählen, etwa einen loopMIDI-Port oder dein Interface.
3. In der DAW: eine neue MIDI- oder Instrument-Spur, Programm oder Instrument laden, **MIDI-Eingang = CheatChord**,
   Monitor/Aufnahme scharf.
   - **MPC:** Bei der Ziel-Spur unter MIDI-Eingang „CheatChord“ wählen, unter Preferences → MIDI den Port aktivieren.
   - **Ableton:** Einstellungen → Link/Tempo/MIDI: beim Eingang „CheatChord“ **Spur** einschalten. Dann bei der Spur
     „MIDI From: CheatChord“ und Monitor „In“.
4. **Wichtig:** Die CheatChord-Spur selbst bekommt als MIDI-Eingang nur dein Keyboard oder deine Pads, nicht „Alle
   Eingänge“. Sonst kommen die Akkorde wieder zurück. CheatChord filtert solche Echos zwar heraus, aber so bleibt es
   sauber.

Willst du nur den Klang des fremden Instruments, stell in CheatChord **SYNTH WITH MIDI** aus.
Mehrere CheatChord-Instanzen bekommen je einen eigenen Port („CheatChord 2“ usw.).

## Helper

Der **Helper** ist ein kleines Fenster, das erklärt, worauf die Maus gerade zeigt: Knöpfe, Regler, Listen, Tabs, Panels.

- Am Titel lässt er sich verschieben.
- **HELPER ON/OFF** oben (oder die Taste **H**, oder das x) schaltet ihn aus und ein.
- Ist er aus, zeigen sich die Erklärungen wieder als kurze Tooltips.

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
