#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <list>
#include <map>
#include <memory>

#include "PluginProcessor.h"
#include "TwinPanel.h"

//==============================================================================
/** 64-bit key from a few numbers (cache keys). */
inline uint64_t hashKey (std::initializer_list<uint64_t> values)
{
    uint64_t h = 1469598103934665603ull;
    for (const auto v : values)
        h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
}

/** Rendered images by key. Once the byte budget is exceeded the least recently used ones are dropped,
    so memory stays bounded however often the look or the window size changes. */
class ImageLru
{
public:
    explicit ImageLru (size_t budgetBytes) : budget (budgetBytes) {}

    juce::Image find (uint64_t key)
    {
        for (auto it = items.begin(); it != items.end(); ++it)
            if (it->key == key)
            {
                items.splice (items.begin(), items, it);   // now the most recently used
                return items.front().image;
            }
        return {};
    }

    /** Drops the least recently used images until at most 'bytes' are kept. */
    void trimTo (size_t bytes)
    {
        while (used > bytes && ! items.empty())
        {
            used -= items.back().bytes;
            items.pop_back();
        }
    }

    /** Raises the budget (never lowers it), e.g. so that two complete looks fit in a very large window. */
    void ensureBudget (size_t bytes) { budget = std::max (budget, bytes); }

    /** 'group' tells which window size the image was made for (see UiCache::isShown). */
    void put (uint64_t key, const juce::Image& image, uint64_t group)
    {
        const size_t bytes = (size_t) image.getWidth() * (size_t) image.getHeight() * 4u;
        if (bytes > budget)
            return;
        items.push_front ({ key, image, bytes, group });
        used += bytes;
        while (used > budget && ! items.empty())
        {
            used -= items.back().bytes;
            items.pop_back();
        }
    }

    /** For idle-time work: true if 'bytes' more fit. Room is made only by dropping images whose group
        'keep' rejects (sizes no open window shows any more), never anything a window may switch back to. */
    template <typename Keep>
    bool makeRoom (size_t bytes, Keep&& keep)
    {
        for (auto it = items.end(); used + bytes > budget && it != items.begin();)
        {
            --it;
            if (! keep (it->group))
            {
                used -= it->bytes;
                it = items.erase (it);
            }
        }
        return used + bytes <= budget;
    }

private:
    struct Item { uint64_t key; juce::Image image; size_t bytes; uint64_t group; };
    std::list<Item> items;
    size_t budget, used = 0;
};

//==============================================================================
/** Per-editor cache of the procedural textures (brushed metal, frosted lenses, meter paper). They are made
    at full strength and drawn with the wanted strength as opacity, so one texture serves every revision. */
struct GrainCache
{
    juce::Image get (int kind, uint32_t seed, int pixelW, int pixelH, float pixelsPerUnit);
    void trimTo (size_t bytes) { images.trimTo (bytes); }

private:
    ImageLru images { 48u << 20 };
};

/** Rendered artwork shared by every TwinFET window in this process. The plugin keeps a reference while its
    window is closed, so reopening it, or switching to a look it has shown before, needs no rendering. */
struct UiCache
{
    GrainCache grain;
    ImageLru panelLayers { 64u << 20 };
    ImageLru vuLayers { 24u << 20 };

    /** Each open window registers the size it shows ('group'); 0 = none. Idle-time rendering never drops
        images of a shown size, so several open windows cannot keep re-rendering each other's artwork. */
    void showGroup (uint64_t& mine, uint64_t now)
    {
        if (mine == now)
            return;
        if (auto it = shown.find (mine); mine != 0 && it != shown.end() && --(it->second) <= 0)
            shown.erase (it);
        if (now != 0)
            ++shown[now];
        mine = now;
    }
    bool isShown (uint64_t group) const { return shown.count (group) != 0; }

    static std::shared_ptr<UiCache> shared();   // message thread only

private:
    std::map<uint64_t, int> shown;
};

//==============================================================================
/** Draws the knobs and push buttons with the shared panel artwork (TwinPanel.h). */
class TwinLook final : public juce::LookAndFeel_V4
{
public:
    TwinLook();

    float scale = 1.0f;                       // UI scale: pixels per base unit
    twinui::Palette palette = twinui::paletteFor (0, 2);
    juce::Typeface::Ptr face;                 // embedded panel font
    std::shared_ptr<UiCache> cache = UiCache::shared();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override {}

    // pop-ups: tool-tips and the value shown while a knob is turned
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip,
                     const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    int getSliderPopupPlacement (juce::Slider&) override;
    void setComponentEffectForBubbleComponent (juce::BubbleComponent&) override;

    juce::Font panelFont (float heightPx, float kerning) const;

private:
    struct TipLayout
    {
        juce::TextLayout title, body;
        float width = 0.0f, height = 0.0f, gap = 0.0f;
    };
    float popupScale() const;   // pop-up text follows the window size, within readable limits
    TipLayout layoutTip (const juce::String& tip) const;
};

