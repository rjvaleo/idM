#pragma once

#include "../engine/Project.h"

#include <juce_core/juce_core.h>

namespace idm
{

/** Build a project from the interface's own JSON.

    The same shape `.idm` files carry and the same shape the conformance
    fixtures hold, so the plugin and the tests read state through one path
    rather than two that can drift.

    Missing fields fall back to the default project's, so a partial or older
    document loads rather than failing.
*/
ProjectState projectFromJson (const juce::var& root);

} // namespace idm
