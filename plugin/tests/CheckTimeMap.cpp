// The Time Distortion Map, checked against the TypeScript engine.
//
// The fixture carries the map definitions as well as the results, so the two
// languages cannot disagree about what was measured.

#include "Harness.h"

#include "../engine/TimeMap.h"

#include <map>

namespace mclassic::conformance
{
namespace
{

/** `id|length|denominator|x:y;x:y;...` */
std::pair<std::string, TimeMap> parseMap (const std::vector<std::string>& parts)
{
    TimeMap map;
    map.length = std::stod (parts[1]);
    map.denominator = std::stod (parts[2]);

    if (parts.size() > 3 && ! parts[3].empty())
        for (const auto& pair : split (parts[3], ';'))
        {
            const auto xy = split (pair, ':');
            map.points.push_back ({ std::stod (xy[0]), std::stod (xy[1]) });
        }

    return { parts[0], map };
}

} // namespace

void checkTimeMap()
{
    std::map<std::string, TimeMap> maps;

    walk ("timemap.txt", [&] (const std::string& header,
                              const std::vector<std::string>& f,
                              const std::string& at)
    {
        // The map block is pipe-separated; every other block is a comma row
        // naming a map that has already been read.
        if (header.find ("maps:") != std::string::npos)
        {
            const auto joined = [&f]
            {
                std::string s;
                for (size_t i = 0; i < f.size(); ++i)
                    s += (i ? "," : "") + f[i];
                return s;
            }();

            const auto parts = split (joined, '|');
            const auto [id, map] = parseMap (parts);
            maps[id] = map;
            return;
        }

        const auto found = maps.find (f[0]);

        if (found == maps.end())
        {
            std::printf ("  %s: unknown map %s\n", at.c_str(), f[0].c_str());
            ++totals().failures;
            return;
        }

        const auto& map = found->second;

        if (header.find ("normalizeTimeMap") != std::string::npos)
        {
            const auto index = static_cast<size_t> (std::stoul (f[1]));
            const auto got = map.normalized();

            if (index >= got.points.size())
            {
                std::printf ("  %s: %s normalised to %zu points, wanted index %zu\n",
                             at.c_str(), f[0].c_str(), got.points.size(), index);
                ++totals().failures;
                return;
            }

            expectBits (got.points[index].x, fromBits (f[2]), at + " " + f[0] + " point x");
            expectBits (got.points[index].y, fromBits (f[3]), at + " " + f[0] + " point y");
        }
        else if (header.find ("isNeutralTimeMap") != std::string::npos)
        {
            expect (map.isNeutral(), f[1] == "1", at + " " + f[0] + " isNeutral");
        }
        else if (header.find ("realToClock") != std::string::npos)
        {
            expectBits (map.realToClock (fromBits (f[1])), fromBits (f[2]),
                        at + " " + f[0] + " realToClock");
        }
        else if (header.find ("clockToReal") != std::string::npos)
        {
            expectBits (map.clockToReal (fromBits (f[1])), fromBits (f[2]),
                        at + " " + f[0] + " clockToReal");
        }
        else if (header.find ("timeMapSeconds") != std::string::npos)
        {
            expectBits (map.seconds (fromBits (f[1])), fromBits (f[2]),
                        at + " " + f[0] + " seconds");
        }
        else if (header.find ("distortClockSeconds") != std::string::npos)
        {
            expectBits (map.distortClockSeconds (fromBits (f[1]), fromBits (f[2])),
                        fromBits (f[3]), at + " " + f[0] + " distort");
        }
    });

    expect (maps.size(), size_t { 12 }, "timemap.txt: expected 12 maps");
}

} // namespace mclassic::conformance
