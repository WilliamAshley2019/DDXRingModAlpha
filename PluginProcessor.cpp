/*
  DDX3216 Ring Modulator Plugin - Implementation
  JUCE 8.0.11
*/

#include "PluginProcessor.h"  // FIXED: Was "RingModProcessor.h"
#include "PluginEditor.h"     // FIXED: Was "RingModEditor.h"

//==============================================================================
//==============================================================================
DdxRingModAudioProcessor::DdxRingModAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

DdxRingModAudioProcessor::~DdxRingModAudioProcessor() {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DdxRingModAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Rate: 0-1 maps to frequency 0.5Hz to 20Hz (typical ring mod range)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "rate", "Rate",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    // Blend: 0-1 (clean to full mod)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "blend", "Blend",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    // Waveform: 0=Sine, 1=Triangle, 2=Square
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "waveform", "Waveform",
        juce::StringArray{ "Sine", "Triangle", "Square" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "bypass", "Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "simd", "Use SIMD (Low CPU)", false));

    return { params.begin(), params.end() };
}

//==============================================================================
float DdxRingModAudioProcessor::sineWave(float t) const noexcept
{
    return std::sin(t);
}

float DdxRingModAudioProcessor::triangleWave(float t) const noexcept
{
    // Fast triangle wave approximation
    float modt = std::fmod(t, juce::MathConstants<float>::twoPi);
    float val = 2.0f * modt / juce::MathConstants<float>::pi;
    if (val > 1.0f) val = 2.0f - val;
    if (val < -1.0f) val = -2.0f - val;
    return val;
}

float DdxRingModAudioProcessor::squareWave(float t) const noexcept
{
    return std::sin(t) >= 0.0f ? 1.0f : -1.0f;
}

//==============================================================================
void DdxRingModAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    phase = 0.0f;
}

//==============================================================================
void DdxRingModAudioProcessor::releaseResources() {}

//==============================================================================
bool DdxRingModAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

//==============================================================================
void DdxRingModAudioProcessor::processBlockScalar(juce::AudioBuffer<float>& buffer, float blend)
{
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    // Process each channel
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        float localPhase = phase;

        for (int i = 0; i < numSamples; ++i)
        {
            // Calculate modulator value based on waveform
            float modulator = 0.0f;
            switch (currentWaveform)
            {
            case Waveform::Sine:
                modulator = sineWave(localPhase);
                break;
            case Waveform::Triangle:
                modulator = triangleWave(localPhase);
                break;
            case Waveform::Square:
                modulator = squareWave(localPhase);
                break;
            }

            // Ring modulation: input * modulator
            data[i] = (1.0f - blend) * data[i] + blend * modulator * data[i];

            // Update phase
            localPhase += phaseInc;
            if (localPhase >= juce::MathConstants<float>::twoPi)
                localPhase -= juce::MathConstants<float>::twoPi;
        }
    }

    // Update global phase for next block
    phase += static_cast<float>(numSamples) * phaseInc;
    while (phase >= juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;
}

void DdxRingModAudioProcessor::processBlockSIMD(juce::AudioBuffer<float>& buffer, float blend)
{
    using SIMD = juce::dsp::SIMDRegister<float>;
    constexpr size_t simdWidth = SIMD::SIMDRegisterSize;

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);
        float localPhase = phase;
        size_t vectorSamples = (static_cast<size_t>(numSamples) / simdWidth) * simdWidth;

        // SIMD main loop
        for (size_t i = 0; i < vectorSamples; i += simdWidth)
        {
            // Generate SIMD modulator values
            alignas(32) float modVals[simdWidth];
            for (size_t j = 0; j < simdWidth; ++j)
            {
                float t = localPhase + static_cast<float>(j) * phaseInc;
                switch (currentWaveform)
                {
                case Waveform::Sine:
                    modVals[j] = sineWave(t);
                    break;
                case Waveform::Triangle:
                    modVals[j] = triangleWave(t);
                    break;
                case Waveform::Square:
                    modVals[j] = squareWave(t);
                    break;
                }
            }

            SIMD modVec = SIMD::fromRawArray(modVals);
            SIMD inVec = SIMD::fromRawArray(data + i);
            SIMD blendVec(blend);
            SIMD oneMinusBlend(1.0f - blend);

            // Output = (1 - blend) * input + blend * modulator * input
            SIMD outVec = oneMinusBlend * inVec + blendVec * modVec * inVec;
            outVec.copyToRawArray(data + i);

            localPhase += static_cast<float>(simdWidth) * phaseInc;
            if (localPhase >= juce::MathConstants<float>::twoPi)
                localPhase -= juce::MathConstants<float>::twoPi;
        }

        // Scalar tail
        for (int i = static_cast<int>(vectorSamples); i < numSamples; ++i)
        {
            float modulator = 0.0f;
            switch (currentWaveform)
            {
            case Waveform::Sine:
                modulator = sineWave(localPhase);
                break;
            case Waveform::Triangle:
                modulator = triangleWave(localPhase);
                break;
            case Waveform::Square:
                modulator = squareWave(localPhase);
                break;
            }

            data[i] = (1.0f - blend) * data[i] + blend * modulator * data[i];
            localPhase += phaseInc;
            if (localPhase >= juce::MathConstants<float>::twoPi)
                localPhase -= juce::MathConstants<float>::twoPi;
        }
    }

    // Update global phase
    phase += static_cast<float>(numSamples) * phaseInc;
    while (phase >= juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;
}

//==============================================================================
void DdxRingModAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // CPU monitoring
    auto startTime = juce::Time::getMillisecondCounterHiRes();
    auto numSamples = buffer.getNumSamples();

    // Bypass
    if (*apvts.getRawParameterValue("bypass") > 0.5f)
        return;

    // Get parameters
    float rateParam = *apvts.getRawParameterValue("rate");
    float blend = *apvts.getRawParameterValue("blend");
    int waveformIdx = static_cast<int> (*apvts.getRawParameterValue("waveform"));
    useSIMD = *apvts.getRawParameterValue("simd") > 0.5f;
    currentWaveform = static_cast<Waveform> (waveformIdx);

    // Compute phase increment: Map rate (0-1) to frequency (0.5-20Hz) as per SHARC code
    float freq = 0.5f + 19.5f * rateParam; // 0.5Hz to 20Hz
    phaseInc = static_cast<float>(juce::MathConstants<float>::twoPi * freq / currentSampleRate);

    // Process based on SIMD mode
    if (useSIMD)
        processBlockSIMD(buffer, blend);
    else
        processBlockScalar(buffer, blend);

    // Update CPU usage
    auto endTime = juce::Time::getMillisecondCounterHiRes();
    double blockTime = (endTime - startTime) / 1000.0; // seconds
    double expectedBlockTime = static_cast<double>(numSamples) / currentSampleRate;
    cpuUsage = blockTime / expectedBlockTime;
}

//==============================================================================
juce::AudioProcessorEditor* DdxRingModAudioProcessor::createEditor()
{
    return new DdxRingModAudioProcessorEditor(*this);
}

//==============================================================================
void DdxRingModAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DdxRingModAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DdxRingModAudioProcessor();
}