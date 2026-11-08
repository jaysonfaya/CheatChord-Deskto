#include "MidiPort.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace cc {

// ---------------------------------------------------------------- teVirtualMIDI (Windows, installed with loopMIDI)
#if JUCE_WINDOWS
namespace te
{
    typedef void (CALLBACK* DataCb) (void* port, LPBYTE data, DWORD len, DWORD_PTR user);
    typedef void* (CALLBACK* CreateEx2) (LPCWSTR name, DataCb cb, DWORD_PTR user, DWORD maxSysex, DWORD flags);
    typedef void (CALLBACK* Close) (void* port);
    typedef BOOL (CALLBACK* Send) (void* port, LPBYTE data, DWORD len);
    static HMODULE dll = nullptr;
    static CreateEx2 create = nullptr;
    static Close close = nullptr;
    static Send send = nullptr;
    static bool load()
    {
        if (dll != nullptr) return create != nullptr;
        dll = LoadLibraryW (sizeof (void*) == 8 ? L"teVirtualMIDI64.dll" : L"teVirtualMIDI.dll");
        if (dll == nullptr) dll = LoadLibraryW (L"teVirtualMIDI.dll");
        if (dll == nullptr) return false;
        create = (CreateEx2) GetProcAddress (dll, "virtualMIDICreatePortEx2");
        close = (Close) GetProcAddress (dll, "virtualMIDIClosePort");
        send = (Send) GetProcAddress (dll, "virtualMIDISendData");
        return create != nullptr && close != nullptr && send != nullptr;
    }
    static void CALLBACK ignore (void*, LPBYTE, DWORD, DWORD_PTR) {}
    // flags: parse what we send (complete messages only); the port appears to other programs as a MIDI input
    constexpr DWORD PARSE_TX = 2;
}
#endif

static std::atomic<int> instances { 0 };

bool MidiPort::virtualPossible()
{
#if JUCE_WINDOWS
    return te::load();
#else
    return true;
#endif
}

MidiPort::MidiPort() : juce::Thread ("CheatChord MIDI port") { ++instances; }

MidiPort::~MidiPort()
{
    stopThread (500);
    closeAll();
    --instances;
}

std::vector<MidiPort::Choice> MidiPort::choices() const
{
    std::vector<Choice> v;
    v.push_back ({ "", "Off" });
    v.push_back ({ "virtual", virtualPossible() ? "CheatChord (own port)" : "CheatChord (own port: install loopMIDI first)" });
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (! d.name.startsWithIgnoreCase ("CheatChord"))
            v.push_back ({ d.identifier, d.name });
    return v;
}

void MidiPort::closeAll()
{
    const juce::ScopedLock sl (sendLock);
    isOpen = false;
    out = nullptr;
#if JUCE_WINDOWS
    if (te != nullptr && te::close != nullptr) te::close (te);
#endif
    te = nullptr;
}

void MidiPort::select (const juce::String& id)
{
    stopThread (500);
    closeAll();
    current = id;
    err = {};
    portName = {};
    if (id.isEmpty()) return;
    if (id == "virtual")
    {
        const int n = instances.load();
        portName = n > 1 ? "CheatChord " + juce::String (n) : juce::String ("CheatChord");
#if JUCE_WINDOWS
        if (! te::load())
        {
            err = "Windows needs the free loopMIDI driver for an own port: install loopMIDI (tobias-erichsen.de), then choose this again.";
            return;
        }
        te = te::create (portName.toWideCharPointer(), te::ignore, 0, 256, te::PARSE_TX);
        if (te == nullptr)
        {
            err = "The port could not be made (error " + juce::String ((int) GetLastError()) + ").";
            return;
        }
#else
        out = juce::MidiOutput::createNewDevice (portName);
        if (out == nullptr) { err = "The port could not be made."; return; }
#endif
    }
    else
    {
        out = juce::MidiOutput::openDevice (id);
        if (out == nullptr) { err = "This MIDI output could not be opened (another program may hold it)."; return; }
        portName = out->getName();
    }
    fifo.reset();
    isOpen = true;
    startThread (juce::Thread::Priority::high);
}

juce::String MidiPort::status() const
{
    if (err.isNotEmpty()) return err;
    if (! isOpen.load()) return current.isEmpty() ? "Off: the chords go to the DAW's own MIDI routing only." : "Not open.";
    return "Open: in your DAW, set a track's MIDI input to \"" + portName + "\".";
}

void MidiPort::push (double atMs, juce::uint8 a, juce::uint8 b, juce::uint8 c) noexcept
{
    if (! isOpen.load()) return;
    const int st = a & 0xf0;
    if (st == 0x90 && c > 0) sentOn[b & 127] = atMs;
    else if (st == 0x80 || st == 0x90) sentOff[b & 127] = atMs;
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 + n2 < 1) return;   // full: drop rather than block the audio thread
    auto& e = ring[(size_t) (n1 > 0 ? s1 : s2)];
    e.at = atMs;
    e.b[0] = a; e.b[1] = b; e.b[2] = c;
    fifo.finishedWrite (1);
}

bool MidiPort::isEcho (int note, bool on, double nowMs) noexcept
{
    if (! isOpen.load() || note < 0 || note > 127) return false;
    // our own note coming back within half a second (the track listens to "All inputs", or to this port)
    if (on && sentOn[note] > 0 && nowMs - sentOn[note] < 500.0 && nowMs - sentOn[note] > -50.0)
    {
        ++dropOff[note];
        return true;
    }
    if (! on && dropOff[note] > 0) { --dropOff[note]; return true; }
    return false;
}

void MidiPort::sendNow (const juce::uint8* d, int n)
{
    const juce::ScopedLock sl (sendLock);
#if JUCE_WINDOWS
    if (te != nullptr) { te::send (te, (LPBYTE) d, (DWORD) n); return; }
#endif
    if (out != nullptr) out->sendMessageNow (juce::MidiMessage (d, n));
}

void MidiPort::run()
{
    // events in time order: a small sorted list of what is waiting
    std::vector<Ev> waiting;
    waiting.reserve (4096);
    while (! threadShouldExit())
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) waiting.push_back (ring[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) waiting.push_back (ring[(size_t) (s2 + i)]);
        fifo.finishedRead (n1 + n2);
        if (n1 + n2 > 0)
            std::stable_sort (waiting.begin(), waiting.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
        const double now = juce::Time::getMillisecondCounterHiRes();
        size_t k = 0;
        for (; k < waiting.size() && waiting[k].at <= now + 0.5; ++k)
        {
            const int st = waiting[k].b[0] & 0xf0;
            sendNow (waiting[k].b, st == 0xc0 || st == 0xd0 ? 2 : 3);
        }
        if (k > 0) waiting.erase (waiting.begin(), waiting.begin() + (long) k);
        wait (1);
    }
    // nothing hangs: every note still on gets its off
    for (int ch = 0; ch < 16; ++ch)
    {
        juce::uint8 m[3] = { (juce::uint8) (0xb0 | ch), 123, 0 };
        sendNow (m, 3);
    }
}

} // namespace cc
