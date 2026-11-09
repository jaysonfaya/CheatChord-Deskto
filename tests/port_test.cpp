// CheatChord desktop: the own MIDI port. Opens "CheatChord", listens to it like a DAW track would, plays pad 1 and a
// generated progression, and checks that whole chords arrive (not one note). Skips where the system has no MIDI
// (a build machine without the ALSA sequencer).
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"
#include "../plugin/Grooves.h"
#include "../plugin/ParamsMeta.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

struct Listener : juce::MidiInputCallback
{
    std::atomic<int> ons { 0 }, offs { 0 };
    std::atomic<int> maxTogether { 0 }, held { 0 };
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override
    {
        if (m.isNoteOn()) { ++ons; int h = ++held; if (h > maxTogether) maxTogether = h; }
        else if (m.isNoteOff()) { ++offs; --held; }
    }
};

static void blocks (cc::Processor& p, int n, std::function<void (juce::MidiBuffer&, int)> in = {})
{
    juce::AudioBuffer<float> buf (2, 256);
    for (int i = 0; i < n; ++i)
    {
        juce::MidiBuffer m;
        if (in) in (m, i);
        p.processBlock (buf, m);
        juce::Thread::sleep (5);   // about real time, so the port thread sends as it would live
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<cc::Processor> p (dynamic_cast<cc::Processor*> (createPluginFilter()));
    p->setPlayConfigDetails (0, 2, 48000, 256);
    p->prepareToPlay (48000, 256);
    // a host that resets every parameter to its default (some do on load) must leave the chords on
    for (auto* q : p->getParameters()) q->setValueNotifyingHost (q->getDefaultValue());
    blocks (*p, 200);
    if (p->engine().getIntU ("midi_out") != 1) { std::printf ("FAIL: the host defaults switch MIDI Out off\n"); return 1; }

    // the groove library: at least 50, every style name real, every value in range, apply and dice work
    {
        auto* st = cc::findParam ("style");
        auto* rh = cc::findParam ("rhythm");
        int bad = 0;
        for (int i = 0; i < cc::NUM_GROOVES_LIB; ++i)
        {
            const auto& g = cc::GROOVES_LIB[i];
            for (auto& n : juce::StringArray::fromTokens (g.styles, "|", ""))
            {
                bool found = false;
                for (int k = 0; k < st->nopt; ++k) found |= n == st->opt[k];
                if (! found) { std::printf ("FAIL: groove %s: no style \"%s\"\n", g.name, n.toRawUTF8()); ++bad; }
            }
            if (g.rhythm < 0 || g.rhythm >= rh->nopt || g.swing < 50 || g.swing > 75 || g.gate < 10 || g.gate > 100
                || g.strum > 100 || g.velocity < 1 || g.velocity > 127 || g.humanize > 100 || g.strumMode > 4 || g.arpOct > 2)
            { std::printf ("FAIL: groove %s: a value out of range\n", g.name); ++bad; }
        }
        p->applyGroove (5);
        const auto& g = cc::GROOVES_LIB[5];
        if (p->engine().getIntU ("rhythm") != g.rhythm || p->engine().getIntU ("gate") != g.gate || p->grooveEdited()) ++bad;
        p->diceGroove();
        if (p->grooveIndex() == 5 || p->grooveIndex() < 0) ++bad;
        std::printf ("%s %d grooves, apply and dice\n", bad == 0 && cc::NUM_GROOVES_LIB >= 50 ? "ok  " : "FAIL", cc::NUM_GROOVES_LIB);
        if (bad || cc::NUM_GROOVES_LIB < 50) return 1;
    }
    p->selectMidiPort ("virtual");
    std::printf ("port: %s\n", p->midiPort().status().toRawUTF8());
    if (! p->midiPort().active()) { std::printf ("SKIPPED (no MIDI system here)\n"); return 0; }
    juce::MidiDeviceInfo found;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (d.name.startsWith ("CheatChord")) found = d;
    if (found.identifier.isEmpty()) { std::printf ("FAIL: no MIDI input named CheatChord\n"); return 1; }
    Listener l;
    auto in = juce::MidiInput::openDevice (found.identifier, &l);
    if (in == nullptr) { std::printf ("FAIL: cannot open %s\n", found.name.toRawUTF8()); return 1; }
    in->start();
    p->engine().trigger ("generate");
    blocks (*p, 60);
    // pad 1 from a keyboard: chord 1 comes out of the port
    blocks (*p, 1, [] (juce::MidiBuffer& m, int) { m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 3); });
    blocks (*p, 60);
    const int together = l.maxTogether.load();
    blocks (*p, 1, [] (juce::MidiBuffer& m, int) { m.addEvent (juce::MidiMessage::noteOff (1, 36), 3); });
    blocks (*p, 60);
    in->stop();
    const bool ok = l.ons >= 3 && together >= 3 && l.ons == l.offs;
    std::printf ("%s port %s: %d note-ons, %d at once, %d note-offs\n", ok ? "PASSED" : "FAIL", found.name.toRawUTF8(),
                 l.ons.load(), together, l.offs.load());
    p->selectMidiPort ("off");
    p->releaseResources();
    return ok ? 0 : 1;
}
