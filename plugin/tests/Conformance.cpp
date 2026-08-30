// The C++ engine, checked against the TypeScript one.
//
// `src/engine/__goldens__/` is emitted by the real TypeScript implementation.
// Every value here therefore came out of the engine this port replaces, and a
// single differing bit fails the build. Run with no arguments; a non-zero exit
// means a divergence.

#include "../engine/Random.h"

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

int failures = 0;
int checked = 0;

std::string fixturePath (const std::string& name)
{
    return std::string (MCLASSIC_GOLDENS_DIR) + "/" + name;
}

std::string readFixture (const std::string& name)
{
    std::ifstream file (fixturePath (name));

    if (! file)
    {
        std::printf ("cannot read %s\n  run: npm run goldens\n", fixturePath (name).c_str());
        std::exit (2);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<std::string> split (const std::string& line, char delimiter)
{
    std::vector<std::string> out;
    std::stringstream stream (line);
    std::string part;

    while (std::getline (stream, part, delimiter))
        out.push_back (part);

    return out;
}

double fromBits (const std::string& hex)
{
    const auto bits = std::stoull (hex, nullptr, 16);
    double value = 0.0;
    static_assert (sizeof (value) == sizeof (uint64_t), "double must be 64 bits");
    std::memcpy (&value, &bits, sizeof (value));
    return value;
}

template <typename T>
void expect (const T& got, const T& want, const std::string& where)
{
    ++checked;

    if (got == want)
        return;

    if (++failures <= 10)
    {
        std::ostringstream message;
        message << where << "\n    got  " << got << "\n    want " << want;
        std::printf ("  FAIL %s\n", message.str().c_str());
    }
}

/** Which block of `rng.txt` the parser is inside. */
enum class Section { none, rawDraws, intBelow, pickAvoiding, walk };

Section sectionFor (const std::string& header)
{
    if (header.find ("raw u32 draws") != std::string::npos)    return Section::rawDraws;
    if (header.find ("Rng.int") != std::string::npos)          return Section::intBelow;
    if (header.find ("pickIndexAvoiding") != std::string::npos) return Section::pickAvoiding;
    if (header.find ("BrownianWalk") != std::string::npos)     return Section::walk;
    return Section::none;
}

void checkRandom()
{
    const auto text = readFixture ("rng.txt");
    std::stringstream stream (text);
    std::string line;
    int lineNumber = 0;

    auto section = Section::none;
    mclassic::Random rng { 0 };
    mclassic::BrownianWalk walk { mclassic::Random { 0 } };

    while (std::getline (stream, line))
    {
        ++lineNumber;

        if (line.empty())
            continue;

        if (line[0] == '#')
        {
            section = sectionFor (line);
            continue;
        }

        const auto f = split (line, ',');
        const auto at = "rng.txt:" + std::to_string (lineNumber);

        if (section == Section::rawDraws)
        {
            const auto seed = static_cast<uint32_t> (std::stoul (f[0]));
            const auto index = std::stoi (f[1]);
            const auto want = static_cast<uint32_t> (std::stoul (f[2]));

            if (index == 0)
                rng = mclassic::Random { seed };

            expect (rng.nextU32(), want, at + " seed " + f[0] + " draw " + f[1]);
        }
        else if (section == Section::intBelow)
        {
            const auto seed = static_cast<uint32_t> (std::stoul (f[0]));
            const auto n = static_cast<uint32_t> (std::stoul (f[1]));
            const auto index = std::stoi (f[2]);
            const auto want = static_cast<uint32_t> (std::stoul (f[3]));

            if (index == 0)
                rng = mclassic::Random { seed };

            expect (rng.intBelow (n), want, at + " int(" + f[1] + ") #" + f[2]);
        }
        else if (section == Section::pickAvoiding)
        {
            const auto seed = static_cast<uint32_t> (std::stoul (f[0]));
            const auto n = static_cast<uint32_t> (std::stoul (f[1]));
            const auto avoid = static_cast<uint32_t> (std::stoul (f[2]));
            const auto index = std::stoi (f[3]);
            const auto want = static_cast<uint32_t> (std::stoul (f[4]));

            if (index == 0)
                rng = mclassic::Random { seed };

            expect (rng.pickIndexAvoiding (n, avoid),
                    want,
                    at + " pickIndexAvoiding(" + f[1] + ", " + f[2] + ") #" + f[3]);
        }
        else if (section == Section::walk)
        {
            const auto seed = static_cast<uint32_t> (std::stoul (f[0]));
            const auto index = std::stoi (f[1]);
            const auto want = fromBits (f[2]);

            if (index == 0)
                walk = mclassic::BrownianWalk { mclassic::Random { seed } };

            // Compared as bits: "close enough" is not the standard, because the
            // planner's arithmetic runs on top of these values.
            uint64_t gotBits = 0, wantBits = 0;
            const auto got = walk.next();
            std::memcpy (&gotBits, &got, sizeof (gotBits));
            std::memcpy (&wantBits, &want, sizeof (wantBits));

            expect (gotBits, wantBits, at + " walk #" + f[1]);
        }
        else
        {
            std::printf ("  %s: value outside any known section\n", at.c_str());
            ++failures;
        }
    }
}

} // namespace

int main()
{
    checkRandom();

    std::printf ("%s  %d values checked, %d failures\n",
                 failures == 0 ? "PASS" : "FAIL", checked, failures);

    if (checked < 700)
    {
        std::printf ("FAIL  only %d values checked; the fixture looks truncated\n", checked);
        return 1;
    }

    return failures == 0 ? 0 : 1;
}
