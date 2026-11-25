#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "shared/EffectSettings.h"
#include <BinaryData.h>

#if PLUGIN_WEBVIEW_EDITOR
 #include <webview_editor/webview_editor.h>
 #include "ui/SpectrumScope.h"
#endif

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

PresetManager::Config AudioPluginAudioProcessor::createPresetConfig()
{
    PresetManager::Config config;
    config.fileExtension = ".farside";
    config.userDirectory = PresetManager::getDefaultUserDirectory (JucePlugin_Manufacturer, JucePlugin_Name);
    config.author        = JucePlugin_Manufacturer;
    config.formatVersion = Parameters::versionHint;

    // The analyser's switch
    config.ignoredParameterIds = PresetManager::performanceControlIds (ParameterSpecs::all);

    // Factory presets are embedded from Assets/Presets alongside the fonts
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const auto* resource = BinaryData::namedResourceList[i];

        if (! juce::String (BinaryData::getNamedResourceOriginalFilename (resource)).endsWithIgnoreCase (config.fileExtension))
            continue;

        int size = 0;

        if (const auto* data = BinaryData::getNamedResource (resource, size))
            config.factoryPresets.emplace_back (data, (size_t) size);
    }

    return config;
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    // Long enough for most settings: high feedback or a full room rings for longer
    return 10.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1. Presets are managed in the plugin UI instead.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    effect.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels(), getTotalNumOutputChannels());
    spectrumTap.reset();
}

void AudioPluginAudioProcessor::numChannelsChanged()
{
    outputMeter.setNumChannels (getTotalNumOutputChannels());
}

void AudioPluginAudioProcessor::releaseResources()
{
    effect.reset();
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // The input matches the output, or is mono into stereo
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()
     && layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono())
        return false;
   #endif

    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;

    effect.setSettings (makeEffectSettings ([this] (const ParameterSpecs::Spec& spec) -> float
    {
        return *apvts.getRawParameterValue (spec.id);
    }));

    effect.process (buffer, Transport::from (getPlayHead()));

    if (! isSpectrumBypassed())
        spectrumTap.push (buffer);

    outputMeter.process (buffer);
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
   #if PLUGIN_WEBVIEW_EDITOR
    static_assert (WebViewEditor::updateHz == SpectrumScope::updateHz, "onTick is the spectrum's timer");

    // Sent as thousandths: JSON writes a float with all its digits. The page side is webui/src/plugin/index.tsx
    auto sendSpectrum = [this, scope = std::make_shared<SpectrumScope>()] (const WebViewEditor::EmitEvent& emit)
    {
        if (! scope->update (spectrumTap, getSampleRate(), isSpectrumBypassed()))
            return;

        juce::Array<juce::var> points;
        points.ensureStorageAllocated (SpectrumScope::scopeSize);

        for (const auto point : scope->getScope())
            points.add (juce::roundToInt (point * 1000.0f));

        emit ("spectrum", points);
    };

    return new WebViewEditor (*this, { .specs = ParameterSpecs::all,
                                       .parameters = apvts,
                                       .presets = &presetManager,
                                       .meter = &outputMeter,
                                       .width = AudioPluginAudioProcessorEditor::width,
                                       .height = AudioPluginAudioProcessorEditor::height,
                                       .background = juce::Colour (CustomLookAndFeel::backgroundTop),
                                       .onTick = sendSpectrum });
   #else
    return new AudioPluginAudioProcessorEditor (*this);
   #endif
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    presetManager.addPresetInfoTo (state);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        presetManager.restoreState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
