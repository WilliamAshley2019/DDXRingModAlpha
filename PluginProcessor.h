/*
  DDX3216 Ring Modulator Plugin
  JUCE 8.0.11 - Faithful SHARC DSP Port with SIMD Optimization
  Based on Behringer DDX3216's SHARC ADSP-21160 ring mod algorithms
  Supports both scalar (authentic) and SIMD (optimized) processing
  Waveforms: Sine, Triangle, Square
*/

#pragma once
#include <JuceHeader.h>

//==============================================================================
// Main Plugin Processor
//==============================================================================
class DdxRingModAudioProcessor : public juce::AudioProcessor
{
public:
    DdxRingModAudioProcessor();
    ~DdxRingModAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DDX3216 Ring Modulator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // CPU monitoring
    float getCpuUsage() const { return static_cast<float>(cpuUsage); }

private:
    enum class Waveform { Sine, Triangle, Square };

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Oscillator state
    float phase = 0.0f;
    float phaseInc = 0.0f;
    double currentSampleRate = 48000.0;
    bool useSIMD = false;
    Waveform currentWaveform = Waveform::Sine;

    // CPU monitoring
    double cpuUsage = 0.0;

    // Helper functions for waveforms (optimized for audio)
    float sineWave(float t) const noexcept;
    float triangleWave(float t) const noexcept;
    float squareWave(float t) const noexcept;

    // Process blocks
    void processBlockScalar(juce::AudioBuffer<float>& buffer, float blend);
    void processBlockSIMD(juce::AudioBuffer<float>& buffer, float blend);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DdxRingModAudioProcessor)
};