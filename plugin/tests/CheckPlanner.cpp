// The planner, checked against the committed traces.
//
// This is the gate the whole port exists for. The project state is read from
// the JSON the TypeScript emitted rather than rebuilt here, so a divergence is
// the planner's and not a project builder's.

#include "Harness.h"
#include "ProjectLoader.h"

#include "../engine/Planner.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace mclassic::conformance
{
namespace
{

/** Golden ratio in 32 bits — the constant `traceProject` decorrelates the
    per-Voice seeds with. */
constexpr uint32_t seedStride = 0x9e3779b1u;

std::vector<Random> traceRngs (size_t voices, uint32_t seed)
{
    std::vector<Random> rngs;
    rngs.reserve (voices);

    for (size_t voice = 0; voice < voices; ++voice)
        rngs.push_back (Random { seed ^ ((uint32_t) (voice + 1) * seedStride) });

    return rngs;
}

/** Format the way `Array.prototype.join` would. Every value written here is a
    `Math.round` result, so it is integral. */
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

struct Row
{
    double atTick;
    size_t voice;
    int channel;
    int note;
    int velocity;
    std::string text;
};

/** `traceDefaultProject`'s defaults: seed 1, eight seconds, four windows. */
std::string traceProject (const ProjectState& project, uint32_t seed,
                          double spanSec = 8.0, int windows = 4)
{
    auto rngs = traceRngs (project.voices.size(), seed);
    auto cursors = makeCursors (project, 0.0);

    std::vector<Row> rows;
    std::vector<PlannedNote> notes;
    std::vector<VoiceCursor> next;
    std::vector<PlannedStep> steps;

    const auto step = spanSec / (double) windows;

    for (int w = 0; w < windows; ++w)
    {
        planWindow (project, cursors, rngs, step * w, step * (w + 1), notes, next, steps);
        cursors = next;

        for (const auto& note : notes)
            rows.push_back ({ note.atTick, note.voice, note.channel, note.note, note.velocity,
                              jsNumber (note.atTick) + "," + std::to_string (note.voice) + ","
                                  + std::to_string (note.channel) + "," + std::to_string (note.note)
                                  + "," + std::to_string (note.velocity) + ","
                                  + jsNumber (note.durationTicks) });
    }

    // Sorted, not emission-ordered, and stable so notes agreeing on all five
    // keys keep the order they were planned in.
    std::stable_sort (rows.begin(), rows.end(), [] (const Row& a, const Row& b)
    {
        if (a.atTick != b.atTick)   return a.atTick < b.atTick;
        if (a.voice != b.voice)     return a.voice < b.voice;
        if (a.channel != b.channel) return a.channel < b.channel;
        if (a.note != b.note)       return a.note < b.note;
        return a.velocity < b.velocity;
    });

    std::string out;

    for (size_t i = 0; i < rows.size(); ++i)
        out += (i ? "\n" : "") + rows[i].text;

    return out;
}

/** Everything a trace leaves out: the seconds, the Rhythm multiplier and the
    Note Order source, in emission order. */
std::string traceDetail (const ProjectState& project, uint32_t seed,
                         double spanSec = 8.0, int windows = 4)
{
    auto rngs = traceRngs (project.voices.size(), seed);
    auto cursors = makeCursors (project, 0.0);

    std::vector<std::string> rows;
    std::vector<PlannedNote> notes;
    std::vector<VoiceCursor> next;
    std::vector<PlannedStep> steps;

    const auto step = spanSec / (double) windows;

    for (int w = 0; w < windows; ++w)
    {
        planWindow (project, cursors, rngs, step * w, step * (w + 1), notes, next, steps);
        cursors = next;

        for (const auto& note : notes)
            rows.push_back (jsNumber (note.atTick) + "," + std::to_string (note.voice) + ","
                            + std::to_string (note.channel) + "," + std::to_string (note.note) + ","
                            + std::to_string (note.velocity) + "," + jsNumber (note.durationTicks)
                            + "," + nameOf (note.source) + "," + hex16 (note.startSec) + ","
                            + hex16 (note.durationSec) + "," + hex16 (note.rhythm));
    }

    std::string out;

    for (size_t i = 0; i < rows.size(); ++i)
        out += (i ? "\n" : "") + rows[i];

    return out;
}

void compare (const std::string& got, const std::string& fixtureName)
{
    auto want = readFixture (fixtureName);

    while (! want.empty() && want.back() == '\n')
        want.pop_back();

    const auto gotLines = split (got, '\n');
    const auto wantLines = split (want, '\n');

    // Two empty strings compare equal; guard against a planner that produces
    // nothing agreeing with a fixture nobody noticed was blank.
    expect (wantLines.size() >= 8, true, fixtureName + " looks empty");
    expect (gotLines.empty(), false, fixtureName + ": the planner produced no notes at all");
    expect (gotLines.size(), wantLines.size(), fixtureName + " note count");

    for (size_t i = 0; i < std::min (gotLines.size(), wantLines.size()); ++i)
        expect (gotLines[i], wantLines[i], fixtureName + " line " + std::to_string (i + 1));
}

std::string padded (int voices)
{
    return voices < 10 ? "0" + std::to_string (voices) : std::to_string (voices);
}

} // namespace

void checkPlanner()
{
    for (const auto voices : { 1, 4, 8, 16 })
    {
        const auto n = padded (voices);

        compare (traceProject (loadProject ("project-" + n + ".json"), 1), "voices-" + n + ".trace");
        compare (traceProject (loadProject ("rich-project-" + n + ".json"), 1), "rich-" + n + ".trace");
        compare (traceProject (loadProject ("guard-project-" + n + ".json"), 1), "guard-" + n + ".trace");
        compare (traceDetail (loadProject ("rich-project-" + n + ".json"), 1), "detail-" + n + ".txt");
    }
}

} // namespace mclassic::conformance
