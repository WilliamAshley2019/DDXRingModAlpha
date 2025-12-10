/*
  DDX3216 Ring Modulator Plugin - Editor Header
  JUCE 8.0.11
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h" 

//==============================================================================
class DdxRingModAudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    DdxRingModAudioProcessorEditor(DdxRingModAudioProcessor&);
    ~DdxRingModAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    DdxRingModAudioProcessor& audioProcessor;

    // Sliders with labels
    struct ControlGroup
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    ControlGroup rateControl;
    ControlGroup blendControl;

    juce::ComboBox waveformCombo;
    juce::Label waveformLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttachment;

    juce::ToggleButton bypassButton;
    juce::ToggleButton simdButton;
    juce::Label processingModeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> simdAttachment;

    // CPU meter
    float currentCpuUsage = 0.0f;

    void setupControl(ControlGroup& control, const juce::String& paramID,
        const juce::String& labelText, bool isFrequency = false);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DdxRingModAudioProcessorEditor)
};