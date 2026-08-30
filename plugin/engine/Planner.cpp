#include "Planner.h"

#include "Num.h"

#include <algorithm>

namespace idm
{

namespace
{

/** M ticks per quarter note, the unit the Phase control is expressed in. */
constexpr double mTicksPerQuarter = 96.0;

/** The cyclic position wraps every sixteen steps. */
constexpr long long cyclicPositions = 16;

/** The level drawn from a Voice's cycle at `position`. */
int cyclicLevel (const ProjectState& state, CyclicKind kind, size_t voice,
                 long long position, Random& rng)
{
    const auto& cycle = state.cyclic.get (kind)[voice];
    const auto length = (long long) state.cyclicLengths.get (kind)[voice];

    return pickCyclicLevel (cycle[(size_t) (position % length)], rng);
}

/** The value that level maps to. Legato is a percentage; Rhythm is a factor. */
double cyclicMultiplier (const ProjectState& state, CyclicKind kind, size_t voice,
                         long long position, Random& rng)
{
    const auto level = cyclicLevel (state, kind, voice, position, rng);

    const auto value = kind == CyclicKind::legato
        ? state.cyclicValues.legato[(size_t) level]
        : state.cyclicValues.rhythm[(size_t) level];

    return kind == CyclicKind::legato ? value / 100.0 : value;
}

} // namespace

std::vector<VoiceCursor> makeCursors (const ProjectState& state, double startSec)
{
    std::vector<VoiceCursor> cursors;
    cursors.reserve (state.voices.size());

    for (const auto& voice : state.voices)
    {
        const auto phase = voice.phase > 0.0 ? voice.phase : 0.0;
        const auto phaseSec = phase * (60.0 / state.tempo) / mTicksPerQuarter;

        VoiceCursor cursor;
        cursor.order = {};
        cursor.nextTimeSec = startSec + phaseSec;
        cursor.originSec = startSec + phaseSec;
        cursor.clockSec = 0.0;
        cursor.cyclicPos = 0;
        cursor.transportTick = jsRound (phase * ppqn / mTicksPerQuarter);

        cursors.push_back (cursor);
    }

    return cursors;
}

void planWindow (const ProjectState& state,
                 const std::vector<VoiceCursor>& cursors,
                 std::vector<Random>& rngs,
                 double,
                 double windowEnd,
                 std::vector<PlannedNote>& notes,
                 std::vector<VoiceCursor>& nextCursors,
                 std::vector<PlannedStep>& steps)
{
    notes.clear();
    steps.clear();
    nextCursors.clear();
    nextCursors.reserve (state.voices.size());

    // Second-Order Transpose stacks the Voices cumulatively, so each adds the
    // transpositions of the Voices above it, building implied chords.
    std::vector<int> effTrans;
    effTrans.reserve (state.voices.size());

    if (state.secondOrderTranspose)
    {
        auto acc = 0;

        for (const auto& v : state.voices)
        {
            acc += v.transposition;
            effTrans.push_back (acc);
        }
    }
    else
    {
        for (const auto& v : state.voices)
            effTrans.push_back (v.transposition);
    }

    for (size_t vi = 0; vi < state.voices.size(); ++vi)
    {
        const auto& v = state.voices[vi];
        const auto cursor = cursors[vi];

        if (v.timeBaseDenominator <= 0.0 || v.mouseAdvance)
        {
            nextCursors.push_back (cursor); // advanced only by Input Control
            continue;
        }

        auto& rng = rngs[vi];
        const auto& pat = state.patterns[v.patternIndex];
        const auto outLen = (int) std::min ((size_t) std::max (0, pat.outputLength),
                                            pat.steps.size());
        const auto stepDur = stepDurationSeconds (state.tempo, v.timeBaseNumerator,
                                                  v.timeBaseDenominator);

        auto order = cursor.order;
        auto clockSec = cursor.clockSec;
        auto cyclicPos = cursor.cyclicPos;
        auto transportTick = cursor.transportTick;

        // Steps advance clock time evenly; the Time Distortion Map decides where
        // that lands in real time. With a neutral map the two agree.
        const auto realAt = [&] (double clock)
        {
            return cursor.originSec + v.timeDistort.distortClockSeconds (state.tempo, clock);
        };

        auto t = realAt (clockSec);
        const auto baseTicks = ppqn * 4.0 * v.timeBaseNumerator / v.timeBaseDenominator;

        if (outLen <= 0)
        {
            // Nothing to play; keep the clock from spinning forever.
            clockSec += std::max (0.0, windowEnd - t);
            t = realAt (clockSec);
        }
        else
        {
            while (t < windowEnd)
            {
                const auto velocity = velocityForAccent (
                    v.velocityRange,
                    (double) cyclicLevel (state, CyclicKind::accent, vi, cyclicPos, rng));

                const auto legato = cyclicMultiplier (state, CyclicKind::legato, vi, cyclicPos, rng);
                const auto rhythm = cyclicMultiplier (state, CyclicKind::rhythm, vi, cyclicPos, rng);

                const auto nextClockSec = clockSec + stepDur * rhythm;
                const auto nextOnsetSec = realAt (nextClockSec);
                const auto onsetIntervalSec = std::max (0.0, nextOnsetSec - t);

                if (v.playEnabled)
                {
                    NoteSource source {};
                    NoteOrderCursor advanced {};
                    const auto index = nextMixedStepIndex (v.noteOrderMix, order, outLen,
                                                           rng, source, advanced);
                    order = advanced;
                    steps.push_back ({ vi, index });

                    const auto& list = source == NoteSource::cyclic ? pat.scrambledSteps : pat.steps;
                    const auto& step = list[(size_t) index];

                    if (velocity > 0 && ! step.pitches.empty() && gate (v.density, rng))
                    {
                        for (const auto p : step.pitches)
                        {
                            auto n = state.diatonicTranspose
                                ? diatonicTranspose (p, state.root, state.scale, effTrans[vi])
                                : p + effTrans[vi];

                            if (state.scaleSnap)
                                n = snapToScale (n, state.root, state.scale);

                            if (state.chordTones)
                                n = snapToChord (n, state.root, state.scale);

                            for (const auto channel : v.outputChannels)
                            {
                                PlannedNote planned;
                                planned.voice = vi;
                                planned.note = clampMidi ((double) n);
                                planned.velocity = velocity;
                                planned.channel = channel;
                                planned.startSec = t;
                                planned.durationSec = onsetIntervalSec * v.legato * legato;
                                planned.atTick = jsRound (transportTick);
                                planned.durationTicks =
                                    std::max (0.0, jsRound (baseTicks * rhythm * v.legato * legato));
                                planned.source = source;
                                planned.rhythm = rhythm;

                                notes.push_back (planned);
                            }
                        }
                    }
                }

                clockSec = nextClockSec;
                transportTick += jsRound (baseTicks * rhythm);
                t = nextOnsetSec;
                cyclicPos = (cyclicPos + 1) % cyclicPositions;
            }
        }

        VoiceCursor next;
        next.order = order;
        next.nextTimeSec = t;
        next.originSec = cursor.originSec;
        next.clockSec = clockSec;
        next.cyclicPos = cyclicPos;
        next.transportTick = transportTick;

        nextCursors.push_back (next);
    }
}

} // namespace idm
