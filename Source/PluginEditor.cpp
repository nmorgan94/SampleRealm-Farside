#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), spectrumAnalyzer(p)
{
    // Set custom LookAndFeel globally for this editor
    setLookAndFeel(&customLookAndFeel);

    setupSlider(roomSizeSlider, Parameters::roomSizeId, "ROOM SIZE");
    setupSlider(dampingSlider, Parameters::dampingId, "DAMPING");
    setupSlider(widthSlider, Parameters::widthId, "WIDTH");
    setupSlider(mixSlider, Parameters::mixId, "MIX");
    setupSlider(sendGainSlider, Parameters::sendGainId, "SEND GAIN");
    setupSlider(highpassFreqSlider, Parameters::highpassFreqId, "HP FREQ");
    highpassFreqSlider.getSlider().setTextValueSuffix(" Hz");

    addAndMakeVisible(spectrumAnalyzer);

    // Lit while the analyzer runs, dimmed once it is bypassed
    addAndMakeVisible(spectrumBypassButton);
    spectrumBypassButton.setButtonText("ANALYSER");
    spectrumBypassButton.setClickingTogglesState(true);

    spectrumBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.getAPVTS(), Parameters::spectrumBypassId.getParamID(), spectrumBypassButton);

    setSize(width, height);
}

void AudioPluginAudioProcessorEditor::setupSlider(LabeledSlider& slider, const juce::ParameterID& id, const juce::String& labelText)
{
    // The id lets tests find the knob
    slider.setComponentID(id.getParamID());
    slider.setLabelText(labelText);
    addAndMakeVisible(slider);

    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), id.getParamID(), slider.getSlider()));
}

void AudioPluginAudioProcessorEditor::drawSeparatorLine(juce::Graphics& g,
                                                        const LabeledSlider& leftSlider,
                                                        const LabeledSlider& rightSlider)
{
    g.setColour(juce::Colour(0xffe94560)); // Red color

    auto leftBounds = leftSlider.getBounds();
    auto rightBounds = rightSlider.getBounds();

    // Calculate line position (centered between sliders)
    float lineX = (leftBounds.getRight() + rightBounds.getX()) / 2.0f;
    float lineY1 = leftBounds.getY();
    float lineY2 = leftBounds.getY() + sliderSize;

    g.drawLine(lineX, lineY1, lineX, lineY2, 1.0f);
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

    g.setColour(juce::Colour(0xfff0f0f0));
    juce::AttributedString titleText;
    titleText.setJustification(juce::Justification::centred);
    titleText.append("SampleRealm: ", CustomLookAndFeel::orbitronBold().withPointHeight(28.0f), juce::Colour(0xfff0f0f0));
    titleText.append("REVERB", CustomLookAndFeel::orbitronRegular().withPointHeight(28.0f), juce::Colour(0xfff0f0f0));
    titleText.draw(g, titleBounds.toFloat());
    
    // Draw version number small in bottom right of title area
    g.setFont(CustomLookAndFeel::orbitronRegular().withPointHeight(9.0f));
    g.setColour(juce::Colour(0xffe94560).withAlpha(0.8f));
    auto versionArea = juce::Rectangle<int>(titleBounds.getRight() - 50, titleBounds.getBottom() - 12, 50, 12);
    g.drawText("v" + juce::String(JucePlugin_VersionString), versionArea, juce::Justification::centredRight);

    drawSeparatorLine(g, sendGainSlider, roomSizeSlider);  // Pre-reverb | Reverb
    drawSeparatorLine(g, widthSlider, mixSlider);          // Reverb | Mix
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    auto analyzerArea = juce::Rectangle<int>(0, 0, bounds.getWidth(), 100);
    spectrumAnalyzer.setBounds(analyzerArea);
    
    titleBounds = juce::Rectangle<int>(0, 0, bounds.getWidth(), 100);

    // Tucked into the top right corner, above the version number
    spectrumBypassButton.setBounds(titleBounds.getRight() - 66, 6, 58, 20);

    // What the row is centred by, taller than a knob with its label
    int rowHeight = sliderSize + 30;
    auto sliderArea = juce::Rectangle<int>(0, 100, bounds.getWidth(), bounds.getHeight() - 100);
    
    // Layout sliders in a row within slider area
    int spacing = 15;
    int totalWidth = (sliderSize * 6) + (spacing * 5);
    int startX = (sliderArea.getWidth() - totalWidth) / 2;
    int startY = (sliderArea.getHeight() - rowHeight) / 2;
    
    int sliderHeight = sliderSize + LabeledSlider::labelHeight;

    highpassFreqSlider.setBounds(startX, 100 + startY, sliderSize, sliderHeight);
    sendGainSlider.setBounds(startX + (sliderSize + spacing), 100 + startY, sliderSize, sliderHeight);
    roomSizeSlider.setBounds(startX + (sliderSize + spacing) * 2, 100 + startY, sliderSize, sliderHeight);
    dampingSlider.setBounds(startX + (sliderSize + spacing) * 3, 100 + startY, sliderSize, sliderHeight);
    widthSlider.setBounds(startX + (sliderSize + spacing) * 4, 100 + startY, sliderSize, sliderHeight);
    mixSlider.setBounds(startX + (sliderSize + spacing) * 5, 100 + startY, sliderSize, sliderHeight);
}
