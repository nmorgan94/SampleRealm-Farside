#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), spectrumAnalyzer(p)
{
    // Set custom LookAndFeel globally for this editor
    setLookAndFeel(&customLookAndFeel);

    setupSlider(preDelaySlider, Parameters::preDelayId, "PRE-DELAY");
    preDelaySlider.getSlider().setTextValueSuffix(" ms");
    setupSlider(roomSizeSlider, Parameters::roomSizeId, "ROOM SIZE");
    setupSlider(dampingSlider, Parameters::dampingId, "DAMPING");
    setupSlider(widthSlider, Parameters::widthId, "WIDTH");
    setupSlider(mixSlider, Parameters::mixId, "MIX");
    setupSlider(highpassFreqSlider, Parameters::highpassFreqId, "HP FREQ");
    highpassFreqSlider.getSlider().setTextValueSuffix(" Hz");
    setupSlider(lowpassFreqSlider, Parameters::lowpassFreqId, "LP FREQ");
    lowpassFreqSlider.getSlider().setTextValueSuffix(" Hz");
    setupSlider(driveSlider, Parameters::driveId, "DRIVE");
    setupSlider(delayNoteSlider, Parameters::delayNoteId, "TIME");
    setupSlider(delayTimeSlider, Parameters::delayTimeId, "TIME");
    delayTimeSlider.getSlider().setTextValueSuffix(" ms");
    setupSlider(delayFeedbackSlider, Parameters::delayFeedbackId, "FEEDBACK");
    setupSlider(delayLevelSlider, Parameters::delayLevelId, "LEVEL");
    setupSlider(duckSlider, Parameters::duckId, "DUCK");

    attachSlider(output.gain, Parameters::outputGainId, "GAIN");
    output.gain.setNameHeight(gainNameHeight);
    output.gain.setLabelHeight(gainLabelHeight);
    output.gain.getSlider().setTextValueSuffix(" dB");

    setupTextToggle(delaySyncButton, "SYNC");
    delaySyncButton.onStateChange = [this] { showDelayTimeKnob(); };

    delaySyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.getAPVTS(), Parameters::delaySyncId.getParamID(), delaySyncButton);
    showDelayTimeKnob();

    setupTextToggle(reverbTypeButton, {});
    reverbTypeButton.setClickingTogglesState(false);
    reverbTypeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(CustomLookAndFeel::primaryRed).withAlpha(0.8f));
    reverbTypeButton.setComponentID(Parameters::reverbTypeId.getParamID());

    auto& reverbType = *processorRef.getAPVTS().getParameter(Parameters::reverbTypeId.getParamID());
    reverbTypeAttachment = std::make_unique<juce::ParameterAttachment>(reverbType, [this, &reverbType](float type)
    {
        reverbTypeButton.setButtonText(reverbType.getText(reverbType.convertTo0to1(type), 0).toUpperCase());
    });
    reverbTypeAttachment->sendInitialUpdate();

    reverbTypeButton.onClick = [this, &reverbType]
    {
        const auto next = juce::roundToInt(reverbType.convertFrom0to1(reverbType.getValue())) + 1;
        reverbTypeAttachment->setValueAsCompleteGesture(static_cast<float>(next % reverbType.getNumSteps()));
    };

    addAndMakeVisible(spectrumAnalyzer);

    addAndMakeVisible(header);
    header.setTitle("SampleRealm: FARSIDE");
    header.setOwnMenuItems([this](juce::PopupMenu& menu)
    {
        const auto bypassed = processorRef.isSpectrumBypassed();

        menu.addItem("Analyser", true, ! bypassed, [this, bypassed]
        {
            auto* bypass = processorRef.getAPVTS().getParameter(Parameters::spectrumBypassId.getParamID());
            bypass->beginChangeGesture();
            bypass->setValueNotifyingHost(bypassed ? 0.0f : 1.0f);
            bypass->endChangeGesture();
        });
    });
    addChildComponent(prompt);
    addChildComponent(manual);

    setSize(width, height);
}

void AudioPluginAudioProcessorEditor::setupTextToggle(juce::TextButton& button, const juce::String& text)
{
    button.setButtonText(text);
    button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::textColourOnId, juce::Colour(CustomLookAndFeel::primaryRed).withAlpha(0.8f));
    button.setColour(juce::TextButton::textColourOffId, juce::Colour(CustomLookAndFeel::offWhite).withAlpha(0.3f));

    // Text only, so the toggle reads as a label
    button.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    button.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);

    addAndMakeVisible(button);
}

void AudioPluginAudioProcessorEditor::setupSlider(LabeledSlider& slider, const juce::ParameterID& id, const juce::String& labelText)
{
    attachSlider(slider, id, labelText);
    slider.setNameHeight(nameHeight);
    slider.setLabelHeight(labelHeight);
    addAndMakeVisible(slider);
}

