// CheatChord desktop: renders the editor in every built-in layout (and theme) to PNG files, with a progression loaded.
//   cc_ui_shots <output folder>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"
#include "../plugin/PluginEditor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static void pump (cc::Processor& p, double seconds)
{
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer m;
    const int n = (int) (seconds * 48000 / 256);
    for (int i = 0; i < n; ++i) { buf.clear(); m.clear(); p.processBlock (buf, m); }
}

static void shot (cc::Editor& ed, const juce::File& f)
{
    ed.refreshNow();
    juce::Thread::sleep (30);
    ed.refreshNow();
    auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("%s  %dx%d\n", f.getFileName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    out.createDirectory();
    std::unique_ptr<cc::Processor> p (dynamic_cast<cc::Processor*> (createPluginFilter()));
    p->setPlayConfigDetails (0, 2, 48000, 256);
    p->prepareToPlay (48000, 256);
    pump (*p, 1.0);
    // a progression to look at: the "Velvet Neo Soul" factory preset, then chord 3 selected and an idea lit
    p->engine().trigger ("pre_1");
    pump (*p, 0.8);
    p->engine().trigger ("prog_3");
    pump (*p, 0.6);
    p->engine().trigger ("sug_2");
    pump (*p, 0.6);

    std::unique_ptr<cc::Editor> ed (dynamic_cast<cc::Editor*> (p->createEditor()));
    const char* layouts[] = { "Studio", "Wide", "Compact", "Perform", "Discover" };
    for (auto* l : layouts)
    {
        ed->setZoom (1.0f);
        ed->selectLayout (l, false);
        for (auto& d : cc::ui::builtInLayouts())
            if (d.name == l) ed->setSize (d.width, d.height);
        pump (*p, 0.2);
        shot (*ed, out.getChildFile (juce::String ("layout_") + l + ".png"));
    }
    // the other themes, in Studio
    ed->selectLayout ("Studio", false);
    ed->setSize (1560, 980);
    for (int t = 1; t < 3; ++t)
    {
        cc::ui::setTheme (t);
        p->uiState().setProperty ("theme", t, nullptr);
        ed = nullptr;
        ed.reset (dynamic_cast<cc::Editor*> (p->createEditor()));
        ed->setZoom (1.0f);
        ed->selectLayout ("Studio", false);
        ed->setSize (1560, 980);
        shot (*ed, out.getChildFile ("theme_" + cc::ui::themeNames()[t] + ".png"));
    }
    cc::ui::setTheme (0);
    p->uiState().setProperty ("theme", 0, nullptr);
    ed = nullptr;
    ed.reset (dynamic_cast<cc::Editor*> (p->createEditor()));
    // the helper explaining something, a panel in its own window, then EDIT LAYOUT
    ed->selectLayout ("Studio", false);
    ed->setZoom (1.0f);
    ed->setSize (1600, 1000);
    ed->setHelper (true);
    ed->showHelp ("Cruise", "Adds the next chord that fits after the selected one (the suggestion shows on the button).");
    ed->popOut ("sound");
    shot (*ed, out.getChildFile ("studio_helper_window.png"));
    ed->setEditing (true);
    shot (*ed, out.getChildFile ("studio_edit_layout.png"));
    ed->setEditing (false);
    ed->setTextSize (1.5f);
    shot (*ed, out.getChildFile ("studio_text150.png"));
    ed->setTextSize (1.25f);
    shot (*ed, out.getChildFile ("sound_panel.png"));
    ed->setZoom (0.8f);
    ed->setSize (1280, 800);
    shot (*ed, out.getChildFile ("studio_80pct.png"));
    ed = nullptr;
    p->releaseResources();
    return 0;
}
