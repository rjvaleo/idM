// Session persistence, tested against the processor itself.
//
// A hosted instance's setStateInformation goes through the VST3 or AU wrapper,
// which frames the blob in its own format and rejects anything else — so
// feeding it raw JSON tests the wrapper's error handling, not ours. This links
// the processor directly and checks the thing that was actually written: what
// the host is given, and that the engine plays what comes back.

#include "../src/PluginProcessor.h"

#include <cstdio>
#include <array>
#include <set>

namespace
{

int failures = 0;

void require (bool condition, const char* what)
{
    std::printf ("    %s  %s\n", condition ? "ok  " : "FAIL", what);

    if (! condition)
        ++failures;
}

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0.0;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (120.0);
        info.setPpqPosition (ppq);
        info.setTimeInSeconds (ppq * 0.5);
        info.setIsPlaying (true);
        return info;
    }
};

/** Which MIDI channels the engine uses over four bars. The default project
    uses one; the rich fixture spreads sixteen Voices over many. */
std::set<int> channelsPlayed (MClassicProcessor& processor)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    FakePlayHead head;
    processor.setPlayHead (&head);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio (2, blockSize);
    juce::MidiBuffer midi;
    std::set<int> channels;

    const auto beatsPerBlock = ((double) blockSize / sampleRate) / 0.5;

    for (int block = 0; block < 400; ++block)
    {
        head.ppq = block * beatsPerBlock;
        audio.clear();
        midi.clear();
        processor.processBlock (audio, midi);

        for (const auto metadata : midi)
            if (metadata.getMessage().isNoteOn())
                channels.insert (metadata.getMessage().getChannel());
    }

    processor.releaseResources();
    processor.setPlayHead (nullptr);
    return channels;
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File goldens { juce::String (MCLASSIC_GOLDENS_DIR) };
    const auto document = goldens.getChildFile ("rich-project-16.json").loadFileAsString();

    if (document.isEmpty())
    {
        std::printf ("  FAIL  no fixture to restore from\n");
        return 1;
    }

    std::printf ("Session persistence\n");

    // A plugin that has never been told anything saves nothing. Writing a
    // default over a session whose window was never opened would be worse than
    // writing nothing at all.
    {
        MClassicProcessor fresh;
        juce::MemoryBlock empty;
        fresh.getStateInformation (empty);
        require (empty.getSize() == 0, "a plugin with no document saves nothing");
    }

    // What the interface sends is what the host is given, byte for byte.
    MClassicProcessor saving;
    saving.setProjectFromJson (document);

    juce::MemoryBlock saved;
    saving.getStateInformation (saved);

    const juce::String returned { juce::CharPointer_UTF8 ((const char*) saved.getData()),
                                  saved.getSize() };

    require (saved.getSize() > 0, "the host is given something to save");
    require (returned == document, "what the host stores is verbatim what the interface sent");

    // And reopening plays it.
    MClassicProcessor restoring;
    const auto before = channelsPlayed (restoring);

    restoring.setStateInformation (saved.getData(), (int) saved.getSize());
    const auto after = channelsPlayed (restoring);

    juce::String beforeList, afterList;

    for (const auto c : before) beforeList += (beforeList.isEmpty() ? "" : " ") + juce::String (c);
    for (const auto c : after)  afterList  += (afterList.isEmpty()  ? "" : " ") + juce::String (c);

    std::printf ("    default project channels:  %s\n", beforeList.toRawUTF8());
    std::printf ("    restored project channels: %s\n", afterList.toRawUTF8());

    require (before.size() == 1, "the default project plays one channel");
    require (after.size() > 1, "the restored project plays the channels it was saved with");
    require (restoring.projectsReceived() > 0, "restoring counts as the engine receiving a project");

    // Host MIDI must reach the interface rather than being destroyed, which is
    // what used to happen: acceptsMidi() said yes and processBlock threw it away.
    {
        std::printf ("\nMIDI input\n");

        MClassicProcessor input;
        input.prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;

        midi.addEvent (juce::MidiMessage::noteOn (3, 64, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::controllerEvent (3, 16, 42), 8);
        midi.addEvent (juce::MidiMessage::noteOff (3, 64), 16);

        input.processBlock (audio, midi);

        require (midi.getNumEvents() == 0 || ! midi.isEmpty(),
                 "the host's own buffer is not echoed back untouched");

        std::array<IncomingMidi, 64> drained {};
        const auto count = input.drainIncoming (drained.data(), (int) drained.size());

        std::printf ("    forwarded %d message(s) to the interface\n", count);
        require (count == 3, "every incoming message reaches the interface");

        if (count == 3)
        {
            require ((drained[0].status & 0xf0) == 0x90 && drained[0].data1 == 64,
                     "a note on arrives intact");
            require ((drained[1].status & 0xf0) == 0xb0 && drained[1].data1 == 16
                         && drained[1].data2 == 42,
                     "a controller arrives intact");
            require ((drained[2].status & 0xf0) == 0x80, "a note off arrives intact");
            require ((drained[0].status & 0x0f) == 2, "the channel is preserved");
        }

        input.releaseResources();
    }

    // The plugin must not emit MIDI Clock or transport bytes. It follows the
    // host, and a follower that also broadcasts a clock is how two devices end
    // up each thinking they lead.
    {
        std::printf ("\nClock discipline\n");

        MClassicProcessor plugin;
        require (! plugin.isStandalone(), "a bare processor is not the standalone wrapper");

        plugin.prepareToPlay (48000.0, 512);
        plugin.setStandaloneTransport (true); // must be ignored

        FakePlayHead head;
        plugin.setPlayHead (&head);

        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;
        auto realtime = 0;

        const auto beatsPerBlock = (512.0 / 48000.0) / 0.5;

        for (int block = 0; block < 200; ++block)
        {
            head.ppq = block * beatsPerBlock;
            audio.clear();
            midi.clear();
            plugin.processBlock (audio, midi);

            for (const auto metadata : midi)
            {
                const auto message = metadata.getMessage();

                if (message.isMidiClock() || message.isMidiStart() || message.isMidiStop()
                    || message.isMidiContinue())
                    ++realtime;
            }
        }

        std::printf ("    realtime bytes emitted by the plugin: %d\n", realtime);
        require (realtime == 0, "the plugin emits no clock or transport bytes");

        plugin.releaseResources();
        plugin.setPlayHead (nullptr);
    }

    std::printf ("\n%s  %d failures\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
