// CheatChord desktop: its own MIDI port. The chords go out of the plugin as a MIDI device, so any track in any DAW can
// pick "CheatChord" as its MIDI input (also where the host takes no MIDI from plugins: MPC 2, Ableton).
//   macOS / Linux: a virtual port named "CheatChord" (CoreMIDI / ALSA), nothing to install.
//   Windows: a port named "CheatChord" through the teVirtualMIDI driver that comes with loopMIDI (free, by Tobias
//            Erichsen); or any existing MIDI output (a loopMIDI port, a hardware interface).
#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>

namespace cc {

class MidiPort : private juce::Thread
{
public:
    MidiPort();
    ~MidiPort() override;

    struct Choice { juce::String id, name; };   // id "" = off, "virtual" = own port, else a device identifier
    std::vector<Choice> choices() const;
    void select (const juce::String& id);       // message thread
    juce::String selected() const { return current; }
    juce::String status() const;
    bool active() const noexcept { return isOpen.load(); }

    // audio thread: a message at a time in milliseconds (Time::getMillisecondCounterHiRes)
    void push (double atMs, juce::uint8 a, juce::uint8 b, juce::uint8 c) noexcept;
    // input notes that are our own (the port routed back into the CheatChord track) are dropped (audio thread)
    bool isEcho (int note, bool on, double nowMs) noexcept;
    static bool virtualPossible();               // can make a port named "CheatChord" here

private:
    void run() override;
    void closeAll();
    void sendNow (const juce::uint8* d, int n);

    struct Ev { double at; juce::uint8 b[3]; };
    juce::AbstractFifo fifo { 4096 };
    std::vector<Ev> ring = std::vector<Ev> (4096);
    std::unique_ptr<juce::MidiOutput> out;
    void* te = nullptr;   // teVirtualMIDI port (Windows)
    juce::String current, portName, err;
    std::atomic<bool> isOpen { false };
    juce::CriticalSection sendLock;
    // echo guard: notes sent recently
    double sentOn[128] {}, sentOff[128] {};
    int dropOff[128] {};
};

} // namespace cc
