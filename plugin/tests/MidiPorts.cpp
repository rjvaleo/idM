// Lists the MIDI ports the machine can see.
//
// The standalone opens a virtual output so anything on the machine can receive
// M without a cable. A virtual output appears to everyone else as an input, so
// that is where to look for it.

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

    if (wanted.isEmpty())
        return 0;

    std::printf ("\n%s  looking for \"%s\"\n", found ? "PASS" : "FAIL", wanted.toRawUTF8());
    return found ? 0 : 1;
}
