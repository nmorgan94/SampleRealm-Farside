#pragma once

#include "../PluginProcessor.h"
#include "../shared/ParameterSpecs.h"
#include "../ui/SpectrumScope.h"

//==============================================================================
class WebViewEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit WebViewEditor (AudioPluginAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::WebBrowserComponent::Options createOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    AudioPluginAudioProcessor& processorRef;

    template <typename Relay>
    using RelaysBySpec = std::vector<std::pair<const ParameterSpecs::Spec*, std::unique_ptr<Relay>>>;

    // Declaration order matters: relays, browser, attachments
    RelaysBySpec<juce::WebSliderRelay> sliderRelays;
    RelaysBySpec<juce::WebToggleButtonRelay> toggleRelays;
    std::unique_ptr<juce::WebBrowserComponent> browser;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> toggleAttachments;

    SpectrumScope scope;

    juce::MemoryInputStream uiZipStream;
    juce::ZipFile uiZip { uiZipStream };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewEditor)
};
