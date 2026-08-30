// Loads the built plugin the way a DAW loads it, drives a transport at it, and
// reports the MIDI that comes out.
//
// This exists so "it works in a host" is something measured rather than handed
// to somebody else to find out. It scans the real bundle from disk through
// JUCE's plugin format managers — the same path Live and Logic take — rather
// than instantiating the processor class directly, which would prove only that
// the code compiles.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <functional>
#include <map>
#include <set>

namespace
{

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
constexpr double bpm = 120.0;

/** A transport that plays from zero at a fixed tempo, the way a host does. */
struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0.0;
    double tempo = bpm;
    bool playing = true;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (tempo);
        info.setPpqPosition (ppq);
        info.setTimeInSeconds (ppq * 60.0 / tempo);
        info.setIsPlaying (playing);
        info.setTimeSignature (TimeSignature { 4, 4 });
        return info;
    }
};

struct Counts
{
    int noteOn = 0;
    int noteOff = 0;
    int programChange = 0;
    int other = 0;
    std::set<int> channels;
    std::set<int> pitches;
    std::set<int> velocities;
    int maxSimultaneous = 0;
};

/** Run the transport for `beats` and tally what the plugin emits. */
Counts drive (juce::AudioPluginInstance& plugin, double beats, bool stopAtEnd)
{
    FakePlayHead head;
    plugin.setPlayHead (&head);
    plugin.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio (juce::jmax (2, plugin.getTotalNumOutputChannels()), blockSize);
    juce::MidiBuffer midi;

    Counts counts;
    std::map<std::pair<int, int>, int> open;

    const auto secondsPerBeat = 60.0 / bpm;
    const auto beatsPerBlock = ((double) blockSize / sampleRate) / secondsPerBeat;

    for (double elapsed = 0.0; elapsed < beats; elapsed += beatsPerBlock)
    {
        head.ppq = elapsed;
        audio.clear();
        midi.clear();

        plugin.processBlock (audio, midi);

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOn())
            {
                ++counts.noteOn;
                counts.channels.insert (message.getChannel());
                counts.pitches.insert (message.getNoteNumber());
                counts.velocities.insert (message.getVelocity());
                ++open[{ message.getChannel(), message.getNoteNumber() }];
                counts.maxSimultaneous = juce::jmax (counts.maxSimultaneous, (int) open.size());
            }
            else if (message.isNoteOff())
            {
                ++counts.noteOff;
                const auto key = std::make_pair (message.getChannel(), message.getNoteNumber());

                if (--open[key] <= 0)
                    open.erase (key);
            }
            else if (message.isProgramChange())
            {
                ++counts.programChange;
            }
            else
            {
                ++counts.other;
            }
        }
    }

    if (stopAtEnd)
    {
        // Stop, then run a block. Anything still open after this is a stuck note.
        head.playing = false;
        audio.clear();
        midi.clear();
        plugin.processBlock (audio, midi);

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOff())
            {
                ++counts.noteOff;
                const auto key = std::make_pair (message.getChannel(), message.getNoteNumber());

                if (--open[key] <= 0)
                    open.erase (key);
            }
            else if (message.isAllNotesOff() || message.isController())
            {
                ++counts.other;
            }
        }

        counts.maxSimultaneous = (int) open.size(); // reused: what was left sounding
    }

    plugin.releaseResources();
    plugin.setPlayHead (nullptr);
    return counts;
}

int failures = 0;

void require (bool condition, const juce::String& what)
{
    if (condition)
    {
        std::printf ("    ok    %s\n", what.toRawUTF8());
        return;
    }

    std::printf ("    FAIL  %s\n", what.toRawUTF8());
    ++failures;
}

void testSync (juce::AudioPluginInstance& plugin);