/** Tool-tip window that can be switched off (the POP-UPS button) and draws without a square backdrop. */
class TwinTooltipWindow final : public juce::TooltipWindow
{
public:
    TwinTooltipWindow (juce::Component* parent, int delayMs) : juce::TooltipWindow (parent, delayMs) { setOpaque (false); }
    juce::String getTipFor (juce::Component& c) override { return enabled ? juce::TooltipWindow::getTipFor (c) : juce::String(); }
    bool enabled = true;
};

//==============================================================================
/** Rotary knob whose mouse area is the knob itself (the component is padded for the shadow). */
class KnobSlider final : public juce::Slider
{
public:
    bool hitTest (int x, int y) override;
};

/** Lit push button; only the lens/bezel is clickable (the padding holds the shadow). */
class LitButton final : public juce::TextButton
{
public:
    bool hitTest (int x, int y) override;
    float pad = twinui::buttonPad;   // in pixels, updated on resize
};

/** Idle-time rendering runs only up to this screen density (device pixels per base unit; 2 = a 1000 px window
    on a 200 % screen). Larger windows would need texture work too long for one slice (the meter would
    stutter), so there a look is rendered when first shown and then kept (two complete looks always fit). */
inline constexpr float kIdleWorkMaxPixelsPerUnit = 2.25f;

/** Static artwork in two cached layers: the faceplate, and the opened unit below it. Each is rendered
    supersampled when the window is small and averaged down, so textures and print stay crisp. Rendered
    layers are kept per look and size, so switching revision, mode or the drawer back is instant. */
class PanelArt final : public juce::Component
{
public:
    explicit PanelArt (TwinLook& l) : look (l) { setInterceptsMouseClicks (false, false); setOpaque (true); }
    ~PanelArt() override { look.cache->showGroup (shownGroup, 0); }
    void paint (juce::Graphics&) override;
    void invalidate() { repaint(); }   // the look changed: paint picks (or renders) the matching layers

    /** Idle-time work: renders one thin band of a layer for look p at the current size, so that switching to
        that look later is instant. Returns false when there was nothing to do for it (already rendered, or
        no room for it in the cache without dropping something more useful). */
    bool prerender (const twinui::Palette& p, bool internals);

private:
    struct Geometry { int pw = 0, ph = 0, faceRows = 0; float k = 0.0f; };
    Geometry geometryFor (float devicePerPixel) const;
    static uint64_t groupFor (const Geometry& gm);
    uint64_t layerKey (const twinui::Palette& p, bool internals, const Geometry& gm) const;
    juce::Image layer (bool internals, const Geometry& gm);

    struct Job { uint64_t key = 0; twinui::Palette palette; bool internals = false; Geometry gm; int ss = 1, band = 0, bands = 0; juce::Image big; };
    struct Shown { uint64_t key = 0; juce::Image image; };
    TwinLook& look;
    Geometry painted;            // as last painted: the size background work renders for
    uint64_t shownGroup = 0;     // that size, registered with the shared cache
    Shown shown[2];              // the layers on screen (kept even if the cache drops them for another window)
    Job job;
};

/** Faceted mode lamp with the mode's name printed next to it; the lamp colour follows the mode. */
class JewelLamp final : public juce::Component
{
public:
    explicit JewelLamp (TwinLook& l) : look (l) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    static constexpr float pad = 7.0f;   // base units around layout::jewel() for the glow
    juce::String modeName;
private:
    TwinLook& look;
};

/** A screw on the unit's edge that opens or closes it: the four in the rack ears, and the main board's
    corner screws while it is open. It glints under the mouse and looks backed out while the unit is open. */
class ScrewSwitch final : public juce::Button
{
public:
    ScrewSwitch (TwinLook& l, bool onBoard, int index) : juce::Button ("screw"), look (l), board (onBoard), idx (index)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    bool isBoardScrew() const { return board; }
    int index() const { return idx; }
    bool open = false;
private:
    TwinLook& look;
    const bool board;
    const int idx;
};

/** Seven-segment LED read-out; the unit is printed on its filter window. */
class Readout final : public juce::Component
{
public:
    Readout (TwinLook& l, int numCells, const char* unitUtf8, float brightness, const char* captionText = "")
        : look (l), cells (numCells), unit (juce::String::fromUTF8 (unitUtf8)), caption (captionText), intensity (brightness)
    {
        setInterceptsMouseClicks (false, false);
    }
    bool hasCaption() const { return caption.isNotEmpty(); }
    void setValue (const juce::String& v, bool unitOn = true)
    {
        if (v != value || unitOn != unitLit) { value = v; unitLit = unitOn; repaint(); }
    }
    void paint (juce::Graphics&) override;
    static constexpr float pad = 4.0f;   // base units around the window (room for its bezel and shadow)
private:
    TwinLook& look;
    int cells;
    juce::String unit, caption, value;
    bool unitLit = true;
    float intensity;
};

