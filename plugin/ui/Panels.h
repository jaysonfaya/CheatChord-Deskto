// CheatChord desktop panels. Each one lays itself out for whatever box a layout gives it.
#pragma once
#include "Ui.h"

namespace cc::ui {

// every panel a layout can place, by id
struct PanelInfo { const char* id; const char* title; const char* about; };
const std::vector<PanelInfo>& panelList();
std::unique_ptr<Panel> makePanel (const juce::String& id, Ctx& ctx);

class ProgressionPanel : public Panel, public Live
{
public:
    explicit ProgressionPanel (Ctx&);
    void update() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void chordMenu (int i);
    Tile chords[8];
    Bar len, scope;
    Tile cruise { Tile::Primary }, generate { Tile::Primary }, undo { Tile::Action }, redo { Tile::Action },
         del { Tile::Action }, clear { Tile::Action }, drag { Tile::Ghost }, exportMid { Tile::Action };
    Line status;
    juce::String dragMsg;
};

class IdeasPanel : public Panel, public Live
{
public:
    explicit IdeasPanel (Ctx&);
    void update() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Tile ideas[8];
    Tile reroll { Tile::Action }, add { Tile::Primary };
    Line info;
    juce::Rectangle<int> legend;
};

class BrowserPanel : public Panel
{
public:
    explicit BrowserPanel (Ctx&);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Ctx& ctx;
    Bar key, scale;
    SearchList style, artist, producer;
    Picker styleP, artistP, producerP;
    Knob spice, smix, amix, pmix;
    Line styleInfo, artistInfo, producerInfo;
    juce::Rectangle<int> keyLab, scaleLab;
    bool lists = true;
};

class KeysPanel : public Panel
{
public:
    explicit KeysPanel (Ctx&);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Keyboard kb;
    Line name, notes, diff;
    juce::Rectangle<int> legend;
};

class ChordPanel : public Panel, public Live
{
public:
    explicit ChordPanel (Ctx&);
    void update() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Tile prevSlot { Tile::Action }, nextSlot { Tile::Action };
    Line slotName;
    Bar root, type, bass, var, slen, rev, voicing, octave, lead, bassNote;
    juce::Rectangle<int> labs[10];
};

class GroovePanel : public Panel
{
public:
    explicit GroovePanel (Ctx&);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Bar loopSync, direction, push, arpOct, strumDir, mute;
    Picker bars, rhythm, rrate, rate;
    Knob swing, strum, gate, velocity, humanize;
    Line loopInfo;
    juce::Rectangle<int> labs[8];
};

class SoundPanel : public Panel
{
public:
    explicit SoundPanel (Ctx&);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Ctx& ctx;
    Bar sound, midiOut, synth;
    Knob level, tone, release, space;
    Picker channel, bassCh;
    Tile panic { Tile::Action };
    Line port;
    juce::Rectangle<int> labs[4];
};

class PadsPanel : public Panel
{
public:
    explicit PadsPanel (Ctx&);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Picker padBase, padCount;
    Bar velMode;
    Tile learn { Tile::Primary };
    Line learnInfo, lastNote, about;
    juce::Rectangle<int> labs[2];
};

class HistoryPanel : public Panel, public Live
{
public:
    explicit HistoryPanel (Ctx&);
    void update() override;
    void resized() override;
private:
    Tile rows[8];
    Tile undo { Tile::Action }, redo { Tile::Action };
    Line pos;
};

class PresetsPanel : public Panel, public Live
{
public:
    explicit PresetsPanel (Ctx&);
    void update() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    Tile fac[24], usr[16];
    Picker slot;
    Tile save { Tile::Primary }, openDir { Tile::Ghost };
    Line slotName, info;
    juce::Rectangle<int> facLab, usrLab;
};

class LibraryPanel : public Panel
{
public:
    explicit LibraryPanel (Ctx&);
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    SearchList list;
    Line chords;
    Tile load { Tile::Primary };
};

class ProfilePanel : public Panel
{
public:
    explicit ProfilePanel (Ctx&);
    void resized() override;
private:
    Bar kind;
    std::unique_ptr<Line> lines[7];
};

class TransportPanel : public Panel, public Live
{
public:
    explicit TransportPanel (Ctx&);
    void update() override;
    void resized() override;
private:
    Bar mode;
    Tile play { Tile::Primary }, stop { Tile::Action }, cruise { Tile::Primary }, generate { Tile::Primary },
         undo { Tile::Action }, redo { Tile::Action }, reroll { Tile::Action };
    Line now, help;
};

class HelpPanel : public Panel
{
public:
    explicit HelpPanel (Ctx&);
    void paint (juce::Graphics&) override;
};

// the strip on top of every layout
class Header : public juce::Component, public Live
{
public:
    explicit Header (Ctx&);
    void update() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    Tile layoutBtn { Tile::Ghost }, zoomBtn { Tile::Ghost }, helpBtn { Tile::Ghost };
private:
    Line sel1, sel2, chord;
    Keyboard mini;
    Bar mode;
    Tile play { Tile::Primary }, stop { Tile::Action };
    juce::Rectangle<int> logo;
};

// the hint line at the bottom
class Footer : public juce::Component, public Live
{
public:
    explicit Footer (Ctx&);
    void update() override;
    void paint (juce::Graphics&) override;
    void setHint (const juce::String& h) { if (h != hint) { hint = h; repaint(); } }
private:
    juce::String hint, extra;
    int busy = 0;
};

} // namespace cc::ui
