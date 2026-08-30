#pragma once

#include <cstdint>
#include <vector>

namespace mclassic
{

/** One MIDI event on the host's musical timeline. */
struct TimedMidi
{
    double atPpq = 0.0;
    bool isOn = false;
    int channel = 1;   // 1..16
    int note = 0;      // 0..127
    int velocity = 0;  // 0..127
};

/** Decides which notes fall inside a span of the host's timeline.

    Deliberately separate from `processBlock`: the musical decision is a pure
    function of the span, so it can be checked without a host, an audio thread
    or a plugin. This is the seam the ported engine replaces — the planner will
    answer the same question, and `processBlock` will not change.
*/
class StepSource
{
public:
    /** Eighth notes, until the engine decides it per Voice. */
    static constexpr double stepPpq = 0.5;

    /** How long a note is held, as a fraction of the step. */
    static constexpr double gateFraction = 0.9;

    /** Append every event beginning in [ppqStart, ppqEnd) to `out`.

        Note-offs for notes started in earlier spans are the caller's business:
        it holds the sounding set, because only it knows what survived a stop.
    */
    void collect (double ppqStart, double ppqEnd, std::vector<TimedMidi>& out) const;

private:
    /** C minor pentatonic, so a wrong note is audible rather than plausible. */
    static constexpr int scale[] = { 60, 63, 65, 67, 70, 72 };
    static constexpr int scaleSize = 6;
};

} // namespace mclassic
