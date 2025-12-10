/*
  DDX3216 Ring Modulator Plugin - Editor Implementation
  JUCE 8.0.11
*/

#include "PluginProcessor.h"  // FIXED: Was "RingModProcessor.h"
#include "PluginEditor.h"     // FIXED: Was "RingModEditor.h"


//==============================================================================
DdxRingModAudioProcessorEditor::DdxRingModAudioProcessorEditor(DdxRingModAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(700, 380);

    // Setup controls
    setupControl(rateControl, "rate", "Rate", true);
    setupControl(blendControl, "blend", "Blend");

    // Waveform combo box
    addAndMakeVisible(waveformCombo);
    waveformCombo.addItemList(juce::StringArray{ "Sine", "Triangle", "Square" }, 1);
    waveformCombo.setSelectedId(1, juce::dontSendNotification);

    waveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), "waveform", waveformCombo);

    // Waveform label
    addAndMakeVisible(waveformLabel);
    waveformLabel.setText("Waveform", juce::dontSendNotification);
    waveformLabel.setJustificationType(juce::Justification::centred);
    waveformLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    waveformLabel.attachToComponent(&waveformCombo, false);

    // Bypass button
    addAndMakeVisible(bypassButton);
    bypassButton.setButtonText("Bypass");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "bypass", bypassButton);

    // SIMD mode toggle
    addAndMakeVisible(simdButton);
    simdButton.setButtonText("Use SIMD (Low CPU)");
    simdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), "simd", simdButton);

    // Processing mode label
    addAndMakeVisible(processingModeLabel);
    processingModeLabel.setText("Processing Mode:", juce::dontSendNotification);
    processingModeLabel.setJustificationType(juce::Justification::centredLeft);
    processingModeLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));

    // Start timer for CPU monitoring
    startTimerHz(10);
}

DdxRingModAudioProcessorEditor::~DdxRingModAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void DdxRingModAudioProcessorEditor::setupControl(ControlGroup& control,
    const juce::String& paramID,
    const juce::String& labelText,
    bool isFrequency)
{
    addAndMakeVisible(control.slider);
    control.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);

    // Set appropriate text suffix
    if (isFrequency)
        control.slider.setTextValueSuffix(" Hz");
    else
        control.slider.setTextValueSuffix(" %");

    addAndMakeVisible(control.label);
    control.label.setText(labelText, juce::dontSendNotification);
    control.label.setJustificationType(juce::Justification::centred);
    control.label.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    control.label.attachToComponent(&control.slider, false);

    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), paramID, control.slider);
}

