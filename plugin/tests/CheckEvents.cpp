// The note lifecycle, checked against the TypeScript engine.
//
// This is the layer a golden trace cannot see. A trace stops at planned notes;
// everything that decides what reaches a MIDI port happens after that.

#include "Harness.h"
#include "ProjectLoader.h"

#include "../engine/Events.h"

#include <cmath>
#include <sstream>

namespace idm::conformance
{
namespace
{

constexpr uint32_t seedStride = 0x9e3779b1u;

std::string jsNumber (double value)
{
    if (std::isfinite (value) && value == std::floor (value) && std::abs (value) < 9.0e18)
        return std::to_string ((long long) value);

    std::ostringstream out;
    out << value;
    return out.str();
}

std::string hex16 (double value)
{
    char buffer[32];
    std::snprintf (buffer, sizeof (buffer), "%016llx", (unsigned long long) toBits (value));
    return buffer;
}

std::string padded (int voices)
{
    return voices < 10 ? "0" + std::to_string (voices) : std::to_string (voices);
}

} // namespace

void checkEvents()
{
    for (const auto voices : { 1, 4, 8, 16 })
    {
        const auto n = padded (voices);
        const auto project = loadProject ("rich-project-" + n + ".json");

        std::vector<Random> rngs;
        rngs.reserve (project.voices.size());

        for (size_t voice = 0; voice < project.voices.size(); ++voice)
            rngs.push_back (Random { 1u ^ ((uint32_t) (voice + 1) * seedStride) });

        auto cursors = makeCursors (project, 0.0);
        NoteLifecycle lifecycle;

        // Both, because the ordering rule sorts on the destination and one
        // destination cannot show that it does.
        const std::vector<OutputDestination> destinations {
            OutputDestination::synth, OutputDestination::midi
        };

        std::vector<ProgramChange> programs;

        for (size_t index = 0; index < project.voices.size(); ++index)
            for (const auto channel : project.voices[index].outputChannels)
                programs.push_back ({ index, channel, (int) ((index * 7) % 128) });

        lifecycle.addProgramChanges (0.0, 0.0, programs);

        std::vector<std::string> rows;
        std::vector<PlannedNote> notes;
        std::vector<VoiceCursor> next;
        std::vector<PlannedStep> steps;

        const auto step = 8.0 / 4.0;

        for (int w = 0; w < 4; ++w)
        {
            const auto end = step * (w + 1);
            planWindow (project, cursors, rngs, step * w, end, notes, next, steps);
            cursors = next;

            lifecycle.ingest (notes, destinations);

            for (const auto& event : lifecycle.drainBefore (end))
            {
                const auto isProgram = event.kind == EventKind::programChange;

                rows.push_back (std::string (nameOf (event.kind)) + "," + hex16 (event.atSec) + ","
                                + jsNumber (event.atTick) + "," + std::to_string (event.sequence)
                                + "," + nameOf (event.destination) + ","
                                + std::to_string (event.voice) + "," + std::to_string (event.channel)
                                + "," + (isProgram ? "-1" : std::to_string (event.noteId))
                                + "," + (isProgram ? "-1" : std::to_string (event.note))
                                + "," + (isProgram ? "-1" : std::to_string (event.velocity))
                                + "," + (isProgram ? std::to_string (event.program) : "-1"));
            }

            rows.push_back ("# window " + std::to_string (w) + " drained, "
                            + std::to_string (lifecycle.pendingCount()) + " pending");
        }

        auto want = readFixture ("lifecycle-" + n + ".txt");

        while (! want.empty() && want.back() == '\n')
            want.pop_back();

        const auto wantLines = split (want, '\n');

        expect (rows.size(), wantLines.size(), "lifecycle-" + n + ".txt row count");

        for (size_t i = 0; i < std::min (rows.size(), wantLines.size()); ++i)
            expect (rows[i], wantLines[i],
                    "lifecycle-" + n + ".txt line " + std::to_string (i + 1));
    }
}

} // namespace idm::conformance
