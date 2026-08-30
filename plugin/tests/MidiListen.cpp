// Listens on a MIDI port and reports what arrives.
//
// The standalone opens a virtual output; this opens it from the outside, which
// is the only way to show that notes actually leave the process rather than
// merely being written into a buffer.

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <cstdio>

namespace
{

struct Counter : juce::MidiInputCallback
{
    std::atomic<int> noteOn { 0 };
    std::atomic<int> noteOff { 0 };
    std::atomic<int> clock { 0 };
    std::atomic<int> start { 0 };
    std::atomic<int> stop { 0 };

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
    {
        if (message.isNoteOn())          ++noteOn;
        else if (message.isNoteOff())    ++noteOff;
        else if (message.isMidiClock())  ++clock;
        else if (message.isMidiStart())  ++start;
        else if (message.isMidiStop())   ++stop;
    }
};

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String wanted = argc > 1 ? argv[1] : "M Classic";
    const auto seconds = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 5.0;

    juce::MidiDeviceInfo target;

    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (device.name.contains (wanted))
            target = device;

    if (target.identifier.isEmpty())
    {
        std::printf ("FAIL  no MIDI port matching \"%s\"\n", wanted.toRawUTF8());
        return 1;
    }

    Counter counter;
    auto input = juce::MidiInput::openDevice (target.identifier, &counter);

    if (input == nullptr)
    {
        std::printf ("FAIL  could not open \"%s\"\n", target.name.toRawUTF8());
        return 1;
    }

    input->start();
    std::printf ("listening on \"%s\" for %.1fs\n", target.name.toRawUTF8(), seconds);
    juce::Thread::sleep ((int) (seconds * 1000.0));
    input->stop();

    std::printf ("  note-on %d  note-off %d  clock %d  start %d  stop %d\n",
                 counter.noteOn.load(), counter.noteOff.load(), counter.clock.load(),
                 counter.start.load(), counter.stop.load());

    const auto ok = counter.noteOn.load() > 0;
    std::printf ("%s  notes leaving the standalone\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
