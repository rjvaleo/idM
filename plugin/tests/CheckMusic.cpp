// Scales, snapping and transposition, checked against the TypeScript engine.
//
// Notes outside 0..127 are in the fixture deliberately: the arithmetic is
// modular, JavaScript's `%` keeps the sign of the dividend, and a port that
// uses C++'s `%` passes every positive case.

#include "Harness.h"

#include "../engine/Music.h"

namespace idm::conformance
{
namespace
{

Scale scaleNamed (const std::string& name)
{
    Scale scale {};

    if (! scaleFromName (name, scale))
    {
        std::printf ("  unknown scale %s\n", name.c_str());
        ++totals().failures;
    }

    return scale;
}

} // namespace

void checkMusic()
{
    walk ("music.txt", [] (const std::string& header,
                           const std::vector<std::string>& f,
                           const std::string& at)
    {
        if (header.find ("scales:") != std::string::npos)
        {
            const auto degrees = degreesOf (scaleNamed (f[0]));
            const auto want = split (f[1], ';');

            expect ((size_t) degrees.count, want.size(), at + " " + f[0] + " degree count");

            for (size_t i = 0; i < want.size() && (int) i < degrees.count; ++i)
                expect (degrees[(int) i], std::stoi (want[i]),
                        at + " " + f[0] + " degree " + std::to_string (i));
        }
        else if (header.find ("snapToScale") != std::string::npos)
        {
            expect (snapToScale (std::stoi (f[2]), std::stoi (f[1]), scaleNamed (f[0])),
                    std::stoi (f[3]),
                    at + " snapToScale(" + f[2] + ", " + f[1] + ", " + f[0] + ")");
        }
        else if (header.find ("snapToChord") != std::string::npos)
        {
            expect (snapToChord (std::stoi (f[2]), std::stoi (f[1]), scaleNamed (f[0])),
                    std::stoi (f[3]),
                    at + " snapToChord(" + f[2] + ", " + f[1] + ", " + f[0] + ")");
        }
        else if (header.find ("diatonicTranspose") != std::string::npos)
        {
            expect (diatonicTranspose (std::stoi (f[2]), std::stoi (f[1]),
                                       scaleNamed (f[0]), std::stoi (f[3])),
                    std::stoi (f[4]),
                    at + " diatonicTranspose(" + f[2] + ", " + f[1] + ", " + f[0] + ", " + f[3] + ")");
        }
        else if (header.find ("clampMidi") != std::string::npos)
        {
            expect (clampMidi (fromBits (f[0])), std::stoi (f[1]), at + " clampMidi");
        }
        else if (header.find ("midiToName") != std::string::npos)
        {
            expect (midiToName (std::stoi (f[0])), f[1], at + " midiToName(" + f[0] + ")");
        }
    });
}

} // namespace idm::conformance
