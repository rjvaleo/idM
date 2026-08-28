#include "PluginEditor.h"

MClassicEditor::MClassicEditor (MClassicProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (1000, 460);
    setResizable (false, false);
}

void MClassicEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::white);
    g.setColour (juce::Colours::black);
    g.setFont (16.0f);
    g.drawFittedText ("M Classic - toolchain scaffold", getLocalBounds(),
                      juce::Justification::centred, 1);
}
