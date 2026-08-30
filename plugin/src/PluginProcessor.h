#pragma once

#include "../engine/Events.h"
#include "../engine/Planner.h"
#include "../engine/Project.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <vector>

/** A note the processor is holding open, so it can always be closed again. */
struct SoundingNote
{
    int channel = 0;   // 1..16, 0 when the slot is free
    int note = 0;      // 0..127
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

    /** How many notes this instance has emitted since it was loaded. Lets the
        editor answer "is it actually playing" without a DAW. */
    int notesEmitted() const noexcept { return emitted.load (std::memory_order_relaxed); }

private:
    void allNotesOff (juce::MidiBuffer&, int samplePosition);
    void rewind (double ppq);

    mclassic::ProjectState project;
    std::vector<mclassic::VoiceCursor> cursors;
    std::vector<mclassic::Random> rngs;
    mclassic::NoteLifecycle lifecycle;

    // Reused every block so the steady state does not allocate.
    std::vector<mclassic::PlannedNote> planned;
    std::vector<mclassic::VoiceCursor> nextCursors;
    std::vector<mclassic::PlannedStep> steps;
    std::vector<mclassic::OutputDestination> destinations { mclassic::OutputDestination::midi };

    /** One slot per pitch per channel is more than the engine can sound at once. */
    static constexpr int maxSounding = 256;
    std::array<SoundingNote, maxSounding> sounding {};

    double sampleRate = 44100.0;
    /** Where the engine's own clock was zeroed, in host beats. */
    double originPpq = 0.0;
    double lastPpq = 0.0;
    bool wasPlaying = false;

    std::atomic<int> emitted { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MClassicProcessor)
};
