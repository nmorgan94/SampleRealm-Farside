#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "dsp/Effect.h"

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;

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

    drive.process (driven, 1, blockSize);
    CHECK (juce::exactlyEqual (driven.getMagnitude (0, blockSize), input.getMagnitude (0, blockSize)));
    CHECK (juce::exactlyEqual (driven.getRMSLevel (0, 0, blockSize), input.getRMSLevel (0, 0, blockSize)));

    drive.setAmount (1.0f);
    drive.snap();
    drive.process (driven, 1, blockSize);

    CHECK (driven.getMagnitude (0, blockSize) <= Drive::referenceLevel * 1.001f);
    // Squarer than a sine, whose peak is 1.41 times its RMS
    CHECK (driven.getMagnitude (0, blockSize) / driven.getRMSLevel (0, 0, blockSize) < 1.1f);

    // Adds harmonics, not level
    auto atReference = makeSineBuffer (1, blockSize, 440.0f, Drive::referenceLevel);
    drive.process (atReference, 1, blockSize);
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
        delay.snap();

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

TEST_CASE ("The delay holds a half note down to 30 BPM, and a time beyond its buffer stays within it", "[dsp]")
{
    PingPongDelay delay;
    delay.prepare (sampleRate, 2);
    delay.setTime (4.0);
    delay.setFeedback (0.0f);
    delay.snap();

    const auto fourSeconds = static_cast<int> (sampleRate) * 4;
    juce::AudioBuffer<float> buffer (2, fourSeconds + 10);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);
    delay.process (buffer);

    CHECK_THAT (buffer.getSample (0, fourSeconds), WithinAbs (1.0, 1.0e-6));

    delay.setFeedback (0.95f);

    for (const auto seconds : { 0.0, 100.0 })
    {
        delay.setTime (seconds);
        auto tone = makeSineBuffer (2, static_cast<int> (sampleRate), 440.0f, 1.0f);
        delay.process (tone);
        CHECK (allSamplesFinite (tone));
    }
}

//==============================================================================
namespace
{
    // The wet level while a loud dry signal plays, then once it has stopped
    struct WetLevels { float underDry, afterDry; };