/** VU meter with second-order needle ballistics (~300 ms, slight overshoot). Opaque.
    The face is rendered once into an image; only the needle is drawn every frame. */
class VuMeter final : public juce::Component
{
public:
    explicit VuMeter (TwinLook& l) : look (l) { setOpaque (true); setInterceptsMouseClicks (false, false); }
    ~VuMeter() override { look.cache->showGroup (shownGroup, 0); }
    void setMeterIndex (int m)          { if (meterIndex != m) { meterIndex = m; repaint(); } }
    void tick (float target, float dt);  // target deflection 0..1 (1 = +3 VU)
    void paint (juce::Graphics&) override;
    /** Idle-time work: renders the face or the glass for look p (at the current size and meter); false if both
        exist or there is no room for them without dropping something more useful. */
    bool prerender (const twinui::Palette& p);
private:
    uint64_t faceKey (const twinui::Palette& p, int pw, int ph, float k) const;
    static uint64_t groupFor (int pw, int ph, float k);
    juce::Image renderFace (const twinui::Palette& p, bool glass, int pw, int ph, float k);
    TwinLook& look;
    float pos = 0.708f, vel = 0.0f;
    int meterIndex = 0;
    int paintedW = 0, paintedH = 0;   // as last painted (device pixels), for background work
    float paintedK = 0.0f;
    uint64_t shownGroup = 0, shownKey = 0;   // the size registered with the shared cache; the face on screen
    juce::Image shownFace, shownGlass;       // kept even if the cache drops them for another window
    struct Job { uint64_t key = 0; twinui::Palette palette; bool glass = false; int ss = 1, band = 0, bands = 0; juce::Image big; } job;
};

//==============================================================================
class TwinFetEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit TwinFetEditor (TwinFetProcessor&);
    ~TwinFetEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // any mouse activity in the window postpones the idle-time rendering
    void mouseMove (const juce::MouseEvent&) override  { lastActivityMs = juce::Time::getMillisecondCounter(); }
    void mouseDown (const juce::MouseEvent&) override  { lastActivityMs = juce::Time::getMillisecondCounter(); }
    void mouseDrag (const juce::MouseEvent&) override  { lastActivityMs = juce::Time::getMillisecondCounter(); }
    void mouseUp (const juce::MouseEvent&) override    { lastActivityMs = juce::Time::getMillisecondCounter(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override { lastActivityMs = juce::Time::getMillisecondCounter(); }

private:
    void timerCallback() override;
    void prerenderStep();
    void refreshSelectors();
    void setDrawerOpen (bool open);
    void setShowValues (bool show);
    void setShowPopups (bool show);
    void updateSizeLimits();
    void updateVisibility();
    void setupKnob (KnobSlider&, bool big, const juce::String& tooltip, Readout& readout,
                    std::function<void (Readout&, double)> show);
    juce::Rectangle<int> scaled (const twinui::Rect& r, float pad = 0.0f) const;

    TwinFetProcessor& proc;
    TwinLook look;
    float s = 1.0f;

    PanelArt panelArt { look };
    JewelLamp jewelLamp { look };
    juce::OwnedArray<ScrewSwitch> screws;   // 0-3 rack ears, 4-7 main board corners (shown while open)
    KnobSlider inputKnob, outputKnob, attackKnob, releaseKnob, mixKnob, hpfKnob;
    Readout inputRead { look, 4, "dB", 0.85f }, outputRead { look, 4, "dB", 0.85f },
            attackRead { look, 3, "\xc2\xb5s", 0.85f }, releaseRead { look, 4, "ms", 0.85f },
            mixRead { look, 3, "%", 0.85f }, hpfRead { look, 3, "Hz", 0.85f },
            grRead { look, 4, "dB", 1.0f, "GAIN REDUCTION" }, outRead { look, 4, "VU", 1.0f, "OUTPUT LEVEL" };
    std::array<LitButton, 5> ratioButtons;
    std::array<LitButton, 3> modeButtons;
    std::array<LitButton, 2> revButtons;
    std::array<LitButton, 4> meterButtons;
    std::array<LitButton, 6> osButtons;
    LitButton linkButton, compOffButton, noiseButton, valuesButton, popupsButton;
    VuMeter vu { look };
    TwinTooltipWindow tooltipWindow { this, 650 };

    int ratioIndex = 0, modeIndex = 2, revIndex = 0, meterIndex = 0, osIndex = 0;
    int paintedRevision = -1, shownOsFactor = 0;
    float paintedWear = -1.0f;
    bool drawerOpen = false, showValues = true, showPopups = true;
    int frame = 0;
    juce::uint32 lastActivityMs = 0;

    // attachments last: destroyed first
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> inputAtt, outputAtt, attackAtt, releaseAtt, mixAtt, hpfAtt;
    std::unique_ptr<ButtonAttachment> linkAtt, compOffAtt, noiseAtt;
    std::unique_ptr<juce::ParameterAttachment> ratioAtt, modeAtt, revAtt, meterAtt, osAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwinFetEditor)
};
