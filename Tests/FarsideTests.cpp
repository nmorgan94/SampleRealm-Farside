#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;

//==============================================================================
// What hosts and saved sessions see. Change it only on purpose
TEST_CASE_METHOD (ProcessorFixture, "Parameters match the snapshot hosts rely on", "[parameters]")
{
    struct Expected
    {
        const char* id;
        int versionHint;
        const char* name;
        const char* label;
        float start, end, interval, skew, defaultValue;
        std::array<const char*, 3> textAt0HalfAnd1;
        bool automatable;
        bool isBool;
    };

    const std::vector<Expected> snapshot {
        { "roomSize",       1, "Room Size",       "",  0.0f,     1.0f, 0.01f, 1.0f,    0.8f, { "0.00", "0.50", "1.00" },  true,  false },
        { "damping",        1, "Damping",         "",  0.0f,     1.0f, 0.01f, 1.0f,    0.8f, { "0.00", "0.50", "1.00" },  true,  false },
        { "width",          1, "Width",           "",  0.0f,     1.0f, 0.01f, 1.0f,    0.5f, { "0.00", "0.50", "1.00" },  true,  false },
        { "mix",            1, "Mix",             "",  0.0f,     1.0f, 0.01f, 1.0f,    0.5f, { "0.00", "0.50", "1.00" },  true,  false },
        { "highpassFreq",   1, "Highpass Freq",   "", 20.0f, 20000.0f,  1.0f, 0.2f, 250.0f,  { "20", "644", "20000" },    true,  false },
        { "drive",          1, "Drive",           "",  0.0f,     1.0f, 0.01f, 1.0f,    0.0f, { "0.00", "0.50", "1.00" },  true,  false },
        { "delaySync",      1, "Delay Sync",      "",  0.0f,     1.0f,  1.0f, 1.0f,    1.0f, { "Off", "On", "On" },       true,  true  },
        { "delayNote",      1, "Delay Note",      "",  0.0f,     6.0f,  1.0f, 1.0f,    4.0f, { "1/16", "1/8D", "1/2" },   true,  false },
        { "delayTime",      1, "Delay Time",      "", 10.0f,  2000.0f,  1.0f, 0.5f,  375.0f, { "10", "508", "2000" },     true,  false },
        { "delayFeedback",  1, "Delay Feedback",  "",  0.0f,    0.95f, 0.01f, 1.0f,   0.35f, { "0.00", "0.48", "0.95" },  true,  false },
        { "delayLevel",     1, "Delay Level",     "",  0.0f,     1.0f, 0.01f, 1.0f,    0.0f, { "0.00", "0.50", "1.00" },  true,  false },
        { "duck",           1, "Duck",            "",  0.0f,     1.0f, 0.01f, 1.0f,    0.0f, { "0.00", "0.50", "1.00" },  true,  false },
        { "outputGain",     1, "Output Gain",     "", -24.0f,   12.0f,  0.1f, 1.0f,    0.0f, { "-24.0", "-6.0", "12.0" }, true,  false },
        { "spectrumBypass", 1, "Spectrum Bypass", "",  0.0f,     1.0f,  1.0f, 1.0f,    0.0f, { "Off", "On", "On" },       false, true  },
    };

    const auto& parameters = processor.getParameters();
    REQUIRE (static_cast<size_t> (parameters.size()) == snapshot.size());

    for (size_t i = 0; i < snapshot.size(); ++i)
    {
        const auto& expected = snapshot[i];
        CAPTURE (i, expected.id);

        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (parameters[static_cast<int> (i)]);
        REQUIRE (parameter != nullptr);

        CHECK (parameter->getParameterID() == expected.id);
        CHECK (parameter->getVersionHint() == expected.versionHint);
        CHECK (parameter->getName (100) == expected.name);
        CHECK (parameter->getLabel() == expected.label);
        CHECK (parameter->isAutomatable() == expected.automatable);
        CHECK ((dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr) == expected.isBool);

        const auto& range = parameter->getNormalisableRange();
        CHECK_THAT (range.start, WithinAbs (expected.start, 1.0e-6));
        CHECK_THAT (range.end, WithinAbs (expected.end, 1.0e-6));
        CHECK_THAT (range.interval, WithinAbs (expected.interval, 1.0e-6));
        CHECK_THAT (range.skew, WithinAbs (expected.skew, 1.0e-6));
        CHECK_THAT (parameter->convertFrom0to1 (parameter->getDefaultValue()), WithinAbs (expected.defaultValue, 1.0e-4));

        CHECK (parameter->getText (0.0f, 100) == expected.textAt0HalfAnd1[0]);
        CHECK (parameter->getText (0.5f, 100) == expected.textAt0HalfAnd1[1]);
        CHECK (parameter->getText (1.0f, 100) == expected.textAt0HalfAnd1[2]);
    }
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "Output stays finite for any block size, input and parameter setting", "[dsp]")
{
    const auto numSamples = GENERATE (1, 64, blockSize);

    // Silence, denormals, full scale and very loud
    const auto amplitude = GENERATE (0.0f, 1.0e-40f, 1.0f, 1000.0f);

    CAPTURE (numSamples, amplitude);

    // The drive, the delay at its longest feedback and the duck: all off, then all the way up
    const auto extras = GENERATE (0.0f, 1.0f);
    setParameter (processor, Parameters::driveId, extras);
    setParameter (processor, Parameters::delayLevelId, extras);
    setParameter (processor, Parameters::delayFeedbackId, extras * ParameterSpecs::delayFeedback.max);
    setParameter (processor, Parameters::duckId, extras);
    CAPTURE (extras);

    // Sweeps the send path across its range between blocks, as host automation would
    for (const auto highpassFreq : { 20.0f, 1000.0f, 20000.0f })
    {
        for (const auto mix : { 0.0f, 0.5f, 1.0f })
        {
            CAPTURE (highpassFreq, mix);

            setParameter (processor, Parameters::highpassFreqId, highpassFreq);
            setParameter (processor, Parameters::mixId, mix);

            auto buffer = makeSineBuffer (2, numSamples, 440.0f, amplitude);
            process (processor, buffer);

            CHECK (allSamplesFinite (buffer));
        }
    }
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "Mono or stereo out is supported, from as many channels or from mono", "[buses]")
{
    using Set = juce::AudioChannelSet;

    // Each layout is { { input bus }, { output bus } }
    CHECK (processor.isBusesLayoutSupported ({ { Set::mono() },   { Set::mono() } }));
    CHECK (processor.isBusesLayoutSupported ({ { Set::stereo() }, { Set::stereo() } }));

    CHECK (processor.isBusesLayoutSupported ({ { Set::mono() },   { Set::stereo() } }));

    CHECK_FALSE (processor.isBusesLayoutSupported ({ { Set::stereo() }, { Set::mono() } }));
    CHECK_FALSE (processor.isBusesLayoutSupported ({ { Set::create5point1() }, { Set::create5point1() } }));
}

