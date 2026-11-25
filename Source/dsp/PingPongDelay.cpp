#include "PingPongDelay.h"

//==============================================================================
void PingPongDelay::prepare (double newSampleRate, int numChannels)
{
    sampleRate = newSampleRate;

    // A sample more than the longest delay, for the interpolated read
    lines.setSize (std::min (numChannels, maxChannels), juce::roundToInt (std::ceil (maxSeconds * sampleRate)) + 2);

    constexpr double dampingFrequency = 6000.0;
    dampingCoefficient = static_cast<float> (1.0 - std::exp (-juce::MathConstants<double>::twoPi * dampingFrequency / sampleRate));

    delayInSamples.reset (sampleRate, 0.05);
    timeIsSet = false;
    reset();
}

void PingPongDelay::reset()
{
    lines.clear();
    writeIndex = 0;
    damped.fill (0.0f);
    delayInSamples.setCurrentAndTargetValue (delayInSamples.getTargetValue());
}

void PingPongDelay::setTime (double seconds)
{
    const auto samples = static_cast<float> (std::clamp (seconds, minSeconds, maxSeconds) * sampleRate);

    if (timeIsSet)
        delayInSamples.setTargetValue (samples);
    else
        delayInSamples.setCurrentAndTargetValue (samples);

    timeIsSet = true;
}

void PingPongDelay::setFeedback (float newFeedback)
{
    feedback = newFeedback;
}

void PingPongDelay::process (juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto size = lines.getNumSamples();
    const auto stereo = buffer.getNumChannels() > 1 && lines.getNumChannels() > 1;

    auto* left = buffer.getWritePointer (0);
    auto* right = stereo ? buffer.getWritePointer (1) : nullptr;
    auto* leftLine = lines.getWritePointer (0);
    auto* rightLine = stereo ? lines.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto delay = delayInSamples.getNextValue();
        const auto whole = static_cast<int> (delay);
        const auto fraction = delay - static_cast<float> (whole);

        // The delay is shorter than the line, so one wrap each
        auto newer = writeIndex - whole;

        if (newer < 0)
            newer += size;

        const auto older = newer == 0 ? size - 1 : newer - 1;

        const auto read = [&] (const float* line) { return line[newer] + fraction * (line[older] - line[newer]); };
        const auto leftEcho = read (leftLine);

        if (stereo)
        {
            const auto rightEcho = read (rightLine);

            damped[0] += dampingCoefficient * (rightEcho - damped[0]);
            damped[1] += dampingCoefficient * (leftEcho - damped[1]);

            leftLine[writeIndex] = 0.5f * (left[i] + right[i]) + feedback * damped[0];
            rightLine[writeIndex] = feedback * damped[1];

            left[i] = leftEcho;
            right[i] = rightEcho;
        }
        else
        {
            damped[0] += dampingCoefficient * (leftEcho - damped[0]);
            leftLine[writeIndex] = left[i] + feedback * damped[0];
            left[i] = leftEcho;
        }

        if (++writeIndex == size)
            writeIndex = 0;
    }
}
