// CheatChord desktop: REC. Records what CheatChord plays (its MIDI out: the chords from the pads, the keyboard, the
// progression) into a take, optionally after a one-bar count-in with a click. The take becomes a .mid file to drag
// into the DAW or find in Documents / JJ DA PLUG / CheatChord / MIDI Exports.
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace cc {

class Recorder
{
public:
    enum Phase { Idle = 0, Request = 1, CountIn = 2, Recording = 3, Done = 4, StopRequest = 5 };

    // message thread
    void toggle();                       // idle -> count-in / recording; count-in -> cancel; recording -> stop
    void poll();                         // a finished take becomes a file
    juce::String status() const;
    int phase() const noexcept { return ph.load(); }
    juce::File lastTake() const { return take; }
    bool countIn = true;                 // one bar of clicks before recording
    int lengthBars = 0;                  // 0: until REC is pressed again; else stops by itself after 4 / 8 bars
    float clickLevel = 0.5f;

    // audio thread: out = what CheatChord played in this block
    void process (const juce::MidiBuffer& out, int n, double sr, double ppq, double bpm, bool hostPlaying, float* L, float* R);

private:
    struct Ev { double beat; juce::uint8 b[3]; };
    static constexpr int MAXEV = 20000;
    Ev ev[MAXEV];
    std::atomic<int> ph { Idle }, nev { 0 };
    std::atomic<int> countBeat { 0 };
    std::atomic<double> recBeats { 0 };
    bool own = false;                    // our own clock (the DAW was stopped when REC was pressed)
    double ownBeat = 0, startBeat = 0, endBeat = 0, takeBpm = 120;
    int clickPos = -1, clickLen = 0;
    bool clickHi = false;
    bool armedCountIn = true;
    int armedLength = 0;
    juce::File take;
    int takeNo = 0;
    juce::String msg;
};

} // namespace cc
