// CheatChord desktop: the own MIDI port. Opens "CheatChord", listens to it like a DAW track would, plays pad 1 and a
// generated progression, and checks that whole chords arrive (not one note). Skips where the system has no MIDI
// (a build machine without the ALSA sequencer).
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"

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
    blocks (*p, 200);
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
    p->selectMidiPort ("");
    p->releaseResources();
    return ok ? 0 : 1;
}
