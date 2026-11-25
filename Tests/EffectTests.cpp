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
        { .roomSize = 0.2f, .damping = 0.1f, .width = 1.0f, .mix = 1.0f, .highpassFreq = 20.0f },
        { .roomSize = 1.0f, .damping = 0.0f, .width = 0.0f, .mix = 0.3f, .highpassFreq = 80.0f },
        { .roomSize = 0.5f, .damping = 1.0f, .width = 0.5f, .mix = 0.7f, .highpassFreq = 12000.0f },
        { .roomSize = 0.9f, .damping = 0.5f, .width = 0.8f, .mix = 1.0f, .highpassFreq = 440.0f },
    };

    Effect effect;
    JuceDspChain reference;
    effect.prepare (sampleRate, blockSize, numChannels, numChannels);
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
        CHECK (maxError < 3.0e-4f * peak);
    }
}

//==============================================================================
TEST_CASE ("A block longer than the prepared size gives what the same input gives in prepared-size blocks", "[dsp]")
{
    const auto numChannels = GENERATE (1, 2);
    const auto numSamples = GENERATE (blockSize * 4, blockSize * 2 + 37);
    CAPTURE (numChannels, numSamples);

    Effect whole, inBlocks;
    whole.prepare (sampleRate, blockSize, numChannels, numChannels);
    inBlocks.prepare (sampleRate, blockSize, numChannels, numChannels);

    // Every stage on, so each one's state carries across the chunks
    const Effect::Settings settings { .drive = 0.5f, .delaySync = false, .delayTimeMs = 20.0f, .delayLevel = 0.5f, .duck = 0.5f };
    whole.setSettings (settings);
    inBlocks.setSettings (settings);

    auto actual = makeSineBuffer (numChannels, numSamples, 440.0f, 0.5f);
    auto expected = actual;

    whole.process (actual, {});

    for (int start = 0; start < numSamples; start += blockSize)
    {
        juce::AudioBuffer<float> block { expected.getArrayOfWritePointers(), numChannels, start, std::min (blockSize, numSamples - start) };
        inBlocks.process (block, {});
    }

    for (int channel = 0; channel < numChannels; ++channel)
        for (int i = 0; i < numSamples; ++i)
            REQUIRE (juce::exactlyEqual (actual.getSample (channel, i), expected.getSample (channel, i)));
}

//==============================================================================
TEST_CASE ("Drive leaves the signal alone at 0, and at 1 squares it off at the reference level", "[dsp]")
{
    auto input = makeSineBuffer (1, blockSize, 440.0f, 2.0f);
    auto driven = input;
    Drive drive;

    drive.process (driven.getWritePointer (0), blockSize);
    CHECK (juce::exactlyEqual (driven.getMagnitude (0, blockSize), input.getMagnitude (0, blockSize)));
    CHECK (juce::exactlyEqual (driven.getRMSLevel (0, 0, blockSize), input.getRMSLevel (0, 0, blockSize)));

    drive.setAmount (1.0f);
    drive.process (driven.getWritePointer (0), blockSize);

    CHECK (driven.getMagnitude (0, blockSize) <= Drive::referenceLevel * 1.001f);
    // Squarer than a sine, whose peak is 1.41 times its RMS
    CHECK (driven.getMagnitude (0, blockSize) / driven.getRMSLevel (0, 0, blockSize) < 1.1f);

    // Adds harmonics, not level
    auto atReference = makeSineBuffer (1, blockSize, 440.0f, Drive::referenceLevel);
    drive.process (atReference.getWritePointer (0), blockSize);
    CHECK (std::abs (atReference.getMagnitude (0, blockSize) - Drive::referenceLevel) < 1.0e-3f);
}

//==============================================================================
namespace
{
    constexpr int echoTime = 4800;

    // An impulse on every channel, then silence, through a delay of echoTime samples
    juce::AudioBuffer<float> echoesOfAnImpulse (int numChannels)
    {
        PingPongDelay delay;
        delay.prepare (sampleRate, numChannels);
        delay.setTime (echoTime / sampleRate);
        delay.setFeedback (0.5f);

        juce::AudioBuffer<float> buffer (numChannels, echoTime * 4);
        buffer.clear();

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.setSample (channel, 0, 1.0f);

        delay.process (buffer);
        return buffer;
    }

    // Each repeat is lowpassed, which spreads it over the samples after it
    float levelAfter (const juce::AudioBuffer<float>& buffer, int channel, int start)
    {
        return buffer.getMagnitude (channel, start, 100);
    }
}

TEST_CASE ("In stereo the echoes alternate sides, starting on the left and getting quieter", "[dsp]")
{
    const auto echoes = echoesOfAnImpulse (2);

    CHECK (juce::exactlyEqual (levelAfter (echoes, 0, 0), 0.0f));
    CHECK (juce::exactlyEqual (levelAfter (echoes, 1, 0), 0.0f));

    CHECK_THAT (echoes.getSample (0, echoTime), WithinAbs (1.0, 1.0e-6));
    CHECK (juce::exactlyEqual (levelAfter (echoes, 1, echoTime), 0.0f));

    const auto second = levelAfter (echoes, 1, echoTime * 2);
    CHECK (second > 0.1f);
    CHECK (second < 0.5f);
    CHECK (juce::exactlyEqual (levelAfter (echoes, 0, echoTime * 2), 0.0f));

    const auto third = levelAfter (echoes, 0, echoTime * 3);
    CHECK (third > 0.01f);
    CHECK (third < second);
    CHECK (juce::exactlyEqual (levelAfter (echoes, 1, echoTime * 3), 0.0f));
}

