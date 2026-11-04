#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParamsMeta.h"

namespace cc {

// the host-automatable settings, in the order a DAW lists them; triggers last
static const char* const HOST_VALUES[] = {
    "key", "scale", "style", "artist", "producer", "spice", "style_mix", "artist_mix", "producer_mix",
    "gen_len", "scope", "listen", "run", "loop_sync", "bars", "rhythm", "rrate", "rate", "arp_oct", "direction", "push",
    "strum", "strum_mode", "swing", "gate", "velocity", "humanize", "mute",
    "voicing", "lead", "octave", "bass",
    "sound", "level", "tone", "release", "space", "midi_out", "synth", "channel", "bass_ch",
};
static const char* const HOST_TRIGGERS[] = {
    "generate", "cruise", "reroll", "add", "undo", "redo", "delete", "clear", "stop", "panic",
};

static juce::String hostName (const ParamMeta& m)
{
    juce::String n (m.name);
    if (juce::String (m.key) == "listen") n = "Play Mode";
    if (juce::String (m.key) == "run") n = "Listen Play";
    return n;
}

static juce::StringArray optionNames (const ParamMeta& m)
{
    juce::StringArray a;
    for (int i = 0; i < m.nopt; ++i)
        a.add (desktopText (m.opt[i]).replace ("Auto (DAW)", "Auto (Host)").replace ("MPC", "Host"));
    return a;
}

static int paramInt (const juce::RangedAudioParameter& p)
{
    return (int) std::lround (p.convertFrom0to1 (p.getValue()));
}

Processor::Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    int version = 1;
    for (auto* k : HOST_VALUES)
    {
        const ParamMeta* m = findParam (k);
        jassert (m != nullptr && m->kind == 0);
        auto hp = std::make_unique<HostParam>();
        hp->key = m->key;
        hp->trigger = false;
        juce::ParameterID id { m->key, version };
        if (m->nopt > 0)
            hp->p = new juce::AudioParameterChoice (id, hostName (*m), optionNames (*m), juce::jlimit (0, m->nopt - 1, m->def));
        else
            hp->p = new juce::AudioParameterInt (id, hostName (*m), m->min, m->max, juce::jlimit (m->min, m->max, m->def));
        addParameter (hp->p);
        hostParams.push_back (std::move (hp));
    }
    for (auto* k : HOST_TRIGGERS)
    {
        const ParamMeta* m = findParam (k);
        jassert (m != nullptr && m->kind == 1);
        auto hp = std::make_unique<HostParam>();
        hp->key = m->key;
        hp->trigger = true;
        hp->p = new juce::AudioParameterBool (juce::ParameterID { juce::String ("do_") + m->key, version },
                                              juce::String ("Action: ") + m->name, false);
        addParameter (hp->p);
        hostParams.push_back (std::move (hp));
    }

    cc_set_midi_sink (eng.raw(), &Processor::midiSink, this);
    {
        std::lock_guard<std::mutex> g (eng.mutex());
        eng.api()->set_param (eng.raw(), "panel", "0");   // the desktop screen shows every panel at once
    }
    syncParamsFromEngine();
    outMidi.ensureSize (8192);
    startTimerHz (15);
}

Processor::~Processor()
{
    stopTimer();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    scratchL.assign ((size_t) juce::jmax (128, samplesPerBlock), 0.0f);
    scratchR.assign ((size_t) juce::jmax (128, samplesPerBlock), 0.0f);
    std::lock_guard<std::mutex> g (eng.mutex());
    cc_set_sample_rate (eng.raw(), sr);
    outMidi.ensureSize (8192);
}

void Processor::releaseResources()
{
    std::lock_guard<std::mutex> g (eng.mutex());
    eng.api()->set_param (eng.raw(), "mains", "0");   // nothing keeps playing
}

void Processor::midiSink (void* ctx, int offset, uint8_t st, uint8_t d1, uint8_t d2)
{
    auto* self = static_cast<Processor*> (ctx);
    if (self->outMidi.getNumEvents() > 16384)
        return;
    const int pos = juce::jmax (0, self->sinkBase + offset);
    self->outMidi.addEvent (juce::MidiMessage (st, d1, d2), pos);
}

void Processor::applyHostParams()
{
    if (uiWriting.load() > 0)
        return;
    auto* E = eng.api();
    void* in = eng.raw();
    for (auto& hp : hostParams)
    {
        const int v = paramInt (*hp->p);
        const int was = hp->applied.load();
        if (hp->trigger)
        {
            if (v == 1 && was == 0)
                E->set_param (in, hp->key, "1");
            hp->applied = v;
            continue;
        }
        if (v == was)
            continue;
        E->set_param (in, hp->key, juce::String (v).toRawUTF8());
        hp->applied = eng.getIntU (hp->key);
    }
}

