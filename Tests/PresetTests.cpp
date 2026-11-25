#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using namespace TestHelpers;

// PresetManager's own behaviour is tested in SampleRealm-Modules; these are Farside's presets and its analyser switch

namespace
{
    // The plugin's config, with user presets in a folder of their own
    struct PresetFixture : ProcessorFixture
    {
        ~PresetFixture()   { userDirectory.deleteRecursively(); }

        juce::File userDirectory = juce::File::createTempFile ("presets");

        PresetManager presets { processor.getAPVTS(), [this]
        {
            auto config = AudioPluginAudioProcessor::createPresetConfig();
            config.userDirectory = userDirectory;
            return config;
        }() };
    };
}

//==============================================================================
TEST_CASE_METHOD (PresetFixture, "The analyser's switch doesn't mark the preset modified, but a knob does", "[presets]")
{
    REQUIRE (presets.loadPreset (presets.find ("Vocal Plate", true).value()));

    setParameter (processor, Parameters::spectrumBypassId, 1.0f);
    CHECK_FALSE (presets.isModified());

    setParameter (processor, Parameters::mixId, 0.9f);
    CHECK (presets.isModified());
}

TEST_CASE_METHOD (PresetFixture, "A saved preset leaves out the analyser's switch", "[presets]")
{
    setParameter (processor, Parameters::spectrumBypassId, 1.0f);
    REQUIRE (presets.saveUserPreset ("Mine", {}).wasOk());

    const auto xml = juce::XmlDocument::parse (userDirectory.getChildFile ("Mine.farside"));
    REQUIRE (xml != nullptr);
    const auto* state = xml->getChildByName (processor.getAPVTS().state.getType());
    REQUIRE (state != nullptr);
    CHECK (state->getChildByAttribute ("id", Parameters::spectrumBypassId.getParamID()) == nullptr);
    CHECK (state->getChildByAttribute ("id", Parameters::mixId.getParamID()) != nullptr);
}

TEST_CASE_METHOD (ProcessorFixture, "Every shipped factory preset sets every parameter but the analyser's switch in range, and loads unmodified", "[presets]")
{
    auto& presets = processor.getPresetManager();
    REQUIRE_FALSE (presets.getFactoryPresets().isEmpty());

    for (const auto& data : AudioPluginAudioProcessor::createPresetConfig().factoryPresets)
    {
        const auto xml = juce::parseXML (data.toString());
        REQUIRE (xml != nullptr);
        const auto* state = xml->getChildByName (processor.getAPVTS().state.getType());
        REQUIRE (state != nullptr);

        for (const auto* spec : ParameterSpecs::all)
        {
            INFO (xml->getStringAttribute ("name") << ": " << spec->id);
            const auto* param = state->getChildByAttribute ("id", spec->id);

            if (spec->isPerformanceControl)
            {
                CHECK (param == nullptr);
                continue;
            }

            REQUIRE (param != nullptr);
            const auto value = param->getStringAttribute ("value").getFloatValue();
            CHECK (value >= spec->min);
            CHECK (value <= spec->max);
        }
    }

    for (const auto& preset : presets.getFactoryPresets())
    {
        INFO (preset.name);
        CHECK (presets.loadPreset (preset));
        CHECK (presets.getCurrent().name == preset.name);
        CHECK_FALSE (presets.isModified());
    }
}

TEST_CASE_METHOD (ProcessorFixture, "A session reopens showing its preset", "[presets]")
{
    auto& presets = processor.getPresetManager();
    REQUIRE (presets.loadPreset (presets.find ("Slap", true).value()));

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    AudioPluginAudioProcessor reopened;
    reopened.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    CHECK (reopened.getPresetManager().getCurrent().name == "Slap");
    CHECK_FALSE (reopened.getPresetManager().isModified());
    CHECK (getParameter (reopened, Parameters::delayTimeId) == 110.0f);
}
