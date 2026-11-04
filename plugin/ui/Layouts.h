// CheatChord desktop layouts: which panels a window shows, where, at what size. Built in, or loaded from .json files in
// Documents/JJ DA PLUG/CheatChord/Layouts (the built-in ones are written to Layouts/Examples as a starting point).
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
};

std::vector<LayoutDef> builtInLayouts();
juce::File layoutsFolder();
std::vector<LayoutDef> userLayouts (juce::StringArray& errors);
void writeExampleLayouts();   // once: the built-in ones as files to copy and edit

// several panels in one place: tabs on top
class TabGroup : public Panel
{
public:
    TabGroup (Ctx&, const juce::StringArray& ids, juce::ValueTree state, const juce::String& stateKey);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void show (int i);
private:
    juce::Rectangle<float> tab (int i) const;
    Ctx& ctx;
    juce::StringArray ids, names;
    std::vector<std::unique_ptr<Panel>> panels;   // made when first shown
    juce::ValueTree state;
    juce::String stateKey;
    int cur = 0, hover = -1;
};

// the panels of one layout, on its grid
class LayoutView : public juce::Component
{
public:
    LayoutView (Ctx&, const LayoutDef&, juce::ValueTree uiState);
    void resized() override;
private:
    LayoutDef def;
    std::vector<std::unique_ptr<juce::Component>> parts;
};

} // namespace cc::ui
