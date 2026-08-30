#include "Diagnostics.h"

namespace idm
{

Diagnostics& Diagnostics::get()
{
    static Diagnostics shared;
    return shared;
}

Diagnostics::Diagnostics()
{
    logFile = juce::FileLogger::getSystemLogFileFolder().getChildFile ("Idm.log");
    logFile.getParentDirectory().createDirectory();

    // Started fresh each session, so what is in it is this run rather than
    // an archaeology of every run since the plugin was installed.
    logFile.replaceWithText ({});

    log ("--- idM started ---");
}

void Diagnostics::log (const juce::String& line)
{
    const juce::ScopedLock guard (lock);

    logFile.appendText (juce::Time::getCurrentTime().toString (false, true, true, true)
                        + "  " + line + juce::newLine);
}

void Diagnostics::logThrottled (const juce::String& tag, const juce::String& line, int intervalMs)
{
    const auto now = juce::Time::currentTimeMillis();

    {
        const juce::ScopedLock guard (lock);
        const auto previous = lastWrite.find (tag);

        if (previous != lastWrite.end() && now - previous->second < intervalMs)
            return;

        lastWrite[tag] = now;
    }

    log (line);
}

} // namespace idm
