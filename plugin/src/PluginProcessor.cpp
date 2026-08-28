#include "PluginProcessor.h"
#include "PluginEditor.h"

MClassicProcessor::MClassicProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MClassicProcessor::prepareToPlay (double, int) {}

void MClassicProcessor::releaseResources() {}

void MClassicProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear();
}

juce::AudioProcessorEditor* MClassicProcessor::createEditor()
{
    return new MClassicEditor (*this);
}

void MClassicProcessor::getStateInformation (juce::MemoryBlock&) {}

void MClassicProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MClassicProcessor();
}
