// Lokal test: spiller toner gennem Blip ved flere kugle-positioner, gemmer wav og et billede af vinduet.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PluginProcessor.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    BlipAudioProcessor proc;
    const double sr = 48000.0;
    const int block = 512;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    juce::AudioBuffer<float> all (2, (int) (sr * 14 * 2.0));
    all.clear();
    int writePos = 0;
    bool ok = true;

    for (int s = 0; s < 14; ++s)
    {
        auto [yaw, pitch] = blip::facingAngles (s);
        proc.apvts.getParameter ("yaw")->setValueNotifyingHost (proc.apvts.getParameter ("yaw")->convertTo0to1 (yaw));
        proc.apvts.getParameter ("pitch")->setValueNotifyingHost (proc.apvts.getParameter ("pitch")->convertTo0to1 (pitch));
        proc.apvts.getParameter ("kegle")->setValueNotifyingHost (0.1f);

        float peak = 0.0f; double rms = 0.0; int count = 0;
        const int total = (int) (sr * 2.0);
        for (int pos = 0; pos < total; pos += block)
        {
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            if (pos == 0) { midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 90), 0); }
            if (pos >= (int) (sr * 1.2) && pos < (int) (sr * 1.2) + block) { midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0); midi.addEvent (juce::MidiMessage::noteOff (1, 55), 0); }
            proc.processBlock (buf, midi);
            for (int n = 0; n < block; ++n)
            {
                const float v = buf.getSample (0, n);
                if (! std::isfinite (v)) ok = false;
                peak = std::max (peak, std::abs (v)); rms += v * v; ++count;
                if (writePos + n < all.getNumSamples()) { all.setSample (0, writePos + n, v); all.setSample (1, writePos + n, buf.getSample (1, n)); }
            }
            writePos += block;
        }
        std::printf ("%-12s peak %.3f  rms %.4f\n", blip::soundNameUtf8 (s), peak, std::sqrt (rms / count));
        if (peak < 0.01f) ok = false;
    }

    juce::File out ("/home/claude/blip/test_out.wav");
    out.deleteFile();
    juce::WavAudioFormat wav;
    if (auto stream = out.createOutputStream())
        if (auto* w = wav.createWriterFor (stream.get(), sr, 2, 16, {}, 0)) { stream.release(); std::unique_ptr<juce::AudioFormatWriter> wr (w); wr->writeFromAudioSampleBuffer (all, 0, std::min (writePos, all.getNumSamples())); }

    // billede af vinduet
    proc.apvts.getParameter ("yaw")->setValueNotifyingHost (proc.apvts.getParameter ("yaw")->convertTo0to1 (0.55f));
    proc.apvts.getParameter ("pitch")->setValueNotifyingHost (proc.apvts.getParameter ("pitch")->convertTo0to1 (-0.3f));
    proc.apvts.getParameter ("kegle")->setValueNotifyingHost (0.5f);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File png ("/home/claude/blip/test_ui.png");
    png.deleteFile();
    juce::PNGImageFormat fmt;
    if (auto s = png.createOutputStream()) fmt.writeImageToStream (img, *s);
    ed.reset();

    std::printf (ok ? "OK\n" : "FEJL\n");
    return ok ? 0 : 1;
}
