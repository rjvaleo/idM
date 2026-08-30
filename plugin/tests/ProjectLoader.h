#pragma once

#include "../engine/Project.h"

#include <string>

namespace mclassic::conformance
{

/** Load a project the TypeScript engine emitted.

    Reading the state rather than rebuilding it isolates whatever is under test:
    a divergence is then the planner's, never a project builder's.
*/
ProjectState loadProject (const std::string& fixtureName);

} // namespace mclassic::conformance
