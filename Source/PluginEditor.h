#pragma once

#include "PluginProcessor.h"
#include "ui/CustomLookAndFeel.h"
#include "ui/SpectrumAnalyzer.h"
#include <BinaryData.h>

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    static constexpr int width = 700, height = 250;

    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int sliderSize = 100;

    void setupSlider(LabeledSlider& slider, const juce::ParameterID& id, const juce::String& labelText);
    void drawSeparatorLine(juce::Graphics& g, const LabeledSlider& leftSlider, const LabeledSlider& rightSlider);

    juce::Rectangle<int> titleBounds;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;

    LabeledSlider roomSizeSlider, dampingSlider, widthSlider, mixSlider, sendGainSlider, highpassFreqSlider;

    juce::TextButton spectrumBypassButton;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> spectrumBypassAttachment;

    CustomLookAndFeel customLookAndFeel;

    SpectrumAnalyzer spectrumAnalyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
