#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "dsp/Effect.h"

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;

//==============================================================================
namespace
{
    Effect::Settings reverbOnly (int type, float roomSize)
    {
        return { .reverbType = type, .roomSize = roomSize, .damping = 0.0f, .width = 1.0f, .mix = 1.0f, .highpassFreq = 20.0f };
    }

    juce::AudioBuffer<float> noiseBuffer (int numChannels, double seconds, float level)
    {
        juce::Random random (99);
        juce::AudioBuffer<float> buffer (numChannels, static_cast<int> (seconds * sampleRate));

        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (channel, i, level * (random.nextFloat() * 2.0f - 1.0f));

        return buffer;
    }

    // The time an impulse's tail takes to fall 60 dB, from the slope of what's left of it between -5 and -35 dB
    double measuredDecaySeconds (int type, float roomSize)
    {
        Effect effect;
        effect.prepare (sampleRate, blockSize, 2, 2);
        effect.setSettings (reverbOnly (type, roomSize));

        // Long enough for what's left at the end not to count
        juce::AudioBuffer<float> buffer (2, static_cast<int> (sampleRate * 1.5 * ReverbParts::decaySecondsFor (roomSize)));
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        effect.process (buffer, {});

        std::vector<double> remaining (static_cast<size_t> (buffer.getNumSamples()));
        auto energy = 0.0;

        for (int i = buffer.getNumSamples(); --i >= 0;)
        {
            energy += juce::square (static_cast<double> (buffer.getSample (0, i))) + juce::square (static_cast<double> (buffer.getSample (1, i)));
            remaining[static_cast<size_t> (i)] = energy;
        }

        const auto timeAt = [&] (double db)
        {
            const auto level = remaining.front() * std::pow (10.0, db / 10.0);
            const auto found = std::find_if (remaining.begin(), remaining.end(), [level] (double left) { return left < level; });
            return static_cast<double> (found - remaining.begin()) / sampleRate;
        };

        return 2.0 * (timeAt (-35.0) - timeAt (-5.0));
    }
}

TEST_CASE ("Every reverb rings for as long as the room size says", "[reverb]")
{
    const auto type = GENERATE (Effect::classic, Effect::plate, Effect::hall);
    const auto roomSize = GENERATE (0.2f, 0.5f, 0.8f);
    const auto expected = static_cast<double> (ReverbParts::decaySecondsFor (roomSize));
    const auto measured = measuredDecaySeconds (type, roomSize);
    CAPTURE (type, roomSize, expected, measured);

    CHECK_THAT (measured, WithinAbs (expected, expected * 0.25));
}

TEST_CASE ("Every reverb is about as loud as the others", "[reverb]")
{
    const auto numChannels = GENERATE (1, 2);
    CAPTURE (numChannels);

    const auto levelOf = [numChannels] (int type, float width = 1.0f)
    {
        auto settings = reverbOnly (type, 0.8f);
        settings.width = width;

        Effect effect;
        effect.prepare (sampleRate, blockSize, numChannels, numChannels);
        effect.setSettings (settings);
        auto buffer = noiseBuffer (numChannels, 2.0, 0.25f);
        effect.process (buffer, {});
        return juce::Decibels::gainToDecibels (buffer.getRMSLevel (0, buffer.getNumSamples() / 2, buffer.getNumSamples() / 2));
    };

    const auto classic = levelOf (Effect::classic);
    const auto plate = levelOf (Effect::plate);
    const auto hall = levelOf (Effect::hall);
    CAPTURE (classic, plate, hall);

    CHECK_THAT (plate, WithinAbs (classic, 2.0));
    CHECK_THAT (hall, WithinAbs (classic, 2.0));

    // There's no width in mono, so the knob does nothing
    if (numChannels == 1)
        for (const auto [type, level] : { std::pair { Effect::classic, classic }, std::pair { Effect::plate, plate }, std::pair { Effect::hall, hall } })
            CHECK_THAT (levelOf (type, 0.0f), WithinAbs (level, 1.0e-3));
}

TEST_CASE ("A mono output is as loud as each side of a stereo one", "[reverb]")
{
    const auto type = GENERATE (Effect::classic, Effect::plate, Effect::hall);
    CAPTURE (type);

    const auto levelOf = [type] (int numChannels)
    {
        Effect effect;
        effect.prepare (sampleRate, blockSize, numChannels, numChannels);
        effect.setSettings (reverbOnly (type, 0.8f));

        // The same on both sides
        const auto noise = noiseBuffer (1, 2.0, 0.25f);
        juce::AudioBuffer<float> buffer (numChannels, noise.getNumSamples());

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.copyFrom (channel, 0, noise, 0, 0, noise.getNumSamples());

        effect.process (buffer, {});
        return juce::Decibels::gainToDecibels (buffer.getRMSLevel (0, buffer.getNumSamples() / 2, buffer.getNumSamples() / 2));
    };

    CHECK_THAT (levelOf (1), WithinAbs (levelOf (2), 1.0e-3));
}

