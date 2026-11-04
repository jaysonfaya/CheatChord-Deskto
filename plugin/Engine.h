// CheatChord desktop: the C engine (engine/*.c, the MPC build's code) behind one lock.
#pragma once
#include <juce_core/juce_core.h>
#include <mutex>
#include <string>

extern "C" {
#include "engine.h"
#include "cc_desktop.h"
}

namespace cc {

// Display texts come from the MPC build. A few words change for a desktop: tap -> click, MPC -> DAW.
juce::String desktopText (const char* s);

class Engine
{
public:
    Engine();
    ~Engine();

    // every engine call goes through one of these (one lock per instance: audio, screen and host threads)
    std::mutex& mutex() noexcept { return lock; }
    void* raw() const noexcept { return inst; }
    const mpc_engine_t* api() const noexcept { return E; }

    // locked helpers for the screen
    void set (const char* key, const juce::String& v);
    void set (const char* key, int v) { set (key, juce::String (v)); }
    void trigger (const char* key) { set (key, "1"); }

    // unlocked: the caller holds mutex()
    std::string getU (const char* key) const;
    int getIntU (const char* key) const;

private:
    const mpc_engine_t* E = nullptr;
    void* inst = nullptr;
    std::mutex lock;
    JUCE_DECLARE_NON_COPYABLE (Engine)
};

} // namespace cc
