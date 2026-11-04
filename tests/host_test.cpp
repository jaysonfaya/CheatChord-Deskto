// CheatChord desktop: loads the built VST3 like a DAW does and checks it.
//   cc_host_test <path to CheatChord.vst3>
// Factory, parameters, programs; generate a progression through a host parameter; play it with the transport at 48 and
// 96 kHz (MIDI out on the beat, audio from the synth); a pad note from a MIDI keyboard; state round trip.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (cond) { std::printf ("ok   "); } else { std::printf ("FAIL "); ++fails; } std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

struct PlayHead : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120;
    bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

struct Run
{
    juce::AudioPluginInstance& p;
    PlayHead ph;
    double sr;
    int block;
    std::vector<std::pair<juce::int64, juce::MidiMessage>> out;   // absolute sample, message
    juce::int64 t = 0;
    float peak = 0;
    bool nan = false;

    Run (juce::AudioPluginInstance& pi, double s, int b) : p (pi), sr (s), block (b)
    {
        p.setPlayHead (&ph);
        p.setPlayConfigDetails (0, 2, sr, block);
        p.prepareToPlay (sr, block);
    }
    void blocks (int n, std::function<void (juce::MidiBuffer&, int)> in = {})
    {
        juce::AudioBuffer<float> buf (2, block);
        for (int i = 0; i < n; ++i)
        {
            juce::MidiBuffer m;
            if (in) in (m, i);
            buf.clear();
            p.processBlock (buf, m);
            for (const auto ev : m) out.push_back ({ t + ev.samplePosition, ev.getMessage() });
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < block; ++k)
                {
                    float v = buf.getSample (c, k);
                    if (! std::isfinite (v)) nan = true;
                    peak = juce::jmax (peak, std::abs (v));
                }
            t += block;
            if (ph.playing) ph.ppq += block * ph.bpm / 60.0 / sr;
        }
    }
    void seconds (double s) { blocks ((int) std::ceil (s * sr / block)); }
};

static juce::AudioProcessorParameter* paramNamed (juce::AudioPluginInstance& p, const juce::String& name)
{
    for (auto* q : p.getParameters())
        if (q->getName (100) == name) return q;
    return nullptr;
}

// the plugin's own state inside the host's VST3 state (<VST3PluginState><IComponent>base64</IComponent>)
static std::unique_ptr<juce::XmlElement> pluginXml (juce::AudioPluginInstance& p)
{
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    auto outer = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
    if (outer == nullptr) outer = juce::parseXML (mb.toString());
    if (outer == nullptr) return {};
    auto* ic = outer->getChildByName ("IComponent");
    if (ic == nullptr) return {};
    juce::MemoryBlock inner;
    inner.fromBase64Encoding (ic->getAllSubText());
    return juce::AudioProcessor::getXmlFromBinary (inner.getData(), (int) inner.getSize());
}

static juce::String engineState (juce::AudioPluginInstance& p)
{
    auto xml = pluginXml (p);
    if (xml == nullptr) return {};
    if (auto* e = xml->getChildByName ("ENGINE")) return e->getAllSubText();
    return {};
}

static int slotCount (const juce::String& st)
{
    auto s = st.fromFirstOccurrenceOf ("slots=", false, false).upToFirstOccurrenceOf (";", false, false);
    if (s.isEmpty()) return 0;
    return juce::StringArray::fromTokens (s, "|", "").size();
}

static void trigger (juce::AudioPluginInstance& p, const juce::String& name, Run& r)
{
    auto* q = paramNamed (p, name);
    if (q == nullptr) { std::printf ("no parameter %s\n", name.toRawUTF8()); return; }
    q->setValueNotifyingHost (1.0f);
    r.blocks (4);
    q->setValueNotifyingHost (0.0f);
    r.blocks (4);
}

