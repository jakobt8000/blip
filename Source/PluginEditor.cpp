#include "PluginEditor.h"
#include "BinaryData.h"

using namespace blip;

namespace
{
// Blip-logoet som pixels (31 x 15), tegnet efter Jakobs pixel-logo
const char* const kLogo[] = {
    "....####.#.....................",
    "..###..####....................",
    "..#.....#..#...................",
    ".#.....###.#....##.............",
    ".#..#..####....###.............",
    "..##..##...#...#.#..#......#...",
    "......#....##.#.#..........#...",
    ".....##.#...#.#.#...#.....###..",
    "#....#.#....###....##....##..#.",
    "#...#..#....#.#..##.#..##.#.#..",
    ".###...##..#...##....##..###...",
    "........###..............#.....",
    ".........................#.....",
    "........................#......",
    "........................#......",
};
constexpr int kLogoCols = 31, kLogoRows = 15;

const char* const kParamIds[] = { "attack", "decay", "sustain", "release", "cutoff", "reso", "kegle", "glide", "output" };
const char* const kKnobLabels[] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "CUTOFF", "RESO", "KEGLE", "GLIDE", "OUTPUT" };

// Layout (punkter i et 1000 x 760 vindue)
constexpr int kW = 1000, kH = 760;
constexpr int kLeftX = 44, kLeftW = 460;
constexpr int kLogoY = 38, kLogoH = 223;
constexpr int kSphereY = kLogoY + kLogoH;
constexpr int kRightX = 560, kRightW = 388, kTop = 100;
constexpr int kGridY = kTop + 45, kRowH = 125, kRowGap = 18, kColW = kRightW / 3;

float param (BlipAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

void setParam (BlipAudioProcessor& p, const char* id, float v)
{
    if (auto* prm = p.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

juce::String nameOf (int i) { return juce::String::fromUTF8 (soundNameUtf8 (i)); }

juce::String sentenceCase (const juce::String& s)
{
    auto low = s.toLowerCase()
                   .replace (juce::String::fromUTF8 ("\xc3\x86"), juce::String::fromUTF8 ("\xc3\xa6"))
                   .replace (juce::String::fromUTF8 ("\xc3\x98"), juce::String::fromUTF8 ("\xc3\xb8"));
    return low.substring (0, 1).toUpperCase() + low.substring (1);
}
} // namespace

juce::Font blip::monoFont (float height, bool semiBold, float kerning)
{
    static auto regular = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);
    static auto bold = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoSemiBold_ttf, BinaryData::IBMPlexMonoSemiBold_ttfSize);
    return juce::Font (juce::FontOptions (semiBold ? bold : regular).withHeight (height).withKerningFactor (kerning));
}

//==============================================================================
void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    // Tegnet i et 64 x 64 koordinatsystem, ligesom skitsen, og skaleret op
    const float s = (float) juce::jmin (w, h) / 64.0f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    auto pt = [&] (float r, float a) { return juce::Point<float> (cx + r * s * std::sin (a), cy - r * s * std::cos (a)); };

    for (int i = 0; i < 21; ++i)
    {
        const float a = startAngle + (float) i / 20.0f * (endAngle - startAngle);
        const bool lit = (float) i / 20.0f <= pos + 0.001f;
        g.setColour (lit ? colours::ink : colours::faint);
        g.drawLine ({ pt (26.0f, a), pt (i % 5 == 0 ? 31.0f : 29.0f, a) }, 0.8f * s);
    }

    g.setColour (colours::ink);
    g.drawEllipse (cx - 22.0f * s, cy - 22.0f * s, 44.0f * s, 44.0f * s, 0.8f * s);

    const float angle = startAngle + pos * (endAngle - startAngle);
    if (pos > 0.005f)
    {
        juce::Path arc;
        arc.addCentredArc (cx, cy, 22.0f * s, 22.0f * s, 0.0f, startAngle, angle, true);
        g.strokePath (arc, juce::PathStrokeType (2.5f * s));
    }

    g.drawLine ({ { cx, cy }, pt (16.0f, angle) }, 1.2f * s);
    g.fillEllipse (cx - 2.0f * s, cy - 2.0f * s, 4.0f * s, 4.0f * s);
}

