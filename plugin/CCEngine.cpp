#include "CCEngine.h"

namespace cc {

static void ensureDocsRoot()
{
    static std::once_flag once;
    std::call_once (once, []
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("JJ DA PLUG").getChildFile ("CheatChord");
        dir.createDirectory();
        cc_set_docs_root (dir.getFullPathName().toRawUTF8());
    });
}

Engine::Engine()
{
    ensureDocsRoot();
    E = mpc_engine();
    inst = E->create (nullptr);
    jassert (inst != nullptr);
}

Engine::~Engine()
{
    if (inst != nullptr)
        E->destroy (inst);
}

void Engine::set (const char* key, const juce::String& v)
{
    std::lock_guard<std::mutex> g (lock);
    E->set_param (inst, key, v.toRawUTF8());
}

std::string Engine::getU (const char* key) const
{
    char buf[1024];
    buf[0] = 0;
    if (E->get_param (inst, key, buf, (int) sizeof buf) <= 0)
        return {};
    buf[sizeof buf - 1] = 0;
    return buf;
}

int Engine::getIntU (const char* key) const
{
    return std::atoi (getU (key).c_str());
}

juce::String desktopText (const char* s)
{
    juce::String t = juce::String::fromUTF8 (s);
    if (! t.containsIgnoreCase ("MPC") && ! t.contains ("TAP"))
        return t;

    struct R { const char* a; const char* b; };
    static const R reps[] = {
        { "MPC DOCUMENTS/CHEATCHORD/PRESETS", "DOCUMENTS/JJ DA PLUG/CHEATCHORD/PRESETS" },
        { "MPC'S PRESET MENU HAS THEM TOO", "THE DAW'S PRESET LIST HAS THEM TOO" },
        { "PRESS PLAY ON THE MPC OR LISTEN", "PRESS PLAY IN YOUR DAW, OR LISTEN" },
        { "WRITES A .MID FILE MPC CAN LOAD", "WRITES A .MID FILE (OR DRAG THE MIDI BUTTON INTO YOUR DAW)" },
        { "MPC: THE LOOP FOLLOWS MPC PLAY AND STOP", "HOST: THE LOOP FOLLOWS THE DAW'S PLAY AND STOP" },
        { "MPC PLAY / STOP WORK TOO", "THE DAW'S PLAY / STOP WORK TOO" },
        { "ASSUMED TILL MPC LOOPS", "ASSUMED TILL THE DAW LOOPS" },
        { "MPC, MEASURED", "DAW, MEASURED" },
        { "MPC LOOP:", "DAW LOOP:" },
        { "THE MPC", "THE DAW" },
        { "MPC'S", "THE DAW'S" },
        { "(MPC)", "(DAW)" },
        { "MPC ", "DAW " },
        { "TAPPED", "CLICKED" },
        { "TAP ", "CLICK " },
        { "TAPS", "CLICKS" },
    };
    for (auto& r : reps)
        t = t.replace (r.a, r.b);
    return t;
}

} // namespace cc
