// The page side of these names is webui/src/plugin/juceBackend.ts
#include "WebViewEditor.h"
#include "../PluginEditor.h"
#include "../shared/ParameterSpecs.h"
#include "../shared/UiInfoJson.h"
#include "../ui/About.h"
#include <WebUIData.h>

#if JUCE_MAC
 #import <Foundation/Foundation.h>
#endif

namespace
{
    const juce::Colour background { CustomLookAndFeel::backgroundTop };

    // Sent as thousandths: JSON writes a float with all its digits
    constexpr float spectrumScale = 1000.0f;

    const char* mimeTypeFor (const juce::String& fileName)
    {
        static const std::unordered_map<juce::String, const char*> types {
            { "html", "text/html" },
            { "js",   "text/javascript" },
            { "css",  "text/css" },
            { "ttf",  "font/ttf" },
            { "json", "application/json" }
        };

        const auto it = types.find (fileName.fromLastOccurrenceOf (".", false, false).toLowerCase());
        return it != types.end() ? it->second : "application/octet-stream";
    }

    std::vector<std::byte> readAll (juce::InputStream& stream)
    {
        std::vector<std::byte> bytes (static_cast<size_t> (stream.getTotalLength()));
        stream.read (bytes.data(), static_cast<int> (bytes.size()));
        return bytes;
    }

    struct SinglePageBrowser final : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;

        bool pageAboutToLoad (const juce::String& newURL) override
        {
            return newURL == getResourceProviderRoot();
        }
    };

    // The Mac web view is white until the page draws, and JUCE has no option for it
    void hideMacWebViewBackground ([[maybe_unused]] juce::WebBrowserComponent& browser)
    {
       #if JUCE_MAC
        for (auto* child : browser.getChildren())
            if (auto* host = dynamic_cast<juce::NSViewComponent*> (child))
                [(id) host->getView() setValue: @NO forKey: @"drawsBackground"];
       #endif
    }
}

//==============================================================================
WebViewEditor::WebViewEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      uiZipStream (WebUIData::WebUI_zip, static_cast<size_t> (WebUIData::WebUI_zipSize), false)
{
    for (const auto* spec : ParameterSpecs::all)
    {
        if (spec->kind == ParameterSpecs::Kind::toggle)
            toggleRelays.emplace_back (spec, std::make_unique<juce::WebToggleButtonRelay> (spec->id));
        else
            sliderRelays.emplace_back (spec, std::make_unique<juce::WebSliderRelay> (spec->id));
    }

    browser = std::make_unique<SinglePageBrowser> (createOptions());
    hideMacWebViewBackground (*browser);

    auto& apvts = processorRef.getAPVTS();

    for (auto& [spec, relay] : sliderRelays)
        sliderAttachments.push_back (std::make_unique<juce::WebSliderParameterAttachment> (*apvts.getParameter (spec->id), *relay, nullptr));

    for (auto& [spec, relay] : toggleRelays)
        toggleAttachments.push_back (std::make_unique<juce::WebToggleButtonParameterAttachment> (*apvts.getParameter (spec->id), *relay, nullptr));

    addAndMakeVisible (*browser);
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setSize (AudioPluginAudioProcessorEditor::width, AudioPluginAudioProcessorEditor::height);
    startTimerHz (SpectrumScope::updateHz);
}

juce::WebBrowserComponent::Options WebViewEditor::createOptions()
{
    auto options = juce::WebBrowserComponent::Options{}
                       .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                       .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                                                    .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory))
                                                    .withBackgroundColour (background))
                       .withNativeIntegrationEnabled()
                       .withResourceProvider ([this] (const auto& url) { return getResource (url); });

    for (auto& entry : sliderRelays)
        options = options.withOptionsFrom (*entry.second);

    for (auto& entry : toggleRelays)
        options = options.withOptionsFrom (*entry.second);

    return options
        .withInitialisationData ("uiInfo", juce::String (uiInfoJson (JucePlugin_VersionString, About::commit)))
        .withInitialisationData ("running", About::running (processorRef));
}

std::optional<juce::WebBrowserComponent::Resource> WebViewEditor::getResource (const juce::String& url)
{
    const auto path = url == "/" ? juce::String ("index.html") : url.fromFirstOccurrenceOf ("/", false, false);

    if (auto* entry = uiZip.getEntry (path))
    {
        const std::unique_ptr<juce::InputStream> stream (uiZip.createStreamForEntry (*entry));

        if (stream != nullptr)
            return juce::WebBrowserComponent::Resource { readAll (*stream), mimeTypeFor (path) };
    }

    return std::nullopt;
}

void WebViewEditor::timerCallback()
{
    if (! scope.update (processorRef.getSpectrumTap(), processorRef.getSampleRate(), processorRef.isSpectrumBypassed()))
        return;

    juce::Array<juce::var> points;
    points.ensureStorageAllocated (SpectrumScope::scopeSize);

    for (const auto point : scope.getScope())
        points.add (juce::roundToInt (point * spectrumScale));

    browser->emitEventIfBrowserIsVisible ("spectrum", points);
}

void WebViewEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
}

void WebViewEditor::resized()
{
    browser->setBounds (getLocalBounds());
}
