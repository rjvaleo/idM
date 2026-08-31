// Lists the MIDI ports the machine can see.
//
// idM opens a virtual output so anything on the machine can receive it without
// a cable, and a virtual input so anything can play into it. The names are
// crossed over from the other side: our output is an input to everyone else,
// and our input is one of their outputs. Both lists are printed for that
// reason - looking in only one of them is how you conclude a port is missing
// when it is sitting in the other.

#include <juce_audio_devices/juce_audio_devices.h>

#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String wanted = argc > 1 ? argv[1] : "";
    auto found = false;

    std::printf ("MIDI inputs visible to other applications:\n");

    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        std::printf ("  %s\n", device.name.toRawUTF8());

        if (wanted.isNotEmpty() && device.name.contains (wanted))
            found = true;
    }

    std::printf ("\nMIDI outputs visible to other applications:\n");

    for (const auto& device : juce::MidiOutput::getAvailableDevices())
    {
        std::printf ("  %s\n", device.name.toRawUTF8());

        if (wanted.isNotEmpty() && device.name.contains (wanted))
            found = true;
    }

    if (wanted.isEmpty())
        return 0;

    std::printf ("\n%s  looking for \"%s\"\n", found ? "PASS" : "FAIL", wanted.toRawUTF8());
    return found ? 0 : 1;
}
