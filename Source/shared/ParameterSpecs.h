#pragma once

#include <samplerealm/ParameterSpec.h>

//==============================================================================
namespace ParameterSpecs
{
    inline constexpr Spec roomSize     { "roomSize",     "Room Size",     Kind::real,  0.0f,     1.0f,    0.8f, 0.01f };
    inline constexpr Spec damping      { "damping",      "Damping",       Kind::real,  0.0f,     1.0f,    0.8f, 0.01f };
    inline constexpr Spec width        { "width",        "Width",         Kind::real,  0.0f,     1.0f,    0.5f, 0.01f };
    inline constexpr Spec mix          { "mix",          "Mix",           Kind::real,  0.0f,     1.0f,    0.5f, 0.01f };
    inline constexpr Spec sendGain     { "sendGain",     "Send Gain",     Kind::real,  0.0f,     2.0f,    1.0f, 0.01f };
    inline constexpr Spec highpassFreq { "highpassFreq", "Highpass Freq", Kind::real, 20.0f, 20000.0f, 1000.0f, 1.0f, 0.2f };

    // A display toggle rather than a musical control, so hosts should not automate it
    inline constexpr Spec spectrumBypass = notAutomatable (toggle ("spectrumBypass", "Spectrum Bypass", false));

    // The host's parameter order
    inline constexpr std::array all { &roomSize, &damping, &width, &mix, &sendGain, &highpassFreq, &spectrumBypass };

    static_assert (areValid (all));
}