//==============================================================================
void SphereView::paint (juce::Graphics& g)
{
    const float C = (float) getHeight() * 0.5f, R = C * (200.0f / 230.0f);
    const float yaw = param (proc, "yaw") + proc.modWheel.load() * BlipAudioProcessor::modYawAmount;
    const float pitch = param (proc, "pitch");

    std::array<float, kNumSounds> w {};
    computeWeights (yaw, pitch, param (proc, "kegle"), w);

    // punktskyen
    for (int i = 0; i < 480; ++i)
    {
        const auto r = rotate (fibPoint (480, i), yaw, pitch);
        g.setColour (r.z > 0.0f ? (r.z > 0.55f ? juce::Colour (0xff4e4d49) : juce::Colour (0xffa3a29c)) : juce::Colour (0xffe6e5e0));
        g.fillEllipse (C + r.x * R - 0.95f, C + r.y * R - 0.95f, 1.9f, 1.9f);
    }

    std::array<Vec3, kNumSounds> p {};
    int top = 0;
    for (int i = 0; i < kNumSounds; ++i)
    {
        p[(size_t) i] = rotate (soundPos (i), yaw, pitch);
        if (w[(size_t) i] > w[(size_t) top]) top = i;
    }
    auto sx = [&] (int i) { return C + p[(size_t) i].x * R; };
    auto sy = [&] (int i) { return C + p[(size_t) i].y * R; };

    // stjernebilledets streger (kun forsiden)
    g.setColour (colours::ink);
    for (auto& e : constellationEdges())
        if (p[(size_t) e.first].z > 0.1f && p[(size_t) e.second].z > 0.1f)
            g.drawLine (sx (e.first), sy (e.first), sx (e.second), sy (e.second), 0.8f);

    // lydene, bagerst først
    std::array<int, kNumSounds> order {};
    for (int i = 0; i < kNumSounds; ++i) order[(size_t) i] = i;
    std::sort (order.begin(), order.end(), [&] (int a, int b) { return p[(size_t) a].z < p[(size_t) b].z; });

    for (int i : order)
    {
        const float z = p[(size_t) i].z;
        const float r = z <= 0.1f ? 1.5f : (i == top ? 6.0f : 2.8f + z * 2.0f);
        g.setColour (z <= 0.1f ? colours::faint : colours::ink);
        g.fillEllipse (sx (i) - r, sy (i) - r, r * 2.0f, r * 2.0f);
    }

    g.setColour (colours::ink);
    g.drawEllipse (sx (top) - 11.0f, sy (top) - 11.0f, 22.0f, 22.0f, 1.0f);

    // trådkorset = lyttepunktet
    g.drawLine (C - 6.0f, C, C + 6.0f, C, 1.5f);
    g.drawLine (C, C - 6.0f, C, C + 6.0f, 1.5f);

    // navne på forsiden
    hits.clear();
    for (int i : order)
    {
        if (p[(size_t) i].z <= 0.1f) continue;
        const bool isTop = i == top;
        const auto f = monoFont (11.0f, isTop);
        const auto text = nameOf (i);
        const float tw = juce::GlyphArrangement::getStringWidth (f, text);
        const juce::Rectangle<float> area (sx (i) + 13.0f, sy (i) - 7.0f, tw, 14.0f);

        g.setFont (f);
        g.setColour (w[(size_t) i] > 0.005f ? colours::ink : colours::mid);
        g.drawText (text, area, juce::Justification::centredLeft, false);
        if (isTop)
            g.fillRect (area.getX(), area.getBottom() + 2.0f, tw, 1.0f);

        hits.push_back ({ area.expanded (6.0f, 4.0f), i });
    }
}

