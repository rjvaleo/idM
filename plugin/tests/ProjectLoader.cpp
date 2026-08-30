#include "ProjectLoader.h"
#include "Harness.h"

#include "../src/ProjectJson.h"

namespace mclassic::conformance
{

ProjectState loadProject (const std::string& fixtureName)
{
    const auto root = juce::JSON::parse (juce::String (readFixture (fixtureName)));

    if (! root.isObject())
    {
        std::printf ("  %s is not valid JSON\n", fixtureName.c_str());
        std::exit (2);
    }

    return projectFromJson (root);
}

} // namespace mclassic::conformance
