#pragma once

#include "Planner.h"

#include <cstdint>
#include <map>
#include <vector>

namespace mclassic
{

/** Ordered Midi before Synth, matching JavaScript's `localeCompare` on those
    two strings. The tie-break is part of the contract, not an accident of how
    the enum happens to be written. */
enum class OutputDestination { midi, synth };

const char* nameOf (OutputDestination destination) noexcept;

/** Kinds in their tie-break order: at one instant a program change precedes a
    release, and a release precedes an attack. */
enum class EventKind { programChange, noteOff, noteOn };

const char* nameOf (EventKind kind) noexcept;

/** One event bound for an output adapter.

    Flat rather than a tagged union, because this crosses into the audio thread.
    `noteId`, `note` and `velocity` are meaningless on a program change and
    `program` is meaningless on the others; none of them participate in ordering.
*/
struct EngineEvent
{
    EventKind kind = EventKind::noteOn;
    double atSec = 0.0;
    double atTick = 0.0;
    uint64_t sequence = 0;
    OutputDestination destination = OutputDestination::midi;
    size_t voice = 0;
    int channel = 1;
    uint64_t noteId = 0;
    int note = 0;
    int velocity = 0;
    int program = -1;
};

/** The total order an adapter receives events in: time, kind, destination,
    channel, then the sequence number — which is monotonic, so the comparison
    never ends in a tie. */
bool engineEventLess (const EngineEvent& a, const EngineEvent& b) noexcept;

struct ProgramChange
{
    size_t voice = 0;
    int channel = 1;
    int program = 0;
};

/** Owns future note-offs and resolves overlapping notes before they reach an
    output adapter. */
class NoteLifecycle
{
public:
    /** Take planned notes for every destination they are bound for.

        The notes are put in onset order first, because the retrigger rule
        depends on the order they arrive in: which note is "previous" is decided
        here.
    */
    void ingest (const std::vector<PlannedNote>& notes,
                 const std::vector<OutputDestination>& destinations);

    void addProgramChanges (double atSec, double atTick,
                            const std::vector<ProgramChange>& programs);

    /** Everything due before `endSec`, in order, removed from the queue. */
    std::vector<EngineEvent> drainBefore (double endSec);

    size_t pendingCount() const noexcept { return pending.size(); }

    void reset();

private:
    struct ActiveNote
    {
        uint64_t noteId = 0;
        uint64_t offSequence = 0;
    };

    /** A sounding note, keyed the way the TypeScript keys its map. */
    struct ActiveKey
    {
        OutputDestination destination;
        int channel;
        int note;

        bool operator< (const ActiveKey& other) const noexcept
        {
            if (destination != other.destination) return destination < other.destination;
            if (channel != other.channel)         return channel < other.channel;
            return note < other.note;
        }
    };

    void addNote (const PlannedNote& note, OutputDestination destination);
    uint64_t takeSequence() noexcept { return sequence++; }

    std::vector<EngineEvent> pending;
    std::map<ActiveKey, ActiveNote> active;
    uint64_t sequence = 0;
    uint64_t noteId = 0;
};

} // namespace mclassic