TEST_CASE_METHOD (ProcessorFixture, "A mono track into stereo gets its input on both sides", "[buses]")
{
    using Set = juce::AudioChannelSet;

    AudioPluginAudioProcessor monoIntoStereo;
    REQUIRE (monoIntoStereo.setBusesLayout ({ { Set::mono() }, { Set::stereo() } }));
    monoIntoStereo.setRateAndBufferSizeDetails (sampleRate, blockSize);
    monoIntoStereo.prepareToPlay (sampleRate, blockSize);
    setParameter (monoIntoStereo, Parameters::mixId, 0.0f);

    // Hosts leave whatever was there in the second channel
    auto buffer = makeSineBuffer (2, blockSize, 440.0f, 0.5f);
    const auto input = buffer;
    juce::FloatVectorOperations::fill (buffer.getWritePointer (1), 123.0f, blockSize);
    process (monoIntoStereo, buffer);

    for (int channel = 0; channel < 2; ++channel)
        for (int i = 0; i < blockSize; ++i)
            REQUIRE (juce::exactlyEqual (buffer.getSample (channel, i), input.getSample (0, i)));
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "State round trips into a new instance", "[state]")
{
    setParameter (processor, Parameters::roomSizeId, 0.25f);
    setParameter (processor, Parameters::highpassFreqId, 440.0f);

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    AudioPluginAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    CHECK_THAT (getParameter (restored, Parameters::roomSizeId), WithinAbs (0.25, 1.0e-4));
    CHECK_THAT (getParameter (restored, Parameters::highpassFreqId), WithinAbs (440.0, 1.0e-4));
}

TEST_CASE_METHOD (ProcessorFixture, "Invalid state is ignored", "[state]")
{
    setParameter (processor, Parameters::roomSizeId, 0.25f);

    SECTION ("Random bytes")
    {
        const char garbage[] = "definitely not plugin state";
        processor.setStateInformation (garbage, static_cast<int> (sizeof (garbage)));
    }

    SECTION ("Empty data")
    {
        processor.setStateInformation (nullptr, 0);
    }

    SECTION ("Valid XML with the wrong root tag")
    {
        juce::MemoryBlock state;
        juce::AudioProcessor::copyXmlToBinary (juce::XmlElement ("SomethingElse"), state);
        processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    }

    CHECK_THAT (getParameter (processor, Parameters::roomSizeId), WithinAbs (0.25, 1.0e-4));
}
