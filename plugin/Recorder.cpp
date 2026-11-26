#include "Recorder.h"
#include "../engine/cc_desktop.h"
#include <cmath>

namespace cc {

void Recorder::toggle()
{
    const int p = ph.load();
    if (p == Idle) { armedCountIn = countIn; armedLength = lengthBars; nev = 0; msg = {}; ph = Request; }
    else if (p == Request || p == CountIn) { ph = Idle; msg = "REC CANCELLED"; }
    else if (p == Recording) ph = StopRequest;
}

juce::String Recorder::status() const
{
    switch (ph.load())
    {
        case Request:   return "REC: GET READY";
        case CountIn:   return "COUNT-IN  " + juce::String (juce::jlimit (1, 4, countBeat.load())) + " / 4";
        case Recording: return "REC  " + juce::String (recBeats.load() / 4.0, 1) + " BARS" + (armedLength > 0 ? " OF " + juce::String (armedLength) : juce::String()) + "  (REC AGAIN = STOP)";
        case StopRequest:
        case Done:      return "SAVING THE TAKE...";
        default:        return msg;
    }
}

void Recorder::process (const juce::MidiBuffer& out, int n, double sr, double ppq, double bpm, bool hostPlaying, float* L, float* R)
{
    int p = ph.load();
    if (p == Idle || p == Done || n <= 0 || sr <= 0) return;
    if (bpm <= 0) bpm = 120;
    const double inc = bpm / 60.0 / sr;
    if (p == Request)
    {
        own = ! (hostPlaying && ppq >= 0);
        double b0 = own ? 0.0 : ppq;
        ownBeat = 0;
        // with the DAW playing, the take starts on a bar line: the next one, or (count-in) the one after
        const double bar = std::ceil (b0 / 4.0 - 1e-6) * 4.0;
        startBeat = own ? (armedCountIn ? 4.0 : 0.0) : (armedCountIn ? bar + 4.0 : b0);
        if (! own && armedCountIn && bar - b0 < 0.05) startBeat = bar + 4.0;
        takeBpm = bpm;
        nev = 0;
        p = armedCountIn ? CountIn : Recording;
        ph = p;
    }
    if (! own && ! (hostPlaying && ppq >= 0))
    {
        // the DAW stopped: a take ends there, a count-in is cancelled
        if (p == Recording || p == StopRequest) { endBeat = recBeats.load(); ph = Done; }
        else ph = Idle;
        return;
    }
    const double b0 = own ? ownBeat : ppq;
    const double b1 = b0 + n * inc;

    // the click: four beats before the take starts (beat 1 higher)
    if (p == CountIn)
        for (int j = 0; j < 4; ++j)
        {
            const double bk = startBeat - 4.0 + j;
            if (bk >= b0 - 1e-9 && bk < b1)
            {
                clickPos = -(int) std::floor ((bk - b0) / inc);   // starts that many samples into this block
                clickHi = j == 0;
                clickLen = (int) (0.045 * sr);
                countBeat = j + 1;
            }
        }
    if (clickPos > -n * 4 && clickPos < clickLen && clickLen > 0)
    {
        const double f = clickHi ? 1760.0 : 1180.0;
        for (int i = 0; i < n; ++i)
        {
            const int k = clickPos + i;
            if (k < 0 || k >= clickLen) continue;
            const float env = std::exp (-6.0f * (float) k / (float) clickLen);
            const float v = clickLevel * env * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * k / sr);
            L[i] += v; R[i] += v;
        }
        clickPos += n;
        if (clickPos >= clickLen) clickLen = 0;
    }

    if (p == CountIn && b1 > startBeat) { p = Recording; ph = p; }

    // what CheatChord played; a hit up to a 16th early on the count-in's last beat counts as on the one
    if (p == CountIn || p == Recording || p == StopRequest)
        for (const auto meta : out)
        {
            const auto m = meta.getMessage();
            if (m.getRawDataSize() < 1 || m.getRawDataSize() > 3) continue;
            double t = b0 + meta.samplePosition * inc - startBeat;
            if (t < -0.26) continue;
            if (armedLength > 0 && t >= armedLength * 4.0) continue;
            int k = nev.load();
            if (k >= MAXEV) continue;
            ev[k].beat = juce::jmax (0.0, t);
            const auto* d = m.getRawData();
            ev[k].b[0] = d[0]; ev[k].b[1] = m.getRawDataSize() > 1 ? d[1] : 0; ev[k].b[2] = m.getRawDataSize() > 2 ? d[2] : 0;
            nev = k + 1;
        }
    if (p == Recording || p == StopRequest) recBeats = juce::jmax (0.0, b1 - startBeat);
    if (p == StopRequest) { endBeat = recBeats.load(); ph = Done; }
    else if (p == Recording && armedLength > 0 && b1 - startBeat >= armedLength * 4.0) { endBeat = armedLength * 4.0; ph = Done; }
    if (own) ownBeat = b1;
}

void Recorder::poll()
{
    if (ph.load() != Done) return;
    const int k = nev.load();
    const int TPQ = 960;
    double end = endBeat;
    if (armedLength == 0) end = std::ceil (juce::jmax (end, 0.01) / 4.0 - 1e-6) * 4.0;   // to the bar
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / takeBpm)), 0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    bool on[16][128] = {};
    int notes = 0;
    for (int i = 0; i < k; ++i)
    {
        juce::MidiMessage m (ev[i].b, (ev[i].b[0] & 0xf0) == 0xc0 || (ev[i].b[0] & 0xf0) == 0xd0 ? 2 : 3, 0.0);
        if (ev[i].beat > end) continue;
        const double t = ev[i].beat * TPQ;
        if (m.isNoteOn()) { on[m.getChannel() - 1][m.getNoteNumber()] = true; ++notes; }
        else if (m.isNoteOff()) { if (! on[m.getChannel() - 1][m.getNoteNumber()]) continue; on[m.getChannel() - 1][m.getNoteNumber()] = false; }
        m.setTimeStamp (t);
        seq.addEvent (m);
    }
    for (int c = 0; c < 16; ++c)   // still held at the end: they end there
        for (int nn = 0; nn < 128; ++nn)
            if (on[c][nn]) seq.addEvent (juce::MidiMessage::noteOff (c + 1, nn).withTimeStamp (end * TPQ));
    seq.addEvent (juce::MidiMessage::endOfTrack().withTimeStamp (end * TPQ));
    seq.updateMatchedPairs();
    if (notes == 0)
    {
        msg = "NOTHING RECORDED: PLAY PADS OR KEYS WHILE REC RUNS (MIDI OUT MUST BE ON)";
        ph = Idle;
        return;
    }
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (TPQ);
    mf.addTrack (seq);
    auto dir = juce::File (juce::String::fromUTF8 (cc_docs_root())).getChildFile ("MIDI Exports");
    dir.createDirectory();
    auto f = dir.getNonexistentChildFile ("CheatChord Take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M"), ".mid", true);
    if (auto os = f.createOutputStream())
    {
        mf.writeTo (*os);
        os.reset();
        take = f;
        ++takeNo;
        msg = "TAKE " + juce::String (takeNo) + " SAVED: " + juce::String (notes) + " NOTES, " + juce::String (end / 4.0, 1) + " BARS - DRAG TAKE INTO YOUR DAW";
    }
    else
        msg = "THE TAKE COULD NOT BE SAVED";
    ph = Idle;
}

} // namespace cc
