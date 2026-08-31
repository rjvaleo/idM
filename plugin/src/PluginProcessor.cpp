#include "PluginProcessor.h"
#if ! IDM_NO_EDITOR
 #include "PluginEditor.h"
#endif

#include "Diagnostics.h"
#include "ProjectJson.h"

#include <cmath>

/** A MIDI effect has no audio buses at all; an instrument has a stereo output.

    M generates MIDI, and it is built both ways because hosts disagree about
    which shape may send MIDI onward: Ableton does not route MIDI out of an
    instrument, and takes it from a MIDI effect.
*/
juce::AudioProcessor::BusesProperties IdmProcessor::busesForThisBuild()
{
   #if JucePlugin_IsMidiEffect
    return {};
   #else
    return BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #endif
}

IdmProcessor::IdmProcessor()
    : AudioProcessor (busesForThisBuild())
{
    projects[0] = idm::createDefaultProject();
    projects[1] = projects[0];

    planned.reserve (1024);
    nextCursors.reserve (16);
    steps.reserve (1024);
    rewind (0.0);
}

/** Open a MIDI destination when there is no host to hand notes to.

    A virtual port first, because it costs the user nothing: M appears as a MIDI
    source that any DAW or synth on the machine can select. Where the platform
    has no virtual ports, the first real output is better than silence. Failing
    to open one is not fatal — the interface still runs, and the plugin builds
    still have their host.
*/
void IdmProcessor::openStandalonePort()
{
    if (portOpened)
        return;

    portOpened = true;

    // The first instance gets the plain name; a second copy in the same set
    // takes the next one. Counting instances instead would make the name depend
    // on how many processors happened to be built first, so the port a user is
    // told to select would not be the one they see.
    for (int attempt = 1; attempt <= 16 && midiOut == nullptr; ++attempt)
    {
        const auto name = attempt == 1 ? juce::String ("idM")
                                       : "idM " + juce::String (attempt);

        midiOut = juce::MidiOutput::createNewDevice (name);
    }

    // Virtual ports are macOS, iOS and Linux only - JUCE's own header says so.
    // On Windows there is no fallback: the host path is the only route, which
    // is why it has to work rather than merely usually work.
    if (midiOut == nullptr && wrapperType == wrapperType_Standalone)
    {
        // The standalone has nowhere else to go, so the first real output beats
        // silence. A plugin does not do this: hijacking whatever hardware the
        // user happens to own would be worse than relying on the host.
        const auto devices = juce::MidiOutput::getAvailableDevices();

        if (! devices.isEmpty())
            midiOut = juce::MidiOutput::openDevice (devices[0].identifier);
    }

    if (midiOut != nullptr)
        midiOut->startBackgroundThread();

    // The manual's "to M 1" and "to M 2": a destination other software can send
    // into, so idM can be played from another program without a host between
    // them. Named to match whatever the output ended up called, because a user
    // told to look for "idM" should find "to idM" beside it.
    const auto suffix = midiOut != nullptr ? midiOut->getName().fromFirstOccurrenceOf ("idM", false, false)
                                           : juce::String();

    midiIn = juce::MidiInput::createNewDevice ("to idM" + suffix, this);

    if (midiIn != nullptr)
        midiIn->start();
}

/*  Core MIDI's own thread.

    Nothing here touches the engine: incoming MIDI belongs to the interface,
    where M's Input Control System lives. All this does is queue the bytes.
*/
void IdmProcessor::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    pushIncoming (virtualInFifo, virtualIn, message.getRawData(), message.getRawDataSize());
}

void IdmProcessor::pushIncoming (juce::AbstractFifo& fifo,
                                 std::array<IncomingMidi, incomingCapacity>& store,
                                 const uint8_t* raw, int bytes)
{
    if (bytes < 1 || bytes > 3)
        return; // SysEx is not forwarded

    const auto scope = fifo.write (1);

    if (scope.blockSize1 + scope.blockSize2 < 1)
        return; // the interface is not draining; drop rather than stall

    const auto index = scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2;

    store[(size_t) index] = IncomingMidi { (uint8_t) raw[0],
                                           (uint8_t) (bytes > 1 ? raw[1] : 0),
                                           (uint8_t) (bytes > 2 ? raw[2] : 0) };
}

