#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

/** One auxiliary window, as a real OS window.

    The panel is fixed at 1000 x 460 and the auxiliary editors do not fit in it.
    They are not overlays and the panel does not grow: each opens as a window you
    can move, resize and put on a second monitor, which is what the interface
    does outside a plugin too.

    It is another webview loading the same bundle, told by the URL fragment which
    window it is showing. Created on open and destroyed on close rather than
    pooled, because ten idle webviews is a real cost for windows that are mostly
    shut.
*/
class PopOutWindow final : public juce::DocumentWindow
{
public:
    using Provider = std::function<std::optional<juce::WebBrowserComponent::Resource> (const juce::String&)>;

    PopOutWindow (const juce::String& windowId,
                  const juce::String& title,
                  Provider provider,
                  std::function<void()> onClosed);

    /** The window's own close button. The owner is told so its record of what
        is open stays true, and so the session saves the right thing. */
    void closeButtonPressed() override;

    const juce::String& windowId() const noexcept { return id; }

private:
    juce::String id;
    std::function<void()> closed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PopOutWindow)
};
