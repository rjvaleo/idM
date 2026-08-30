// The note source, checked without a host.
//
// `processBlock` asks one question — what plays between these two musical
// positions — and this is the answer to it. Checking it here means the MIDI
// path can be verified before a DAW is involved.

#include "Harness.h"

#include "../engine/StepSource.h"

#include <cmath>

namespace mclassic::conformance
{

void checkStepSource()
{
    const StepSource source;

    // One bar at 4/4 in a single span: eight eighth notes, each with a release.
    {
        std::vector<TimedMidi> events;
        source.collect (0.0, 4.0, events);

        int ons = 0, offs = 0;
        for (const auto& e : events)
            (e.isOn ? ons : offs)++;

        expect (ons, 8, "one bar should hold eight eighth-note attacks");
        expect (offs, 8, "every attack needs a release");
        expectBits (events.front().atPpq, 0.0, "the first attack sits on the downbeat");
    }

    // Cut the same bar into 128 blocks. Splitting the span must not change,
    // duplicate or drop a single note - that is what makes block size
    // irrelevant to the music.
    {
        std::vector<TimedMidi> whole, pieces;
        source.collect (0.0, 4.0, whole);

        const auto blockPpq = 4.0 / 128.0;
        for (int i = 0; i < 128; ++i)
            source.collect (i * blockPpq, (i + 1) * blockPpq, pieces);

        expect (pieces.size(), whole.size(), "block size must not change the note count");

        for (size_t i = 0; i < std::min (whole.size(), pieces.size()); ++i)
        {
            expectBits (pieces[i].atPpq, whole[i].atPpq, "split span, event " + std::to_string (i) + " position");
            expect (pieces[i].note, whole[i].note, "split span, event " + std::to_string (i) + " pitch");
            expect (pieces[i].isOn, whole[i].isOn, "split span, event " + std::to_string (i) + " kind");
        }
    }

    // A locate lands on the right step rather than replaying the gap.
    {
        std::vector<TimedMidi> events;
        source.collect (16.0, 16.5, events);

        expect (events.empty(), false, "a locate to bar five should still produce a note");
        expectBits (events.front().atPpq, 16.0, "the note lands on the located beat, not before it");
    }

    // An empty or reversed span asks for nothing.
    {
        std::vector<TimedMidi> events;
        source.collect (2.0, 2.0, events);
        source.collect (3.0, 1.0, events);
        expect (events.empty(), true, "an empty or reversed span yields nothing");
    }

    // Velocities and channels stay in range, so nothing needs clamping later.
    {
        std::vector<TimedMidi> events;
        source.collect (0.0, 8.0, events);

        for (const auto& e : events)
        {
            expect (e.channel >= 1 && e.channel <= 16, true, "channel in range");
            expect (e.note >= 0 && e.note <= 127, true, "note in range");
            expect (e.velocity >= 0 && e.velocity <= 127, true, "velocity in range");
        }
    }
}

} // namespace mclassic::conformance
