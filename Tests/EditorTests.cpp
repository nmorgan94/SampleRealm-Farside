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
    CHECK (descendantsOf<LabeledSlider> (editor).size() == 13);

    for (const auto* id : { &Parameters::roomSizeId, &Parameters::dampingId, &Parameters::widthId,
                            &Parameters::mixId, &Parameters::driveId,
                            &Parameters::delayFeedbackId, &Parameters::delayLevelId, &Parameters::duckId })
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

TEST_CASE_METHOD (ProcessorFixture, "The gain knob is in the header and reads in dB, which the editor adds", "[editor]")
{
    AudioPluginAudioProcessorEditor editor (processor);
    const auto headers = descendantsOf<HeaderBar> (editor);
    REQUIRE (headers.size() == 1);

    LabeledSlider* gain = nullptr;

    for (auto* slider : descendantsOf<LabeledSlider> (*headers[0]))
        if (slider->getComponentID() == Parameters::outputGainId.getParamID())
            gain = slider;

    REQUIRE (gain != nullptr);
    auto& knob = gain->getSlider();

    setParameter (processor, Parameters::outputGainId, -6.0f);
    CHECK_THAT (knob.getValue(), WithinAbs (-6.0, 1.0e-4));
    CHECK (knob.getTextFromValue (knob.getValue()) == "-6.0 dB");

    knob.setValue (3.0, juce::sendNotificationSync);
    CHECK_THAT (getParameter (processor, Parameters::outputGainId), WithinAbs (3.0, 1.0e-4));
}

TEST_CASE_METHOD (ProcessorFixture, "The time knob is the note while synced, and the time in ms otherwise", "[editor]")
{
    AudioPluginAudioProcessorEditor editor (processor);
    auto& note = findKnob (editor, Parameters::delayNoteId);
    auto& time = findKnob (editor, Parameters::delayTimeId);
    auto isShown = [] (juce::Slider& knob) { return knob.getParentComponent()->isVisible(); };

    CHECK (note.getParentComponent()->getBounds() == time.getParentComponent()->getBounds());
    CHECK (isShown (note));
    CHECK_FALSE (isShown (time));

    setParameter (processor, Parameters::delayNoteId, 3.0f);
    CHECK (note.getTextFromValue (note.getValue()) == "1/8D");

    setParameter (processor, Parameters::delaySyncId, 0.0f);
    CHECK_FALSE (isShown (note));
    CHECK (isShown (time));

    setParameter (processor, Parameters::delayTimeId, 250.0f);
    CHECK (time.getTextFromValue (time.getValue()) == "250 ms");
}

TEST_CASE_METHOD (ProcessorFixture, "The header, its buttons and its menus are in Orbitron", "[editor]")
{
    AudioPluginAudioProcessorEditor editor (processor);
    auto& lookAndFeel = editor.getLookAndFeel();

    juce::Label regular, bold;
    bold.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    juce::TextButton button;

    CHECK (lookAndFeel.getLabelFont (regular).getTypefacePtr()->getName() == "Orbitron");
    CHECK (lookAndFeel.getLabelFont (bold).getTypefacePtr()->getName() == "Orbitron");
    CHECK (lookAndFeel.getTextButtonFont (button, 24).getTypefacePtr()->getName() == "Orbitron");
    CHECK (lookAndFeel.getPopupMenuFont().getTypefacePtr()->getName() == "Orbitron");
}
