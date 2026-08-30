#include "PopOutWindow.h"

PopOutWindow::PopOutWindow (const juce::String& windowId,
                            const juce::String& title,
                            Provider provider,
                            std::function<void()> onClosed)
    : DocumentWindow (title,
                      juce::Colours::white,
                      DocumentWindow::closeButton | DocumentWindow::minimiseButton),
      id (windowId),
      closed (std::move (onClosed))
{
    auto* view = new juce::WebBrowserComponent (
        juce::WebBrowserComponent::Options {}.withResourceProvider (std::move (provider)));

    setUsingNativeTitleBar (true);
    setContentOwned (view, true);
    setResizable (true, false);

    // Sized generously and centred; the interface's own layout decides what the
    // window actually needs, and the user can drag it from there.
    centreWithSize (520, 420);

    // The fragment is not sent to the resource provider, so the same bundle is
    // served and the page reads the fragment to decide what to show.
    view->goToURL (juce::WebBrowserComponent::getResourceProviderRoot() + "#detached=" + id);

    setVisible (true);
    toFront (true);
}

void PopOutWindow::closeButtonPressed()
{
    if (closed != nullptr)
        closed();
}
