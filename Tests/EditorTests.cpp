#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include <EditorFixtures.h>

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;
using EditorFixtures::descendantsOf;

static juce::Slider& findKnob (AudioPluginAudioProcessorEditor& editor, const juce::ParameterID& id)
{
    auto* knob = dynamic_cast<LabeledSlider*> (editor.findChildWithID (id.getParamID()));
    REQUIRE (knob != nullptr);
    return knob->getSlider();
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "Each knob and its parameter follow each other", "[editor]")
{
    AudioPluginAudioProcessorEditor editor (processor);

    CHECK (editor.getWidth() == AudioPluginAudioProcessorEditor::width);
    CHECK (editor.getHeight() == AudioPluginAudioProcessorEditor::height);
    CHECK (descendantsOf<LabeledSlider> (editor).size() == 6);

    for (const auto* id : { &Parameters::roomSizeId, &Parameters::dampingId, &Parameters::widthId,
                            &Parameters::mixId, &Parameters::sendGainId })
    {
        CAPTURE (id->getParamID());
        auto& knob = findKnob (editor, *id);

        knob.setValue (0.25, juce::sendNotificationSync);
        CHECK_THAT (getParameter (processor, *id), WithinAbs (0.25, 1.0e-4));

        setParameter (processor, *id, 0.75f);
        CHECK_THAT (knob.getValue(), WithinAbs (0.75, 1.0e-4));
        CHECK (knob.getTextFromValue (knob.getValue()) == "0.75");
    }
}

TEST_CASE_METHOD (ProcessorFixture, "The highpass knob reads in Hz, which the editor adds", "[editor]")
{
    AudioPluginAudioProcessorEditor editor (processor);
    auto& knob = findKnob (editor, Parameters::highpassFreqId);

    setParameter (processor, Parameters::highpassFreqId, 440.0f);
    CHECK_THAT (knob.getValue(), WithinAbs (440.0, 1.0e-4));
    CHECK (knob.getTextFromValue (knob.getValue()) == "440 Hz");
}
