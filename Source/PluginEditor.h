#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace blip
{
namespace colours
{
    const juce::Colour paper { 0xfffbfaf6 };
    const juce::Colour ink   { 0xff161514 };
    const juce::Colour mid   { 0xff8f8e88 };
    const juce::Colour soft  { 0xff6e6d68 };
    const juce::Colour faint { 0xffdddcd6 };
}

juce::Font monoFont (float height, bool semiBold = false, float kerning = 0.06f);

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// Kuglen: punktsky, stjernebillede og navne. Træk for at dreje, klik på et navn for at hoppe.
class SphereView : public juce::Component
{
public:
    explicit SphereView (BlipAudioProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void (int)> onJumpTo;

private:
    BlipAudioProcessor& proc;
    float dragYaw = 0.0f, dragPitch = 0.0f;
    bool dragging = false;
    struct Hit { juce::Rectangle<float> area; int index; };
    std::vector<Hit> hits;
};
} // namespace blip

class BlipAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BlipAudioProcessorEditor (BlipAudioProcessor&);
    ~BlipAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void jumpTo (int soundIndex);
    juce::String formatValue (int knob) const;

    BlipAudioProcessor& proc;
    blip::KnobLookAndFeel knobLook;
    blip::SphereView sphere;

    static constexpr int kNumKnobs = 9;
    std::array<juce::Slider, kNumKnobs> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

    // lille glide-animation når man klikker på et navn
    bool animating = false;
    float animFromYaw = 0, animFromPitch = 0, animToYaw = 0, animToPitch = 0, animT = 0;

    float lastSnapshot = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlipAudioProcessorEditor)
};