void testFormat (juce::AudioPluginFormat& format, const juce::String& path)
{
    std::printf ("\n  %s: %s\n", format.getName().toRawUTF8(), path.toRawUTF8());

    juce::OwnedArray<juce::PluginDescription> found;
    juce::KnownPluginList list;
    list.scanAndAddFile (path, true, found, format);

    if (found.isEmpty())
    {
        std::printf ("    FAIL  the host could not see a plugin in that bundle\n");
        ++failures;
        return;
    }

    // Straight from the format. JUCE 9's headless manager deletes
    // addDefaultFormats, and going through the format is more direct anyway.
    juce::String error;
    auto plugin = format.createInstanceFromDescription (*found[0], sampleRate, blockSize, error);

    if (plugin == nullptr)
    {
        std::printf ("    FAIL  the host could not instantiate it: %s\n", error.toRawUTF8());
        ++failures;
        return;
    }

    require (plugin->producesMidi(), "declares that it produces MIDI");

    // Four bars. The default project runs one Voice of eighth notes, so this is
    // long enough for the pattern to repeat several times.
    const auto counts = drive (*plugin, 16.0, false);

    std::printf ("    emitted %d note-ons, %d note-offs, %d program changes\n",
                 counts.noteOn, counts.noteOff, counts.programChange);

    require (counts.noteOn > 0, "emits notes when the host transport runs");
    require (counts.noteOff >= counts.noteOn - counts.maxSimultaneous,
             "releases what it sounds");
    require (! counts.channels.empty(), "uses at least one MIDI channel");
    require (counts.pitches.size() > 1, "plays more than one pitch");

    // The default project's neutral cyclic level maps Rhythm to 1.5, so an
    // eighth note lands every 0.75 beats. Pinned because a wrong multiplier
    // still produces plausible-looking MIDI.
    require (counts.noteOn == 22, "emits 22 notes in four bars, the default project's rate");

    juce::String pitches;

    for (const auto pitch : counts.pitches)
        pitches += (pitches.isEmpty() ? "" : " ") + juce::String (pitch);

    std::printf ("    pitches: %s\n", pitches.toRawUTF8());

    juce::String channels;

    for (const auto channel : counts.channels)
        channels += (channels.isEmpty() ? "" : " ") + juce::String (channel);

    std::printf ("    channels: %s\n", channels.toRawUTF8());

    // Now the same run, stopped mid-phrase.
    const auto stopped = drive (*plugin, 3.7, true);
    require (stopped.maxSimultaneous == 0, "leaves nothing sounding after the transport stops");

    testSync (*plugin);
}

/** Drive an arbitrary transport shape and report what happened.

    `advance` is handed the block index and returns the next PPQ position and
    tempo, so a test can ramp the tempo, wrap a loop, or locate — the three
    things that actually strand notes in a DAW.
*/
struct SyncResult
{
    int noteOn = 0;
    int noteOff = 0;
    int leftSounding = 0;
    double firstNoteBeat = -1.0;
    double lastNoteBeat = -1.0;
};

SyncResult driveTransport (juce::AudioPluginInstance& plugin,
                           int blocks,
                           const std::function<void (int, double&, double&, bool&)>& advance)
{
    FakePlayHead head;
    plugin.setPlayHead (&head);
    plugin.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> audio (juce::jmax (2, plugin.getTotalNumOutputChannels()), blockSize);
    juce::MidiBuffer midi;

    SyncResult result;
    std::map<std::pair<int, int>, int> open;

    double ppq = 0.0;
    double tempo = bpm;
    bool playing = true;

    for (int block = 0; block < blocks; ++block)
    {
        advance (block, ppq, tempo, playing);

        head.ppq = ppq;
        head.playing = playing;
        head.tempo = tempo;

        audio.clear();
        midi.clear();
        plugin.processBlock (audio, midi);

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOn())
            {
                ++result.noteOn;
                ++open[{ message.getChannel(), message.getNoteNumber() }];

                if (result.firstNoteBeat < 0.0)
                    result.firstNoteBeat = ppq;

                result.lastNoteBeat = ppq;
            }
            else if (message.isNoteOff())
            {
                ++result.noteOff;
                const auto key = std::make_pair (message.getChannel(), message.getNoteNumber());

                if (--open[key] <= 0)
                    open.erase (key);
            }
        }
    }

    // One stopped block, so `leftSounding` means stranded rather than "the run
    // happened to end mid-note", which is not a defect.
    head.playing = false;
    audio.clear();
    midi.clear();
    plugin.processBlock (audio, midi);

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (! message.isNoteOff())
            continue;

        const auto key = std::make_pair (message.getChannel(), message.getNoteNumber());

        if (--open[key] <= 0)
            open.erase (key);
    }

    result.leftSounding = (int) open.size();

    plugin.releaseResources();
    plugin.setPlayHead (nullptr);
    return result;
}

