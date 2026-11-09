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

// port names in use in this process: "CheatChord", "CheatChord 2"... (a closed one frees its name)
static juce::CriticalSection namesLock;
static bool nameUsed[17] {};

bool MidiPort::virtualPossible()
{
#if JUCE_WINDOWS
    return te::load();
#else
    return true;
#endif
}

MidiPort::MidiPort() : juce::Thread ("CheatChord MIDI port") {}

MidiPort::~MidiPort()
{
    stopThread (500);
    closeAll();
}

std::vector<MidiPort::Choice> MidiPort::choices() const
{
    std::vector<Choice> v;
    v.push_back ({ "off", "Off" });
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
    if (nameSlot > 0) { const juce::ScopedLock nl (namesLock); nameUsed[nameSlot] = false; nameSlot = 0; }
}

void MidiPort::select (const juce::String& id)
{
    stopThread (500);
    closeAll();
    current = id;
    err = {};
    portName = {};
    if (id.isEmpty() || id == "off") return;
    if (id == "virtual")
    {
#if JUCE_WINDOWS
        if (! te::load())
        {
            err = "Windows needs the free loopMIDI driver for an own port: install loopMIDI (tobias-erichsen.de), then choose this again.";
            return;
        }
#endif
        for (int k = 1; k <= 16 && ! (te != nullptr || out != nullptr); ++k)
        {
            {
                const juce::ScopedLock nl (namesLock);
                if (nameUsed[k]) continue;
                nameUsed[k] = true;
            }
            nameSlot = k;
            portName = k > 1 ? "CheatChord " + juce::String (k) : juce::String ("CheatChord");
#if JUCE_WINDOWS
            te = te::create (portName.toWideCharPointer(), te::ignore, 0, 256, te::PARSE_TX);
            if (te == nullptr) err = "The port could not be made (error " + juce::String ((int) GetLastError()) + ").";
#else
            out = juce::MidiOutput::createNewDevice (portName);
            if (out == nullptr) err = "The port could not be made.";
#endif
            if (te == nullptr && out == nullptr)
            {
                const juce::ScopedLock nl (namesLock);
                nameUsed[k] = false;   // another program may hold this name: try the next one
                nameSlot = 0;
            }
        }
        if (te == nullptr && out == nullptr) return;
        err = {};
    }
    else
    {
        out = juce::MidiOutput::openDevice (id);
        if (out == nullptr) { err = "This MIDI output could not be opened (another program may hold it)."; return; }
        portName = out->getName();
    }
    fifo.reset();
    nOn = 0; maxAtOnce = 0; nEcho = 0; holding = 0;
    for (auto& c : held) for (auto& x : c) x = 0;
    isOpen = true;
    startThread (juce::Thread::Priority::high);
}

juce::String MidiPort::status() const
{
    if (err.isNotEmpty()) return err;
    if (! isOpen.load()) return current.isEmpty() || current == "off" ? "Off: the chords go to the DAW's own MIDI routing only." : "Not open.";
    juce::String t = "Open: in your DAW, set a track's MIDI input to \"" + portName + "\".";
    if (nOn.load() > 0)
        t << "  Sent " << nOn.load() << " notes, up to " << maxAtOnce.load() << " at once"
          << (nEcho.load() > 0 ? ", " + juce::String (nEcho.load()) + " echoes dropped" : juce::String()) << ".";
    return t;
}

void MidiPort::push (double atMs, juce::uint8 a, juce::uint8 b, juce::uint8 c) noexcept
{
    if (! isOpen.load()) return;
    const int st = a & 0xf0;
    if (st == 0x90 && c > 0) { sentOn[b & 127] = atMs; sentKey[b & 127] = (int) ((a & 15) << 7 | (c & 127)); }
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 + n2 < 1) return;   // full: drop rather than block the audio thread
    auto& e = ring[(size_t) (n1 > 0 ? s1 : s2)];
    e.at = atMs;
    e.b[0] = a; e.b[1] = b; e.b[2] = c;
    fifo.finishedWrite (1);
}

bool MidiPort::isEcho (int ch, int note, int vel, bool on, double nowMs) noexcept
{
    if (! isOpen.load() || note < 0 || note > 127) return false;
    // our own note coming back (the CheatChord track listens to "All inputs", or to this port): the same note on the
    // same channel with the same velocity, a moment after we sent it. A pad hit is not taken for one: it comes on its
    // own channel and velocity, and a real loop comes back within a few audio blocks.
    if (on && sentOn[note] > 0 && sentKey[note] == ((ch & 15) << 7 | (vel & 127))
        && nowMs - sentOn[note] < 250.0 && nowMs - sentOn[note] > -50.0)
    {
        ++dropOff[note];
        ++nEcho;
        return true;
    }
    if (! on && dropOff[note] > 0) { --dropOff[note]; return true; }
    return false;
}

void MidiPort::sendNow (const juce::uint8* d, int n)
{
    const juce::ScopedLock sl (sendLock);
    if (n >= 3)
    {
        const int st = d[0] & 0xf0, ch = d[0] & 15, nt = d[1] & 127;
        if (st == 0x90 && d[2] > 0) { ++nOn; if (held[ch][nt]++ == 0) ++holding; if (holding > maxAtOnce) maxAtOnce = holding; }
        else if ((st == 0x80 || st == 0x90) && held[ch][nt] > 0) { if (--held[ch][nt] == 0) --holding; }
        else if (st == 0xb0 && (d[1] == 123 || d[1] == 120)) { for (auto& x : held[ch]) { if (x > 0) --holding; x = 0; } }
    }
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