TEST_CASE ("In mono the echoes repeat on the one channel", "[dsp]")
{
    const auto echoes = echoesOfAnImpulse (1);

    CHECK_THAT (echoes.getSample (0, echoTime), WithinAbs (1.0, 1.0e-6));

    const auto second = levelAfter (echoes, 0, echoTime * 2);
    CHECK (second > 0.1f);
    CHECK (second < 0.5f);
    CHECK (levelAfter (echoes, 0, echoTime * 3) < second);
}

TEST_CASE ("The delay time stays within its buffer", "[dsp]")
{
    PingPongDelay delay;
    delay.prepare (sampleRate, 2);
    delay.setFeedback (0.95f);

    for (const auto seconds : { 0.0, 100.0 })
    {
        delay.setTime (seconds);
        auto buffer = makeSineBuffer (2, static_cast<int> (sampleRate), 440.0f, 1.0f);
        delay.process (buffer);
        CHECK (allSamplesFinite (buffer));
    }
}

//==============================================================================
namespace
{
    // The wet level while a loud dry signal plays, then once it has stopped
    struct WetLevels { float underDry, afterDry; };

    WetLevels wetLevelsWithDuck (float duck)
    {
        const auto second = static_cast<int> (sampleRate);

        Effect effect;
        effect.prepare (sampleRate, blockSize, 2, 2);
        effect.setSettings ({ .mix = 1.0f, .duck = duck });

        auto playing = makeSineBuffer (2, second, 2000.0f, 0.5f);
        effect.process (playing, {});

        juce::AudioBuffer<float> stopped (2, second);
        stopped.clear();
        effect.process (stopped, {});

        return { playing.getRMSLevel (0, second / 2, second / 2), stopped.getRMSLevel (0, second / 2, second / 2) };
    }
}

TEST_CASE ("Duck holds the wet signal down under the dry one, and lets the tail up once it stops", "[dsp]")
{
    const auto open = wetLevelsWithDuck (0.0f);
    const auto ducked = wetLevelsWithDuck (1.0f);
    CAPTURE (open.underDry, open.afterDry, ducked.underDry, ducked.afterDry);

    CHECK (open.underDry > 1.0e-2f);
    CHECK (ducked.underDry < open.underDry / 100.0f);
    CHECK (ducked.afterDry > open.afterDry * 0.8f);
    CHECK (ducked.afterDry < open.afterDry);
}

TEST_CASE ("A mono input into stereo is copied to both sides even when prepared with no block", "[dsp]")
{
    Effect effect;
    effect.prepare (sampleRate, 0, 1, 2);

    auto buffer = makeConstantBuffer (2, blockSize, 0.5f);
    juce::FloatVectorOperations::fill (buffer.getWritePointer (1), 123.0f, blockSize);

    effect.process (buffer, {});

    CHECK (juce::exactlyEqual (buffer.getMagnitude (1, 0, blockSize), 0.5f));
}

TEST_CASE ("The output gain and the delay level start at their settings, and ramp when they move", "[dsp]")
{
    struct Block { float first, last; };

    const auto blockWith = [] (Effect& effect, const Effect::Settings& settings)
    {
        effect.setSettings (settings);
        auto buffer = makeConstantBuffer (1, blockSize, 0.5f);
        effect.process (buffer, {});
        return Block { buffer.getSample (0, 0), buffer.getSample (0, blockSize - 1) };
    };

    const Effect::Settings quiet { .mix = 0.0f, .outputGainDb = -20.0f };
    const Effect::Settings unity { .mix = 0.0f };

    Effect effect;
    effect.prepare (sampleRate, blockSize, 1, 1);
    CHECK_THAT (blockWith (effect, quiet).first, WithinAbs (0.05, 1.0e-6));

    const auto moved = blockWith (effect, unity);
    CHECK_THAT (moved.first, WithinAbs (0.05, 1.0e-6));
    CHECK (moved.last > 0.49f);

    effect.reset();
    CHECK_THAT (blockWith (effect, quiet).first, WithinAbs (0.05, 1.0e-6));

    // An echo inside the first block
    const auto echoAt = 480;
    Effect echoing;
    echoing.prepare (sampleRate, blockSize, 1, 1);
    echoing.setSettings ({ .mix = 1.0f, .highpassFreq = 20.0f, .delaySync = false,
                           .delayTimeMs = echoAt * 1000.0f / static_cast<float> (sampleRate), .delayFeedback = 0.0f, .delayLevel = 1.0f });
    juce::AudioBuffer<float> impulse (1, blockSize);
    impulse.clear();
    impulse.setSample (0, 0, 1.0f);
    echoing.process (impulse, {});
    CHECK_THAT (impulse.getSample (0, echoAt), WithinAbs (1.0, 1.0e-4));
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
