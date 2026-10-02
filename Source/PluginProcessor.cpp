#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> msRange (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout BlipAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<P> (juce::ParameterID { "attack", 1 }, "Attack", msRange (1.0f, 2000.0f, 150.0f), 30.0f, A().withLabel ("ms")));
    p.push_back (std::make_unique<P> (juce::ParameterID { "decay", 1 }, "Decay", msRange (10.0f, 4000.0f, 600.0f), 820.0f, A().withLabel ("ms")));
    p.push_back (std::make_unique<P> (juce::ParameterID { "sustain", 1 }, "Sustain", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f));
    p.push_back (std::make_unique<P> (juce::ParameterID { "release", 1 }, "Release", msRange (10.0f, 8000.0f, 1000.0f), 1300.0f, A().withLabel ("ms")));
    p.push_back (std::make_unique<P> (juce::ParameterID { "cutoff", 1 }, "Cutoff", msRange (40.0f, 20000.0f, 1200.0f), 1700.0f, A().withLabel ("Hz")));
    p.push_back (std::make_unique<P> (juce::ParameterID { "reso", 1 }, "Reso", juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f));
    p.push_back (std::make_unique<P> (juce::ParameterID { "kegle", 1 }, "Kegle", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    p.push_back (std::make_unique<P> (juce::ParameterID { "glide", 1 }, "Glide", juce::NormalisableRange<float> (0.0f, 400.0f), 80.0f, A().withLabel ("ms")));
    p.push_back (std::make_unique<P> (juce::ParameterID { "output", 1 }, "Output", juce::NormalisableRange<float> (-24.0f, 6.0f), 0.0f, A().withLabel ("dB")));

    // Kuglens drejning. Kan automatiseres i Ableton.
    p.push_back (std::make_unique<P> (juce::ParameterID { "yaw", 1 }, "Drej vandret",
                                      juce::NormalisableRange<float> (-juce::MathConstants<float>::pi, juce::MathConstants<float>::pi), 0.55f));
    p.push_back (std::make_unique<P> (juce::ParameterID { "pitch", 1 }, "Drej lodret", juce::NormalisableRange<float> (-1.4f, 1.4f), -0.3f));

    return { p.begin(), p.end() };
}

BlipAudioProcessor::BlipAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BLIP", createLayout())
{
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new blip::BlipVoice (shared));
    synth.addSound (new blip::BlipSound());
    blip::computeWeights (0.55f, -0.3f, 0.5f, shared.weights);
}

bool BlipAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BlipAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<blip::BlipVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate);

    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));
}

void BlipAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isControllerOfType (1))
            modWheel.store ((float) m.getControllerValue() / 127.0f);
    }

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    juce::ADSR::Parameters env { get ("attack") / 1000.0f, get ("decay") / 1000.0f, get ("sustain"), get ("release") / 1000.0f };
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<blip::BlipVoice*> (synth.getVoice (i)))
            v->setADSR (env);

    shared.cutoffHz = get ("cutoff");
    shared.reso = get ("reso");
    shared.glideSec = get ("glide") / 1000.0f;

    const float yaw = get ("yaw") + modWheel.load() * modYawAmount;
    blip::computeWeights (yaw, get ("pitch"), get ("kegle"), shared.weights);

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    outGain.setTargetValue (juce::Decibels::decibelsToGain (get ("output")));
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        const float g = outGain.getNextValue();
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* d = buffer.getWritePointer (c);
            d[n] = std::tanh (d[n] * g);
        }
    }
}

juce::AudioProcessorEditor* BlipAudioProcessor::createEditor() { return new BlipAudioProcessorEditor (*this); }

void BlipAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BlipAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BlipAudioProcessor(); }
