// CheatChord desktop screen: the live model of the engine and the widgets every panel is built from.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <map>
#include <set>
#include <string>
#include "Theme.h"
#include "../ParamsMeta.h"

namespace cc { class Processor; }

namespace cc::ui {

// What the screen shows, read from the engine 30 times a second in one go (one lock).
class Model
{
public:
    void want (const std::string& key) { keys.insert (key); }
    bool refresh (cc::Processor& p);   // true when anything changed
    juce::String text (const std::string& key) const;
    int val (const std::string& key) const;

    // straight from the engine (cc_desktop.h)
    int count = 0, loopLen = 4, playSlot = -1, selIdea = -1, cursor = 0, target = 0, kbLo = 36, busy = 0;
    int ideaClass[8] {}, slotClass[8] {};
    juce::String slotRoman[8];
    int shownN = -1, shownBass = -1, shown[16] {};
    double bpm = 120;
    bool hostPlaying = false;
    juce::uint32 revision = 0;

private:
    std::set<std::string> keys;
    std::map<std::string, std::string> v;
};

class Live;

// The context every widget gets: the processor, the model and the hint line.
struct Ctx
{
    explicit Ctx (cc::Processor& p) : proc (p) {}
    cc::Processor& proc;
    Model model;
    std::vector<Live*> lives;
    // the helper: what the mouse points at (title + explanation); empty title and text = nothing
    std::function<void (const juce::String& title, const juce::String& text)> hint;
    std::function<void()> changed;                    // an action happened: refresh now
    std::function<void (const juce::String& panelId)> popOut;   // open a panel as its own window
    void help (const juce::String& title, const juce::String& text) { if (hint) hint (title, text); }

    int val (const char* k) { return model.val (k); }
    juce::String text (const char* k) { return model.text (k); }
    void setValue (const char* key, int v);
    void tap (const char* key);
    void gesture (const char* key, bool begin);
    void updateAll();
};

// A widget that follows the model.
class Live
{
public:
    explicit Live (Ctx& c) : ctx (c) { ctx.lives.push_back (this); }
    virtual ~Live() { ctx.lives.erase (std::remove (ctx.lives.begin(), ctx.lives.end(), this), ctx.lives.end()); }
    virtual void update() = 0;
protected:
    Ctx& ctx;
};

// ---------------------------------------------------------------- Tile: a big clickable chord / idea / action
class Tile : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Style { Normal, Action, Primary, Chord, Ghost };
    Tile (Style s = Normal) : style (s) { setRepaintsOnMouseActivity (true); }

    bool set (const juce::String& t, const juce::String& sub = {}, bool on = false);
    void setBadge (const juce::String& b) { if (b != badge) { badge = b; repaint(); } }
    void setAccent (juce::Colour c) { if (c != accent) { accent = c; repaint(); } }
    void setHot (bool h) { if (h != hot) { hot = h; repaint(); } }
    void setDim (bool d) { if (d != dim) { dim = d; repaint(); } }
    void setEmpty (bool e) { if (e != empty) { empty = e; repaint(); } }
    void setOn (bool o) { if (o != on) { on = o; repaint(); } }
    void setTextScale (float s) { textScale = s; repaint(); }
    void setHelp (const juce::String& h) { help = h; setTooltip (h); }
    void setHelp (const juce::String& t, const juce::String& h) { helpTitle = t; setHelp (h); }

    std::function<void (const juce::MouseEvent&)> onClick, onDouble, onDragStart;
    std::function<void (const juce::String&, const juce::String&)> onHint;   // title, text

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { if (onHint && help.isNotEmpty()) onHint (helpTitle.isNotEmpty() ? helpTitle : text, help); }
    juce::String helpTitle;

    juce::String text, sub, badge, help;
    bool on = false, hot = false, dim = false, empty = false;
    juce::Colour accent { juce::Colours::transparentBlack };
    float textScale = 1.0f;
    Style style;
private:
    bool dragged = false;
};

// ---------------------------------------------------------------- Bar: one choice among a few, all visible
class Bar : public juce::Component, public Live, public juce::SettableTooltipClient
{
public:
    Bar (Ctx&, const char* key, juce::StringArray labels = {}, int columns = 0, int valueOffset = 0);
    void update() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void setHelp (const juce::String& h) { help = h; setTooltip (h); }
    void setHelp (const juce::String& t, const juce::String& h) { helpTitle = t; setHelp (h); }
    juce::String helpTitle;
    juce::Rectangle<float> cell (int i) const;
    int rowsNeeded() const { return cols > 0 ? (labels.size() + cols - 1) / cols : 1; }

