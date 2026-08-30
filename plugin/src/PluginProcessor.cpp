#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

MClassicProcessor::MClassicProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      project (mclassic::createDefaultProject())
{
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

    cursors = mclassic::makeCursors (project, 0.0);

    rngs.clear();
    rngs.reserve (project.voices.size());

    for (size_t voice = 0; voice < project.voices.size(); ++voice)
        rngs.push_back (mclassic::Random { project.seed ^ ((uint32_t) (voice + 1) * 0x9e3779b1u) });

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

    // The host's incoming MIDI is not ours to echo. Clearing keeps it from
    // leaving again as if this plugin had generated it.
    midi.clear();

    const auto numSamples = buffer.getNumSamples();

    auto playing = false;
    auto bpm = project.tempo;
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
    mclassic::planWindow (project, cursors, rngs, blockStartSec, blockEndSec,
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

juce::AudioProcessorEditor* MClassicProcessor::createEditor()
{
    return new MClassicEditor (*this);
}

void MClassicProcessor::getStateInformation (juce::MemoryBlock&) {}

void MClassicProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MClassicProcessor();
}
