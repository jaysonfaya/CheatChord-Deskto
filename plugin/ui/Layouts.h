// CheatChord desktop layouts: which panels a window shows, where, at what size.
// Built in, loaded from .json files in Documents/JJ DA PLUG/CheatChord/Layouts, or arranged by hand in EDIT LAYOUT
// (move, resize, put panels together as tabs, split them apart, open any panel as its own window).
// Every layout always reaches every panel: what a layout leaves out is added as tabs.
#pragma once
#include "Panels.h"

namespace cc::ui {

struct LayoutDef
{
    struct Item { juce::StringArray ids; int x = 0, y = 0, w = 1, h = 1; };
    juce::String name, about;
    int width = 1500, height = 940, minWidth = 900, minHeight = 600, cols = 60, rows = 40;
    std::vector<Item> items;
    bool builtIn = true;
    juce::File file;

    static bool fromJson (const juce::var&, LayoutDef& out, juce::String& error);
    juce::var toJson() const;
    void complete();   // every panel somewhere: the missing ones become tabs
};

std::vector<LayoutDef> builtInLayouts();
juce::File layoutsFolder();
std::vector<LayoutDef> userLayouts (juce::StringArray& errors);
void writeExampleLayouts();

// what the layout view asks of the editor
struct LayoutHost
{
    virtual ~LayoutHost() = default;
    virtual bool isFloating (const juce::String& id) = 0;
    virtual void popOut (const juce::String& id) = 0;
    virtual void dock (const juce::String& id) = 0;
    virtual bool editing() = 0;
    virtual void itemMenu (int item, const juce::String& tabId, juce::Component* target) = 0;
    virtual void moveItem (int item, juce::Rectangle<int> cells) = 0;   // grid cells
    virtual void mergeItem (int from, int into) = 0;
};

// several panels in one place: coloured tabs on top
class TabGroup : public Panel
{
public:
    TabGroup (Ctx&, LayoutHost&, int item, const juce::StringArray& ids, juce::ValueTree state, const juce::String& stateKey);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void show (int i);
    void refreshFloating();
private:
    juce::Rectangle<float> tab (int i) const;
    juce::Rectangle<float> popBtn() const;
    int tabAt (juce::Point<float>) const;
    Ctx& ctx;
    LayoutHost& host;
    int item;
    juce::StringArray ids, names;
    std::vector<std::unique_ptr<juce::Component>> panels;   // made when first shown
    juce::ValueTree state;
    juce::String stateKey;
    int cur = 0, hover = -1;
};

// the panels of one layout, on its grid; in EDIT LAYOUT each one gets a frame to move and resize it
class LayoutView : public juce::Component
{
public:
    LayoutView (Ctx&, LayoutHost&, const LayoutDef&, juce::ValueTree uiState);
    ~LayoutView() override;
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    juce::Rectangle<int> cellRect (const LayoutDef::Item&) const;
    juce::Rectangle<int> cellsAt (juce::Rectangle<int> px) const;   // pixels -> grid cells
    int itemAt (juce::Point<int>) const;
    void setEditing (bool);
    void refreshFloating();
    const LayoutDef& layout() const { return def; }
private:
    class EditFrame;
    Ctx& ctx;
    LayoutHost& host;
    LayoutDef def;
    std::vector<std::unique_ptr<juce::Component>> parts;
    std::vector<std::unique_ptr<EditFrame>> frames;
};

// a panel in its own small window (inside the plugin window): move it, resize it, dock it back
class FloatPanel : public juce::Component
{
public:
    FloatPanel (Ctx&, const juce::String& id, std::function<void()> onDock, std::function<void()> onMoved);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    juce::String id;
private:
    juce::Rectangle<int> titleBar() const { return getLocalBounds().removeFromTop (U (32)); }
    juce::Rectangle<int> dockBtn() const;
    juce::Rectangle<int> grip() const { return getLocalBounds().removeFromBottom (18).removeFromRight (18); }
    Ctx& ctx;
    std::unique_ptr<Panel> panel;
    std::function<void()> onDock, onMoved;
    juce::ComponentDragger dragger;
    juce::Rectangle<int> startBounds;
    int mode = 0;   // 1 move, 2 resize
};

// the helper: a small box that explains what the mouse points at
class HelperBox : public juce::Component
{
public:
    HelperBox (std::function<void()> onClose, std::function<void()> onMoved);
    void set (const juce::String& title, const juce::String& text);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int preferredHeight (int width) const;
private:
    juce::Rectangle<int> closeBtn() const;
    juce::String title, text;
    std::function<void()> onClose, onMoved;
    juce::ComponentDragger dragger;
};

} // namespace cc::ui