    std::function<void (int)> onPick;   // default: set the engine value
    std::vector<int> values;            // optional: the engine value of each cell (else index + offset)
    juce::StringArray labels;
    juce::String help;
    float textH = 17.0f;
private:
    const char* key;
    int cols, offset, sel = -1, hover = -1;
};

// ---------------------------------------------------------------- Knob
class Knob : public juce::Component, public Live, public juce::SettableTooltipClient
{
public:
    Knob (Ctx&, const char* key, const juce::String& label, std::function<juce::String (int)> fmt = {});
    void update() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void setHelp (const juce::String& h) { help = h; setTooltip (h); }
private:
    void send (int v);
    const char* key;
    juce::String label, help;
    std::function<juce::String (int)> fmt;
    int mn = 0, mx = 100, def = 0, v = 0;
    float dragStartV = 0;
};

// ---------------------------------------------------------------- Picker: a value from a long list (< value >, click opens it)
class Picker : public juce::Component, public Live, public juce::SettableTooltipClient
{
public:
    Picker (Ctx&, const char* key, const juce::String& label, bool searchable = false);
    void update() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void setHelp (const juce::String& h) { help = h; setTooltip (h); }
    juce::StringArray options;
    int offset = 0;   // the engine value of options[0] (1 for 1..16)
private:
    void step (int d);
    void open();
    const char* key;
    juce::String label, help;
    bool searchable;
    int v = 0;
    Tile prev { Tile::Action }, next { Tile::Action };
    juce::Rectangle<int> field;
};

// ---------------------------------------------------------------- SearchList: a long list with a search box, always open
class SearchList : public juce::Component, public Live, private juce::ListBoxModel, private juce::TextEditor::Listener
{
public:
    SearchList (Ctx&, const char* key, const juce::String& title, std::function<juce::String (int)> detail = {});
    void update() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onPicked;   // a CallOutBox closes itself
    void focusSearch() { search.grabKeyboardFocus(); }
    juce::String help;
    void mouseEnter (const juce::MouseEvent&) override { ctx.help (title, help); }
private:
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override { pick (row); }
    void returnKeyPressed (int row) override { pick (row); }
    void textEditorTextChanged (juce::TextEditor&) override { filter(); }
    void textEditorReturnKeyPressed (juce::TextEditor&) override { if (! rows.empty()) pick (0); }
    void pick (int row);
    void filter();
    void showCurrent();
    const char* key;
    juce::String title;
    juce::StringArray options;
    std::vector<int> rows;
    std::function<juce::String (int)> detail;
    int v = -1;
    juce::TextEditor search;
    juce::ListBox list;
};

// ---------------------------------------------------------------- Line: one readout of the engine
class Line : public juce::Component, public Live
{
public:
    Line (Ctx&, const char* key, float h = 15.0f, juce::Justification j = juce::Justification::centred, int lines = 1);
    Line (Ctx&, std::function<juce::String()> src, float h = 15.0f, juce::Justification j = juce::Justification::centred, int lines = 1);
    void update() override;
    void paint (juce::Graphics&) override;
    juce::Colour colour = pal().text;
    bool boxed = true;
    int weight = 1;
    juce::String helpTitle, help;
    void mouseEnter (const juce::MouseEvent&) override { if (help.isNotEmpty()) ctx.help (helpTitle, help); }
private:
    const char* key = nullptr;
    std::function<juce::String()> src;
    juce::String t;
    float h;
    juce::Justification just;
    int lines;
};

// ---------------------------------------------------------------- Keyboard: the chord's notes as they sound
class Keyboard : public juce::Component, public Live
{
public:
    Keyboard (Ctx&, bool big);
    void update() override;
    void paint (juce::Graphics&) override;
private:
    bool big;
    std::vector<int> state;
    int lo = 36;
};

// ---------------------------------------------------------------- Panel: a titled section
class Panel : public juce::Component
{
public:
    explicit Panel (const juce::String& t) : title (t) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    juce::Rectangle<int> content() const;   // inside the frame, below the title
    juce::Rectangle<int> titleBar() const;
    juce::Rectangle<int> popButton() const;   // the small "open as window" button in the title bar
    juce::String title, id, about;
    juce::Colour accent { 0xffe2b866 };
    bool frame = true;
    std::function<void()> onPopOut;
    std::function<void (const juce::String&, const juce::String&)> onHint;
};

// small layout helpers
juce::Rectangle<int> take (juce::Rectangle<int>& r, int h, int gap = 6);          // from the top
juce::Rectangle<int> takeLeft (juce::Rectangle<int>& r, int w, int gap = 6);
void grid (std::vector<juce::Component*> items, juce::Rectangle<int> r, int cols, int gap = 6);
void label (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, juce::Justification j = juce::Justification::centredLeft);

} // namespace cc::ui