void SphereView::mouseDown (const juce::MouseEvent& e)
{
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->area.contains (e.position))
        {
            if (onJumpTo) onJumpTo (it->index);
            return;
        }

    dragging = true;
    dragYaw = param (proc, "yaw");
    dragPitch = param (proc, "pitch");
    proc.apvts.getParameter ("yaw")->beginChangeGesture();
    proc.apvts.getParameter ("pitch")->beginChangeGesture();
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void SphereView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const auto d = e.getOffsetFromDragStart().toFloat();
    setParam (proc, "yaw", wrapPi (dragYaw - d.x * 0.008f));
    setParam (proc, "pitch", juce::jlimit (-1.4f, 1.4f, dragPitch - d.y * 0.008f));
    repaint();
}

void SphereView::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    proc.apvts.getParameter ("yaw")->endChangeGesture();
    proc.apvts.getParameter ("pitch")->endChangeGesture();
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

//==============================================================================
BlipAudioProcessorEditor::BlipAudioProcessorEditor (BlipAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), sphere (p)
{
    setSize (kW, kH);

    addAndMakeVisible (sphere);
    sphere.onJumpTo = [this] (int i) { jumpTo (i); };

    for (int i = 0; i < kNumKnobs; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.setLookAndFeel (&knobLook);
        k.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        addAndMakeVisible (k);
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, kParamIds[i], k));
        if (auto* prm = proc.apvts.getParameter (kParamIds[i]))
            k.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }

    startTimerHz (30);
}

BlipAudioProcessorEditor::~BlipAudioProcessorEditor()
{
    for (auto& k : knobs) k.setLookAndFeel (nullptr);
}

void BlipAudioProcessorEditor::resized()
{
    sphere.setBounds (kLeftX, kSphereY, kLeftW + 50, kLeftW); // lidt ekstra bredde til navne i højre kant

    for (int i = 0; i < kNumKnobs; ++i)
    {
        const int col = i % 3, row = i / 3;
        const int cx = kRightX + col * kColW + kColW / 2;
        const int ry = kGridY + row * (kRowH + kRowGap);
        knobs[(size_t) i].setBounds (cx - 44, ry + 17, 88, 88);
    }
}

juce::String BlipAudioProcessorEditor::formatValue (int i) const
{
    const float v = param (proc, kParamIds[i]);
    switch (i)
    {
        case 0: case 1: case 7: return juce::String (juce::roundToInt (v)) + " MS";
        case 3: return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " S" : juce::String (juce::roundToInt (v)) + " MS";
        case 2: case 5: return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        case 4: return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + "K" : juce::String (juce::roundToInt (v));
        case 6: return juce::String (juce::roundToInt (juce::radiansToDegrees (coneAngle (v)))) + juce::String::fromUTF8 ("\xc2\xb0");
        case 8: return juce::String (v, 1) + " DB";
        default: return {};
    }
}

void BlipAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::paper);
    g.setColour (colours::ink);

    // logo
    const float cell = (float) kLeftW / (float) kLogoCols;
    for (int r = 0; r < kLogoRows; ++r)
        for (int c = 0; c < kLogoCols; ++c)
            if (kLogo[r][c] == '#')
                g.fillRect (juce::Rectangle<float> ((float) kLeftX + (float) c * cell, (float) kLogoY + (float) r * cell, cell + 0.5f, cell + 0.5f));

    // højre kolonne: overskrift
    g.setFont (monoFont (11.0f, true, 0.1f));
    g.drawText ("FORM", kRightX, kTop, 200, 16, juce::Justification::centredLeft);
    g.setFont (monoFont (11.0f, false, 0.1f));
    g.drawText (juce::String::fromUTF8 ("TR\xc3\x86K = JUST\xc3\x89R"), kRightX, kTop, kRightW, 16, juce::Justification::centredRight);
    g.fillRect (kRightX, kTop + 24, kRightW, 1);

    // knap-navne og værdier
    for (int i = 0; i < kNumKnobs; ++i)
    {
        const int col = i % 3, row = i / 3;
        const int x = kRightX + col * kColW;
        const int ry = kGridY + row * (kRowH + kRowGap);
        g.setFont (monoFont (10.0f, false, 0.1f));
        g.drawText (kKnobLabels[i], x, ry, kColW, 13, juce::Justification::centred);
        g.setFont (monoFont (12.0f, false, 0.0f));
        g.drawText (formatValue (i), x, ry + 109, kColW, 16, juce::Justification::centred);
    }

    // nærmeste lyd og blandingen
    const float yaw = param (proc, "yaw") + proc.modWheel.load() * BlipAudioProcessor::modYawAmount;
    std::array<float, kNumSounds> w {};
    computeWeights (yaw, param (proc, "pitch"), param (proc, "kegle"), w);

    std::array<int, kNumSounds> order {};
    for (int i = 0; i < kNumSounds; ++i) order[(size_t) i] = i;
    std::sort (order.begin(), order.end(), [&] (int a, int b) { return w[(size_t) a] > w[(size_t) b]; });

    const int ry = kGridY + 3 * kRowH + 2 * kRowGap + 24;
    g.fillRect (kRightX, ry, kRightW, 1);
    g.setFont (monoFont (11.0f, false, 0.1f));
    g.drawText (juce::String::fromUTF8 ("N\xc3\x86RMEST"), kRightX, ry + 12, 150, 16, juce::Justification::centredLeft);
    g.setFont (monoFont (11.0f, true, 0.1f));
    g.drawText (nameOf (order[0]) + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (juce::roundToInt (w[(size_t) order[0]] * 100.0f)) + "%",
                kRightX, ry + 12, kRightW, 16, juce::Justification::centredRight);

    juce::StringArray parts;
    for (int i : order)
        if (w[(size_t) i] > 0.005f)
            parts.add (sentenceCase (nameOf (i)) + " (" + juce::String (w[(size_t) i] * 100.0f, 1).replaceCharacter ('.', ',') + "%)");

    juce::AttributedString as;
    as.append (parts.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 ")), monoFont (11.0f, false, 0.0f), colours::soft);
    as.setWordWrap (juce::AttributedString::byWord);
    as.setLineSpacing (4.0f);
    juce::TextLayout layout;
    layout.createLayout (as, (float) kRightW);
    layout.draw (g, juce::Rectangle<float> ((float) kRightX, (float) ry + 36.0f, (float) kRightW, 60.0f));
}

void BlipAudioProcessorEditor::jumpTo (int i)
{
    auto [yawFace, pitchFace] = facingAngles (i);
    animFromYaw = param (proc, "yaw");
    animFromPitch = param (proc, "pitch");
    animToYaw = animFromYaw + wrapPi (yawFace - proc.modWheel.load() * BlipAudioProcessor::modYawAmount - animFromYaw);
    animToPitch = juce::jlimit (-1.4f, 1.4f, pitchFace);
    animT = 0.0f;
    if (! animating)
    {
        proc.apvts.getParameter ("yaw")->beginChangeGesture();
        proc.apvts.getParameter ("pitch")->beginChangeGesture();
    }
    animating = true;
}

void BlipAudioProcessorEditor::timerCallback()
{
    if (animating)
    {
        animT = juce::jmin (1.0f, animT + 1.0f / 9.0f);
        const float e = animT < 0.5f ? 2.0f * animT * animT : 1.0f - std::pow (-2.0f * animT + 2.0f, 2.0f) / 2.0f;
        setParam (proc, "yaw", wrapPi (animFromYaw + (animToYaw - animFromYaw) * e));
        setParam (proc, "pitch", animFromPitch + (animToPitch - animFromPitch) * e);
        if (animT >= 1.0f)
        {
            animating = false;
            proc.apvts.getParameter ("yaw")->endChangeGesture();
            proc.apvts.getParameter ("pitch")->endChangeGesture();
        }
    }

    // tegn kun igen når noget har ændret sig (automation, modhjul, knapper)
    float snap = proc.modWheel.load() * 7.0f;
    for (auto* id : { "yaw", "pitch", "kegle", "attack", "decay", "sustain", "release", "cutoff", "reso", "glide", "output" })
        snap = snap * 1.0001f + param (proc, id);
    if (! juce::exactlyEqual (snap, lastSnapshot))
    {
        lastSnapshot = snap;
        repaint();
    }
}
