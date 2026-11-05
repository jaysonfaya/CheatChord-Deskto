// CheatChord desktop editor: header, the layout's panels, panel windows, the helper, the hint line.
// Zoom scales everything, TEXT SIZE scales the writing, resizing the window reflows the panels.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Layouts.h"

namespace cc {

class Editor : public juce::AudioProcessorEditor, private juce::Timer, private juce::AsyncUpdater, public ui::LayoutHost
{
public:
    explicit Editor (Processor&);
    ~Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // tests and screenshots
    void selectLayout (const juce::String& name, bool resizeWindow = true);
    void setZoom (float z);
    void setTextSize (float s);
    void setEditing (bool e);
    void setHelper (bool on);
    void showHelp (const juce::String& title, const juce::String& text);
    void popOut (const juce::String& id) override;
    void dock (const juce::String& id) override;
    juce::Component& contentComponent() { return content; }
    void refreshNow();

private:
    // LayoutHost
    bool isFloating (const juce::String& id) override;
    bool editing() override { return editMode; }
    void itemMenu (int item, const juce::String& tabId, juce::Component* target) override;
    void moveItem (int item, juce::Rectangle<int> cells) override;
    void mergeItem (int from, int into) override;

    void timerCallback() override;
    void handleAsyncUpdate() override { refreshNow(); }
    void build();
    void rebuildView();
    void layoutMenu();
    void zoomMenu();
    void loadLayouts();
    void saveSettings();
    void saveWork();
    void saveFloats();
    void layoutChildren();
    float fitZoom (const ui::LayoutDef&) const;
    const ui::LayoutDef* findLayout (const juce::String& name) const;
    juce::Rectangle<int> screenArea() const;
    int headerH() const { return ui::U (68) + 16; }
    int footerH() const { return ui::U (30); }

    class EditBar;
    Processor& proc;
    ui::LookAndFeel lnf;
    ui::Ctx ctx;
    juce::Component content;
    std::unique_ptr<ui::Header> header;
    std::unique_ptr<ui::LayoutView> view;
    std::unique_ptr<ui::Footer> footer;
    std::unique_ptr<EditBar> editBar;
    std::unique_ptr<ui::HelperBox> helper;
    std::vector<std::unique_ptr<ui::FloatPanel>> floats;
    std::unique_ptr<juce::TooltipWindow> tips;
    std::vector<ui::LayoutDef> layouts;
    juce::StringArray layoutErrors;
    ui::LayoutDef work;          // the layout as shown (a copy of the chosen one, changed by EDIT LAYOUT)
    juce::String baseName;
    bool edited = false, editMode = false, helperOn = true;
    float zoom = 1.0f;
    bool building = false;
    std::unique_ptr<juce::PropertiesFile> settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace cc