TEST_CASE ("Every reverb stays bounded with the room full and nothing damped, and is silent with nothing going in", "[reverb]")
{
    const auto type = GENERATE (Effect::classic, Effect::plate, Effect::hall);
    const auto numChannels = GENERATE (1, 2);
    CAPTURE (type, numChannels);

    Effect effect;
    effect.prepare (sampleRate, blockSize, numChannels, numChannels);
    effect.setSettings (reverbOnly (type, 1.0f));

    juce::AudioBuffer<float> silence (numChannels, blockSize * 8);
    silence.clear();
    effect.process (silence, {});
    CHECK (juce::exactlyEqual (silence.getMagnitude (0, silence.getNumSamples()), 0.0f));

    auto noise = noiseBuffer (numChannels, 30.0, 0.5f);
    effect.process (noise, {});

    CHECK (allSamplesFinite (noise));
    CHECK (noise.getMagnitude (0, noise.getNumSamples()) < 20.0f);
    CHECK (noise.getRMSLevel (0, noise.getNumSamples() / 2, noise.getNumSamples() / 2) > 1.0e-3f);
}

TEST_CASE ("Width 0 gives every reverb the same on both sides", "[reverb]")
{
    const auto type = GENERATE (Effect::classic, Effect::plate, Effect::hall);
    CAPTURE (type);

    auto settings = reverbOnly (type, 0.5f);
    settings.width = 0.0f;

    Effect effect;
    effect.prepare (sampleRate, blockSize, 2, 2);
    effect.setSettings (settings);

    auto buffer = noiseBuffer (2, 1.0, 0.25f);
    effect.process (buffer, {});

    // After juce::Reverb's 10 ms ramp from its defaults
    const auto settled = static_cast<int> (sampleRate) / 10;
    auto largestDifference = 0.0f;

    for (int i = settled; i < buffer.getNumSamples(); ++i)
        largestDifference = std::max (largestDifference, std::abs (buffer.getSample (0, i) - buffer.getSample (1, i)));

    CHECK (buffer.getRMSLevel (0, settled, buffer.getNumSamples() - settled) > 1.0e-3f);
    CHECK (largestDifference < 1.0e-5f);
}

TEST_CASE ("A change of reverb fades one out and the other in", "[reverb]")
{
    const auto from = GENERATE (Effect::classic, Effect::plate, Effect::hall);
    const auto to = GENERATE (Effect::classic, Effect::plate, Effect::hall);

    if (from == to)
        return;

    CAPTURE (from, to);

    // Hosts' buffers can be much shorter than the fade
    const auto hostBlock = GENERATE (16, blockSize);
    CAPTURE (hostBlock);

    Effect effect;
    effect.prepare (sampleRate, blockSize, 1, 1);
    effect.setSettings (reverbOnly (from, 0.8f));

    // Whole periods of a tone, a host's buffer at a time
    const auto play = [&] (int numPeriods)
    {
        auto played = makeSineBuffer (1, numPeriods * blockSize, 93.75f, 0.25f);

        for (int start = 0; start < played.getNumSamples(); start += hostBlock)
        {
            juce::AudioBuffer<float> block (played.getArrayOfWritePointers(), 1, start, hostBlock);
            effect.process (block, {});
        }

        return played;
    };

    play (40);
    const auto before = largestStep (play (1));

    effect.setSettings (reverbOnly (to, 0.8f));
    const auto changing = play (4);

    // Out over 15 ms, to nothing
    constexpr int fadeSamples = 720;
    CHECK (largestStep (changing) < before * 1.5f);
    CHECK (changing.getMagnitude (0, 0, fadeSamples / 2) > 1.0e-3f);
    CHECK (std::abs (changing.getSample (0, fadeSamples - 1)) < 1.0e-4f);

    play (4);
    CHECK (play (1).getRMSLevel (0, 0, blockSize) > 1.0e-3f);
}

TEST_CASE ("The plate's and the hall's room size, damping and width move without a jump", "[reverb]")
{
    const auto type = GENERATE (Effect::plate, Effect::hall);
    CAPTURE (type);

    auto before = reverbOnly (type, 0.8f);
    auto after = before;
    SECTION ("Room size")   { after.roomSize = 0.1f; }
    SECTION ("Damping")     { after.damping = 1.0f; }
    SECTION ("Width")       { after.width = 0.0f; }

    const auto steady = [] (Effect& effect)
    {
        auto buffer = makeSineBuffer (2, blockSize * 8, 93.75f, 0.25f);
        effect.process (buffer, {});
        return buffer;
    };

    Effect effect;
    effect.prepare (sampleRate, blockSize, 2, 2);
    effect.setSettings (before);

    for (int i = 0; i < 10; ++i)
        steady (effect);

    const auto unmoved = largestStep (steady (effect));

    effect.setSettings (after);
    const auto moving = largestStep (steady (effect));

    for (int i = 0; i < 10; ++i)
        steady (effect);

    const auto moved = largestStep (steady (effect));
    CAPTURE (unmoved, moving, moved);
    CHECK (moving < std::max (unmoved, moved) * 1.5f);
}