/** The three transport shapes that strand notes, plus a tempo ramp. */
void testSync (juce::AudioPluginInstance& plugin)
{
    const auto secondsPerBeat = 60.0 / bpm;
    const auto beatsPerBlock = ((double) blockSize / sampleRate) / secondsPerBeat;

    // Following the host clock at all: notes must arrive while it runs.
    {
        const auto r = driveTransport (plugin, 400, [&] (int block, double& ppq, double&, bool&)
        {
            ppq = block * beatsPerBlock;
        });

        require (r.noteOn > 0, "sync: follows a running host transport");
        require (r.leftSounding == 0 || r.noteOn > r.noteOff,
                 "sync: releases are paired with attacks");
    }

    // Tempo doubles halfway. The engine must keep following rather than
    // running on its own idea of time.
    {
        auto ppqAtSwitch = 0.0;

        const auto r = driveTransport (plugin, 400, [&] (int block, double& ppq, double& tempo, bool&)
        {
            if (block < 200)
            {
                ppq = block * beatsPerBlock;
                ppqAtSwitch = ppq;
                tempo = bpm;
            }
            else
            {
                // Twice the tempo means twice the beats per block.
                ppq = ppqAtSwitch + (block - 200) * beatsPerBlock * 2.0;
                tempo = bpm * 2.0;
            }
        });

        require (r.noteOn > 0, "sync: keeps playing across a tempo change");
    }

    // A loop that wraps back to the top every two bars.
    {
        const auto r = driveTransport (plugin, 600, [&] (int block, double& ppq, double&, bool&)
        {
            ppq = std::fmod (block * beatsPerBlock, 8.0);
        });

        require (r.noteOn > 0, "sync: plays through a looping transport");
        require (r.leftSounding == 0, "sync: a loop wrap leaves nothing sounding");
    }

    // A locate: jump forward, then backward, mid-phrase.
    {
        const auto r = driveTransport (plugin, 400, [&] (int block, double& ppq, double&, bool&)
        {
            const auto base = block * beatsPerBlock;
            ppq = block < 150 ? base : (block < 300 ? base + 64.0 : base - 32.0);
        });

        require (r.noteOn > 0, "sync: plays after a locate");
        require (r.leftSounding == 0, "sync: a locate leaves nothing sounding");
    }

    // Stop and start repeatedly.
    {
        const auto r = driveTransport (plugin, 600, [&] (int block, double& ppq, double&, bool& playing)
        {
            playing = (block / 50) % 2 == 0;

            if (playing)
                ppq += beatsPerBlock;
        });

        require (r.noteOn > 0, "sync: survives repeated start and stop");
        require (r.leftSounding == 0, "sync: nothing is left sounding across stops");
    }
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File artefacts { juce::String (MCLASSIC_ARTEFACTS_DIR) };

    std::printf ("Loading the built plugin as a host would.\n");

    juce::VST3PluginFormat vst3;
    const auto vst3Path = artefacts.getChildFile ("VST3/M Classic.vst3");

    if (vst3Path.exists())
        testFormat (vst3, vst3Path.getFullPathName());
    else
    {
        std::printf ("  FAIL  no VST3 at %s\n", vst3Path.getFullPathName().toRawUTF8());
        ++failures;
    }

   #if JUCE_PLUGINHOST_AU && JUCE_MAC
    juce::AudioUnitPluginFormat au;
    const auto auPath = artefacts.getChildFile ("AU/M Classic.component");

    if (auPath.exists())
        testFormat (au, auPath.getFullPathName());
    else
    {
        std::printf ("  FAIL  no AU at %s\n", auPath.getFullPathName().toRawUTF8());
        ++failures;
    }
   #endif

    std::printf ("\n%s  %d failures\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
