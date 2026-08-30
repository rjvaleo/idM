#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "../engine/StepSource.h"

#include <cmath>



MClassicProcessor::MClassicProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MClassicProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    lastPpq = 0.0;
    wasPlaying = false;
    sounding.fill ({});
}

void MClassicProcessor::releaseResources()
{
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

void MClassicProcessor::closeNotesDueBefore (juce::MidiBuffer& midi, double ppqEnd,
                                             double ppqStart, double samplesPerPpq,
                                             int numSamples)
{
    for (auto& slot : sounding)
    {
        if (slot.channel == 0 || slot.offPpq >= ppqEnd)
            continue;

        auto offset = (int) std::floor ((slot.offPpq - ppqStart) * samplesPerPpq);
        offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), offset);

        midi.addEvent (juce::MidiMessage::noteOff (slot.channel, slot.note), offset);
        slot = {};
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
    auto bpm = 120.0;
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

    // Stopped, or the transport jumped — a loop wrap or a locate. Either way
    // every sounding note has to be released, because the position it was going
    // to be released at no longer arrives.
    const auto jumped = std::abs (ppq - lastPpq) > 1.0;

    if ((wasPlaying && ! playing) || (playing && jumped))
    {
        allNotesOff (midi, 0);
        lastPpq = ppq;
    }

    wasPlaying = playing;

    if (! playing)
    {
        lastPpq = ppq;
        return;
    }

    const auto secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);
    const auto samplesPerPpq = sampleRate * secondsPerBeat;
    const auto blockPpq = (double) numSamples / samplesPerPpq;
    const auto ppqStart = ppq;
    const auto ppqEnd = ppq + blockPpq;

    closeNotesDueBefore (midi, ppqEnd, ppqStart, samplesPerPpq, numSamples);

    // What plays in this span is a pure question, answered away from the audio
    // thread's concerns. This is the seam the ported engine replaces.
    pending.clear();
    steps.collect (ppqStart, ppqEnd, pending);

    for (const auto& event : pending)
    {
        if (! event.isOn)
            continue;

        auto offset = (int) std::floor ((event.atPpq - ppqStart) * samplesPerPpq);
        offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), offset);

        midi.addEvent (juce::MidiMessage::noteOn (event.channel, event.note,
                                                  (juce::uint8) event.velocity),
                       offset);
        emitted.fetch_add (1, std::memory_order_relaxed);

        for (auto& slot : sounding)
        {
            if (slot.channel != 0)
                continue;

            slot = { event.channel, event.note,
                     event.atPpq + mclassic::StepSource::stepPpq * mclassic::StepSource::gateFraction };
            break;
        }
    }

    lastPpq = ppqEnd;
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
