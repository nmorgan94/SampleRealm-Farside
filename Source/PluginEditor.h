#pragma once

#include "PluginProcessor.h"
#include "ui/CustomLookAndFeel.h"
#include "ui/SpectrumAnalyzer.h"
#include <BinaryData.h>

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    static constexpr int width = 430, height = 556;

    // The output gain and the meter beside it, in the header's meter slot
    static constexpr int gainWidth = 48, gainLabelHeight = 12, gainGap = 4, meterInset = 2;
    static constexpr float gainNameHeight = 8.0f;

    // As HEADER_LAYOUT in webui/src/Editor.tsx. The title is behind the spectrum instead
    static constexpr HeaderBar::Layout headerLayout { .height = 44, .edge = 10, .titleWidth = 0, .titleHeight = 12.0f,
                                                      .gap = 8, .controlsHeight = 24,
                                                      .meterWidth = gainWidth + gainGap + LevelMeter::preferredWidth, .meterInset = 2 };

    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int sliderWidth = 88, knobSize = 70, labelHeight = 20, knobSpacing = 27, knobsPerRow = 3;
    static constexpr float nameHeight = 13.0f;
    static constexpr int spectrumHeight = 100;
    static constexpr int rowsTop = headerLayout.height + spectrumHeight + 4, rowPitch = 104;
    static constexpr int stageWidth = 64, stageGap = 4, stageNameTop = 27, stageNameHeight = 16, stageArrowTop = 47;
    static constexpr float arrowHead = 6.0f;
    static constexpr int syncWidth = 58, syncHeight = 12;

    void setupSlider(LabeledSlider& slider, const juce::ParameterID& id, const juce::String& labelText);
    void attachSlider(LabeledSlider& slider, const juce::ParameterID& id, const juce::String& labelText);
    void setupTextToggle(juce::TextButton& button, const juce::String& text);
    void drawStage(juce::Graphics& g, const juce::String& name, const LabeledSlider& firstSlider, bool hasNextStage);
    void showDelayTimeKnob();

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;

    LabeledSlider roomSizeSlider, dampingSlider, widthSlider, mixSlider, highpassFreqSlider, lowpassFreqSlider;
    LabeledSlider driveSlider, delayFeedbackSlider, delayLevelSlider, duckSlider;

    // In the same place: the note while synced, the time otherwise
    LabeledSlider delayNoteSlider, delayTimeSlider;

    juce::TextButton delaySyncButton;

    CustomLookAndFeel customLookAndFeel;

    SpectrumAnalyzer spectrumAnalyzer;

    PromptOverlay prompt;
    ManualOverlay manual { juce::String::createStringFromData (BinaryData::Manual_md, BinaryData::Manual_mdSize) };
    struct Output : juce::Component
    {
        explicit Output (PeakMeter& peaks) : meter (peaks)
        {
            addAndMakeVisible (gain);
            addAndMakeVisible (meter);
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            meter.setBounds (bounds.removeFromRight (LevelMeter::preferredWidth).reduced (0, meterInset));
            gain.setBounds (bounds.removeFromLeft (gainWidth));
        }

        LabeledSlider gain;
        LevelMeter meter;
    };

    Output output { processorRef.getOutputMeter() };
    HeaderBar header { juce::String(), headerLayout, processorRef.getPresetManager(), prompt, [this] { manual.show(); },
                       About::text (processorRef), output };

    // After the controls they attach to
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> delaySyncAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