    WetLevels wetLevelsWithDuck (float duck, float dryLevel = 0.5f)
    {
        const auto second = static_cast<int> (sampleRate);

        Effect effect;
        effect.prepare (sampleRate, blockSize, 2, 2);
        effect.setSettings ({ .mix = 1.0f, .duck = duck });

        auto playing = makeSineBuffer (2, second, 2000.0f, dryLevel);
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

TEST_CASE ("A quiet dry signal ducks the wet one as far as a loud one does", "[dsp]")
{
    // 20 dB apart
    const auto loud = wetLevelsWithDuck (0.5f, 0.5f);
    const auto quiet = wetLevelsWithDuck (0.5f, 0.05f);
    const auto loudOpen = wetLevelsWithDuck (0.0f, 0.5f);
    const auto quietOpen = wetLevelsWithDuck (0.0f, 0.05f);

    const auto loudDucked = loud.underDry / loudOpen.underDry;
    CHECK_THAT (loudDucked, WithinAbs (0.5, 0.05));
    CHECK_THAT (quiet.underDry / quietOpen.underDry, WithinAbs (loudDucked, 0.01));
}

TEST_CASE ("A dry signal under -60 dB doesn't duck the wet one much", "[dsp]")
{
    const auto noise = wetLevelsWithDuck (1.0f, 0.0001f);
    const auto open = wetLevelsWithDuck (0.0f, 0.0001f);

    CHECK (noise.underDry > open.underDry * 0.5f);
}

TEST_CASE ("A loud hit after a quiet passage keeps the wet signal ducked", "[dsp]")
{
    Ducker ducker;
    ducker.prepare (sampleRate);

    for (int i = 0; i < static_cast<int> (sampleRate); ++i)
        ducker.gainFor (0.01f, 1.0f);

    auto highestGain = 0.0f;

    for (int i = 0; i < static_cast<int> (sampleRate) / 10; ++i)
        highestGain = std::max (highestGain, ducker.gainFor (0.5f, 1.0f));

    CHECK (highestGain < 0.01f);
}

TEST_CASE ("An effect with no settings yet runs at its defaults, at the rate it's prepared for", "[dsp]")
{
    const auto rate = GENERATE (48000.0, 96000.0);

    const auto processed = [rate] (bool giveSettings)
    {
        Effect effect;
        effect.prepare (48000.0, blockSize, 1, 1);
        effect.prepare (rate, blockSize, 1, 1);

        if (giveSettings)
            effect.setSettings ({});

        auto buffer = makeSineBuffer (1, blockSize * 4, 300.0f, 0.5f);
        effect.process (buffer, {});
        return buffer;
    };

    const auto without = processed (false);
    const auto with = processed (true);

    for (int i = 0; i < without.getNumSamples(); ++i)
        REQUIRE (juce::exactlyEqual (without.getSample (0, i), with.getSample (0, i)));
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

    // The mix moves the dry signal only past half way, where it fades out
    Effect mixing;
    mixing.prepare (sampleRate, blockSize, 1, 1);
    const Effect::Settings dryOnly { .roomSize = 0.0f, .mix = 0.0f }, halfWay { .roomSize = 0.0f, .mix = 0.5f }, wetOnly { .roomSize = 0.0f, .mix = 1.0f };
    CHECK_THAT (blockWith (mixing, dryOnly).last, WithinAbs (0.5, 1.0e-6));
    mixing.reset();
    CHECK (blockWith (mixing, wetOnly).first < 0.1f);

    mixing.reset();
    blockWith (mixing, halfWay);
    const auto fading = blockWith (mixing, wetOnly);
    const auto faded = blockWith (mixing, wetOnly);
    CHECK (std::abs (fading.first - fading.last) > 0.2f);
    CHECK (std::abs (faded.first - faded.last) < 0.05f);

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
namespace
{
    // The wet signal's level for a tone through the send's filters, with next to no room
    float wetLevelOfTone (float hz, float highpassFreq, float lowpassFreq)
    {
        Effect effect;
        effect.prepare (sampleRate, blockSize, 1, 1);
        effect.setSettings ({ .roomSize = 0.0f, .mix = 1.0f, .highpassFreq = highpassFreq, .lowpassFreq = lowpassFreq });
        auto buffer = makeSineBuffer (1, static_cast<int> (sampleRate), hz, 0.5f);
        effect.process (buffer, {});
        return buffer.getRMSLevel (0, buffer.getNumSamples() / 2, buffer.getNumSamples() / 2);
    }
}

TEST_CASE ("The lowpass takes the top off the send, and is off at the top of its range", "[dsp]")
{
    const auto open = wetLevelOfTone (8000.0f, 20.0f, 20000.0f);
    CHECK (open > 1.0e-3f);

    // Three octaves under the tone, at 12 dB an octave
    CHECK_THAT (juce::Decibels::gainToDecibels (wetLevelOfTone (8000.0f, 20.0f, 1000.0f) / open), WithinAbs (-36.0, 3.0));
}

TEST_CASE ("The send's cutoffs, drive and feedback move without a jump", "[dsp]")
{
    // The echoes land in step with the tone, so more feedback only builds it up
    constexpr float tonePeriod = 512.0f;
    const Effect::Settings open { .roomSize = 0.0f, .mix = 1.0f, .highpassFreq = 20.0f, .delaySync = false, .delayTimeMs = tonePeriod * 1000.0f / static_cast<float> (sampleRate), .delayFeedback = 0.0f, .delayLevel = 1.0f };

    auto before = open;
    auto after = open;
    SECTION ("Highpass")   { after.highpassFreq = 2000.0f; }
    SECTION ("Highpass going off")   { before.highpassFreq = 150.0f; }
    SECTION ("Lowpass going off")    { before.lowpassFreq = 500.0f; }
    SECTION ("Lowpass")    { after.lowpassFreq = 300.0f; }
    SECTION ("Drive")      { after.drive = 1.0f; }
    SECTION ("Feedback")   { after.delayFeedback = 0.95f; }

    // A low tone, whose own steps are small
    const auto steady = [&] (Effect& effect)
    {
        auto buffer = makeSineBuffer (2, blockSize * 8, static_cast<float> (sampleRate) / tonePeriod, 0.25f);
        effect.process (buffer, {});
        return buffer;
    };

    Effect effect;
    effect.prepare (sampleRate, blockSize, 2, 2);
    effect.setSettings (before);
    const auto unmoved = largestStep (steady (effect));

    effect.setSettings (after);
    const auto moving = largestStep (steady (effect));

    for (int i = 0; i < 10; ++i)
        steady (effect);

    // No steeper on the way than the steeper of where it was and where it ends up
    const auto moved = largestStep (steady (effect));
    CAPTURE (unmoved, moving, moved);
    CHECK (moving < std::max (unmoved, moved) * 1.5f);

    // And back, with the filters' state from before
    effect.setSettings (before);
    CHECK (largestStep (steady (effect)) < std::max (unmoved, moved) * 1.5f);
}

TEST_CASE ("The filters' curve is what the send's filters do to a tone", "[dsp]")
{
    const auto highpass = 400.0f, lowpass = 3000.0f;
    // Not near half the sample rate, where the lowpass falls faster than its curve
    const auto hz = GENERATE (100.0f, 400.0f, 1000.0f, 3000.0f, 6000.0f);
    CAPTURE (hz);

    const auto filtered = juce::Decibels::gainToDecibels (wetLevelOfTone (hz, highpass, lowpass) / wetLevelOfTone (hz, 20.0f, 20000.0f));
    CHECK_THAT (filtered, WithinAbs (Effect::filtersDb (hz, highpass, lowpass), 1.5));
    CHECK_THAT (Effect::filtersDb (hz, Effect::highpassOffFreq, Effect::lowpassOffFreq), WithinAbs (0.0, 0.0));
}
