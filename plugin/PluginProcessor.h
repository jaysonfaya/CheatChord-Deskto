// CheatChord desktop plugin (VST3 / AU / Standalone): the processor.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <climits>
#include <vector>
#include <set>
#include <string>
#include "CCEngine.h"
#include "MidiPort.h"

namespace cc {

// The settings a DAW can automate (and an MPC Q-Link can be mapped to). Their IDs are the engine keys.
struct HostParam
{
    const char* key;
    bool trigger;                      // an action: a rise from off to on fires it
    juce::RangedAudioParameter* p = nullptr;
    std::atomic<int> applied { INT_MIN }; // the engine value last sent or read back
};

class Processor : public juce::AudioProcessor, private juce::Timer
{
public:
    Processor();
    ~Processor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CheatChord"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // for the editor
    Engine& engine() noexcept { return eng; }
    // set a value from the screen: through the host parameter when it has one (automation records it)
    void setValue (const char* key, int v);
    void gesture (const char* key, bool begin);   // a knob drag: one automation gesture
    // keep the engine alive while the host doesn't call processBlock (stopped transport in some hosts, no audio device)
    void pumpIfIdle();
    // the editor's saved look (layout, zoom): kept in the project
    juce::ValueTree& uiState() noexcept { return ui; }
    // write the progression as a .mid for drag and drop; returns the file (or an empty File)
    juce::File writeDragMidi (juce::String& msg);
    double lastBpm() const noexcept { return bpm.load(); }
    // the plugin's own MIDI port (a track's MIDI input in any DAW)
    MidiPort& midiPort() noexcept { return port; }
    void selectMidiPort (const juce::String& id);
    bool hostPlaying() const noexcept { return playingNow.load(); }

private:
    template <typename F> void process (juce::AudioBuffer<F>&, juce::MidiBuffer&);
    void applyHostParams();         // engine lock held
    void timerCallback() override;  // engine -> host parameters (message thread)
    void syncParamsFromEngine();    // after a state load or preset
    static void midiSink (void* ctx, int offset, uint8_t st, uint8_t d1, uint8_t d2);

    Engine eng;
    MidiPort port;
    void toPort (const juce::MidiBuffer& m, double baseMs);
    std::vector<std::unique_ptr<HostParam>> hostParams;
    std::atomic<int> uiWriting { 0 };
    std::set<std::string> inGesture;   // message thread
    juce::ValueTree ui { "UI" };

    // audio-thread state
    juce::MidiBuffer outMidi;
    int sinkBase = 0;
    double sr = 44100.0;
    std::atomic<double> bpm { 120.0 };
    std::atomic<bool> playingNow { false };
    std::atomic<juce::int64> lastProcessMs { 0 };
    juce::String lastCycle;
    std::vector<float> scratchL, scratchR;
    int64_t idleMs = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Processor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};

} // namespace cc