//==============================================================================
void DdxRingModAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background gradient (DDX3216 blue/grey theme)
    g.fillAll(juce::Colour(0xff2a2d3a));

    auto bounds = getLocalBounds();

    // Header
    auto headerArea = bounds.removeFromTop(60);
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff3a4a5a), 0.0f, 0.0f,
        juce::Colour(0xff2a3a4a), 0.0f, static_cast<float> (headerArea.getHeight()), false));
    g.fillRect(headerArea);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    g.drawFittedText("DDX3216 RING MODULATOR", headerArea.reduced(10, 0),
        juce::Justification::centred, 1);

    g.setFont(juce::FontOptions(12.0f));
    g.drawFittedText("SHARC DSP Authentic Port | Stereo Ring Modulation",
        headerArea.reduced(10, 35).removeFromBottom(15),
        juce::Justification::centred, 1);

    // Control panel background
    auto controlArea = bounds.removeFromTop(220);
    g.setColour(juce::Colour(0xff1a1d2a));
    g.fillRoundedRectangle(controlArea.reduced(10, 5).toFloat(), 8.0f);

    // Draw waveform visualizer
    auto vizArea = controlArea.reduced(20, 10);
    vizArea = vizArea.withTop(vizArea.getBottom() - 80).withHeight(60);

    g.setColour(juce::Colour(0xff3a4a6a).withAlpha(0.3f));
    g.fillRoundedRectangle(vizArea.toFloat(), 4.0f);

    // Draw waveform preview based on current selection
    g.setColour(juce::Colours::lightgreen.withAlpha(0.8f));
    juce::Path wavePath;

    int waveformIdx = static_cast<int>(*audioProcessor.getAPVTS().getRawParameterValue("waveform"));
    int numPoints = 100;

    for (int i = 0; i <= numPoints; ++i)
    {
        float x = vizArea.getX() + (vizArea.getWidth() * i / (float)numPoints);
        float t = (i / (float)numPoints) * juce::MathConstants<float>::twoPi;
        float y = 0.0f;

        // Draw based on selected waveform
        switch (waveformIdx)
        {
        case 0: // Sine
            y = std::sin(t);
            break;
        case 1: // Triangle
        {
            float modt = std::fmod(t, juce::MathConstants<float>::twoPi);
            float val = 2.0f * modt / juce::MathConstants<float>::pi;
            if (val > 1.0f) val = 2.0f - val;
            if (val < -1.0f) val = -2.0f - val;
            y = val;
        }
        break;
        case 2: // Square
            y = std::sin(t) >= 0.0f ? 1.0f : -1.0f;
            break;
        }

        float yPos = vizArea.getCentreY() - (y * vizArea.getHeight() * 0.35f);

        if (i == 0)
            wavePath.startNewSubPath(x, yPos);
        else
            wavePath.lineTo(x, yPos);
    }

    g.strokePath(wavePath, juce::PathStrokeType(2.0f));

    // Footer with CPU meter
    auto footerArea = bounds;
    g.setColour(juce::Colour(0xff1a1d2a));
    g.fillRect(footerArea.reduced(10, 5));

    // CPU usage meter
    bool usingSIMD = *audioProcessor.getAPVTS().getRawParameterValue("simd") > 0.5f;
    juce::String cpuText = juce::String("CPU: ") +
        juce::String(currentCpuUsage * 100.0f, 1) +
        "% | Mode: " +
        (usingSIMD ? "SIMD (Optimized)" : "Scalar (Authentic)");

    g.setColour(usingSIMD ? juce::Colours::lightgreen : juce::Colours::orange);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(cpuText, footerArea.reduced(15, 10), juce::Justification::centredLeft);

    // Draw CPU usage bar
    auto cpuBarArea = footerArea.reduced(15, 20).removeFromRight(200).withHeight(15);
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(cpuBarArea);

    float cpuBarWidth = juce::jlimit(0.0f, 1.0f, currentCpuUsage);
    auto filledArea = cpuBarArea.withWidth(cpuBarArea.getWidth() * cpuBarWidth);

    if (cpuBarWidth < 0.5f)
        g.setColour(juce::Colours::lightgreen);
    else if (cpuBarWidth < 0.8f)
        g.setColour(juce::Colours::orange);
    else
        g.setColour(juce::Colours::red);

    g.fillRect(filledArea);
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRect(cpuBarArea, 1);

    // Draw dividers
    g.setColour(juce::Colour(0xff4a5a6a).withAlpha(0.3f));
    g.drawLine(10.0f, 60.0f, static_cast<float> (getWidth()) - 10.0f, 60.0f, 1.0f);
    g.drawLine(10.0f, static_cast<float> (getHeight()) - 95.0f,
        static_cast<float> (getWidth()) - 10.0f,
        static_cast<float> (getHeight()) - 95.0f, 1.0f);
}

//==============================================================================
void DdxRingModAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(65); // Skip header

    // Control sliders in a row
    auto controlArea = bounds.removeFromTop(200).reduced(20, 10);
    const int sliderWidth = 100;
    const int spacing = 20;

    rateControl.slider.setBounds(controlArea.removeFromLeft(sliderWidth));
    rateControl.label.setBounds(rateControl.slider.getX(),
        rateControl.slider.getY() - 25,
        sliderWidth, 20);

    controlArea.removeFromLeft(spacing);

    blendControl.slider.setBounds(controlArea.removeFromLeft(sliderWidth));
    blendControl.label.setBounds(blendControl.slider.getX(),
        blendControl.slider.getY() - 25,
        sliderWidth, 20);

    controlArea.removeFromLeft(spacing * 3);

    // Waveform selector
    waveformCombo.setBounds(controlArea.removeFromLeft(180).withHeight(30));
    waveformLabel.setBounds(waveformCombo.getX(),
        waveformCombo.getY() - 25,
        180, 20);

    // Footer controls
    auto footerArea = bounds.removeFromTop(80).reduced(20, 10);

    processingModeLabel.setBounds(footerArea.removeFromTop(25));

    auto buttonArea = footerArea.removeFromTop(30);
    bypassButton.setBounds(buttonArea.removeFromLeft(120));
    buttonArea.removeFromLeft(20);
    simdButton.setBounds(buttonArea.removeFromLeft(200));
}

//==============================================================================
void DdxRingModAudioProcessorEditor::timerCallback()
{
    // Update CPU usage display
    currentCpuUsage = audioProcessor.getCpuUsage();

    // Only repaint footer area (CPU meter) and waveform visualizer
    repaint(0, getHeight() - 95, getWidth(), 95);
    repaint(getWidth() - 300, 140, 280, 60); // Waveform visualizer area
}