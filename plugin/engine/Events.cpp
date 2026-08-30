#include "Events.h"

#include <algorithm>

namespace idm
{

const char* nameOf (OutputDestination destination) noexcept
{
    return destination == OutputDestination::midi ? "midi" : "synth";
}

const char* nameOf (EventKind kind) noexcept
{
    switch (kind)
    {
        case EventKind::programChange: return "program-change";
        case EventKind::noteOff:       return "note-off";
        case EventKind::noteOn:        return "note-on";
    }

    return "note-on";
}

bool engineEventLess (const EngineEvent& a, const EngineEvent& b) noexcept
{
    if (a.atSec != b.atSec)               return a.atSec < b.atSec;
    if (a.kind != b.kind)                 return a.kind < b.kind;
    if (a.destination != b.destination)   return a.destination < b.destination;
    if (a.channel != b.channel)           return a.channel < b.channel;
    return a.sequence < b.sequence;
}

void NoteLifecycle::ingest (const std::vector<PlannedNote>& notes,
                            const std::vector<OutputDestination>& destinations)
{
    std::vector<const PlannedNote*> sorted;
    sorted.reserve (notes.size());

    for (const auto& note : notes)
        sorted.push_back (&note);

    // Stable, like `Array.prototype.sort`: notes agreeing on both keys keep the
    // order the planner emitted them in.
    std::stable_sort (sorted.begin(), sorted.end(),
                      [] (const PlannedNote* a, const PlannedNote* b)
                      {
                          if (a->startSec != b->startSec) return a->startSec < b->startSec;
                          return a->voice < b->voice;
                      });

    for (const auto* note : sorted)
        for (const auto destination : destinations)
            addNote (*note, destination);
}

void NoteLifecycle::addNote (const PlannedNote& note, OutputDestination destination)
{
    const ActiveKey key { destination, note.channel, note.note };
    const auto found = active.find (key);

    // Already sounding: withdraw the note-off that has not happened yet and
    // release the old note at the replacement's onset instead.
    if (found != active.end())
    {
        const auto previous = found->second;

        pending.erase (std::remove_if (pending.begin(), pending.end(),
                                       [&] (const EngineEvent& e)
                                       { return e.sequence == previous.offSequence; }),
                       pending.end());

        EngineEvent early;
        early.kind = EventKind::noteOff;
        early.atSec = note.startSec;
        early.atTick = note.atTick;
        early.sequence = takeSequence();
        early.destination = destination;
        early.voice = note.voice;
        early.channel = note.channel;
        early.noteId = previous.noteId;
        early.note = note.note;
        early.velocity = 0;

        pending.push_back (early);
    }

    const auto id = noteId++;
    const auto onSequence = takeSequence();
    const auto offSequence = takeSequence();

    EngineEvent on;
    on.kind = EventKind::noteOn;
    on.atSec = note.startSec;
    on.atTick = note.atTick;
    on.sequence = onSequence;
    on.destination = destination;
    on.voice = note.voice;
    on.channel = note.channel;
    on.noteId = id;
    on.note = note.note;
    on.velocity = note.velocity;

    EngineEvent off;
    off.kind = EventKind::noteOff;
    off.atSec = note.startSec + std::max (0.0, note.durationSec);
    off.atTick = note.atTick + note.durationTicks;
    off.sequence = offSequence;
    off.destination = destination;
    off.voice = note.voice;
    off.channel = note.channel;
    off.noteId = id;
    off.note = note.note;
    off.velocity = 0;

    pending.push_back (on);
    pending.push_back (off);

    active[key] = { id, offSequence };
}

void NoteLifecycle::addProgramChanges (double atSec, double atTick,
                                       const std::vector<ProgramChange>& programs)
{
    for (const auto& item : programs)
    {
        EngineEvent event;
        event.kind = EventKind::programChange;
        event.atSec = atSec;
        event.atTick = atTick;
        event.sequence = takeSequence();
        event.destination = OutputDestination::midi;
        event.voice = item.voice;
        event.channel = item.channel;
        event.program = item.program;

        pending.push_back (event);
    }
}

std::vector<EngineEvent> NoteLifecycle::drainBefore (double endSec)
{
    std::vector<EngineEvent> ready, future;

    for (const auto& event : pending)
        (event.atSec < endSec ? ready : future).push_back (event);

    pending = std::move (future);
    std::sort (ready.begin(), ready.end(), engineEventLess);

    // A note stops being active once its own off has gone out. The id check
    // matters: a retrigger has already replaced the entry, and that newer note
    // must not be forgotten by the older one's release.
    for (const auto& event : ready)
    {
        if (event.kind != EventKind::noteOff)
            continue;

        const ActiveKey key { event.destination, event.channel, event.note };
        const auto found = active.find (key);

        if (found != active.end() && found->second.noteId == event.noteId)
            active.erase (found);
    }

    return ready;
}

void NoteLifecycle::reset()
{
    pending.clear();
    active.clear();
}

} // namespace idm
