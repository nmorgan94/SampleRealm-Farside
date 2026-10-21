#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "Float parameters have the expected range and default", "[parameters]")
{
    const auto [id, name, rangeStart, rangeEnd, defaultValue] =
        GENERATE (table<const char*, const char*, float, float, float> ({
            { "roomSize",     "Room Size",      0.0f,     1.0f,    0.80f },
            { "damping",      "Damping",        0.0f,     1.0f,    0.80f },
            { "width",        "Width",          0.0f,     1.0f,    0.50f },
            { "mix",          "Mix",            0.0f,     1.0f,    0.50f },
            { "sendGain",     "Send Gain",      0.0f,     2.0f,    1.00f },
            { "highpassFreq", "Highpass Freq", 20.0f, 20000.0f, 1000.00f },
        }));

    CAPTURE (id);

    auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (
        processor.getAPVTS().getParameter (id));

    REQUIRE (parameter != nullptr);
    CHECK (parameter->getName (100) == name);
    CHECK_THAT (parameter->range.start, WithinAbs (rangeStart, 1.0e-6));
    CHECK_THAT (parameter->range.end, WithinAbs (rangeEnd, 1.0e-6));
    CHECK_THAT (parameter->get(), WithinAbs (defaultValue, 1.0e-6));
}

TEST_CASE_METHOD (ProcessorFixture, "Spectrum bypass is a bool parameter, off by default", "[parameters]")
{
    auto* parameter = dynamic_cast<juce::AudioParameterBool*> (
        processor.getAPVTS().getParameter ("spectrumBypass"));

    REQUIRE (parameter != nullptr);
    CHECK (parameter->getName (100) == "Spectrum Bypass");
    CHECK_FALSE (parameter->get());
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "Output stays finite for any block size, input and parameter setting", "[dsp]")
{
    const auto numSamples = GENERATE (1, 64, blockSize);

    // Silence, denormals, full scale and very loud
    const auto amplitude = GENERATE (0.0f, 1.0e-40f, 1.0f, 1000.0f);

    CAPTURE (numSamples, amplitude);

    // Sweeps the send path across its range between blocks, as host automation would
    for (const auto sendGain : { 0.0f, 1.0f, 2.0f })
    {
        for (const auto highpassFreq : { 20.0f, 1000.0f, 20000.0f })
        {
            for (const auto mix : { 0.0f, 0.5f, 1.0f })
            {
                CAPTURE (sendGain, highpassFreq, mix);

                setParameter (processor, "sendGain", sendGain);
                setParameter (processor, "highpassFreq", highpassFreq);
                setParameter (processor, "mix", mix);

                auto buffer = makeSineBuffer (2, numSamples, 440.0f, amplitude);
                process (processor, buffer);

                CHECK (allSamplesFinite (buffer));
            }
        }
    }
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "Only matching mono or stereo layouts are supported", "[buses]")
{
    using Set = juce::AudioChannelSet;

    // Each layout is { { input bus }, { output bus } }
    CHECK (processor.isBusesLayoutSupported ({ { Set::mono() },   { Set::mono() } }));
    CHECK (processor.isBusesLayoutSupported ({ { Set::stereo() }, { Set::stereo() } }));

    CHECK_FALSE (processor.isBusesLayoutSupported ({ { Set::mono() },   { Set::stereo() } }));
    CHECK_FALSE (processor.isBusesLayoutSupported ({ { Set::stereo() }, { Set::mono() } }));
    CHECK_FALSE (processor.isBusesLayoutSupported ({ { Set::create5point1() }, { Set::create5point1() } }));
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "State round trips into a new instance", "[state]")
{
    setParameter (processor, "roomSize", 0.25f);
    setParameter (processor, "highpassFreq", 440.0f);

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    AudioPluginAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    CHECK_THAT (getParameter (restored, "roomSize"), WithinAbs (0.25, 1.0e-4));
    CHECK_THAT (getParameter (restored, "highpassFreq"), WithinAbs (440.0, 1.0e-4));
}

TEST_CASE_METHOD (ProcessorFixture, "Invalid state is ignored", "[state]")
{
    setParameter (processor, "roomSize", 0.25f);

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

    CHECK_THAT (getParameter (processor, "roomSize"), WithinAbs (0.25, 1.0e-4));
}
