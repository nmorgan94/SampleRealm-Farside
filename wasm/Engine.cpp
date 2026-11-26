#include <juce_audio_basics/juce_audio_basics.h>
#include <samplerealm/BuildCommit.h>
#include <samplerealm/ParameterValues.h>
#include <samplerealm/PeakMeter.h>
#include <samplerealm/UiInfoJson.h>
#include <samplerealm/WasmExport.h>
#include "shared/ParameterSpecs.h"
#include "shared/EffectSettings.h"
#include "dsp/Effect.h"
#include "dsp/SpectrumFrameSource.h"

//==============================================================================
namespace
{
    constexpr int maxBlockSize = 128;
    constexpr int maxChannels = Effect::maxChannels;

    ParameterValues values { ParameterSpecs::all };

    Effect effect;
    PeakMeter meter { maxChannels };
    juce::AudioBuffer<float> buffer { maxChannels, maxBlockSize };
    int numChannels = maxChannels;
    double currentSampleRate = 48000.0;
    Transport transport;

    SpectrumTap spectrumTap;
    SpectrumFrameSource spectrum;

    bool spectrumBypassed()   { return values (ParameterSpecs::spectrumBypass) >= 0.5f; }
}

WASM_EXPORT (uiInfo) const char* uiInfo()
{
    static const auto json = uiInfoJson (ParameterSpecs::all, PLUGIN_VERSION, buildCommit);
    return json.c_str();
}

// Mono or stereo, the same in and out
WASM_EXPORT (prepare) void prepare (double sampleRate, int channels)
{
    numChannels = std::clamp (channels, 1, maxChannels);
    currentSampleRate = sampleRate;
    meter.setNumChannels (numChannels);
    buffer.clear();
    effect.prepare (sampleRate, maxBlockSize, numChannels, numChannels);
    spectrumTap.reset();
}

WASM_EXPORT (setParam) void setParam (int index, float value)
{
    values.set (index, value);
}

// The page plays the host: render moves the position on while playing
WASM_EXPORT (setTransport) void setTransport (double bpm, bool playing, double ppq)
{
    transport = { bpm, playing, ppq };
}

// Write the input here, and read the output back after render
WASM_EXPORT (channel) float* channel (int index)
{
    return buffer.getWritePointer (std::clamp (index, 0, maxChannels - 1));
}

WASM_EXPORT (render) void render (int numSamples)
{
    juce::AudioBuffer<float> block { buffer.getArrayOfWritePointers(), numChannels, std::clamp (numSamples, 0, maxBlockSize) };

    effect.setSettings (makeEffectSettings (values));
    effect.setTap (spectrumBypassed() ? nullptr : &spectrumTap);
    effect.process (block, transport);
    meter.process (block);
    transport.advance (block.getNumSamples(), currentSampleRate);
}

// The loudest output sample since the last call, as linear gain
WASM_EXPORT (peak) float peak (int index)
{
    return index >= 0 && index < numChannels ? meter.readAndReset (index) : 0.0f;
}

// The display now as SpectrumFrame::size floats, given how long it's been since the last call
WASM_EXPORT (frame) const float* frame (double seconds)
{
    spectrum.update (spectrumTap, seconds, currentSampleRate, spectrumBypassed());
    return spectrum.getFrame().data();
}

WASM_EXPORT (frameSize) int frameSize()
{
    return SpectrumFrame::size;
}
