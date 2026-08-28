#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

/** Placeholder panel at the settled 1000 x 460. The real editor hosts the
    existing React windows in a webview; nothing here is meant to survive M3. */
class MClassicEditor : public juce::AudioProcessorEditor
{
public:
    explicit MClassicEditor (MClassicProcessor&);
    ~MClassicEditor() override = default;

    void paint (juce::Graphics&) override;

private:
    MClassicProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MClassicEditor)
};
