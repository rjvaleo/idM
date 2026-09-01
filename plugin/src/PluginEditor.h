#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>


#include <memory>
#include <optional>
#include <vector>

/** WebBrowserComponent that reports when a page has settled. */
class IdmWebView final : public juce::WebBrowserComponent
{
public:
    using juce::WebBrowserComponent::WebBrowserComponent;

    std::function<void()> onPageLoaded;

    void pageFinishedLoading (const juce::String&) override
    {
        if (onPageLoaded != nullptr)
            onPageLoaded();
    }
};

/** Hosts the browser app's own UI. The windows are not reimplemented here and
    must not be: this class serves the single-file bundle to a WebBrowserComponent
    and gets out of the way. Fixed at the measured 1000 x 460. */
class IdmEditor final : public juce::AudioProcessorEditor
{
public:
    explicit IdmEditor (IdmProcessor&);
    ~IdmEditor() override = default;

    void resized() override;

private:
    static std::optional<juce::WebBrowserComponent::Resource> provide (const juce::String& path);

   #if IDM_UI_PROBE
    void probe (int attempt);
    void probeTheme();
   #endif

    juce::var pollEngine();

   #if IDM_UI_PROBE
    /** How many auxiliary windows are open as real OS windows. */
   #endif

    IdmProcessor& processorRef;
    IdmWebView webView;


    /** Owned by the editor, so closing the plugin window takes them with it.
        A leaked window outlives its engine and is unreachable. */

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IdmEditor)
};