void AudioPluginAudioProcessorEditor::attachSlider(LabeledSlider& slider, const juce::ParameterID& id, const juce::String& labelText)
{
    // The id lets tests find the knob
    slider.setComponentID(id.getParamID());
    slider.setLabelText(labelText);

    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), id.getParamID(), slider.getSlider()));
}

void AudioPluginAudioProcessorEditor::showDelayTimeKnob()
{
    const auto synced = delaySyncButton.getToggleState();
    delayNoteSlider.setVisible(synced);
    delayTimeSlider.setVisible(! synced);
}

void AudioPluginAudioProcessorEditor::drawStage(juce::Graphics& g, const juce::String& name, int row)
{
    // A small name in a column beside the knobs
    auto column = juce::Rectangle<int>(slidersX - stageGap - stageWidth, rowsTop + rowPitch * row, stageWidth, knobSize).toFloat();

    g.setFont(CustomLookAndFeel::orbitronBold().withPointHeight(8.0f));
    g.setColour(juce::Colour(CustomLookAndFeel::offWhite).withAlpha(0.8f));
    g.drawText(name, column.withY(column.getY() + stageNameTop).withHeight(stageNameHeight), juce::Justification::centred);

    if (row == numStages - 1)
        return;

    // Down to the next stage's name
    auto x = column.getCentreX();
    auto top = column.getY() + stageArrowTop;
    auto tip = column.getY() + rowPitch + stageNameTop - 4;

    g.setColour(juce::Colour(CustomLookAndFeel::primaryRed).withAlpha(0.6f));
    auto base = tip - arrowHead;
    g.drawLine(x, top, x, base, 1.0f);

    juce::Path head;
    head.addTriangle(x - 4.0f, base, x + 4.0f, base, x, tip);
    g.fillPath(head);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    // Remove global LookAndFeel
    setLookAndFeel(nullptr);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient gradient(
        juce::Colour(CustomLookAndFeel::backgroundTop), bounds.getWidth() / 2, 0,
        juce::Colour(CustomLookAndFeel::backgroundBottom), bounds.getWidth() / 2, bounds.getHeight(),
        false);
    g.setGradientFill(gradient);
    g.fillAll();

    const auto offWhite = juce::Colour(CustomLookAndFeel::offWhite);
    juce::AttributedString titleText;
    titleText.setJustification(juce::Justification::centred);
    titleText.append("SampleRealm: ", CustomLookAndFeel::orbitronBold().withPointHeight(28.0f), offWhite);
    titleText.append("FARSIDE", CustomLookAndFeel::orbitronRegular().withPointHeight(28.0f), offWhite);
    titleText.draw(g, spectrumAnalyzer.getBounds().toFloat());

    // Top down, in the order the signal goes through them
    int row = 0;

    for (const auto* name : { "SEND", "DELAY", "REVERB", "OUTPUT" })
        drawStage(g, name, row++);
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    header.setBounds(bounds.removeFromTop(headerLayout.height));

    auto analyzerArea = bounds.removeFromTop(spectrumHeight);
    spectrumAnalyzer.setBounds(analyzerArea);

    int sliderHeight = knobSize + labelHeight;

    // A row per stage: its name in a column on the left, then its sliders, each in the middle of an equal share of the row.
    // The same sum as knobLeft in webui/src/Editor.tsx
    auto layOutRow = [&](int row, std::initializer_list<LabeledSlider*> sliders)
    {
        const auto count = static_cast<int>(sliders.size());
        int column = 0;

        for (auto* slider : sliders)
        {
            const auto left = slidersX + ((2 * column++ + 1) * slidersWidth - count * sliderWidth) / (2 * count);
            slider->setBounds(left, rowsTop + rowPitch * row, sliderWidth, sliderHeight);
        }
    };

    layOutRow(0, { &highpassFreqSlider, &lowpassFreqSlider, &driveSlider });
    layOutRow(1, { &delayNoteSlider, &delayFeedbackSlider, &delayLevelSlider });
    layOutRow(2, { &preDelaySlider, &roomSizeSlider, &dampingSlider, &widthSlider });
    layOutRow(3, { &duckSlider, &mixSlider });

    delayTimeSlider.setBounds(delayNoteSlider.getBounds());

    // Under the knob it switches
    auto underKnob = [](const LabeledSlider& slider)
    {
        return juce::Rectangle<int>(slider.getBounds().getCentreX() - syncWidth / 2, slider.getBottom() + 1, syncWidth, syncHeight);
    };

    delaySyncButton.setBounds(underKnob(delayNoteSlider));
    reverbTypeButton.setBounds(underKnob(roomSizeSlider));
}