template <typename F>
void Processor::process (juce::AudioBuffer<F>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    lastProcessMs = juce::Time::currentTimeMillis();
    if (n <= 0)
        return;
    if ((int) scratchL.size() < n)   // a host that sends more than it announced: once, then never again
    {
        scratchL.resize ((size_t) n);
        scratchR.resize ((size_t) n);
    }

    double ppq = -1.0, tempo = 0.0, loopStart = 0.0, loopEnd = 0.0;
    bool playing = false, looping = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) tempo = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            playing = pos->getIsPlaying();
            looping = pos->getIsLooping();
            if (auto lp = pos->getLoopPoints()) { loopStart = lp->ppqStart; loopEnd = lp->ppqEnd; }
        }
    }
    if (tempo > 0) bpm = tempo;
    playingNow = playing;

    std::unique_lock<std::mutex> g (eng.mutex());
    auto* E = eng.api();
    void* in = eng.raw();
    applyHostParams();

    if (looping && loopEnd - loopStart >= 1.0)
    {
        juce::String c = juce::String (loopStart, 6) + " " + juce::String (loopEnd, 6);
        if (c != lastCycle)
        {
            lastCycle = c;
            E->set_param (in, "host_cycle", c.toRawUTF8());
        }
    }

    float* L = scratchL.data();
    float* R = scratchR.data();
    auto it = midi.cbegin();
    const auto end = midi.cend();
    auto feed = [&] (const juce::MidiMessage& m)
    {
        if (m.isNoteOnOrOff())
        {
            const uint8_t* d = m.getRawData();
            uint8_t raw[3] = { d[0], d[1], (uint8_t) (m.getRawDataSize() > 2 ? d[2] : 0) };
            if (m.isNoteOff()) raw[0] = (uint8_t) (0x80 | (raw[0] & 15));
            E->midi (in, raw, 3);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            E->set_param (in, "panic", "1");
    };

    const double beatsPerFrame = tempo > 0 ? tempo / 60.0 / sr : 0.0;
    int pos = 0;
    while (pos < n)
    {
        while (it != end && (*it).samplePosition <= pos)
        {
            feed ((*it).getMessage());
            ++it;
        }
        const int next = it != end ? juce::jmax (pos + 1, (*it).samplePosition) : n;
        const int len = juce::jmin (128, next - pos, n - pos);
        sinkBase = pos;
        if (E->song_pos != nullptr)
            E->song_pos (in, ppq >= 0 ? ppq + pos * beatsPerFrame : -1.0, tempo, playing ? 1 : 0, len);
        cc_render_float (in, L + pos, R + pos, len);
        pos += len;
    }
    for (; it != end; ++it)
        feed ((*it).getMessage());
    sinkBase = 0;

    // the notes sent in this block replace the input (an instrument consumes what it plays)
    midi.swapWith (outMidi);
    outMidi.clear();
    g.unlock();

    const int nch = buffer.getNumChannels();
    if (nch >= 2)
    {
        auto* o0 = buffer.getWritePointer (0);
        auto* o1 = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i) { o0[i] = (F) L[i]; o1[i] = (F) R[i]; }
    }
    else if (nch == 1)
    {
        auto* o0 = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) o0[i] = (F) (0.5f * (L[i] + R[i]));
    }
}

void Processor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { process (b, m); }
void Processor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer& m) { process (b, m); }

void Processor::pumpIfIdle()
{
    const auto now = juce::Time::currentTimeMillis();
    if (now - lastProcessMs.load() < 150)
    {
        idleMs = now;
        return;
    }
    if (idleMs == 0) idleMs = now;
    const auto elapsed = juce::jlimit<juce::int64> (0, 250, now - idleMs);
    idleMs = now;
    int frames = (int) (elapsed * sr / 1000.0);
    float l[128], r[128];
    std::lock_guard<std::mutex> g (eng.mutex());
    applyHostParams();
    auto* E = eng.api();
    while (frames > 0)
    {
        const int len = juce::jmin (128, frames);
        sinkBase = 0;
        if (E->song_pos != nullptr)
            E->song_pos (eng.raw(), -1.0, bpm.load(), 0, len);
        cc_render_float (eng.raw(), l, r, len);
        frames -= len;
    }
    if (outMidi.getNumEvents() > 4096)
        outMidi.clear();
}

void Processor::timerCallback()
{
    // values the engine changed by itself (a preset, an artist bringing its style, LISTEN ending...) go to the host
    std::vector<std::pair<HostParam*, int>> changes;
    ++uiWriting;
    {
        std::lock_guard<std::mutex> g (eng.mutex());
        for (auto& hp : hostParams)
        {
            if (hp->trigger) continue;
            const int e = eng.getIntU (hp->key);
            if (e != hp->applied.load())
            {
                hp->applied = e;
                changes.push_back ({ hp.get(), e });
            }
        }
    }
    for (auto& [hp, e] : changes)
        if (paramInt (*hp->p) != e)
            hp->p->setValueNotifyingHost (hp->p->convertTo0to1 ((float) e));
    --uiWriting;
}

