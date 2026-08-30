#pragma once

#include "../engine/Events.h"
#include "../engine/Planner.h"
#include "../engine/Project.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <vector>

/** One incoming MIDI message, on its way to the interface.

    Three bytes covers every channel message and every realtime byte, which is
    all M's Input Control System reads. SysEx is not forwarded.
*/
struct IncomingMidi
{
    uint8_t status = 0;
    uint8_t data1 = 0;
    uint8_t data2 = 0;
};

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

    /** The MIDI destination when there is no host to hand notes to.

        A plugin's notes leave through the buffer the host passes in. The
        standalone has no such host, so it opens a port of its own — a virtual
        one where the platform allows it, so anything on the machine can receive
        M without a cable. Null in a plugin, and null if no port could be
        opened, which is not fatal: the interface still runs.
    */
    juce::MidiOutput* standalonePort() const noexcept { return midiOut.get(); }

    /** How many notes this instance has emitted since it was loaded. Lets the
        editor answer "is it actually playing" without a DAW. */
    int notesEmitted() const noexcept { return emitted.load (std::memory_order_relaxed); }

    /** Replace the project the engine is playing. Called from the message
        thread when the interface changes something.

        The audio thread never blocks on this: the new state is built into the
        spare half of a double buffer and swapped in at a block boundary. Only
        one change is in flight at a time, which is why the message thread also
        checks the flag before writing.
    */
    void setProjectFromJson (const juce::String& json);

    /** Start or stop the standalone's own transport.

        A plugin takes its transport from the host and ignores this. The
        standalone has no host, so its Start button has to drive something —
        this is that something, and without it the standalone renders an
        interface that never plays.
    */
    void setStandaloneTransport (bool running) noexcept
    {
        standaloneRunning.store (running, std::memory_order_release);
    }

    bool isStandalone() const noexcept { return wrapperType == wrapperType_Standalone; }

    /** How many projects the interface has sent. Zero means the bridge never
        fired, which looks identical to a working plugin until you change
        something and nothing happens. */
    int projectsReceived() const noexcept { return received.load (std::memory_order_relaxed); }

    /** Voices in the project the engine is currently playing. */
    int liveVoiceCount() const noexcept { return (int) projects[liveProject].voices.size(); }

    /** The document the host should hand back next time, or empty if the
        interface has not sent one yet. Read on the message thread only. */
    juce::String restoredDocument() const { return documentJson; }

    /** Which auxiliary windows were open, as a JSON array. Interface state, not
        musical state: restoring a session must not be able to corrupt a project
        because a window moved. */
    juce::String restoredWindows() const { return windowsJson; }

    void setWindowsJson (const juce::String& json) { windowsJson = json; }

    /** True when a session was restored and the interface has not yet been told
        about it. Cleared by the editor once it has pushed it into the UI. */
    bool takeRestoredFlag() noexcept { return restoredPending.exchange (false); }

    /** Take everything the host has sent us since the last call.

        The interface owns M's Input Control System — note input, Echo Map,
        Keyboard Transpose, Step Advance — and all of it edits the project,
        which then reaches the engine the ordinary way. So input crosses to the
        interface rather than being handled here.

        Called from the message thread. Returns how many messages were written.
    */
    int drainIncoming (IncomingMidi* destination, int capacity);

private:
    void allNotesOff (juce::MidiBuffer&, int samplePosition);
    void rewind (double ppq);

    /** Two halves: the audio thread reads one while the message thread fills
        the other. */
    mclassic::ProjectState projects[2];
    int liveProject = 0;
    std::atomic<bool> projectPending { false };
    std::vector<mclassic::VoiceCursor> cursors;
    std::vector<mclassic::Random> rngs;
    mclassic::NoteLifecycle lifecycle;

    // Reused every block so the steady state does not allocate.
    std::vector<mclassic::PlannedNote> planned;
    std::vector<mclassic::VoiceCursor> nextCursors;
    std::vector<mclassic::PlannedStep> steps;
    std::vector<mclassic::OutputDestination> destinations { mclassic::OutputDestination::midi };

    /** Host input on its way to the interface. Dropped rather than blocking if
        the interface is not draining — losing a controller move is better than
        stalling the audio thread. */
    static constexpr int incomingCapacity = 1024;
    juce::AbstractFifo incomingFifo { incomingCapacity };
    std::array<IncomingMidi, incomingCapacity> incoming {};

    /** One slot per pitch per channel is more than the engine can sound at once. */
    static constexpr int maxSounding = 256;
    std::array<SoundingNote, maxSounding> sounding {};

    const mclassic::ProjectState& project() const noexcept { return projects[liveProject]; }

    void openStandalonePort();
    void sendRealtime (juce::MidiBuffer&, uint8_t status, int samplePosition);
    void scheduleClock (juce::MidiBuffer&, double ppqStart, double ppqEnd,
                        double samplesPerPpq, int numSamples);

    /** Only opened when running standalone. */
    std::unique_ptr<juce::MidiOutput> midiOut;

    std::atomic<bool> standaloneRunning { false };
    /** The standalone's own position, advanced by the block size. */
    double freePpq = 0.0;
    /** The next MIDI Clock pulse, in beats. 24 per quarter note. */
    double nextPulsePpq = 0.0;

    double sampleRate = 44100.0;
    /** Where the engine's own clock was zeroed, in host beats. */
    double originPpq = 0.0;
    double lastPpq = 0.0;
    bool wasPlaying = false;

    std::atomic<int> emitted { 0 };
    std::atomic<int> received { 0 };

    /** The document verbatim, so what the host stores is exactly what the
        interface produced — no re-serialising, nothing to drift. */
    juce::String documentJson;
    juce::String windowsJson;
    std::atomic<bool> restoredPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MClassicProcessor)
};
