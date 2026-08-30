#include "PluginProcessor.h"
#if ! MCLASSIC_NO_EDITOR
 #include "PluginEditor.h"
#endif

#include "ProjectJson.h"

#include <cmath>

MClassicProcessor::MClassicProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    projects[0] = mclassic::createDefaultProject();
    projects[1] = projects[0];

    planned.reserve (1024);
    nextCursors.reserve (16);
    steps.reserve (1024);
    rewind (0.0);
}

void MClassicProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    wasPlaying = false;
    rewind (0.0);
}

void MClassicProcessor::releaseResources()
{
}

/** Put the engine back to the top and anchor its clock to `ppq`.

    Called on transport start and on any discontinuity — a loop wrap or a
    locate. The RNGs are re-seeded exactly as `traceProject` seeds them, so a
    performance is reproducible from its seed rather than from how long the
    transport happened to be running.
*/
void MClassicProcessor::rewind (double ppq)
{
    originPpq = ppq;
    lastPpq = ppq;

    cursors = mclassic::makeCursors (project(), 0.0);

    rngs.clear();
    rngs.reserve (project().voices.size());

    for (size_t voice = 0; voice < project().voices.size(); ++voice)
        rngs.push_back (mclassic::Random { project().seed ^ ((uint32_t) (voice + 1) * 0x9e3779b1u) });

    lifecycle.reset();
}

void MClassicProcessor::allNotesOff (juce::MidiBuffer& midi, int samplePosition)
{
    // Close what we opened, by name, before the blunt instrument. Hardware and
    // plenty of software ignore CC123; an explicit note off is never ignored.
    for (auto& slot : sounding)
    {
        if (slot.channel == 0)
            continue;

        midi.addEvent (juce::MidiMessage::noteOff (slot.channel, slot.note), samplePosition);
        slot = {};
    }

    for (int channel = 1; channel <= 16; ++channel)
    {
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, 64, 0), samplePosition);
        midi.addEvent (juce::MidiMessage::controllerEvent (channel, 121, 0), samplePosition);
        midi.addEvent (juce::MidiMessage::allNotesOff (channel), samplePosition);
    }
}

void MClassicProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // Take what the host sent before clearing. It is not ours to echo — the
    // buffer is where our own notes go — but destroying it would take M's whole
    // Input Control System with it, which is what used to happen here.
    if (! midi.isEmpty())
    {
        const auto scope = incomingFifo.write (midi.getNumEvents());
        auto written = 0;

        for (const auto metadata : midi)
        {
            if (written >= scope.blockSize1 + scope.blockSize2)
                break; // the interface is not draining; drop rather than stall

            const auto* raw = metadata.data;
            const auto bytes = metadata.numBytes;

            if (bytes < 1 || bytes > 3)
                continue; // SysEx is not forwarded

            const IncomingMidi message { (uint8_t) raw[0],
                                         (uint8_t) (bytes > 1 ? raw[1] : 0),
                                         (uint8_t) (bytes > 2 ? raw[2] : 0) };

            const auto index = written < scope.blockSize1
                ? scope.startIndex1 + written
                : scope.startIndex2 + (written - scope.blockSize1);

            incoming[(size_t) index] = message;
            ++written;
        }
    }

    midi.clear();

    // Take a waiting edit at a block boundary, never mid-block. Swapping the
    // live half is a single assignment; nothing is copied on this thread.
    if (projectPending.load (std::memory_order_acquire))
    {
        liveProject = 1 - liveProject;
        projectPending.store (false, std::memory_order_release);
        rewind (lastPpq);
    }

    const auto numSamples = buffer.getNumSamples();

    auto playing = false;
    auto bpm = project().tempo;
    auto ppq = lastPpq;

    if (auto* head = getPlayHead())
    {
        if (const auto position = head->getPosition())
        {
            playing = position->getIsPlaying();

            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;

            if (const auto hostPpq = position->getPpqPosition())
                ppq = *hostPpq;
        }
    }

    const auto started = playing && ! wasPlaying;
    const auto jumped = playing && std::abs (ppq - lastPpq) > 1.0;

    if ((wasPlaying && ! playing) || started || jumped)
    {
        allNotesOff (midi, 0);

        if (playing)
            rewind (ppq);
    }

    wasPlaying = playing;

    if (! playing)
    {
        lastPpq = ppq;
        return;
    }

    // The engine keeps its own clock in seconds, zeroed where the transport
    // started. The host's tempo turns beats into that.
    const auto secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);
    const auto blockStartSec = (ppq - originPpq) * secondsPerBeat;
    const auto blockEndSec = blockStartSec + (double) numSamples / sampleRate;

    // M's own planner decides what plays. This is the whole point of the port.
    mclassic::planWindow (project(), cursors, rngs, blockStartSec, blockEndSec,
                          planned, nextCursors, steps);
    cursors = nextCursors;

    lifecycle.ingest (planned, destinations);

    for (const auto& event : lifecycle.drainBefore (blockEndSec))
    {
        auto offset = (int) std::floor ((event.atSec - blockStartSec) * sampleRate);
        offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), offset);

        const auto channel = juce::jlimit (1, 16, event.channel);
        const auto note = juce::jlimit (0, 127, event.note);

        if (event.kind == mclassic::EventKind::noteOn)
        {
            midi.addEvent (juce::MidiMessage::noteOn (channel, note,
                                                      (juce::uint8) juce::jlimit (0, 127, event.velocity)),
                           offset);
            emitted.fetch_add (1, std::memory_order_relaxed);

            for (auto& slot : sounding)
            {
                if (slot.channel != 0)
                    continue;

                slot = { channel, note };
                break;
            }
        }
        else if (event.kind == mclassic::EventKind::noteOff)
        {
            midi.addEvent (juce::MidiMessage::noteOff (channel, note), offset);

            for (auto& slot : sounding)
            {
                if (slot.channel == channel && slot.note == note)
                {
                    slot = {};
                    break;
                }
            }
        }
        else
        {
            midi.addEvent (juce::MidiMessage::programChange (channel,
                                                             juce::jlimit (0, 127, event.program)),
                           offset);
        }
    }

    lastPpq = ppq;
}

void MClassicProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();

    // Bypass must not strand a note. It is one of the four ways a plugin leaves
    // something sounding forever.
    allNotesOff (midi, 0);
}

int MClassicProcessor::drainIncoming (IncomingMidi* destination, int capacity)
{
    const auto ready = juce::jmin (capacity, incomingFifo.getNumReady());

    if (ready <= 0)
        return 0;

    const auto scope = incomingFifo.read (ready);
    auto written = 0;

    for (int i = 0; i < scope.blockSize1; ++i)
        destination[written++] = incoming[(size_t) (scope.startIndex1 + i)];

    for (int i = 0; i < scope.blockSize2; ++i)
        destination[written++] = incoming[(size_t) (scope.startIndex2 + i)];

    return written;
}

void MClassicProcessor::setProjectFromJson (const juce::String& json)
{
    // A change is already waiting; the newer state wins, so overwrite the same
    // spare half rather than queueing. The interface sends whole projects, so
    // nothing is lost by dropping an intermediate one.
    const auto spare = 1 - liveProject;
    projects[spare] = mclassic::projectFromJson (juce::JSON::parse (json));

    // Kept verbatim. What the host stores is exactly what the interface
    // produced, so a round trip cannot lose a field this port does not read.
    documentJson = json;

    projectPending.store (true, std::memory_order_release);
    received.fetch_add (1, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* MClassicProcessor::createEditor()
{
   #if MCLASSIC_NO_EDITOR
    // The state test links the processor without the webview, which would drag
    // in the whole UI bundle for a check that never opens a window.
    return nullptr;
   #else
    return new MClassicEditor (*this);
   #endif
}

void MClassicProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    // Empty until the interface has sent something. Saving a default project
    // over a session that never opened its window would be worse than saving
    // nothing.
    if (documentJson.isEmpty())
        return;

    destination.replaceAll (documentJson.toRawUTF8(),
                            (size_t) documentJson.getNumBytesAsUTF8());
}

void MClassicProcessor::setStateInformation (const void* data, int size)
{
    if (data == nullptr || size <= 0)
        return;

    const juce::String json { juce::CharPointer_UTF8 (static_cast<const char*> (data)),
                              (size_t) size };

    if (json.isEmpty())
        return;

    setProjectFromJson (json);

    // The engine has it; the interface has not. The editor may not exist yet —
    // a host restores state before opening the window, and often never opens it
    // at all — so this is a flag rather than a call.
    restoredPending.store (true, std::memory_order_release);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MClassicProcessor();
}
