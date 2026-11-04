#pragma once

#include <algorithm>
#include <array>
#include <span>

//==============================================================================
namespace ParameterSpecs
{
    enum class Kind { real, integer, choice, toggle };

    struct Spec
    {
        const char* id;
        const char* name;
        Kind kind;
        float min, max, defaultValue;
        float step = 0.0f;
        float skew = 1.0f;
        const char* unit = "";
        std::span<const char* const> choices = {};
        bool automatable = true;
    };

    constexpr Spec integer (const char* id, const char* name, int min, int max, int defaultValue, const char* unit = "")
    {
        return { id, name, Kind::integer, static_cast<float> (min), static_cast<float> (max),
                 static_cast<float> (defaultValue), 1.0f, 1.0f, unit };
    }

    constexpr Spec choice (const char* id, const char* name, std::span<const char* const> choices, int defaultValue)
    {
        return { id, name, Kind::choice, 0.0f, static_cast<float> (choices.size() - 1),
                 static_cast<float> (defaultValue), 1.0f, 1.0f, "", choices };
    }

    constexpr Spec toggle (const char* id, const char* name, bool defaultValue, bool automatable = true)
    {
        return { id, name, Kind::toggle, 0.0f, 1.0f, defaultValue ? 1.0f : 0.0f, 1.0f, 1.0f, "", {}, automatable };
    }

    inline constexpr Spec roomSize     { "roomSize",     "Room Size",     Kind::real,  0.0f,     1.0f,    0.8f, 0.01f };
    inline constexpr Spec damping      { "damping",      "Damping",       Kind::real,  0.0f,     1.0f,    0.8f, 0.01f };
    inline constexpr Spec width        { "width",        "Width",         Kind::real,  0.0f,     1.0f,    0.5f, 0.01f };
    inline constexpr Spec mix          { "mix",          "Mix",           Kind::real,  0.0f,     1.0f,    0.5f, 0.01f };
    inline constexpr Spec sendGain     { "sendGain",     "Send Gain",     Kind::real,  0.0f,     2.0f,    1.0f, 0.01f };
    inline constexpr Spec highpassFreq { "highpassFreq", "Highpass Freq", Kind::real, 20.0f, 20000.0f, 1000.0f, 1.0f, 0.2f };

    // A display toggle rather than a musical control, so hosts should not automate it
    inline constexpr Spec spectrumBypass = toggle ("spectrumBypass", "Spectrum Bypass", false, false);

    // The host's parameter order
    inline constexpr std::array all { &roomSize, &damping, &width, &mix, &sendGain, &highpassFreq, &spectrumBypass };

    static_assert (std::ranges::all_of (all, [] (const Spec* spec)
    {
        switch (spec->kind)
        {
            case Kind::real:    return true;
            case Kind::integer: return spec->step == 1.0f;
            case Kind::toggle:  return spec->step == 1.0f && spec->min == 0.0f && spec->max == 1.0f;
            case Kind::choice:  break;
        }

        return spec->step == 1.0f && spec->min == 0.0f && static_cast<size_t> (spec->max) == spec->choices.size() - 1;
    }), "Integer, choice and toggle specs need step 1, a choice spec's range is 0 to its last choice, and a toggle's is 0 to 1");
}
