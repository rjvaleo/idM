#pragma once

#include <juce_core/juce_core.h>

namespace idm
{

/** A log the plugin writes wherever it is running.

    Both plugin formats drop MIDI on a host decision they never report, and the
    interface only shows what it is told. When something fails inside a DAW
    there is otherwise nothing to read: no error, no log line, no difference on
    screen between a dead engine and a host that is ignoring a working one.

    So the plugin says what it is doing, to
    `~/Library/Logs/Idm.log` (or the platform equivalent). Rate-limited,
    because processBlock runs hundreds of times a second and a log that costs
    real time would change what it is measuring.
*/
class Diagnostics
{
public:
    static Diagnostics& get();

    /** Write a line. Cheap enough to call from the message thread; never call
        it from the audio thread. */
    void log (const juce::String& line);

    /** Write a line at most once every `intervalMs`, keyed by `tag`, so a
        per-block fact can be recorded without flooding. */
    void logThrottled (const juce::String& tag, const juce::String& line, int intervalMs = 2000);

    juce::File file() const { return logFile; }

private:
    Diagnostics();

    juce::File logFile;
    juce::CriticalSection lock;
    std::map<juce::String, juce::int64> lastWrite;
};

} // namespace idm