static void playTest (juce::AudioPluginInstance& p, double sr, int block)
{
    std::printf ("---- %g Hz, %d frames\n", sr, block);
    Run r (p, sr, block);
    r.seconds (1.0);   // the quiet time after loading
    if (slotCount (engineState (p)) == 0)
    {
        trigger (p, "Action: Generate", r);
        for (int i = 0; i < 100 && slotCount (engineState (p)) == 0; ++i) r.seconds (0.05);
    }
    const int n = slotCount (engineState (p));
    CHECK (n >= 4, "GENERATE through the host parameter: %d chords", n);

    // Play Mode = Host (option 1), chord length 1 bar, sustained chords
    if (auto* q = paramNamed (p, "Play Mode")) q->setValueNotifyingHost (1.0f / (float) juce::jmax (1, q->getNumSteps() - 1));
    if (auto* q = paramNamed (p, "Rhythm")) q->setValueNotifyingHost (0.0f);
    if (auto* q = paramNamed (p, "Loop Sync")) q->setValueNotifyingHost (0.0f);
    if (auto* q = paramNamed (p, "Humanize")) q->setValueNotifyingHost (0.0f);   // exact timing for the grid check
    r.blocks (4);
    r.out.clear();
    r.peak = 0;
    r.ph.playing = true;
    r.ph.ppq = 0;
    const auto t0 = r.t;
    r.seconds (8.0 * 4 * 60.0 / r.ph.bpm + 0.1);   // 8 bars
    r.ph.playing = false;
    r.seconds (0.5);
    int ons = 0, offs = 0;
    std::vector<double> starts;   // in beats
    for (auto& [at, m] : r.out)
    {
        if (m.isNoteOn()) { ++ons; starts.push_back ((at - t0) * r.ph.bpm / 60.0 / sr); }
        if (m.isNoteOff()) ++offs;
    }
    CHECK (ons >= 8 * 3, "MIDI out while the host plays 8 bars: %d note-ons", ons);
    CHECK (ons == offs, "every note ends (%d on, %d off)", ons, offs);
    // chord starts on bar lines: the chord changes at 0, 4, 8 ... beats (within one sample)
    int onGrid = 0, groups = 0;
    double last = -10;
    for (double b : starts)
        if (b - last > 0.5)
        {
            ++groups;
            last = b;
            double nearest = std::round (b / 4.0) * 4.0;
            if (std::abs (b - nearest) * 60.0 / r.ph.bpm * sr < 1.5) ++onGrid;
        }
    CHECK (groups >= 8 && onGrid == groups, "chords start exactly on the bar lines: %d of %d", onGrid, groups);
    if (onGrid != groups) { for (double b : starts) std::printf ("%.4f ", b); std::printf ("\n"); }
    CHECK (r.peak > 0.01f && r.peak < 1.0f && ! r.nan, "the synth sounds: peak %.3f, no NaN", r.peak);

    // a MIDI keyboard: pad 1 (note 36) plays chord 1 while held
    r.out.clear();
    r.blocks (1, [] (juce::MidiBuffer& m, int) { m.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 17); });
    r.blocks ((int) (0.3 * sr / block));
    r.blocks (1, [] (juce::MidiBuffer& m, int) { m.addEvent (juce::MidiMessage::noteOff (1, 36), 5); });
    r.seconds (0.3);
    int pon = 0, poff = 0;
    juce::int64 first = -1;
    for (auto& [at, m] : r.out)
    {
        if (m.isNoteOn()) { ++pon; if (first < 0) first = at; }
        if (m.isNoteOff()) ++poff;
    }
    CHECK (pon >= 3 && pon == poff, "pad 1 plays chord 1 and stops on release (%d on, %d off)", pon, poff);
    p.releaseResources();
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("usage: cc_host_test <CheatChord.vst3>\n"); return 2; }
    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> descs;
    fmt.findAllTypesForFile (descs, juce::String (argv[1]));
    CHECK (descs.size() == 1, "the bundle has one plugin (%d)", descs.size());
    if (descs.isEmpty()) return 1;
    auto& d = *descs[0];
    CHECK (d.name == "CheatChord" && d.manufacturerName == "JJ DA PLUG" && d.isInstrument, "%s by %s, instrument=%d (%s)",
           d.name.toRawUTF8(), d.manufacturerName.toRawUTF8(), (int) d.isInstrument, d.category.toRawUTF8());
    juce::String err;
    auto p = fmt.createInstanceFromDescription (d, 48000.0, 512, err);
    CHECK (p != nullptr, "instance %s", err.toRawUTF8());
    if (p == nullptr) return 1;
    CHECK (p->acceptsMidi() && p->producesMidi(), "MIDI in and out");
    CHECK (p->getParameters().size() >= 50, "%d parameters", p->getParameters().size());
    CHECK (p->getNumPrograms() == 40 && p->getProgramName (1).isNotEmpty(), "40 programs, #2 = %s", p->getProgramName (1).toRawUTF8());

    playTest (*p, 48000.0, 512);
    playTest (*p, 96000.0, 333);
    playTest (*p, 44100.0, 64);

    // state: a second instance opens to the same engine state
    auto st = engineState (*p);
    juce::MemoryBlock mb;
    p->getStateInformation (mb);
    auto q = fmt.createInstanceFromDescription (d, 48000.0, 512, err);
    q->setStateInformation (mb.getData(), (int) mb.getSize());
    auto st2 = engineState (*q);
    CHECK (st.isNotEmpty() && st == st2, "state round trip (%d bytes)", st.length());
    // the host parameter follows the loaded state
    auto* keyP = paramNamed (*q, "Key");
    const int k1 = st.fromFirstOccurrenceOf ("key=", false, false).getIntValue();
    CHECK (keyP != nullptr && juce::roundToInt (keyP->getValue() * 11.0f) == k1, "host parameter Key = %d after loading", k1);

    q = nullptr;
    p = nullptr;
    std::printf (fails ? "FAILED (%d of %d)\n" : "PASSED (%d checks)\n", fails ? fails : checks, checks);
    return fails ? 1 : 0;
}
