#pragma once

#include "Project.h"
#include "Random.h"
#include "Transform.h"

#include <vector>

namespace idm
{

/** Pulses per quarter note on the shared transport timeline. */
inline constexpr double ppqn = 960.0;

struct PlannedNote
{
    size_t voice = 0;
    int note = 0;
    int velocity = 0;
    int channel = 1;
    double startSec = 0.0;
    double durationSec = 0.0;
    /** Absolute musical position on the shared transport timeline. */
    double atTick = 0.0;
    double durationTicks = 0.0;
    NoteSource source = NoteSource::original;
    /** The Rhythm variable's multiplier for this Voice on this step — its clock
        divider, carried out so a display can give each lane its own rate. */
    double rhythm = 1.0;
};

struct PlannedStep
{
    size_t voice = 0;
    int step = 0;
};

struct VoiceCursor
{
    NoteOrderCursor order {};
    /** Real time of the next event — what actually gets scheduled. */
    double nextTimeSec = 0.0;
    /** Absolute time this Voice's clock started: the origin of its time map. */
    double originSec = 0.0;
    /** Undistorted clock time elapsed since `originSec`. */
    double clockSec = 0.0;
    long long cyclicPos = 0;
    double transportTick = 0.0;
};

/** One fresh cursor per Voice, all starting at `startSec`. */
std::vector<VoiceCursor> makeCursors (const ProjectState& state, double startSec);

/** Plan every note beginning within `[windowStart, windowEnd)`.

    `windowStart` is unused directly — each Voice carries its own
    `nextTimeSec`, which is where playback actually resumes — but it documents
    the caller's scheduling window.

    The order randomness is consumed in is part of the contract, not an
    implementation detail. Each step draws accent, then legato, then rhythm, and
    only then — if the Voice is playing — the note-order index and the density
    gate. Moving any of those reorders every draw that follows and silently
    changes the music.
*/
void planWindow (const ProjectState& state,
                 const std::vector<VoiceCursor>& cursors,
                 std::vector<Random>& rngs,
                 double windowStart,
                 double windowEnd,
                 std::vector<PlannedNote>& notes,
                 std::vector<VoiceCursor>& nextCursors,
                 std::vector<PlannedStep>& steps);

} // namespace idm
