#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "dsp/Effect.h"

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "The spectrum tap gets the dry and wet signals as they are in the output, mixed to mono, unless bypassed", "[spectrum]")
{
    struct Pulled { std::vector<float> dry, wet; };

    auto pulled = [this]
    {
        Pulled samples;

        processor.getSpectrumTap().pull ([&] (const float* dry, const float* wet, int numSamples)
        {
            samples.dry.insert (samples.dry.end(), dry, dry + numSamples);
            samples.wet.insert (samples.wet.end(), wet, wet + numSamples);
        });

        return samples;
    };

    auto buffer = makeSineBuffer (2, blockSize, 440.0f, 0.5f);
    buffer.applyGain (1, 0, blockSize, 0.5f);
    const auto input = buffer;

    SECTION ("Bypassed")
    {
        setParameter (processor, Parameters::spectrumBypassId, 1.0f);
        process (processor, buffer);

        CHECK (pulled().dry.empty());
    }

    SECTION ("Running")
    {
        setParameter (processor, Parameters::mixId, 0.75f);
        process (processor, buffer);

        const auto samples = pulled();
        REQUIRE (samples.dry.size() == static_cast<size_t> (blockSize));
        REQUIRE (samples.wet.size() == static_cast<size_t> (blockSize));

        for (const auto i : { 0, 100, blockSize - 1 })
        {
            const auto index = static_cast<size_t> (i);
            CHECK_THAT (samples.dry[index], WithinAbs (0.5f * (input.getSample (0, i) + input.getSample (1, i)) / 2.0f, 1.0e-6));
            CHECK_THAT (samples.dry[index] + samples.wet[index], WithinAbs ((buffer.getSample (0, i) + buffer.getSample (1, i)) / 2.0f, 1.0e-6));
        }

        CHECK (pulled().dry.empty());
    }
}

namespace
{
    // A stereo effect with its tap, and the display reading it
    struct DisplayFixture
    {
        DisplayFixture()
        {
            effect.prepare (sampleRate, blockSize, 2, 2);
            effect.setTap (&tap);
        }

        static constexpr double frameSeconds = 1.0 / SpectrumFrame::updateHz;

        SpectrumTap tap;
        SpectrumFrameSource source;
        Effect effect;
    };
}

TEST_CASE_METHOD (DisplayFixture, "The tap holds the lowest duck gain until it's read", "[spectrum]")
{
    auto loud = makeSineBuffer (2, blockSize * 8, 440.0f, 0.5f);

    SECTION ("No duck leaves the gain at 1")
    {
        effect.setSettings ({ .duck = 0.0f });
        effect.process (loud, {});
        CHECK_THAT (tap.readDuckGain(), WithinAbs (1.0, 0.0));
    }

    SECTION ("A full duck under a loud dry signal takes it to 0")
    {
        effect.setSettings ({ .duck = 1.0f });
        effect.process (loud, {});
        CHECK_THAT (tap.readDuckGain(), WithinAbs (0.0, 0.01));
        CHECK_THAT (tap.readDuckGain(), WithinAbs (1.0, 0.0));
    }
}

TEST_CASE_METHOD (DisplayFixture, "The display's frame shows a tone in the dry and wet bands, and nothing while bypassed", "[spectrum]")
{
    effect.setSettings ({ .mix = 0.5f, .highpassFreq = 20.0f });

    // Three decades from 20 Hz
    const auto band = static_cast<size_t> (std::floor (SpectrumFrame::numBands * std::log10 (1000.0f / SpectrumFrame::minHz) / 3.0f));

    // A second, a frame's worth at a time
    constexpr int samplesPerFrame = 1600;
    auto tone = makeSineBuffer (2, samplesPerFrame * SpectrumFrame::updateHz, 1000.0f, 0.5f);

    for (int start = 0; start < tone.getNumSamples(); start += samplesPerFrame)
    {
        juce::AudioBuffer<float> buffer { tone.getArrayOfWritePointers(), 2, start, samplesPerFrame };
        effect.process (buffer, {});
        source.update (tap, frameSeconds, sampleRate, false);
    }

    const auto& frame = source.getFrame();

    // At half way the dry signal is at unity: a 0.5 sine is -6 dB
    CHECK_THAT (frame[SpectrumFrame::dryOffset + band], WithinAbs (-6.0, 2.0));
    CHECK (frame[SpectrumFrame::wetOffset + band] > -40.0f);
    CHECK (frame[SpectrumFrame::dryOffset] < -60.0f);
    CHECK_THAT (frame[SpectrumFrame::duckGain], WithinAbs (1.0, 0.0));

    SECTION ("With no more audio it falls to the floor")
    {
        for (int i = 0; i < SpectrumFrame::updateHz * 10; ++i)
            source.update (tap, frameSeconds, sampleRate, false);

        CHECK (source.getFrame() == SpectrumFrame::silent());
    }

    SECTION ("Bypassed, it's cleared once")
    {
        CHECK (source.update (tap, frameSeconds, sampleRate, true));
        CHECK (source.getFrame() == SpectrumFrame::silent());
        CHECK_FALSE (source.update (tap, frameSeconds, sampleRate, true));
    }
}

TEST_CASE_METHOD (DisplayFixture, "The display keeps the duck between blocks, and drops what was played while nothing read it", "[spectrum]")
{
    effect.setSettings ({ .duck = 1.0f });

    auto loud = makeSineBuffer (2, blockSize * 8, 1000.0f, 0.5f);

    SECTION ("A frame with no block since the last one shows the same duck")
    {
        source.update (tap, frameSeconds, sampleRate, false);
        effect.process (loud, {});
        source.update (tap, frameSeconds, sampleRate, false);
        const auto ducked = source.getFrame()[SpectrumFrame::duckGain];
        CHECK (ducked < 0.05f);

        source.update (tap, frameSeconds, sampleRate, false);
        CHECK_THAT (source.getFrame()[SpectrumFrame::duckGain], WithinAbs (ducked, 0.0));
    }

    SECTION ("Played before the first frame")
    {
        effect.process (loud, {});
        CHECK_FALSE (source.update (tap, frameSeconds, sampleRate, false));
        CHECK (source.getFrame() == SpectrumFrame::silent());
    }

    SECTION ("Played during a long gap between frames")
    {
        source.update (tap, frameSeconds, sampleRate, false);
        effect.process (loud, {});
        CHECK_FALSE (source.update (tap, 2.0, sampleRate, false));
    }

    SECTION ("Played while bypassed")
    {
        source.update (tap, frameSeconds, sampleRate, false);
        effect.process (loud, {});
        source.update (tap, frameSeconds, sampleRate, true);
        CHECK_FALSE (source.update (tap, frameSeconds, sampleRate, false));
    }
}
