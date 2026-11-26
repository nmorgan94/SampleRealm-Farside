#pragma once

#include <samplerealm/ParameterSpec.h>

//==============================================================================
namespace ParameterSpecs
{
    // Effect::delayNoteBeats has each one's length
    inline constexpr std::array<const char*, 7> delayNotes { "1/16", "1/8T", "1/8", "1/8D", "1/4", "1/4D", "1/2" };

    inline constexpr Spec roomSize      { "roomSize",      "Room Size",      Kind::real,  0.0f,     1.0f,    0.8f, 0.01f };
    inline constexpr Spec damping       { "damping",       "Damping",        Kind::real,  0.0f,     1.0f,    0.8f, 0.01f };
    inline constexpr Spec width         { "width",         "Width",          Kind::real,  0.0f,     1.0f,    0.5f, 0.01f };
    inline constexpr Spec mix           { "mix",           "Mix",            Kind::real,  0.0f,     1.0f,    0.5f, 0.01f };
    inline constexpr Spec highpassFreq  { "highpassFreq",  "Highpass Freq",  Kind::real, 20.0f,  5000.0f, 250.0f, 1.0f, 0.4f };
    inline constexpr Spec lowpassFreq   { "lowpassFreq",   "Lowpass Freq",   Kind::real, 500.0f, 20000.0f, 20000.0f, 1.0f, 0.4f };
    inline constexpr Spec drive         { "drive",         "Drive",          Kind::real,  0.0f,     1.0f,    0.0f, 0.01f };
    inline constexpr Spec delaySync     = toggle ("delaySync", "Delay Sync", true);
    inline constexpr Spec delayNote     = choice ("delayNote", "Delay Note", delayNotes, 4);
    inline constexpr Spec delayTime     { "delayTime",     "Delay Time",     Kind::real, 10.0f,  2000.0f,  375.0f, 1.0f, 0.5f };
    inline constexpr Spec delayFeedback { "delayFeedback", "Delay Feedback", Kind::real,  0.0f,    0.95f,   0.35f, 0.01f };
    inline constexpr Spec delayLevel    { "delayLevel",    "Delay Level",    Kind::real,  0.0f,     1.0f,    0.0f, 0.01f };
    inline constexpr Spec duck          { "duck",          "Duck",           Kind::real,  0.0f,     1.0f,    0.0f, 0.01f };

    // dB, on the dry and wet signals together
    inline constexpr Spec outputGain    { "outputGain",    "Output Gain",    Kind::real, -24.0f,   12.0f,    0.0f, 0.1f };

    // A display toggle rather than a musical control: hosts should not automate it, and presets don't hold it
    inline constexpr Spec spectrumBypass = performanceControl (notAutomatable (toggle ("spectrumBypass", "Spectrum Bypass", false)));

    // The host's parameter order
    inline constexpr std::array all { &roomSize, &damping, &width, &mix, &highpassFreq, &lowpassFreq, &drive,
                                      &delaySync, &delayNote, &delayTime, &delayFeedback, &delayLevel, &duck, &outputGain,
                                      &spectrumBypass };

    static_assert (areValid (all));
}
