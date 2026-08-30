#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../engine/StepSource.h"

#include <array>
#include <atomic>
#include <vector>

/** A note the processor is holding open, so it can always be closed again. */
struct SoundingNote
{
    int channel = 0;   // 1..16, 0 when the slot is free
    int note = 0;      // 0..127
    double offPpq = 0.0;
};

class MClassicProcessor : public juce::AudioProcessor
{
public:
    MClassicProcessor();
    ~MClassicProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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

    /** How many notes this instance has emitted since it was loaded. Read by
        the editor so "is it actually playing" is answerable without a DAW. */
    int notesEmitted() const noexcept { return emitted.load (std::memory_order_relaxed); }

private:
    void allNotesOff (juce::MidiBuffer&, int samplePosition);
    void closeNotesDueBefore (juce::MidiBuffer&, double ppqEnd, double ppqStart,
                              double samplesPerPpq, int numSamples);

    /** Sixteen slots is one per MIDI channel, which is the most the engine can
        sound at once per pitch. Fixed so nothing allocates on the audio thread. */
    static constexpr int maxSounding = 128;
    std::array<SoundingNote, maxSounding> sounding {};

    mclassic::StepSource steps;
    /** Reused every block so the audio thread never allocates. */
    std::vector<mclassic::TimedMidi> pending;

    double sampleRate = 44100.0;
    double lastPpq = 0.0;
    bool wasPlaying = false;

    std::atomic<int> emitted { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MClassicProcessor)
};
