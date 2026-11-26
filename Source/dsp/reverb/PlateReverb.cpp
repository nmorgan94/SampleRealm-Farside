#include "PlateReverb.h"

namespace
{
    // The paper's sample rate, which its lengths are for
    constexpr double paperRate = 29761.0;

    constexpr float bandwidth = 0.9995f;
    constexpr float inputDiffusion1 = 0.75f, inputDiffusion2 = 0.625f, decayDiffusion1 = 0.7f;
    constexpr int excursionInThePaper = 16;

    // Once round the tank: its two modulated allpasses, two diffusers and four delays
    constexpr double loopSeconds = (672 + 4453 + 1800 + 3720 + 908 + 4217 + 2656 + 3163) / paperRate;

    // To sit at juce::Reverb's level
    constexpr float outputGain = 1.75f;
}

//==============================================================================
int PlateReverb::scaled (int samplesInThePaper) const
{
    return std::max (1, static_cast<int> (std::lround (samplesInThePaper * sampleRate / paperRate)));
}

void PlateReverb::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    excursion = static_cast<float> (excursionInThePaper * sampleRate / paperRate);

    constexpr std::array inputs { 142, 107, 379, 277 };

    for (size_t i = 0; i < inputs.size(); ++i)
    {
        inputLengths[i] = scaled (inputs[i]);
        inputDiffusers[i].setSize (inputLengths[i]);
    }

    const auto prepareHalf = [this] (Half& half, int modulated, int first, int diffuser, int second, double lfoHz)
    {
        half.modulatedLength = scaled (modulated);
        half.firstLength = scaled (first);
        half.diffuserLength = scaled (diffuser);
        half.secondLength = scaled (second);

        half.modulated.setSize (half.modulatedLength + static_cast<int> (std::ceil (excursion)) + 1);
        half.first.setSize (half.firstLength);
        half.diffuser.setSize (half.diffuserLength);
        half.second.setSize (half.secondLength);
        half.lfo.setRate (lfoHz, sampleRate);
    };

    prepareHalf (leftHalf, 672, 4453, 1800, 3720, 1.0);
    prepareHalf (rightHalf, 908, 4217, 2656, 3163, 0.73);

    constexpr std::array left { 266, 2974, 1913, 1996, 1990, 187, 1066 };
    constexpr std::array right { 353, 3627, 1228, 2673, 2111, 335, 121 };

    for (size_t i = 0; i < left.size(); ++i)
    {
        leftTaps[i] = scaled (left[i]);
        rightTaps[i] = scaled (right[i]);
    }

    for (auto* value : { &decay, &damping, &width })
        value->reset (sampleRate, ReverbParts::glideSeconds);

    reset();
}

void PlateReverb::reset()
{
    for (auto& diffuser : inputDiffusers)
        diffuser.clear();

    for (auto* half : { &leftHalf, &rightHalf })
    {
        half->modulated.clear();
        half->diffuser.clear();
        half->first.clear();
        half->second.clear();
        half->damped = 0.0f;
    }

    leftHalf.lfo.reset (0.0f);
    rightHalf.lfo.reset (std::numbers::pi_v<float> / 2.0f);
    bandwidthState = 0.0f;
    snap();
}

void PlateReverb::snap()
{
    for (auto* value : { &decay, &damping, &width })
        value->setCurrentAndTargetValue (value->getTargetValue());
}

void PlateReverb::setParameters (float decaySeconds, float newDamping, float newWidth)
{
    // The tank's gain is the decay four times a loop
    decay.setTargetValue (static_cast<float> (std::pow (10.0, -0.75 * loopSeconds / decaySeconds)));
    damping.setTargetValue (ReverbParts::dampingCoefficientFor (newDamping, sampleRate));
    width.setTargetValue (newWidth);
}

//==============================================================================
void PlateReverb::throughHalf (Half& half, float input, float decayNow, float dampingNow)
{
    const auto modulatedLength = static_cast<float> (half.modulatedLength) + excursion * half.lfo.next();
    auto sample = half.modulated.process (input, modulatedLength, -decayDiffusion1);

    const auto delayed = half.first.read (half.firstLength);
    half.first.write (sample);

    half.damped = delayed + dampingNow * (half.damped - delayed);
    sample = half.diffuser.process (half.damped * decayNow, half.diffuserLength, std::clamp (decayNow + 0.15f, 0.25f, 0.5f));
    half.second.write (sample);
}

void PlateReverb::process (float* left, float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const auto decayNow = decay.getNextValue();
        const auto dampingNow = damping.getNextValue();
        const auto widthGains = ReverbParts::widthGainsFor (width.getNextValue());

        auto input = 0.5f * (left[i] + right[i]);
        bandwidthState += bandwidth * (input - bandwidthState);
        input = bandwidthState;

        input = inputDiffusers[0].process (input, inputLengths[0], inputDiffusion1);
        input = inputDiffusers[1].process (input, inputLengths[1], inputDiffusion1);
        input = inputDiffusers[2].process (input, inputLengths[2], inputDiffusion2);
        input = inputDiffusers[3].process (input, inputLengths[3], inputDiffusion2);

        // Each side is fed by the end of the other
        const auto fromRight = rightHalf.second.read (rightHalf.secondLength);
        const auto fromLeft = leftHalf.second.read (leftHalf.secondLength);

        // The taps, before this sample is written
        const auto leftOut = rightHalf.first.read (leftTaps[0]) + rightHalf.first.read (leftTaps[1])
                           - rightHalf.diffuser.getLine().read (leftTaps[2]) + rightHalf.second.read (leftTaps[3])
                           - leftHalf.first.read (leftTaps[4]) - leftHalf.diffuser.getLine().read (leftTaps[5])
                           - leftHalf.second.read (leftTaps[6]);

        const auto rightOut = leftHalf.first.read (rightTaps[0]) + leftHalf.first.read (rightTaps[1])
                            - leftHalf.diffuser.getLine().read (rightTaps[2]) + leftHalf.second.read (rightTaps[3])
                            - rightHalf.first.read (rightTaps[4]) - rightHalf.diffuser.getLine().read (rightTaps[5])
                            - rightHalf.second.read (rightTaps[6]);

        throughHalf (leftHalf, input + decayNow * fromRight, decayNow, dampingNow);
        throughHalf (rightHalf, input + decayNow * fromLeft, decayNow, dampingNow);

        left[i] = outputGain * (leftOut * widthGains.same + rightOut * widthGains.other);
        right[i] = outputGain * (rightOut * widthGains.same + leftOut * widthGains.other);
    }
}
