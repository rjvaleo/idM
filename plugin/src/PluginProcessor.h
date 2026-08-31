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

/** A note the engine played, on its way back to the interface.

    The interface's Midi View is a monitor: it shows what M generated. With the
    engine in the processor, the interface has no other way to know — so what
    was played travels back, purely to be displayed.
*/
struct PlayedNote
{
    int voice = 0;
    int note = 0;
    int velocity = 0;
    int channel = 1;
    double atTick = 0.0;
    double durationTicks = 0.0;
    double startSec = 0.0;
};

/** A note the processor is holding open, so it can always be closed again. */
struct SoundingNote
{
    int channel = 0;   // 1..16, 0 when the slot is free
    int note = 0;      // 0..127
};

class IdmProcessor : public juce::AudioProcessor,
                     private juce::MidiInputCallback
{
public:
    IdmProcessor();
    ~IdmProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "idM"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
   #if defined (JucePlugin_IsMidiEffect)
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect != 0; }
   #else
    bool isMidiEffect() const override { return false; }
   #endif
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

    /** The name of the port M is publishing, or empty if none opened. Shown in
        the interface so "where is my MIDI" has an answer on screen. */
    juce::String portName() const { return midiOut != nullptr ? midiOut->getName() : juce::String(); }
    juce::String inputPortName() const { return midiIn != nullptr ? midiIn->getName() : juce::String(); }

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
    /** Whether program changes are sent at all.

        Off by default. M emits one per Voice when the patch changes, and a
        stray program change silently repatches whatever is downstream — a
        real cost for a message the product does not depend on. VST3 delivery
        of them is unreliable besides: Steinberg marked non-note events legacy
        and hosts implement them inconsistently, so a plugin that relies on
        them behaves differently in every DAW.
    */
    void setSendProgramChanges (bool on) noexcept
    {
        sendPrograms.store (on, std::memory_order_release);
    }

    bool sendsProgramChanges() const noexcept
    {
        return sendPrograms.load (std::memory_order_acquire);
    }

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

    /** Take the notes played since the last call, for the interface to show.
        Message thread only. */
    int drainPlayed (PlayedNote* destination, int capacity);

    /** What the host's transport is doing, for the interface's own lights. */
    bool hostIsPlaying() const noexcept { return publishedPlaying.load (std::memory_order_relaxed); }
    double hostTempo() const noexcept { return publishedTempo.load (std::memory_order_relaxed); }

    /** Seconds since the transport started, on the engine's own clock.

        The interface's displays scroll on elapsed time. Outside a plugin they
        take it from the local runtime; in a plugin that runtime is deliberately
        never started, so without this they would sit at zero and show nothing
        while the engine played perfectly.
    */
    double elapsedSeconds() const noexcept { return publishedElapsed.load (std::memory_order_relaxed); }

private:
    static BusesProperties busesForThisBuild();

    void allNotesOff (juce::MidiBuffer&, int samplePosition);
    void rewind (double ppq);

    /** Two halves: the audio thread reads one while the message thread fills
        the other. */
    idm::ProjectState projects[2];
    int liveProject = 0;
    std::atomic<bool> projectPending { false };
    std::vector<idm::VoiceCursor> cursors;
    std::vector<idm::Random> rngs;
    idm::NoteLifecycle lifecycle;

    // Reused every block so the steady state does not allocate.
    std::vector<idm::PlannedNote> planned;
    std::vector<idm::VoiceCursor> nextCursors;
    std::vector<idm::PlannedStep> steps;
    std::vector<idm::OutputDestination> destinations { idm::OutputDestination::midi };

    /** Host input on its way to the interface. Dropped rather than blocking if
        the interface is not draining — losing a controller move is better than
        stalling the audio thread. */
    static constexpr int incomingCapacity = 1024;
    juce::AbstractFifo incomingFifo { incomingCapacity };
    std::array<IncomingMidi, incomingCapacity> incoming {};

    /** The virtual input's own queue, and not merely for tidiness:
        juce::AbstractFifo is single-producer, single-consumer. The host writes
        the queue above from the audio thread; Core MIDI delivers on a thread of
        its own. Sharing one queue between them would break that contract in a
        way that shows up as corruption under load rather than as a crash. Two
        queues, one consumer, both drained by drainIncoming. */
    juce::AbstractFifo virtualInFifo { incomingCapacity };
    std::array<IncomingMidi, incomingCapacity> virtualIn {};

    void pushIncoming (juce::AbstractFifo& fifo,
                       std::array<IncomingMidi, incomingCapacity>& store,
                       const uint8_t* raw, int bytes);

    /** Core MIDI's thread, for the virtual input. */
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    /** One slot per pitch per channel is more than the engine can sound at once. */
    static constexpr int maxSounding = 256;
    std::array<SoundingNote, maxSounding> sounding {};

    const idm::ProjectState& project() const noexcept { return projects[liveProject]; }

    void openStandalonePort();
    void sendRealtime (juce::MidiBuffer&, uint8_t status, int samplePosition);
    void scheduleClock (juce::MidiBuffer&, double ppqStart, double ppqEnd,
                        double samplesPerPpq, int numSamples);

    /** Our own MIDI destination, in the plugin as well as the standalone. */
    std::unique_ptr<juce::MidiOutput> midiOut;
    /** And our own MIDI source, so other software can play into idM without a
        host in between - the manual's "to M" ports. macOS and Linux only. */
    std::unique_ptr<juce::MidiInput> midiIn;
    bool portOpened = false;

    std::atomic<bool> standaloneRunning { false };
    std::atomic<bool> sendPrograms { false };
    /** The standalone's own position, advanced by the block size. */
    double freePpq = 0.0;
    /** The next MIDI Clock pulse, in beats. 24 per quarter note. */
    double nextPulsePpq = 0.0;

    double sampleRate = 44100.0;
    /** Where the engine's own clock was zeroed, in host beats. */
    double originPpq = 0.0;
    double lastPpq = 0.0;
    bool wasPlaying = false;

    /** Played notes on their way to the interface's monitor. Dropped rather
        than blocking: a missing line in a display beats a stalled audio thread. */
    static constexpr int playedCapacity = 2048;
    juce::AbstractFifo playedFifo { playedCapacity };
    std::array<PlayedNote, playedCapacity> played {};

    std::atomic<bool> publishedPlaying { false };
    std::atomic<double> publishedTempo { 120.0 };
    std::atomic<double> publishedElapsed { 0.0 };

    std::atomic<int> emitted { 0 };
    std::atomic<int> received { 0 };

    /** The document verbatim, so what the host stores is exactly what the
        interface produced — no re-serialising, nothing to drift. */
    juce::String documentJson;
    juce::String windowsJson;
    std::atomic<bool> restoredPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IdmProcessor)
};
