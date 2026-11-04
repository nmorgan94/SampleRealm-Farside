#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_dsp/juce_dsp.h>

#include "TestHelpers.h"
#include "dsp/Effect.h"

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;

//==============================================================================
namespace
{
    // processBlock as it was before the DSP moved into Effect, built on juce_dsp
    struct JuceDspChain
    {
        using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

        juce::dsp::Reverb reverb;
        Filter highpass1, highpass2;
        juce::AudioBuffer<float> wetBuffer;
        double sampleRate = 0.0;

        void prepare (double newSampleRate, int maxBlockSize, int numChannels)
        {
            sampleRate = newSampleRate;
            const juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (maxBlockSize), static_cast<juce::uint32> (numChannels) };
            reverb.prepare (spec);
            highpass1.prepare (spec);
            highpass2.prepare (spec);
            wetBuffer.setSize (numChannels, maxBlockSize);
            wetBuffer.clear();
        }

        void process (juce::AudioBuffer<float>& buffer, const Effect::Settings& settings)
        {
            juce::dsp::Reverb::Parameters parameters;
            parameters.roomSize = settings.roomSize;
            parameters.damping = settings.damping;
            parameters.width = settings.width;
            parameters.wetLevel = 1.0f;
            parameters.dryLevel = 0.0f;
            parameters.freezeMode = 0.0f;
            reverb.setParameters (parameters);

            *highpass1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, settings.highpassFreq);
            *highpass2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, settings.highpassFreq);

            const auto numChannels = buffer.getNumChannels();
            const auto numSamples = buffer.getNumSamples();

            for (int channel = 0; channel < numChannels; ++channel)
                wetBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

            juce::dsp::AudioBlock<float> wetBlock (wetBuffer.getArrayOfWritePointers(), static_cast<size_t> (numChannels),
                                                   0, static_cast<size_t> (numSamples));
            juce::dsp::ProcessContextReplacing<float> wetContext (wetBlock);

            if (settings.highpassFreq > 20.0f)
            {
                highpass1.process (wetContext);
                highpass2.process (wetContext);
            }

            wetBuffer.applyGain (settings.sendGain);
            reverb.process (wetContext);

            for (int channel = 0; channel < numChannels; ++channel)
            {
                buffer.applyGain (channel, 0, numSamples, 1.0f - settings.mix);
                buffer.addFrom (channel, 0, wetBuffer, channel, 0, numSamples, settings.mix);
            }
        }
    };
}

//==============================================================================
TEST_CASE ("The effect matches the juce_dsp chain it replaced", "[dsp]")
{
    const auto numChannels = GENERATE (1, 2);
    CAPTURE (numChannels);

    const std::vector<Effect::Settings> settingsInTurn {
        {},
        { .roomSize = 0.2f, .damping = 0.1f, .width = 1.0f, .mix = 1.0f, .sendGain = 2.0f, .highpassFreq = 20.0f },
        { .roomSize = 1.0f, .damping = 0.0f, .width = 0.0f, .mix = 0.3f, .sendGain = 0.5f, .highpassFreq = 80.0f },
        { .roomSize = 0.5f, .damping = 1.0f, .width = 0.5f, .mix = 0.7f, .sendGain = 1.0f, .highpassFreq = 12000.0f },
        { .roomSize = 0.9f, .damping = 0.5f, .width = 0.8f, .mix = 1.0f, .sendGain = 1.5f, .highpassFreq = 440.0f },
    };

    Effect effect;
    JuceDspChain reference;
    effect.prepare (sampleRate, blockSize, numChannels);
    reference.prepare (sampleRate, blockSize, numChannels);

    juce::Random random (1234);
    int block = 0;

    // Includes blocks shorter than the prepared size, which hosts send
    for (size_t index = 0; index < settingsInTurn.size(); ++index)
    {
        const auto& settings = settingsInTurn[index];
        float maxError = 0.0f, peak = 0.0f;

        for (const auto numSamples : { blockSize, 100, 1, blockSize, blockSize })
        {
            auto input = makeSineBuffer (numChannels, numSamples, 220.0f + 110.0f * static_cast<float> (block++), 0.5f);

            for (int channel = 0; channel < numChannels; ++channel)
                for (int i = 0; i < numSamples; ++i)
                    input.setSample (channel, i, input.getSample (channel, i) + 0.25f * (random.nextFloat() * 2.0f - 1.0f));

            auto expected = input;
            reference.process (expected, settings);

            effect.setSettings (settings);
            effect.process (input, {});

            for (int channel = 0; channel < numChannels; ++channel)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    maxError = std::max (maxError, std::abs (input.getSample (channel, i) - expected.getSample (channel, i)));
                    peak = std::max (peak, std::abs (expected.getSample (channel, i)));
                }
            }
        }

        // juce::IIRCoefficients are computed in double, juce::dsp's in float
        CAPTURE (index, maxError, peak);
        CHECK (maxError < 1.0e-4f * peak);
    }
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "The spectrum tap gets the output mixed to mono, unless bypassed", "[spectrum]")
{
    auto pulled = [this]
    {
        std::vector<float> samples;
        processor.getSpectrumTap().pull ([&] (const float* data, int numSamples) { samples.insert (samples.end(), data, data + numSamples); });
        return samples;
    };

    SECTION ("Bypassed")
    {
        setParameter (processor, Parameters::spectrumBypassId, 1.0f);
        auto buffer = makeSineBuffer (2, blockSize, 440.0f, 0.5f);
        process (processor, buffer);

        CHECK (pulled().empty());
    }

    SECTION ("Running")
    {
        auto buffer = makeSineBuffer (2, blockSize, 440.0f, 0.5f);
        buffer.applyGain (1, 0, blockSize, 0.5f);
        process (processor, buffer);

        const auto samples = pulled();
        REQUIRE (samples.size() == static_cast<size_t> (blockSize));

        for (const auto i : { 0, 100, blockSize - 1 })
            CHECK_THAT (samples[static_cast<size_t> (i)],
                        WithinAbs ((buffer.getSample (0, i) + buffer.getSample (1, i)) / 2.0f, 1.0e-6));

        CHECK (pulled().empty());
    }
}
