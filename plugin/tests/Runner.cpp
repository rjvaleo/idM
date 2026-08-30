// The conformance runner. Exit non-zero means the C++ engine and the TypeScript
// engine disagree about something.

#include "Harness.h"

int main()
{
    using namespace idm::conformance;

    checkRandom();
    checkTimeMap();
    checkMusic();
    checkCyclic();
    checkTransform();
    checkPlanner();
    checkEvents();

    const auto& t = totals();
    std::printf ("%s  %d values checked, %d failures\n",
                 t.failures == 0 ? "PASS" : "FAIL", t.checked, t.failures);

    if (t.checked < 700)
    {
        std::printf ("FAIL  only %d values checked; the fixtures look truncated\n", t.checked);
        return 1;
    }

    return t.failures == 0 ? 0 : 1;
}
