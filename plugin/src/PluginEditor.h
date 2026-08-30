#pragma once

#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

#include "PopOutWindow.h"

#include <memory>
#include <optional>
#include <vector>

/** WebBrowserComponent that reports when a page has settled. */
class MClassicWebView final : public juce::WebBrowserComponent
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
class MClassicEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit MClassicEditor (MClassicProcessor&);
    ~MClassicEditor() override = default;

    void resized() override;

    /** Pumps host MIDI across to the interface. */
    void timerCallback() override;

private:
    static std::optional<juce::WebBrowserComponent::Resource> provide (const juce::String& path);

   #if MCLASSIC_UI_PROBE
    void probe (int attempt);
    void probeTheme();
   #endif

    void openPopOut (const juce::String& id, const juce::String& title);
    void closePopOut (const juce::String& id);
    void tellUiPopOutClosed (const juce::String& id);

   #if MCLASSIC_UI_PROBE
    /** How many auxiliary windows are open as real OS windows. */
    int popOutCount() const noexcept { return (int) popOuts.size(); }
    juce::String popOutTitles() const;
   #endif

    MClassicProcessor& processorRef;
    MClassicWebView webView;

    /** Owned by the editor, so closing the plugin window takes them with it.
        A leaked window outlives its engine and is unreachable. */
    std::vector<std::unique_ptr<PopOutWindow>> popOuts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MClassicEditor)
};