TEST_CASE ("The pre-delay holds the reverb back, and not the echoes", "[reverb]")
{
    const auto type = GENERATE (Effect::classic, Effect::plate, Effect::hall);
    CAPTURE (type);

    constexpr int preDelaySamples = 4800, echoSamples = 480;

    auto settings = reverbOnly (type, 0.5f);
    settings.preDelayMs = 100.0f;
    settings.delaySync = false;
    settings.delayTimeMs = 10.0f;
    settings.delayFeedback = 0.0f;
    settings.delayLevel = 1.0f;

    Effect effect;
    effect.prepare (sampleRate, blockSize, 1, 1);
    effect.setSettings (settings);

    juce::AudioBuffer<float> buffer (1, preDelaySamples * 2);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    effect.process (buffer, {});

    // Only the echo is heard before the reverb
    CHECK_THAT (buffer.getSample (0, echoSamples), WithinAbs (1.0, 1.0e-4));
    CHECK (juce::exactlyEqual (buffer.getMagnitude (0, 0, echoSamples), 0.0f));
    CHECK (juce::exactlyEqual (buffer.getMagnitude (0, echoSamples + 1, preDelaySamples - echoSamples - 1), 0.0f));
    CHECK (buffer.getMagnitude (0, preDelaySamples, preDelaySamples) > 1.0e-4f);
}

TEST_CASE ("The tail is the pre-delay, the room and the echoes, up to its limit", "[tail]")
{
    Effect::Settings settings { .preDelayMs = 100.0f, .roomSize = 0.5f, .delaySync = false, .delayTimeMs = 500.0f, .delayFeedback = 0.1f, .delayLevel = 0.0f };
    const auto room = static_cast<double> (ReverbParts::decaySecondsFor (0.5f));

    CHECK_THAT (Effect::tailSecondsFor (settings, 120.0), WithinAbs (0.1 + room, 1.0e-6));

    // Three repeats to fall 60 dB at a tenth each
    settings.delayLevel = 0.5f;
    CHECK_THAT (Effect::tailSecondsFor (settings, 120.0), WithinAbs (0.1 + room + 1.5, 1.0e-6));

    // A quarter note at 60 BPM
    settings.delaySync = true;
    settings.delayNote = 4;
    CHECK_THAT (Effect::tailSecondsFor (settings, 60.0), WithinAbs (0.1 + room + 3.0, 1.0e-6));

    settings.roomSize = 1.0f;
    settings.delayFeedback = 0.95f;
    CHECK_THAT (Effect::tailSecondsFor (settings, 60.0), WithinAbs (Effect::maxTailSeconds, 0.0));
}

TEST_CASE ("With only the room on, the classic reverb is juce::Reverb", "[reverb]")
{
    const auto numChannels = GENERATE (1, 2);
    CAPTURE (numChannels);

    const Effect::Settings settings { .roomSize = 0.9f, .damping = 0.5f, .width = 0.8f, .mix = 1.0f, .highpassFreq = 20.0f };

    Effect effect;
    effect.prepare (sampleRate, blockSize, numChannels, numChannels);
    effect.setSettings (settings);

    // A mono output is its left at full width, from the same on both sides
    juce::Reverb reverb;
    reverb.setSampleRate (sampleRate);
    reverb.setParameters ({ .roomSize = settings.roomSize, .damping = settings.damping, .wetLevel = 1.0f, .dryLevel = 0.0f,
                            .width = numChannels == 1 ? 1.0f : settings.width });

    auto input = noiseBuffer (2, 0.05, 0.5f);

    if (numChannels == 1)
        input.copyFrom (1, 0, input, 0, 0, input.getNumSamples());

    juce::AudioBuffer<float> output (numChannels, input.getNumSamples());

    for (int channel = 0; channel < numChannels; ++channel)
        output.copyFrom (channel, 0, input, channel, 0, input.getNumSamples());

    effect.process (output, {});
    reverb.processStereo (input.getWritePointer (0), input.getWritePointer (1), input.getNumSamples());

    auto maxError = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
        for (int i = 0; i < input.getNumSamples(); ++i)
            maxError = std::max (maxError, std::abs (output.getSample (channel, i) - input.getSample (channel, i)));

    CHECK (input.getMagnitude (0, input.getNumSamples()) > 0.1f);
    CHECK (maxError < 1.0e-6f);
}
