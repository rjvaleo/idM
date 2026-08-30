#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>

MClassicProcessor::MClassicProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    pending.reserve (queueCapacity);
}

void MClassicProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    hostSec = 0.0;
    pending.clear();
    lateEvents.store (0, std::memory_order_relaxed);
}

void MClassicProcessor::releaseResources()
{
    clearPlanned();
}

void MClassicProcessor::submitPlanned (const std::vector<PlannedMidi>& events)
{
    if (events.empty())
        return;

    const auto scope = fifo.write ((int) events.size());
    int written = 0;

    for (int i = 0; i < scope.blockSize1; ++i)
        queue[(size_t) (scope.startIndex1 + i)] = events[(size_t) written++];

    for (int i = 0; i < scope.blockSize2; ++i)
        queue[(size_t) (scope.startIndex2 + i)] = events[(size_t) written++];

    // A full queue means the UI ran far ahead of the audio thread. Count the
    // shortfall rather than pretending everything landed.
    if (written < (int) events.size())
        lateEvents.fetch_add ((int) events.size() - written, std::memory_order_relaxed);
}

void MClassicProcessor::clearPlanned()
{
    panicRequested.store (true, std::memory_order_release);
}

HostTransport MClassicProcessor::transport() const
{
    return { publishedSec.load (std::memory_order_relaxed),
             publishedBpm.load (std::memory_order_relaxed),
             publishedPpq.load (std::memory_order_relaxed),
             publishedPlaying.load (std::memory_order_relaxed) };
}

void MClassicProcessor::sendAllNotesOff (juce::MidiBuffer& midi, int samplePosition)
{
    for (int channel = 1; channel <= 16; ++channel)
        midi.addEvent (juce::MidiMessage::allNotesOff (channel), samplePosition);
}

void MClassicProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // Incoming MIDI is the host's, not ours; the UI reads it through its own
    // path. Clearing keeps it from being echoed back out as if we made it.
    midi.clear();

    const auto numSamples = buffer.getNumSamples();
    const auto blockStart = hostSec;
    const auto blockEnd = blockStart + (double) numSamples / sampleRate;

    // ---- what the host is doing -------------------------------------------
    auto playing = false;
    auto bpm = publishedBpm.load (std::memory_order_relaxed);
    auto ppq = publishedPpq.load (std::memory_order_relaxed);

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

    publishedSec.store (blockStart, std::memory_order_relaxed);
    publishedBpm.store (bpm, std::memory_order_relaxed);
    publishedPpq.store (ppq, std::memory_order_relaxed);
    publishedPlaying.store (playing, std::memory_order_relaxed);

    // ---- take anything the UI has planned since the last block ------------
    const auto ready = fifo.getNumReady();

    if (ready > 0)
    {
        const auto scope = fifo.read (ready);

        for (int i = 0; i < scope.blockSize1; ++i)
            pending.push_back (queue[(size_t) (scope.startIndex1 + i)]);

        for (int i = 0; i < scope.blockSize2; ++i)
            pending.push_back (queue[(size_t) (scope.startIndex2 + i)]);

        std::stable_sort (pending.begin(), pending.end(),
                          [] (const PlannedMidi& a, const PlannedMidi& b)
                          {
                              // Note offs first at equal times, so a retrigger
                              // releases before it sounds again.
                              if (a.atSec != b.atSec)
                                  return a.atSec < b.atSec;
                              return (! a.isNoteOn) && b.isNoteOn;
                          });
    }

    // ---- silence on stop, or when the UI asks ------------------------------
    const auto stopped = wasPlaying.exchange (playing, std::memory_order_relaxed) && ! playing;

    if (stopped || panicRequested.exchange (false, std::memory_order_acquire))
    {
        pending.clear();
        sendAllNotesOff (midi, 0);
    }

    // ---- place this block's events ----------------------------------------
    size_t consumed = 0;

    for (const auto& event : pending)
    {
        if (event.atSec >= blockEnd)
            break;

        ++consumed;

        // Arrived after its moment: play it at the top of the block rather than
        // dropping it, but count it so the lateness is visible.
        auto offset = (int) ((event.atSec - blockStart) * sampleRate);

        if (offset < 0)
        {
            offset = 0;
            lateEvents.fetch_add (1, std::memory_order_relaxed);
        }

        offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), offset);

        const auto channel = juce::jlimit (1, 16, event.channel);
        const auto note = juce::jlimit (0, 127, event.note);

        midi.addEvent (event.isNoteOn
                           ? juce::MidiMessage::noteOn (channel, note,
                                                        (juce::uint8) juce::jlimit (0, 127, event.velocity))
                           : juce::MidiMessage::noteOff (channel, note),
                       offset);
    }

    if (consumed > 0)
        pending.erase (pending.begin(), pending.begin() + (long) consumed);

    hostSec = blockEnd;
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
