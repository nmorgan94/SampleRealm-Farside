#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <samplerealm/Transport.h>
#include "Drive.h"
#include "Ducker.h"
#include "GlidingFilter.h"
#include "PingPongDelay.h"
#include "SpectrumTap.h"

//==============================================================================
/** A send in one insert: the send is highpassed (24 dB/oct), lowpassed (12 dB/oct), driven and echoed into a 100% wet reverb.
    The echoes and the reverb are ducked by the dry signal, then added to it, and the output gain is on the two together. */
class Effect
{
public:
    static constexpr int maxChannels = 2;

    // Beats per echo, in the order of ParameterSpecs::delayNotes
    static constexpr std::array delayNoteBeats { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0 };

    struct Settings
    {
        float roomSize = 0.8f;
        float damping = 0.8f;
        float width = 0.5f;
        float mix = 0.5f;
        float highpassFreq = 1000.0f;
        float lowpassFreq = 20000.0f;
        float drive = 0.0f;
        bool delaySync = true;
        int delayNote = 4;
        float delayTimeMs = 375.0f;
        float delayFeedback = 0.35f;
        float delayLevel = 0.0f;
        float duck = 0.0f;
        float outputGainDb = 0.0f;
    };

    /** Mono or stereo out, from as many channels or from mono. */
    void prepare (double sampleRate, int maxBlockSize, int numInputChannels, int numOutputChannels);
    void reset();
    void setSettings (const Settings& newSettings);

    /** As a send up to half way, the dry signal at unity while the wet one comes up to it, then the dry one fades out. */
    struct MixGains { float dry, wet; };

    static constexpr MixGains mixGainsFor (float mix)
    {
        return { std::min (1.0f, 2.0f * (1.0f - mix)), std::min (1.0f, 2.0f * mix) };
    }

    // Where each filter is off: the ends of their parameters' ranges
    static constexpr float highpassOffFreq = 20.0f, lowpassOffFreq = 20000.0f;

    /** What the send's two filters do to a frequency, in dB. */
    static float filtersDb (float hz, float highpassFreq, float lowpassFreq)
    {
        return Highpass::responseDb (Highpass::Kind::highpass, highpassOffFreq, highpassFreq, hz)
             + Lowpass::responseDb (Lowpass::Kind::lowpass, lowpassOffFreq, lowpassFreq, hz);
    }

    /** Where the dry and wet signals go for the display, as they are in the output. Null for nowhere. */
    void setTap (SpectrumTap* newTap)   { tap = newTap; }

    /** As many channels as the output, of any length. A mono input into stereo is in the first. */
    void process (juce::AudioBuffer<float>& buffer, const Transport& transport);

private:
    // 24 and 12 dB/oct
    using Highpass = GlidingFilter<2>;
    using Lowpass = GlidingFilter<1>;

    void processChunk (juce::AudioBuffer<float>& buffer);

    Settings settings;
    double sampleRate = 0.0;
    bool monoIntoStereo = false;

    juce::Reverb reverb;

    Highpass highpass { Highpass::Kind::highpass, highpassOffFreq };
    Lowpass lowpass { Lowpass::Kind::lowpass, lowpassOffFreq };

    Drive drive;
    PingPongDelay delay;
    Ducker ducker;
    float lastDelayLevel = 0.0f;

    // With the output gain on them
    MixGains gains { 1.0f, 0.0f }, lastGains { 1.0f, 0.0f };
    bool ramping = false;

    juce::AudioBuffer<float> wet, echoes;

    SpectrumTap* tap = nullptr;
};
