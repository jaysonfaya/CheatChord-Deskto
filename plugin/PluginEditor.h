// CheatChord desktop editor: header, the chosen layout's panels, the hint line. Zoom scales everything; resizing reflows.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Layouts.h"

namespace cc {

class Editor : public juce::AudioProcessorEditor, private juce::Timer, private juce::AsyncUpdater
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
    juce::Component& contentComponent() { return content; }
    void refreshNow();

private:
    void timerCallback() override;
    void handleAsyncUpdate() override { refreshNow(); }
    void build();
    void layoutMenu();
    void zoomMenu();
    void loadLayouts();
    void saveSettings();
    float fitZoom (const ui::LayoutDef&) const;
    const ui::LayoutDef* current() const;
    juce::Rectangle<int> screenArea() const;

    Processor& proc;
    ui::LookAndFeel lnf;
    ui::Ctx ctx;
    juce::Component content;
    std::unique_ptr<ui::Header> header;
    std::unique_ptr<ui::LayoutView> view;
    std::unique_ptr<ui::Footer> footer;
    std::unique_ptr<juce::TooltipWindow> tips;
    std::vector<ui::LayoutDef> layouts;
    juce::StringArray layoutErrors;
    juce::String layoutName;
    float zoom = 1.0f;
    bool building = false;
    std::unique_ptr<juce::PropertiesFile> settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Editor)
};

} // namespace cc