void IdmProcessor::prepareToPlay (double newSampleRate, int)
{
    // Not in the constructor: JUCE assigns wrapperType after the processor is
    // built, so anything that depends on it has to wait until the host is
    // actually preparing us.
    openStandalonePort();

    idm::Diagnostics::get().log (
        juce::String ("prepareToPlay  host=\"") + juce::PluginHostType().getHostDescription()
        + "\"  wrapper=" + getWrapperTypeDescription (wrapperType)
        + "  sampleRate=" + juce::String (newSampleRate)
        + "  port=" + (midiOut != nullptr ? midiOut->getName() : juce::String ("none"))
        + "  producesMidi=" + juce::String ((int) producesMidi()));

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    wasPlaying = false;
    rewind (0.0);
}

void IdmProcessor::releaseResources()
{
}

/** Put the engine back to the top and anchor its clock to `ppq`.

    Called on transport start and on any discontinuity — a loop wrap or a
    locate. The RNGs are re-seeded exactly as `traceProject` seeds them, so a
    performance is reproducible from its seed rather than from how long the
    transport happened to be running.
*/
void IdmProcessor::rewind (double ppq)
{
    originPpq = ppq;
    lastPpq = ppq;

    cursors = idm::makeCursors (project(), 0.0);

    rngs.clear();
    rngs.reserve (project().voices.size());

    for (size_t voice = 0; voice < project().voices.size(); ++voice)
        rngs.push_back (idm::Random { project().seed ^ ((uint32_t) (voice + 1) * 0x9e3779b1u) });

    lifecycle.reset();
}

void IdmProcessor::sendRealtime (juce::MidiBuffer& midi, uint8_t status, int samplePosition)
{
    const auto byte = status;
    midi.addEvent (juce::MidiMessage (&byte, 1), samplePosition);
}

/** MIDI Clock, at the 24 pulses per quarter note the spec fixes.

    Standalone only. A plugin follows the host's transport, and a follower also
    broadcasting its own clock is how two devices end up each thinking they lead.
*/
void IdmProcessor::scheduleClock (juce::MidiBuffer& midi, double ppqStart, double ppqEnd,
                                       double samplesPerPpq, int numSamples)
{
    constexpr double pulsesPerQuarter = 24.0;
    constexpr double pulsePpq = 1.0 / pulsesPerQuarter;

    if (nextPulsePpq < ppqStart)
        nextPulsePpq = std::ceil (ppqStart / pulsePpq) * pulsePpq;

    while (nextPulsePpq < ppqEnd)
    {
        auto offset = (int) std::floor ((nextPulsePpq - ppqStart) * samplesPerPpq);
        offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), offset);

        sendRealtime (midi, 0xf8, offset);
        nextPulsePpq += pulsePpq;
    }
}

void IdmProcessor::allNotesOff (juce::MidiBuffer& midi, int samplePosition)
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

void IdmProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    // Take what the host sent before clearing. It is not ours to echo — the
    // buffer is where our own notes go — but destroying it would take M's whole
    // Input Control System with it, which is what used to happen here.
    if (! midi.isEmpty())
    {
        // One reservation per message, deliberately, rather than one for the
        // whole block. AbstractFifo's scoped write commits blockSize1 +
        // blockSize2 when it goes out of scope whatever you actually wrote into
        // it - JUCE's own header says so - so reserving getNumEvents() slots and
        // then skipping the SysEx among them committed slots that were never
        // filled. The interface read whatever those slots held last time round:
        // a phantom note or controller move, arriving from nowhere, only when a
        // host happened to send SysEx. pushIncoming reserves after it validates.
        for (const auto metadata : midi)
            pushIncoming (incomingFifo, incoming, metadata.data, metadata.numBytes);
    }

    midi.clear();

    // Take a waiting edit at a block boundary, never mid-block. Swapping the
    // live half is a single assignment; nothing is copied on this thread.
    if (projectPending.load (std::memory_order_acquire))
    {
        const auto voicesBefore = project().voices.size();

        liveProject = 1 - liveProject;
        projectPending.store (false, std::memory_order_release);

        // An edit is not a transport event. M is played by tweaking it while it
        // runs, so changing a density or a Note Order mix must not restart the
        // pattern - and must not rewind(), which resets the note lifecycle and
        // would drop the note-off for everything currently sounding. That is
        // what made notes hang while the interface was being used.
        if (project().voices.size() != voicesBefore)
        {
            // A different number of Voices does need new cursors, and the notes
            // sounding belong to Voices that may no longer exist. Release them
            // by name first: the lifecycle is about to forget they exist.
            allNotesOff (midi, 0);
            rewind (lastPpq);
        }
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

    // No host means no transport to follow, so the standalone runs its own,
    // driven by the interface's Start button and advanced by the block size.
    if (isStandalone())
    {
        playing = standaloneRunning.load (std::memory_order_acquire);

        if (playing)
        {
            ppq = freePpq;
            freePpq += ((double) numSamples / sampleRate) / (60.0 / juce::jmax (1.0, bpm));
        }
        else
        {
            freePpq = 0.0;
            ppq = 0.0;
        }
    }

    const auto started = playing && ! wasPlaying;
    const auto jumped = playing && std::abs (ppq - lastPpq) > 1.0;

    if ((wasPlaying && ! playing) || started || jumped)
    {
        allNotesOff (midi, 0);

        if (playing)
        {
            rewind (ppq);
            nextPulsePpq = ppq;
        }

        // Transport bytes are the standalone's business: it leads, so it says
        // so. A plugin is following and stays quiet.
        if (isStandalone())
            sendRealtime (midi, playing ? 0xfa : 0xfc, 0);
    }

    publishedPlaying.store (playing, std::memory_order_relaxed);
    publishedTempo.store (bpm, std::memory_order_relaxed);

    // The one fact everything else depends on: is the host telling us it is
    // playing, and where. If this says stopped while the DAW is running, the
    // playhead is the problem and nothing downstream matters.
    idm::Diagnostics::get().logThrottled (
        "transport",
        "transport  playing=" + juce::String ((int) playing)
        + "  ppq=" + juce::String (ppq, 3)
        + "  bpm=" + juce::String (bpm, 2)
        + "  notesSent=" + juce::String (emitted.load (std::memory_order_relaxed)));

    wasPlaying = playing;

    if (! playing)
    {
        publishedElapsed.store (0.0, std::memory_order_relaxed);
        lastPpq = ppq;
        return;
    }

    // The engine keeps its own clock in seconds, zeroed where the transport
    // started. The host's tempo turns beats into that.
    const auto secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);
    const auto blockStartSec = (ppq - originPpq) * secondsPerBeat;
    const auto blockEndSec = blockStartSec + (double) numSamples / sampleRate;

    publishedElapsed.store (blockStartSec, std::memory_order_relaxed);

    if (isStandalone())
        scheduleClock (midi, ppq, ppq + blockEndSec / secondsPerBeat - blockStartSec / secondsPerBeat,
                       sampleRate * secondsPerBeat, numSamples);

    // M's own planner decides what plays. This is the whole point of the port.
    idm::planWindow (project(), cursors, rngs, blockStartSec, blockEndSec,
                          planned, nextCursors, steps);
    cursors = nextCursors;

    lifecycle.ingest (planned, destinations);

    for (const auto& event : lifecycle.drainBefore (blockEndSec))
    {
        auto offset = (int) std::floor ((event.atSec - blockStartSec) * sampleRate);
        offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), offset);

        const auto channel = juce::jlimit (1, 16, event.channel);
        const auto note = juce::jlimit (0, 127, event.note);

        if (event.kind == idm::EventKind::noteOn)
        {
            midi.addEvent (juce::MidiMessage::noteOn (channel, note,
                                                      (juce::uint8) juce::jlimit (0, 127, event.velocity)),
                           offset);
            emitted.fetch_add (1, std::memory_order_relaxed);

            // Back to the interface, so its Midi View shows what M generated.
            // The engine is in the processor now, so this is the only way it
            // can know.
            {
                const auto scope = playedFifo.write (1);

                if (scope.blockSize1 > 0)
                    played[(size_t) scope.startIndex1] =
                        { (int) event.voice, note, event.velocity, channel,
                          event.atTick, 0.0, event.atSec };
            }

            for (auto& slot : sounding)
            {
                if (slot.channel != 0)
                    continue;

                slot = { channel, note };
                break;
            }
        }
        else if (event.kind == idm::EventKind::noteOff)
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
        else if (sendPrograms.load (std::memory_order_acquire))
        {
            midi.addEvent (juce::MidiMessage::programChange (channel,
                                                             juce::jlimit (0, 127, event.program)),
                           offset);
        }
    }

    lastPpq = ppq;

    // Out of our own port as well as into the host's buffer. The host's copy
    // may never leave the wrapper - both formats gate it on a host decision
    // that is never reported - and this one is not subject to that.
    if (midiOut != nullptr && ! midi.isEmpty())
        midiOut->sendBlockOfMessages (midi, juce::Time::getMillisecondCounterHiRes(), sampleRate);
}

void IdmProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();

    // Bypass must not strand a note. It is one of the four ways a plugin leaves
    // something sounding forever.
    allNotesOff (midi, 0);

    if (midiOut != nullptr && ! midi.isEmpty())
        midiOut->sendBlockOfMessages (midi, juce::Time::getMillisecondCounterHiRes(), sampleRate);
}

int IdmProcessor::drainIncoming (IncomingMidi* destination, int capacity)
{
    auto written = 0;

    // Two producers, one consumer: the host's buffer from the audio thread, and
    // the virtual input from Core MIDI's thread. Order between the two queues
    // is arbitrary, which is fine - the Input Control System reads messages,
    // not their interleaving, and neither source is timestamped here.
    const auto drain = [&] (juce::AbstractFifo& fifo,
                            const std::array<IncomingMidi, incomingCapacity>& store)
    {
        const auto ready = juce::jmin (capacity - written, fifo.getNumReady());

        if (ready <= 0)
            return;

        const auto scope = fifo.read (ready);

        for (int i = 0; i < scope.blockSize1; ++i)
            destination[written++] = store[(size_t) (scope.startIndex1 + i)];

        for (int i = 0; i < scope.blockSize2; ++i)
            destination[written++] = store[(size_t) (scope.startIndex2 + i)];
    };

    drain (incomingFifo, incoming);
    drain (virtualInFifo, virtualIn);

    return written;
}

int IdmProcessor::drainPlayed (PlayedNote* destination, int capacity)
{
    const auto ready = juce::jmin (capacity, playedFifo.getNumReady());

    if (ready <= 0)
        return 0;

    const auto scope = playedFifo.read (ready);
    auto written = 0;

    for (int i = 0; i < scope.blockSize1; ++i)
        destination[written++] = played[(size_t) (scope.startIndex1 + i)];

    for (int i = 0; i < scope.blockSize2; ++i)
        destination[written++] = played[(size_t) (scope.startIndex2 + i)];

    return written;
}

void IdmProcessor::setProjectFromJson (const juce::String& json)
{
    // A change is already waiting; the newer state wins, so overwrite the same
    // spare half rather than queueing. The interface sends whole projects, so
    // nothing is lost by dropping an intermediate one.
    const auto spare = 1 - liveProject;
    projects[spare] = idm::projectFromJson (juce::JSON::parse (json));

    // Kept verbatim. What the host stores is exactly what the interface
    // produced, so a round trip cannot lose a field this port does not read.
    documentJson = json;

    projectPending.store (true, std::memory_order_release);
    received.fetch_add (1, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* IdmProcessor::createEditor()
{
   #if IDM_NO_EDITOR
    // The state test links the processor without the webview, which would drag
    // in the whole UI bundle for a check that never opens a window.
    return nullptr;
   #else
    return new IdmEditor (*this);
   #endif
}

void IdmProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    // Empty until the interface has sent something. Saving a default project
    // over a session that never opened its window would be worse than saving
    // nothing.
    if (documentJson.isEmpty())
        return;

    // The musical document and the interface state travel together but stay
    // apart. Reopening a session must not be able to corrupt a project because
    // a window moved.
    const auto blob = "{\"v\":1,\"document\":" + documentJson
                    + ",\"popouts\":" + (windowsJson.isEmpty() ? "[]" : windowsJson) + "}";

    destination.replaceAll (blob.toRawUTF8(), (size_t) blob.getNumBytesAsUTF8());
}

void IdmProcessor::setStateInformation (const void* data, int size)
{
    if (data == nullptr || size <= 0)
        return;

    const juce::String json { juce::CharPointer_UTF8 (static_cast<const char*> (data)),
                              (size_t) size };

    if (json.isEmpty())
        return;

    // Sessions written before the interface state was carried hold a bare
    // document. Reading both keeps those openable.
    const auto parsed = juce::JSON::parse (json);
    const auto wrapped = parsed.isObject() && parsed.hasProperty ("v");

    if (wrapped)
    {
        setProjectFromJson (juce::JSON::toString (parsed.getProperty ("document", juce::var())));
        windowsJson = juce::JSON::toString (parsed.getProperty ("popouts", juce::var()));
    }
    else
    {
        setProjectFromJson (json);
    }

    // The engine has it; the interface has not. The editor may not exist yet —
    // a host restores state before opening the window, and often never opens it
    // at all — so this is a flag rather than a call.
    restoredPending.store (true, std::memory_order_release);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IdmProcessor();
}
