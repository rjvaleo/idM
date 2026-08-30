#pragma once

// Shared plumbing for the conformance runner.
//
// `src/engine/__goldens__/` is emitted by the real TypeScript implementation,
// so every value checked here came out of the engine this port replaces. The
// fixtures are plain text; the check is a comparison of values, not of
// implementations.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <functional>
#include <vector>

namespace mclassic::conformance
{

/** Running totals across every module's checker. */
struct Totals
{
    int checked = 0;
    int failures = 0;
};

Totals& totals();

std::string readFixture (const std::string& name);

std::vector<std::string> split (const std::string& line, char delimiter);

/** A double from its exact bit pattern. */
inline double fromBits (const std::string& hex)
{
    const auto bits = std::stoull (hex, nullptr, 16);
    double value = 0.0;
    std::memcpy (&value, &bits, sizeof (value));
    return value;
}

inline uint64_t toBits (double value)
{
    uint64_t bits = 0;
    std::memcpy (&bits, &value, sizeof (bits));
    return bits;
}

template <typename T>
void expect (const T& got, const T& want, const std::string& where)
{
    auto& t = totals();
    ++t.checked;

    if (got == want)
        return;

    if (++t.failures <= 12)
    {
        std::ostringstream message;
        message << where << "\n    got  " << got << "\n    want " << want;
        std::printf ("  FAIL %s\n", message.str().c_str());
    }
}

/** Floats are compared as bit patterns: "close enough" is not the standard,
    because the planner's arithmetic runs on top of these values.
*/
inline void expectBits (double got, double want, const std::string& where)
{
    expect (toBits (got), toBits (want), where);
}

/** Walk a fixture, handing each non-comment line to `visit` with the header of
    the section it sits under and a `file:line` label for messages.
*/
void walk (const std::string& name,
           const std::function<void (const std::string& section,
                                     const std::vector<std::string>& fields,
                                     const std::string& at)>& visit);

// Each module contributes one of these.
void checkRandom();
void checkTimeMap();
void checkMusic();
void checkCyclic();
void checkTransform();
void checkPlanner();
void checkEvents();

} // namespace mclassic::conformance
