#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

/** One MIDI event the UI has planned, timed on the shared host clock. */
struct PlannedMidi
{
    double atSec = 0.0;
    bool isNoteOn = false;
    int channel = 1;   // 1..16
    int note = 0;      // 0..127
    int velocity = 0;  // 0..127
};

/** What the processor knows about the host, published for the editor to relay. */
struct HostTransport
{
    double hostSec = 0.0;
    double bpm = 120.0;
    double ppqPosition = 0.0;
    bool isPlaying = false;
};

class MClassicProcessor : public juce::AudioProcessor
{
public:
    MClassicProcessor();
    ~MClassicProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "M Classic"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==============================================================================
    /** Called from the editor when the UI has planned more notes. Safe from the
        message thread; the audio thread drains without blocking it. */
    void submitPlanned (const std::vector<PlannedMidi>& events);

    /** Drop everything not yet played — transport stop, or a UI panic. */
    void clearPlanned();

    /** The last transport reading, for the editor to relay to the UI. */
    HostTransport transport() const;

    /** Notes the UI planned but which arrived too late to place. Surfaced so a
        timing problem is visible rather than silently inaudible. */
    int lateEventCount() const { return lateEvents.load (std::memory_order_relaxed); }

private:
    void sendAllNotesOff (juce::MidiBuffer& midi, int samplePosition);

    // The UI plans ahead on the message thread and the audio thread consumes,
    // so the handover is a lock-free FIFO rather than a shared vector.
    static constexpr int queueCapacity = 8192;
    juce::AbstractFifo fifo { queueCapacity };
    std::vector<PlannedMidi> queue { (size_t) queueCapacity };

    // Audio-thread-owned, kept in time order.
    std::vector<PlannedMidi> pending;

    double sampleRate = 44100.0;
    double hostSec = 0.0;

    std::atomic<double> publishedSec { 0.0 };
    std::atomic<double> publishedBpm { 120.0 };
    std::atomic<double> publishedPpq { 0.0 };
    std::atomic<bool> publishedPlaying { false };
    std::atomic<bool> wasPlaying { false };
    std::atomic<int> lateEvents { 0 };
    std::atomic<bool> panicRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MClassicProcessor)
};
