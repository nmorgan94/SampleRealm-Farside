#include <juce_audio_basics/juce_audio_basics.h>
#include "shared/ParameterSpecs.h"
#include "shared/EffectSettings.h"
#include "shared/UiInfoJson.h"
#include <BuildInfo.h>
#include "dsp/Effect.h"

//==============================================================================
namespace
{
    constexpr int maxBlockSize = 128;
    constexpr int maxChannels = Effect::maxChannels;

    constexpr auto numParams = static_cast<int> (ParameterSpecs::all.size());

    constexpr auto indexOf (const ParameterSpecs::Spec& spec)
    {
        return std::ranges::find (ParameterSpecs::all, &spec) - ParameterSpecs::all.begin();
    }

    auto defaultValues()
    {
        std::array<float, ParameterSpecs::all.size()> values;

        for (size_t i = 0; i < values.size(); ++i)
            values[i] = ParameterSpecs::all[i]->defaultValue;

        return values;
    }

    auto values = defaultValues();

    float valueOf (const ParameterSpecs::Spec& spec)
    {
        return values[static_cast<size_t> (indexOf (spec))];
    }

    Effect effect;
    juce::AudioBuffer<float> buffer { maxChannels, maxBlockSize };
    int numChannels = maxChannels;
    double currentSampleRate = 48000.0;
    Transport transport;
}

#define EXPORT(name) extern "C" __attribute__ ((export_name (#name)))

EXPORT (uiInfo) const char* uiInfo()
{
    static const auto json = uiInfoJson (PLUGIN_VERSION, PLUGIN_GIT_COMMIT);
    return json.c_str();
}

// As isBusesLayoutSupported: mono or stereo, the same in and out
EXPORT (prepare) void prepare (double sampleRate, int channels)
{
    numChannels = std::clamp (channels, 1, maxChannels);
    currentSampleRate = sampleRate;
    buffer.clear();
    effect.prepare (sampleRate, maxBlockSize, numChannels);
}

EXPORT (setParam) void setParam (int index, float value)
{
    if (index >= 0 && index < numParams)
        values[static_cast<size_t> (index)] = value;
}

// The page plays the host: render moves the position on while playing
EXPORT (setTransport) void setTransport (double bpm, bool playing, double ppq)
{
    transport = { bpm, playing, ppq };
}

// Write the input here, and read the output back after render
EXPORT (channel) float* channel (int index)
{
    return buffer.getWritePointer (std::clamp (index, 0, maxChannels - 1));
}

EXPORT (render) void render (int numSamples)
{
    juce::AudioBuffer<float> block { buffer.getArrayOfWritePointers(), numChannels, std::clamp (numSamples, 0, maxBlockSize) };

    effect.setSettings (makeEffectSettings (valueOf));
    effect.process (block, transport);
    transport.advance (block.getNumSamples(), currentSampleRate);
}