void Processor::syncParamsFromEngine()
{
    ++uiWriting;
    std::vector<std::pair<HostParam*, int>> vals;
    {
        std::lock_guard<std::mutex> g (eng.mutex());
        for (auto& hp : hostParams)
        {
            if (hp->trigger) { hp->applied = paramInt (*hp->p); continue; }
            const int e = eng.getIntU (hp->key);
            hp->applied = e;
            vals.push_back ({ hp.get(), e });
        }
    }
    for (auto& [hp, e] : vals)
        if (paramInt (*hp->p) != e)
            hp->p->setValueNotifyingHost (hp->p->convertTo0to1 ((float) e));
    --uiWriting;
}

void Processor::setValue (const char* key, int v)
{
    HostParam* hp = nullptr;
    for (auto& h : hostParams)
        if (! h->trigger && std::strcmp (h->key, key) == 0) hp = h.get();
    ++uiWriting;
    int e = v;
    {
        std::lock_guard<std::mutex> g (eng.mutex());
        eng.api()->set_param (eng.raw(), key, juce::String (v).toRawUTF8());
        e = eng.getIntU (key);
        if (hp != nullptr) hp->applied = e;
    }
    if (hp != nullptr && paramInt (*hp->p) != e)
    {
        const bool own = inGesture.count (key) == 0;
        if (own) hp->p->beginChangeGesture();
        hp->p->setValueNotifyingHost (hp->p->convertTo0to1 ((float) e));
        if (own) hp->p->endChangeGesture();
    }
    --uiWriting;
}

void Processor::gesture (const char* key, bool begin)
{
    for (auto& h : hostParams)
        if (! h->trigger && std::strcmp (h->key, key) == 0)
        {
            if (begin && inGesture.insert (key).second) h->p->beginChangeGesture();
            else if (! begin && inGesture.erase (key)) h->p->endChangeGesture();
        }
}

// ---------------------------------------------------------------- programs: the 24 factory presets and 16 user slots
int Processor::getNumPrograms() { return 40; }

int Processor::getCurrentProgram()
{
    std::lock_guard<std::mutex> g (eng.mutex());
    return juce::jlimit (0, 39, eng.getIntU ("preset"));
}

void Processor::setCurrentProgram (int index)
{
    eng.set ("preset", juce::jlimit (0, 39, index));
    syncParamsFromEngine();
}

const juce::String Processor::getProgramName (int index)
{
    std::lock_guard<std::mutex> g (eng.mutex());
    auto k = "preset:" + std::to_string (juce::jlimit (0, 39, index));
    auto s = eng.getU (k.c_str());
    return s.empty() ? juce::String ("Preset ") + juce::String (index + 1) : juce::String::fromUTF8 (s.c_str());
}

// ---------------------------------------------------------------- state
void Processor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("CHEATCHORD");
    root.setAttribute ("version", JucePlugin_VersionString);
    std::string st;
    {
        std::lock_guard<std::mutex> g (eng.mutex());
        char buf[16384];
        buf[0] = 0;
        if (eng.api()->get_param (eng.raw(), "state", buf, (int) sizeof buf) > 0)
            st = buf;
    }
    root.createNewChildElement ("ENGINE")->addTextElement (juce::String::fromUTF8 (st.c_str()));
    if (auto x = ui.createXml())
        root.addChildElement (x.release());
    copyXmlToBinary (root, destData);
}

void Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("CHEATCHORD"))
        return;
    if (auto* e = xml->getChildByName ("ENGINE"))
    {
        const auto st = e->getAllSubText();
        std::lock_guard<std::mutex> g (eng.mutex());
        if (st.isNotEmpty())
            eng.api()->set_param (eng.raw(), "state", st.toRawUTF8());
        eng.api()->set_param (eng.raw(), "panel", "0");
    }
    if (auto* u = xml->getChildByName ("UI"))
        ui = juce::ValueTree::fromXml (*u);
    syncParamsFromEngine();
}

juce::File Processor::writeDragMidi (juce::String& msg)
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("CheatChord Drag");
    dir.createDirectory();
    std::string name;
    {
        std::lock_guard<std::mutex> g (eng.mutex());
        name = eng.getU ("sel1");   // "Eb Major"
    }
    auto base = juce::File::createLegalFileName ("CheatChord " + juce::String::fromUTF8 (name.c_str()));
    auto f = dir.getChildFile (base + ".mid");
    char m[128] = {};
    int ok;
    {
        std::lock_guard<std::mutex> g (eng.mutex());
        ok = cc_write_midi (eng.raw(), f.getFullPathName().toRawUTF8(), m, (int) sizeof m);
    }
    msg = desktopText (m);
    return ok ? f : juce::File();
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }

} // namespace cc

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new cc::Processor(); }
