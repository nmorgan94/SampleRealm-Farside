#include "HallReverb.h"

namespace
{
    // Spread so their echoes don't line up, in milliseconds
    constexpr std::array lineMs { 43.3, 51.7, 59.9, 67.1, 73.9, 83.3, 91.7, 101.9 };
    constexpr std::array<std::array<double, 3>, 2> diffuserMs { { { 5.3, 11.3, 19.1 }, { 6.1, 12.7, 17.9 } } };
    constexpr float diffusion = 0.6f;

    // How far each delay's length swings, and how slowly, which keeps the tail from ringing
    constexpr double excursionMs = 0.35;
    constexpr double slowestLfoHz = 0.31, lfoHzStep = 0.07;

    // To sit at juce::Reverb's level
    constexpr float outputGain = 1.41f;
}

//==============================================================================
void HallReverb::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    excursion = static_cast<float> (excursionMs * sampleRate / 1000.0);

    const auto samplesIn = [this] (double ms) { return std::max (1, static_cast<int> (std::lround (ms * sampleRate / 1000.0))); };

    for (size_t side = 0; side < diffusers.size(); ++side)
    {
        for (size_t i = 0; i < numDiffusers; ++i)
        {
            diffuserLengths[side][i] = samplesIn (diffuserMs[side][i]);
            diffusers[side][i].setSize (diffuserLengths[side][i]);
        }
    }

    for (size_t i = 0; i < lines.size(); ++i)
    {
        lines[i].length = samplesIn (lineMs[i]);
        lines[i].delay.setSize (lines[i].length + static_cast<int> (std::ceil (excursion)) + 1);
        lines[i].lfo.setRate (slowestLfoHz + lfoHzStep * static_cast<double> (i), sampleRate);
        lines[i].gain.reset (sampleRate, ReverbParts::glideSeconds);
    }

    damping.reset (sampleRate, ReverbParts::glideSeconds);
    width.reset (sampleRate, ReverbParts::glideSeconds);

    reset();
}

void HallReverb::reset()
{
    for (auto& side : diffusers)
        for (auto& diffuser : side)
            diffuser.clear();

    for (size_t i = 0; i < lines.size(); ++i)
    {
        lines[i].delay.clear();
        lines[i].damped = 0.0f;
        lines[i].lfo.reset (static_cast<float> (i));
    }

    snap();
}

void HallReverb::snap()
{
    for (auto& line : lines)
        line.gain.setCurrentAndTargetValue (line.gain.getTargetValue());

    damping.setCurrentAndTargetValue (damping.getTargetValue());
    width.setCurrentAndTargetValue (width.getTargetValue());
}

void HallReverb::setParameters (float decaySeconds, float newDamping, float newWidth)
{
    // Each delay loses what its length is of the decay
    for (auto& line : lines)
        line.gain.setTargetValue (static_cast<float> (std::pow (10.0, -3.0 * line.length / (sampleRate * decaySeconds))));

    damping.setTargetValue (ReverbParts::dampingCoefficientFor (newDamping, sampleRate));
    width.setTargetValue (newWidth);
}

//==============================================================================
void HallReverb::process (float* left, float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const auto dampingNow = damping.getNextValue();
        const auto widthGains = ReverbParts::widthGainsFor (width.getNextValue());

        std::array<float, 2> inputs;
        inputs.fill (0.5f * (left[i] + right[i]));

        for (size_t side = 0; side < inputs.size(); ++side)
            for (size_t stage = 0; stage < numDiffusers; ++stage)
                inputs[side] = diffusers[side][stage].process (inputs[side], diffuserLengths[side][stage], diffusion);

        std::array<float, numLines> outputs;
        auto sum = 0.0f;
        std::array<float, 2> sides {};

        for (size_t line = 0; line < lines.size(); ++line)
        {
            auto& state = lines[line];
            const auto delayed = state.delay.read (static_cast<float> (state.length) + excursion * state.lfo.next());

            sides[line % 2] += delayed;
            const auto kept = delayed * state.gain.getNextValue();
            state.damped = kept + dampingNow * (state.damped - kept);
            outputs[line] = state.damped;
            sum += state.damped;
        }

        // The Householder matrix: each delay gets its own output back, less a quarter of them all
        const auto shared = sum * (2.0f / numLines);

        for (size_t line = 0; line < lines.size(); ++line)
            lines[line].delay.write (outputs[line] - shared + inputs[line % 2]);

        left[i] = outputGain * (sides[0] * widthGains.same + sides[1] * widthGains.other);
        right[i] = outputGain * (sides[1] * widthGains.same + sides[0] * widthGains.other);
    }
}
