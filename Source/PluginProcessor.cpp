#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "shared/EffectSettings.h"
#include <BinaryData.h>

#if PLUGIN_WEBVIEW_EDITOR
 #include <webview_editor/webview_editor.h>
 #include "dsp/SpectrumFrameSource.h"
 #include "ui/FrameClock.h"
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

Effect::Settings AudioPluginAudioProcessor::currentSettings() const
{
    return makeEffectSettings ([this] (const ParameterSpecs::Spec& spec) -> float
    {
        return *apvts.getRawParameterValue (spec.id);
    });
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return Effect::tailSecondsFor (currentSettings(), lastBpm);
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

    effect.setSettings (currentSettings());

    effect.setTap (isSpectrumBypassed() ? nullptr : &spectrumTap);
    const auto transport = Transport::from (getPlayHead());
    lastBpm = transport.bpm;
    effect.process (buffer, transport);

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
    static_assert (WebViewEditor::updateHz == SpectrumFrame::updateHz, "onTick is the spectrum's timer");

    // As its bytes in base64. The page side is webui/src/plugin/index.tsx
    auto sendSpectrum = [this, source = std::make_shared<SpectrumFrameSource>(), clock = FrameClock()] (const WebViewEditor::EmitEvent& emit) mutable
    {
        if (! source->update (spectrumTap, clock.seconds(), getSampleRate(), isSpectrumBypassed()))
            return;

        const auto& frame = source->getFrame();
        emit ("spectrum", juce::Base64::toBase64 (frame.data(), frame.size() * sizeof (float)));
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
