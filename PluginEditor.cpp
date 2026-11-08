#include "PluginEditor.h"
#include "FontData.h"

#include <cmath>
#include <vector>

namespace
{
    juce::String utf8 (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    constexpr float minScale = 0.7f, maxScale = 2.0f, defaultScale = 0.7f;   // new windows always open compact

    /** User preferences shared by all instances (%APPDATA%\TwinFET, ~/Library/Application Support/TwinFET).
        Only the POP-UPS choice lives here; everything else is saved with the project. */
    juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "TwinFET";
        o.filenameSuffix = ".settings";
       #if JUCE_LINUX || JUCE_BSD
        o.folderName = ".config/TwinFET";
       #else
        o.folderName = "TwinFET";
       #endif
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }

    // pop-up colours (final values, already graded)
    const juce::Colour tipTitleColour { 0xfff3ecdc }, tipBodyColour { 0xffc8c1b2 }, bubbleTextColour { 0xfff6efdf };

    /** A clean UI face for sentences; the panel font is kept for titles and numbers. */
    juce::Font bodyFont (float px)
    {
       #if JUCE_WINDOWS
        return juce::Font (juce::FontOptions ("Segoe UI", px, juce::Font::plain));
       #elif JUCE_MAC
        return juce::Font (juce::FontOptions ("Helvetica Neue", px, juce::Font::plain));
       #else
        return juce::Font (juce::FontOptions (px));
       #endif
    }

    /** Supersampling for cached artwork: a small window is drawn at 2-3x its pixel density and averaged down. */
    int supersampleFor (float devicePixelsPerUnit, int pw, int ph)
    {
        int ss = juce::jlimit (1, 3, juce::roundToInt (2.2f / juce::jmax (0.1f, devicePixelsPerUnit)));
        while (ss > 1 && (double) pw * (double) ph * (double) (ss * ss) > 12.0e6)
            --ss;
        return ss;
    }

    /** Box-filters an image rendered at ss times the resolution down to its device pixels (premultiplied ARGB). */
    juce::Image downsample (const juce::Image& big, int ss)
    {
        const int pw = big.getWidth() / ss, ph = big.getHeight() / ss;
        juce::Image small (juce::Image::ARGB, pw, ph, false);
        const juce::Image::BitmapData src (big, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dst (small, juce::Image::BitmapData::writeOnly);
        const unsigned n = (unsigned) (ss * ss);
        for (int y = 0; y < ph; ++y)
            for (int x = 0; x < pw; ++x)
            {
                unsigned a = 0, r = 0, gr = 0, b = 0;
                for (int j = 0; j < ss; ++j)
                {
                    const juce::uint8* q = src.getPixelPointer (x * ss, y * ss + j);
                    for (int i = 0; i < ss; ++i, q += src.pixelStride)
                    {
                        const auto* c = reinterpret_cast<const juce::PixelARGB*> (q);
                        a += c->getAlpha(); r += c->getRed(); gr += c->getGreen(); b += c->getBlue();
                    }
                }
                reinterpret_cast<juce::PixelARGB*> (dst.getPixelPointer (x, y))
                    ->setARGB ((juce::uint8) (a / n), (juce::uint8) (r / n), (juce::uint8) (gr / n), (juce::uint8) (b / n));
            }
        return small;
    }

    /** Renders into pw x ph device pixels at ss times the resolution, then box-filters down. */
    template <typename DrawFn>
    juce::Image renderSupersampled (int pw, int ph, int ss, float compW, float compH, DrawFn&& draw)
    {
        juce::Image big (juce::Image::ARGB, pw * ss, ph * ss, true, juce::SoftwareImageType());
        {
            juce::Graphics bg (big);
            bg.addTransform (juce::AffineTransform::scale ((float) (pw * ss) / compW, (float) (ph * ss) / compH));
            draw (bg);
        }
        return downsample (big, ss);
    }

    /** twinui canvas adapter on top of juce::Graphics. Draws in base units (1000 wide). */
    class JuceCanvas
    {
    public:
        /** unitScale: pixels per canvas unit; by default the UI scale (base units), 1 draws in pixels. */
        JuceCanvas (juce::Graphics& gr, TwinLook& look, float unitScale = -1.0f)
            : g (gr), face (look.face), cache (look.cache->grain)
        {
            g.saveState();
            g.addTransform (juce::AffineTransform::scale (unitScale > 0.0f ? unitScale : look.scale));
        }

        ~JuceCanvas() { g.restoreState(); }

        void save()    { g.saveState(); }
        void restore() { g.restoreState(); }

        void clipRect (float x, float y, float w, float h)
        {
            juce::Path p;
            p.addRectangle (x, y, w, h);
            g.reduceClipRegion (p);
        }

        void setColour (uint32_t c) { g.setColour (juce::Colour (twinui::grade (c))); }

        void setLinear (float x1, float y1, float x2, float y2, const twinui::GradStop* st, int n)
        {
            g.setGradientFill (gradient (x1, y1, x2, y2, st, n, false));
        }

        void setRadial (float cx, float cy, float r, const twinui::GradStop* st, int n)
        {
            g.setGradientFill (gradient (cx, cy, cx + r, cy, st, n, true));
        }

        void fillRect (float x, float y, float w, float h)                         { g.fillRect (juce::Rectangle<float> (x, y, w, h)); }
        void fillRoundRect (float x, float y, float w, float h, float r)           { g.fillRoundedRectangle (x, y, w, h, r); }
        void strokeRoundRect (float x, float y, float w, float h, float r, float lw) { g.drawRoundedRectangle (x, y, w, h, r, lw); }
        void fillEllipse (float x, float y, float w, float h)                      { g.fillEllipse (x, y, w, h); }
        void strokeEllipse (float x, float y, float w, float h, float lw)          { g.drawEllipse (x, y, w, h, lw); }
        void line (float x1, float y1, float x2, float y2, float lw)               { g.drawLine (x1, y1, x2, y2, lw); }

        void newPath()                      { path.clear(); }
        void moveTo (float x, float y)      { path.startNewSubPath (x, y); }
        void lineTo (float x, float y)      { path.lineTo (x, y); }
        void arc (float cx, float cy, float r, float a0, float a1, bool newSub) { path.addCentredArc (cx, cy, r, r, 0.0f, a0, a1, newSub); }
        void closePath()                    { path.closeSubPath(); }
        void fillPath()                     { g.fillPath (path); }
        void strokePath (float lw)          { g.strokePath (path, juce::PathStrokeType (lw)); }

        /** Procedural texture at the device's real pixel density (so it stays crisp on HiDPI screens),
            one texture pixel per screen pixel, copied without filtering so it is never softened. */
        void grain (float x, float y, float w, float h, int kind, uint32_t seed, float strength)
        {
            const float context = g.getInternalContext().getPhysicalPixelScaleFactor();   // pixels per base unit
            const float device = (context > 0.05f ? context : 1.0f) / grainDivisor;      // texture pixels per base unit
            float k = device;
            constexpr float maxPixels = 9.0e6f;   // a full-size panel on a 4K screen still gets 1:1 texture
            if (w * h * k * k > maxPixels)
                k = std::sqrt (maxPixels / (w * h));
            const int pw = juce::jlimit (1, 6144, juce::roundToInt (w * k));
            const int ph = juce::jlimit (1, 6144, juce::roundToInt (h * k));
            if (! g.clipRegionIntersects (juce::Rectangle<float> (x, y, w, h).getSmallestIntegerContainer()))
                return;   // e.g. the other layer of the panel
            const juce::Image img = cache.get (kind, seed, pw, ph, k);
            const bool oneToOne = std::abs ((float) pw / (w * device) - 1.0f) < 0.03f;
            g.saveState();
            g.setImageResamplingQuality (oneToOne || grainDivisor > 1.0f ? juce::Graphics::lowResamplingQuality      // pixel copy
                                                                         : juce::Graphics::mediumResamplingQuality); // had to be scaled
            g.setColour (juce::Colours::black.withAlpha (juce::jlimit (0.0f, 1.0f, strength)));   // image opacity = strength
            g.drawImage (img, juce::Rectangle<float> (x, y, w, h));
            g.restoreState();
        }

        /** Textures are made at the context's pixel density divided by this: the opened unit's subtle textures
            use the final screen density even while that layer is drawn supersampled (9x fewer pixels to make). */
        float grainDivisor = 1.0f;

        void text (const char* t, float x, float y, float w, float h, float size, int align, float tracking)
        {
            const juce::FontOptions opts = face != nullptr ? juce::FontOptions (face).withHeight (size)
                                                           : juce::FontOptions (size, juce::Font::bold);
            g.setFont (juce::Font (opts.withKerningFactor (tracking)));
            const juce::Justification just = align < 0 ? juce::Justification::centredLeft
                                           : (align > 0 ? juce::Justification::centredRight : juce::Justification::centred);
            g.drawText (juce::String::fromUTF8 (t), juce::Rectangle<float> (x, y, w, h), just, false);
        }

    private:
        static juce::ColourGradient gradient (float x1, float y1, float x2, float y2,
                                              const twinui::GradStop* st, int n, bool radial)
        {
            juce::ColourGradient grad (juce::Colour (twinui::grade (st[0].argb)), x1, y1,
                                       juce::Colour (twinui::grade (st[n - 1].argb)), x2, y2, radial);
            for (int i = 1; i < n - 1; ++i)
                grad.addColour ((double) st[i].pos, juce::Colour (twinui::grade (st[i].argb)));
            return grad;
        }

        juce::Graphics& g;
        juce::Typeface::Ptr face;
        GrainCache& cache;
        juce::Path path;
    };
}

//==============================================================================
juce::Image GrainCache::get (int kind, uint32_t seed, int w, int h, float pixelsPerUnit)
{
    const uint64_t key = hashKey ({ (uint64_t) kind, (uint64_t) seed, (uint64_t) w, (uint64_t) h,
                                    (uint64_t) juce::roundToInt (pixelsPerUnit * 1000.0f) });
    if (auto cached = images.find (key); cached.isValid())
        return cached;

    std::vector<float> v;
    twinui::makeGrain (v, w, h, kind, seed, pixelsPerUnit);

    juce::Image img (juce::Image::ARGB, w, h, false);   // full strength; drawn with the wanted strength as opacity
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const float t = v[(size_t) y * (size_t) w + (size_t) x];
                const auto a = (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (255.0f * std::abs (t)));
                auto* px = reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (x, y));
                if (t > 0.0f) px->setARGB (a, a, a, a);   // premultiplied white
                else          px->setARGB (a, 0, 0, 0);   // black
            }
    }
    images.put (key, img, 0);
    return img;
}

std::shared_ptr<UiCache> UiCache::shared()
{
    static std::weak_ptr<UiCache> instance;   // alive while any TwinFET (window or processor) holds it
    auto p = instance.lock();
    if (p == nullptr)
    {
        p = std::make_shared<UiCache>();
        instance = p;
    }
    return p;
}

//==============================================================================
TwinLook::TwinLook()
{
    face = juce::Typeface::createSystemTypefaceFor (twinfont::bigShouldersBold, twinfont::bigShouldersBoldSize);

    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0141517));
    setColour (juce::TooltipWindow::textColourId, bubbleTextColour);   // the knob value pop-up reads its text colour here
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff6c727a));
}

juce::Font TwinLook::panelFont (float heightPx, float kerning) const
{
    const juce::FontOptions o = face != nullptr ? juce::FontOptions (face).withHeight (heightPx)
                                                : juce::FontOptions (heightPx, juce::Font::bold);
    return juce::Font (o.withKerningFactor (kerning));
}

float TwinLook::popupScale() const
{
    return juce::jlimit (0.9f, 1.5f, 0.45f + 0.65f * scale);
}

//==============================================================================
// Tool-tips are written "TITLE\nexplanation": the title is set like the panel's silkscreen,
// the explanation in a plain UI face, both on a smoked-glass plate in a thin polished frame.
TwinLook::TipLayout TwinLook::layoutTip (const juce::String& tip) const
{
    const float f = popupScale();
    TipLayout t;
    const juce::String title = tip.upToFirstOccurrenceOf ("\n", false, false).trim().toUpperCase();
    const juce::String body  = tip.fromFirstOccurrenceOf ("\n", false, false).trim();

    juce::AttributedString a;
    a.append (title, panelFont (15.5f * f, 0.12f), tipTitleColour);
    a.setWordWrap (juce::AttributedString::none);
    t.title.createLayout (a, 1.0e5f);
    t.width = t.title.getWidth();
    t.height = t.title.getHeight();

    if (body.isNotEmpty())
    {
        juce::AttributedString b;
        b.append (body, bodyFont (13.0f * f), tipBodyColour);
        b.setWordWrap (juce::AttributedString::byWord);
        b.setLineSpacing (1.5f * f);
        // balanced lines: the narrowest width that still needs no more lines than the widest allowed
        const float maxW = 290.0f * f;
        t.body.createLayout (b, maxW);
        if (const int lines = t.body.getNumLines(); lines > 1)
        {
            float lo = 0.5f * maxW, hi = maxW;
            for (int i = 0; i < 10; ++i)
            {
                const float mid = 0.5f * (lo + hi);
                juce::TextLayout probe;
                probe.createLayout (b, mid);
                if (probe.getNumLines() > lines) lo = mid;
                else                             hi = mid;
            }
            t.body.createLayout (b, hi + 1.0f);
        }
        t.gap = 7.0f * f;
        t.width = juce::jmax (t.width, t.body.getWidth());
        t.height += t.gap + t.body.getHeight();
    }
    return t;
}

juce::Rectangle<int> TwinLook::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                 juce::Rectangle<int> parentArea)
{
    const float f = popupScale();
    const TipLayout t = layoutTip (tipText);
    const float margin = 6.0f * f, padX = 11.0f * f, padY = 8.0f * f;   // margin: room for the shadow
    const int w = (int) std::ceil (t.width + 2.0f * (padX + margin));
    const int h = (int) std::ceil (t.height + 2.0f * (padY + margin));
    const int x = screenPos.x > parentArea.getCentreX() ? screenPos.x - w + (int) margin - 10 : screenPos.x + 16 - (int) margin;
    const int y = screenPos.y > parentArea.getCentreY() ? screenPos.y - h + (int) margin - 8  : screenPos.y + 18 - (int) margin;
    return juce::Rectangle<int> (x, y, w, h).constrainedWithin (parentArea);
}

void TwinLook::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const float f = popupScale();
    const float margin = 6.0f * f, padX = 11.0f * f, padY = 8.0f * f;
    const twinui::Rect plate { margin, margin, (float) width - 2.0f * margin, (float) height - 2.0f * margin };
    {
        JuceCanvas cv (g, *this, 1.0f);   // in pixels
        twinui::popupPlate (cv, plate, 3.5f * f, 1.3f * f, 0.75f * margin);
    }
    const TipLayout t = layoutTip (text);
    const float x = plate.x + padX, y = plate.y + padY;
    t.title.draw (g, { x, y, t.title.getWidth(), t.title.getHeight() });
    if (t.body.getNumLines() > 0)
    {
        const float ly = y + t.title.getHeight() + 0.5f * t.gap;   // hairline between title and text
        g.setColour (juce::Colour (0x26ffffff));
        g.fillRect (x, ly - 0.5f, plate.w - 2.0f * padX, 1.0f);
        g.setColour (juce::Colour (0x66000000));
        g.fillRect (x, ly + 0.5f, plate.w - 2.0f * padX, 1.0f);
        t.body.draw (g, { x, y + t.title.getHeight() + t.gap, t.body.getWidth(), t.body.getHeight() });
    }
}

//==============================================================================
// The value shown while a knob turns: the same plate, a single line in the panel font.
void TwinLook::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&,
                           const juce::Rectangle<float>& body)
{
    const float f = popupScale();
    JuceCanvas cv (g, *this, 1.0f);
    twinui::popupPlate (cv, { body.getX(), body.getY(), body.getWidth(), body.getHeight() }, 3.5f * f, 1.3f * f, 7.0f * f);
}

juce::Font TwinLook::getSliderPopupFont (juce::Slider&)
{
    return panelFont (17.0f * popupScale(), 0.08f);
}

int TwinLook::getSliderPopupPlacement (juce::Slider&)
{
    return juce::BubbleComponent::above;   // over the knob's legend, never over its read-out or the buttons below
}

void TwinLook::setComponentEffectForBubbleComponent (juce::BubbleComponent& b)
{
    b.setComponentEffect (nullptr);   // the plate draws its own soft shadow
}

void TwinLook::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                 float startAngle, float endAngle, juce::Slider& slider)
{
    JuceCanvas cv (g, *this);
    const float cx = ((float) x + 0.5f * (float) width) / scale;
    const float cy = ((float) y + 0.5f * (float) height) / scale;
    const float knobW = (float) juce::jmin (width, height) / scale - 2.0f * twinui::knobPad;
    const bool big = (bool) slider.getProperties().getWithDefault ("big", false);
    const auto seed = (uint32_t) (int) slider.getProperties().getWithDefault ("seed", 1);
    twinui::knob (cv, cx, cy, twinui::knobRadius (knobW), startAngle + sliderPos * (endAngle - startAngle), big, palette.wear, seed);
}

void TwinLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                     bool highlighted, bool down)
{
    JuceCanvas cv (g, *this);
    const float bw = (float) b.getWidth() / scale, bh = (float) b.getHeight() / scale;
    const float pad = twinui::buttonPad;
    const twinui::Rect r { pad, pad, bw - 2.0f * pad, bh - 2.0f * pad };
    const juce::String label = b.getButtonText();
    const juce::String sub = b.getProperties().getWithDefault ("sub", juce::String()).toString();
    const int style = (int) b.getProperties().getWithDefault ("style", 0);   // 0 lit lamp, 1 white cap, 2 black cap
    if (style == 0)
        twinui::litButton (cv, r, b.getToggleState(), highlighted && b.isEnabled(), down && b.isEnabled(),
                           palette, label.toRawUTF8(), sub.toRawUTF8());
    else
        twinui::capButton (cv, r, b.getToggleState(), highlighted && b.isEnabled(), down && b.isEnabled(),
                           palette, label.toRawUTF8(), sub.toRawUTF8(), style == 1);
}

//==============================================================================
bool KnobSlider::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = 0.5f * (float) juce::jmin (getWidth(), getHeight());
    return c.getDistanceFrom (juce::Point<float> ((float) x, (float) y)) <= 0.82f * r;
}

bool LitButton::hitTest (int x, int y)
{
    return getLocalBounds().reduced (juce::roundToInt (pad)).contains (x, y);
}

PanelArt::Geometry PanelArt::geometryFor (float k) const
{
    Geometry gm;
    gm.k = k;
    gm.pw = juce::jmax (1, juce::roundToInt ((float) getWidth() * k));
    gm.ph = juce::jmax (1, juce::roundToInt ((float) getHeight() * k));
    gm.faceRows = juce::jlimit (1, gm.ph, (int) std::ceil (twinui::kMainH * look.scale * k - 0.01f));
    return gm;
}

uint64_t PanelArt::groupFor (const Geometry& gm)
{
    return hashKey ({ 0x9a11e1u, (uint64_t) gm.pw, (uint64_t) gm.ph, (uint64_t) gm.faceRows,
                      (uint64_t) juce::roundToInt (gm.k * 1000.0f) });
}

uint64_t PanelArt::layerKey (const twinui::Palette& p, bool internals, const Geometry& gm) const
{
    return hashKey ({ (uint64_t) internals, (uint64_t) gm.pw, (uint64_t) gm.ph, (uint64_t) gm.faceRows,
                      (uint64_t) juce::roundToInt (gm.k * 1000.0f), p.panelA, p.panelB, p.ink, p.inkDim,
                      (uint64_t) p.lightPanel, (uint64_t) juce::roundToInt (p.wear * 1000.0f),
                      (uint64_t) juce::roundToInt (p.grainStrength * 1000.0f) });
}

void PanelArt::paint (juce::Graphics& g)
{
    const auto gm = geometryFor (juce::jmax (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor()));
    painted = gm;
    look.cache->showGroup (shownGroup, groupFor (gm));
    // room for two complete looks (faceplate + opened unit) even in a very large window, so switching back
    // and forth between two looks never renders again; normal sizes stay within the default budget
    look.cache->panelLayers.ensureBudget (2u * (size_t) gm.pw * (size_t) gm.ph * 4u + (8u << 20));
    const float fh = (float) gm.faceRows / gm.k;

    // the layers have exactly the device's pixels: copy them 1:1, never resample (no softening)
    g.fillAll (juce::Colour (0xff09090a));
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.setColour (juce::Colours::black);
    if (g.clipRegionIntersects ({ 0, 0, getWidth(), (int) std::ceil (fh) }))
        g.drawImage (layer (false, gm), { 0.0f, 0.0f, (float) gm.pw / gm.k, fh });
    if (gm.ph > gm.faceRows && g.clipRegionIntersects ({ 0, (int) fh, getWidth(), getHeight() - (int) fh }))
        g.drawImage (layer (true, gm), { 0.0f, fh, (float) gm.pw / gm.k, (float) (gm.ph - gm.faceRows) / gm.k });   // the open unit
}

juce::Image PanelArt::layer (bool internals, const Geometry& gm)
{
    const auto& p = look.palette;
    const uint64_t key = layerKey (p, internals, gm);
    auto& onScreen = shown[internals ? 1 : 0];
    if (auto cached = look.cache->panelLayers.find (key); cached.isValid())   // (also marks it recently used)
    {
        onScreen = { key, cached };
        return cached;
    }
    if (onScreen.key == key && onScreen.image.isValid())
        return onScreen.image;   // dropped from the cache for another window's art, but still ours: no re-render

    const int row0 = internals ? gm.faceRows : 0, rows = internals ? gm.ph - gm.faceRows : gm.faceRows;
    const int ss = supersampleFor (gm.k * look.scale, gm.pw, rows);
    auto img = renderSupersampled (gm.pw, rows, ss, (float) gm.pw / gm.k, (float) rows / gm.k, [&] (juce::Graphics& cg)
    {
        cg.fillAll (juce::Colour (0xff09090a));
        cg.addTransform (juce::AffineTransform::translation (0.0f, -(float) row0 / gm.k));
        JuceCanvas cv (cg, look);
        if (internals) { cv.grainDivisor = (float) ss; twinui::internalsArt (cv, p); }
        else           twinui::faceArt (cv, p);
    });
    look.cache->panelLayers.put (key, img, groupFor (gm));
    onScreen = { key, img };
    if (job.key == key)
        job = {};   // rendered in one go after all
    return img;
}

bool PanelArt::prerender (const twinui::Palette& p, bool internals)
{
    const Geometry gm = painted;
    if (gm.pw <= 0 || (internals && gm.ph <= gm.faceRows) || gm.k * look.scale > kIdleWorkMaxPixelsPerUnit)
        return false;
    const int row0 = internals ? gm.faceRows : 0, rows = internals ? gm.ph - gm.faceRows : gm.faceRows;
    const uint64_t key = layerKey (p, internals, gm);
    auto& cache = look.cache->panelLayers;
    if (shown[internals ? 1 : 0].key == key || cache.find (key).isValid())
        return false;

    // Idle work only takes free room, or room held by sizes no open window shows: it never pushes out art a
    // window may switch back to. So memory stays bounded, the most likely looks are kept when not all of them
    // fit, and several open windows cannot keep re-rendering each other's art.
    const size_t bytes = (size_t) gm.pw * (size_t) rows * 4u;
    const auto keep = [this] (uint64_t group) { return look.cache->isShown (group); };
    if (job.key != key)
    {
        if (! cache.makeRoom (bytes, keep))
            return false;
        job = {};
        job.key = key;
        job.palette = p;
        job.internals = internals;
        job.gm = gm;
        job.ss = supersampleFor (gm.k * look.scale, gm.pw, rows);
        job.big = juce::Image (juce::Image::ARGB, gm.pw * job.ss, rows * job.ss, true, juce::SoftwareImageType());
        job.bands = juce::jmax (1, (job.big.getWidth() * job.big.getHeight()) / 110000);
    }

    // one thin horizontal band per step: exactly the pixels a single render would give, in small slices
    const int bandH = (job.big.getHeight() + job.bands - 1) / job.bands;
    const int y0 = job.band * bandH, y1 = juce::jmin (job.big.getHeight(), y0 + bandH);
    {
        juce::Graphics bg (job.big);
        bg.reduceClipRegion (0, y0, job.big.getWidth(), y1 - y0);
        bg.fillAll (juce::Colour (0xff09090a));
        bg.addTransform (juce::AffineTransform::scale ((float) job.ss * gm.k));
        bg.addTransform (juce::AffineTransform::translation (0.0f, -(float) row0 / gm.k));
        JuceCanvas cv (bg, look);
        if (job.internals) { cv.grainDivisor = (float) job.ss; twinui::internalsArt (cv, job.palette); }
        else               twinui::faceArt (cv, job.palette);
    }
    if (++job.band >= job.bands)
    {
        if (cache.makeRoom (bytes, keep))   // still room? (something may have been rendered meanwhile)
            cache.put (job.key, downsample (job.big, job.ss), groupFor (job.gm));
        job = {};
    }
    return true;
}

void JewelLamp::paint (juce::Graphics& g)
{
    JuceCanvas cv (g, look);
    const auto j = twinui::layout::jewel(), n = twinui::layout::modeName();
    const float ox = j.x - pad, oy = j.y - pad;   // this component's origin in base units
    twinui::jewel (cv, j.cx() - ox, j.cy() - oy, 0.36f * j.w, look.palette.jewel);
    twinui::ink (cv, look.palette, modeName.toRawUTF8(), { n.x - ox, n.y - oy, n.w, n.h }, 18.0f, -1, 0.18f);
}

void ScrewSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    JuceCanvas cv (g, look);
    const float w = (float) getWidth() / look.scale, h = (float) getHeight() / look.scale;
    const float glint = down ? 0.45f : (highlighted ? 1.0f : 0.0f);
    if (board)
        twinui::hexScrew (cv, 0.5f * w, 0.5f * h, 3.6f, 0.4f * (float) idx, glint);
    else
        twinui::screw (cv, 0.5f * w, h * twinui::layout::earScrewOffset / twinui::layout::earScrew (0).h, 5.5f,
                       open ? 0.75f + 0.3f * (float) idx : 0.0f, open ? 1.0f : 0.0f, glint);
}

void Readout::paint (juce::Graphics& g)
{
    JuceCanvas cv (g, look);
    const float w = (float) getWidth() / look.scale, h = (float) getHeight() / look.scale;
    const float capH = hasCaption() ? twinui::layout::readoutCaptionH : 0.0f;
    const twinui::Rect r { pad, pad, w - 2.0f * pad, h - 2.0f * pad - capH };
    twinui::ledReadout (cv, r, value.toRawUTF8(), unit.toRawUTF8(), look.palette, cells, unitLit, intensity);
    if (hasCaption())
        twinui::readoutCaption (cv, look.palette, { r.x, r.bottom() + 3.0f, r.w, 12.0f }, caption.toRawUTF8());
}

void VuMeter::tick (float target, float dt)
{
    // VU movement: ~300 ms to 99 %, ~1 % overshoot
    constexpr float omega = 19.0f, zeta = 0.80f;
    target = juce::jlimit (0.0f, 1.12f, target);
    const float accel = omega * omega * (target - pos) - 2.0f * zeta * omega * vel;
    vel += accel * dt;
    pos += vel * dt;
    if (pos < -0.03f) { pos = -0.03f; vel = 0.0f; }
    if (pos > 1.15f)  { pos = 1.15f;  vel = 0.0f; }
    repaint();
}

uint64_t VuMeter::faceKey (const twinui::Palette& p, int pw, int ph, float k) const
{
    return hashKey ({ (uint64_t) meterIndex, p.meterLamp, (uint64_t) p.lightPanel, (uint64_t) p.centerLamp,
                      (uint64_t) juce::roundToInt (p.wear * 1000.0f), (uint64_t) pw, (uint64_t) ph,
                      (uint64_t) juce::roundToInt (k * 1000.0f) });
}

uint64_t VuMeter::groupFor (int pw, int ph, float k)
{
    return hashKey ({ 0x7e57abu, (uint64_t) pw, (uint64_t) ph, (uint64_t) juce::roundToInt (k * 1000.0f) });
}

juce::Image VuMeter::renderFace (const twinui::Palette& p, bool glass, int pw, int ph, float k)
{
    const twinui::Rect r { 0.0f, 0.0f, (float) getWidth() / look.scale, (float) getHeight() / look.scale };
    const int ss = supersampleFor (k * look.scale, pw, ph);
    return renderSupersampled (pw, ph, ss, (float) getWidth(), (float) getHeight(), [&] (juce::Graphics& fg)
    {
        JuceCanvas fc (fg, look);
        if (glass) twinui::vuGlass (fc, r, p);
        else       twinui::vuStatic (fc, r, p, meterIndex);
    });
}

void VuMeter::paint (juce::Graphics& g)
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const twinui::Rect r { 0.0f, 0.0f, (float) getWidth() / look.scale, (float) getHeight() / look.scale };

    // face and glass: rendered at the screen's physical resolution, once per look and size
    const float k = juce::jmax (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int pw = juce::jmax (1, juce::roundToInt ((float) getWidth() * k));
    const int ph = juce::jmax (1, juce::roundToInt ((float) getHeight() * k));
    paintedW = pw; paintedH = ph; paintedK = k;
    const uint64_t group = groupFor (pw, ph, k);
    look.cache->showGroup (shownGroup, group);
    const uint64_t key = faceKey (look.palette, pw, ph, k);
    auto& cache = look.cache->vuLayers;
    juce::Image face = cache.find (key), glassLayer = cache.find (key ^ 0x5bd1e995u);
    if (shownKey == key)   // dropped from the cache for another window's art, but still ours: no re-render
    {
        if (face.isNull())       face = shownFace;
        if (glassLayer.isNull()) glassLayer = shownGlass;
    }
    if (face.isNull())       { face = renderFace (look.palette, false, pw, ph, k);      cache.put (key, face, group); }
    if (glassLayer.isNull()) { glassLayer = renderFace (look.palette, true, pw, ph, k); cache.put (key ^ 0x5bd1e995u, glassLayer, group); }
    shownKey = key; shownFace = face; shownGlass = glassLayer;
    if (job.key == key || job.key == (key ^ 0x5bd1e995u))
        job = {};   // rendered in one go after all

    // A child component usually starts between two device pixels (38 px at 125 % = 47.5): drawn with
    // filtering, the cached face would be smeared over two pixels. A straight pixel copy keeps it sharp.
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.setColour (juce::Colours::black);
    g.drawImage (face, getLocalBounds().toFloat());
    {
        JuceCanvas cv (g, look);
        twinui::vuDynamic (cv, r, pos);
    }
    g.setColour (juce::Colours::black);
    g.drawImage (glassLayer, getLocalBounds().toFloat());
}

bool VuMeter::prerender (const twinui::Palette& p)
{
    if (paintedW <= 0 || paintedK * look.scale > kIdleWorkMaxPixelsPerUnit)
        return false;
    auto& cache = look.cache->vuLayers;
    const uint64_t faceId = faceKey (p, paintedW, paintedH, paintedK), glassId = faceId ^ 0x5bd1e995u;
    if (faceId == shownKey)
        return false;   // on screen now
    const bool glass = cache.find (faceId).isValid();
    if (glass && cache.find (glassId).isValid())
        return false;
    const uint64_t key = glass ? glassId : faceId;

    // only free room, or room held by sizes no open window shows (see PanelArt::prerender)
    const size_t bytes = (size_t) paintedW * (size_t) paintedH * 4u;
    const auto keep = [this] (uint64_t group) { return look.cache->isShown (group); };
    if (job.key != key)
    {
        if (! cache.makeRoom (bytes, keep))
            return false;
        job = {};
        job.key = key;
        job.palette = p;
        job.glass = glass;
        job.ss = supersampleFor (paintedK * look.scale, paintedW, paintedH);
        job.big = juce::Image (juce::Image::ARGB, paintedW * job.ss, paintedH * job.ss, true, juce::SoftwareImageType());
        job.bands = juce::jmax (1, (job.big.getWidth() * job.big.getHeight()) / 110000);
    }

    // one thin band per step, the same pixels a single render gives
    const int bandH = (job.big.getHeight() + job.bands - 1) / job.bands;
    const int y0 = job.band * bandH, y1 = juce::jmin (job.big.getHeight(), y0 + bandH);
    {
        const twinui::Rect r { 0.0f, 0.0f, (float) getWidth() / look.scale, (float) getHeight() / look.scale };
        juce::Graphics bg (job.big);
        bg.reduceClipRegion (0, y0, job.big.getWidth(), y1 - y0);
        bg.addTransform (juce::AffineTransform::scale ((float) job.big.getWidth() / (float) getWidth(),
                                                       (float) job.big.getHeight() / (float) getHeight()));
        JuceCanvas fc (bg, look);
        if (job.glass) twinui::vuGlass (fc, r, job.palette);
        else           twinui::vuStatic (fc, r, job.palette, meterIndex);
    }
    if (++job.band >= job.bands)
    {
        if (cache.makeRoom (bytes, keep))
            cache.put (job.key, downsample (job.big, job.ss), groupFor (paintedW, paintedH, paintedK));
        job = {};
    }
    return true;
}

//==============================================================================
TwinFetEditor::TwinFetEditor (TwinFetProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (panelArt);
    addAndMakeVisible (jewelLamp);
    drawerOpen = (bool) proc.apvts.state.getProperty (uiprop::advancedOpen, false);
    showValues = (bool) proc.apvts.state.getProperty (uiprop::showValues, true);
    if (proc.apvts.state.hasProperty (uiprop::popups))
        showPopups = (bool) proc.apvts.state.getProperty (uiprop::popups);
    else
        showPopups = juce::PropertiesFile (settingsOptions()).getBoolValue ("popups", true);
    tooltipWindow.enabled = showPopups;

    for (auto* r : { &inputRead, &outputRead, &attackRead, &releaseRead, &mixRead, &hpfRead, &grRead, &outRead })
        addAndMakeVisible (*r);
    grRead.setValue ("0.0");
    outRead.setValue ("---");

    // what each knob shows on its seven-segment read-out (same mappings as the parameter texts)
    auto showDb      = [] (Readout& r, double v) { r.setValue (juce::String (v, 1)); };
    auto showAttack  = [] (Readout& r, double v) { r.setValue (juce::String (juce::roundToInt (800.0 * std::pow (20.0 / 800.0, (v - 1.0) / 6.0)))); };
    auto showRelease = [] (Readout& r, double v) { r.setValue (juce::String (juce::roundToInt (1100.0 * std::pow (50.0 / 1100.0, (v - 1.0) / 6.0)))); };
    auto showPercent = [] (Readout& r, double v) { r.setValue (juce::String (juce::roundToInt (v))); };
    auto showHpf     = [] (Readout& r, double v)
    {
        if (v <= 20.5) r.setValue ("oFF", false);
        else           r.setValue (juce::String (juce::roundToInt (v)));
    };

    // tool-tips: "TITLE\nexplanation"
    setupKnob (inputKnob,   true,  utf8 ("Input\nHow hard the signal is driven into the fixed threshold. More input means more gain "
                                       "reduction and, in ANALOG, more colour. Double-click to reset."), inputRead, showDb);
    setupKnob (outputKnob,  true,  utf8 ("Output\nMake-up gain. In ANALOG it also drives the output amplifier, so higher settings "
                                       "run it hotter. Double-click to reset."), outputRead, showDb);
    setupKnob (attackKnob,  false, utf8 ("Attack\n1 to 7, where 7 is the fastest (20 \xc2\xb5s) and 1 the slowest (800 \xc2\xb5s). "
                                       "The read-out shows the time in microseconds."), attackRead, showAttack);
    setupKnob (releaseKnob, false, utf8 ("Release\n1 to 7, where 7 is the fastest (50 ms) and 1 the slowest (1.1 s). After long, "
                                       "heavy compression it recovers more slowly, as the hardware does. The read-out shows milliseconds."),
               releaseRead, showRelease);
    setupKnob (mixKnob,     false, utf8 ("Mix\nParallel compression: blends the dry and the compressed signal. Both are time-aligned, "
                                       "so nothing phases."), mixRead, showPercent);
    setupKnob (hpfKnob,     false, utf8 ("Sidechain HPF\nHigh-pass filter in the detector only, so kick and bass pump the compressor "
                                       "less. The audio itself is not filtered. Fully left is off."), hpfRead, showHpf);

    inputAtt   = std::make_unique<SliderAttachment> (proc.apvts, pid::input,   inputKnob);
    outputAtt  = std::make_unique<SliderAttachment> (proc.apvts, pid::output,  outputKnob);
    attackAtt  = std::make_unique<SliderAttachment> (proc.apvts, pid::attack,  attackKnob);
    releaseAtt = std::make_unique<SliderAttachment> (proc.apvts, pid::release, releaseKnob);
    mixAtt     = std::make_unique<SliderAttachment> (proc.apvts, pid::mix,     mixKnob);
    hpfAtt     = std::make_unique<SliderAttachment> (proc.apvts, pid::scHpf,   hpfKnob);

    auto setDefault = [this] (juce::Slider& sl, const char* id)
    {
        if (auto* prm = proc.apvts.getParameter (id))
            sl.setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
    };
    setDefault (inputKnob, pid::input);   setDefault (outputKnob, pid::output);
    setDefault (attackKnob, pid::attack); setDefault (releaseKnob, pid::release);
    setDefault (mixKnob, pid::mix);       setDefault (hpfKnob, pid::scHpf);

    // initial read-outs
    for (auto* k : { &inputKnob, &outputKnob, &attackKnob, &releaseKnob, &mixKnob, &hpfKnob })
        if (k->onValueChange)
            k->onValueChange();

    // ratio
    const char* ratioNames[] = { "4:1", "8:1", "12:1", "20:1", "ALL" };
    const char* ratioTips[]  = {
        "Ratio 4:1\nThe gentlest setting, with the lowest threshold and the softest knee. The usual choice for vocals and bass.",
        "Ratio 8:1\nFirmer control. Higher ratios also raise the threshold and harden the knee, as on the hardware.",
        "Ratio 12:1\nHeavy compression on the edge of limiting.",
        "Ratio 20:1\nLimiting: the highest threshold and the hardest knee, for catching peaks.",
        "All buttons\nAll ratio buttons pressed at once: about 16:1 and beyond, shifted operating points, attack lag on "
        "transients, over-compression and plenty of grit. Made for room mics and parallel drums."
    };
    for (int i = 0; i < 5; ++i)
    {
        auto& b = ratioButtons[(size_t) i];
        b.setButtonText (ratioNames[i]);
        b.getProperties().set ("style", 1);   // white caps
        b.setTooltip (utf8 (ratioTips[i]));
        b.onClick = [this, i] { if (ratioAtt != nullptr) ratioAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    // mode
    const char* modeNames[] = { "FAT", "CLEAN", "ANALOG" };
    const char* modeSubs[]  = { "INFLATE", "PRECISE", "CIRCUIT" };
    const char* modeTips[]  = {
        "Fat mode\nLinear compression plus an inflation stage: more density, weight and harmonics. The harder you push it, "
        "the thicker it gets.",
        "Clean mode\nLinear, precise compression. BLACK adds no colour at all; BLUE adds subtle, high-grade saturation "
        "(transformers and a class-A stage).",
        "Analog mode\nA non-linear model of the circuit: feedback detector, detector ripple on low notes, FET distortion, "
        "amplifiers and transformers. Gain staging matters, as on the real unit."
    };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (modeNames[i]);
        b.getProperties().set ("sub", modeSubs[i]);
        b.setTooltip (utf8 (modeTips[i]));
        b.onClick = [this, i] { if (modeAtt != nullptr) modeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    // revision
    const char* revNames[] = { "BLACK", "BLUE" };
    const char* revTips[]  = { "Black revision\nThe later, low-noise unit: cleaner, quieter and tighter.",
                               "Blue revision\nThe early unit with FET amplifiers: more harmonics and saturation, a juicier "
                               "sound and a slightly slower attack." };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = revButtons[(size_t) i];
        b.setButtonText (revNames[i]);
        b.setTooltip (utf8 (revTips[i]));
        b.onClick = [this, i] { if (revAtt != nullptr) revAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    // meter
    const char* meterNames[] = { "GR", "+4", "+8", "INPUT" };
    const char* meterTips[]  = { "Meter: GR\nThe needle shows gain reduction.",
                                 "Meter: +4\nOutput level, 0 VU = \xe2\x88\x92" "18 dBFS.",
                                 "Meter: +8\nOutput level with 4 dB more headroom, 0 VU = \xe2\x88\x92" "14 dBFS.",
                                 "Meter: Input\nInput level, 0 VU = \xe2\x88\x92" "18 dBFS." };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = meterButtons[(size_t) i];
        b.setButtonText (meterNames[i]);
        b.getProperties().set ("style", 2);   // black caps
        b.setTooltip (utf8 (meterTips[i]));
        b.onClick = [this, i] { if (meterAtt != nullptr) meterAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    // switches
    linkButton.setButtonText ("LINK");
    compOffButton.setButtonText ("COMP OFF");
    noiseButton.setButtonText ("NOISE");
    linkButton.setTooltip (utf8 ("Stereo link\nBoth channels share one detector and get the same gain reduction, so the "
                               "stereo image stays put."));
    compOffButton.setTooltip (utf8 ("Comp off\nNo gain reduction: the signal only passes through the circuit for its colour, "
                                  "like the attack-off trick on the hardware."));
    noiseButton.setTooltip (utf8 ("Noise\nAdds the hardware's hiss and mains hum (ANALOG mode only)."));
    for (auto* b : { &linkButton, &compOffButton, &noiseButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    linkAtt    = std::make_unique<ButtonAttachment> (proc.apvts, pid::link,    linkButton);
    compOffAtt = std::make_unique<ButtonAttachment> (proc.apvts, pid::compOff, compOffButton);
    noiseAtt   = std::make_unique<ButtonAttachment> (proc.apvts, pid::noise,   noiseButton);

    // oversampling
    const char* osNames[] = { "AUTO", "1\xc3\x97", "2\xc3\x97", "4\xc3\x97", "8\xc3\x97", "16\xc3\x97" };
    const char* osTips[]  = {
        "Oversampling: Auto\nFollows the project's sample rate: 4\xc3\x97 at 44.1/48 kHz, 2\xc3\x97 at 88.2/96 kHz, none at "
        "176.4/192 kHz, so the circuit always runs at about 176-192 kHz inside. The small print under the button shows "
        "the factor in use.",
        "Oversampling: off\nLowest latency and CPU, but saturation and inflation alias (around \xe2\x88\x92" "30 dB on hot highs).",
        "Oversampling: 2\xc3\x97\nAliasing around \xe2\x88\x92" "93 dB, 3 samples of latency.",
        "Oversampling: 4\xc3\x97\nAliasing below \xe2\x88\x92" "100 dB, 4 samples of latency. Recommended.",
        "Oversampling: 8\xc3\x97\nHardly cleaner than 4\xc3\x97, twice the CPU.",
        "Oversampling: 16\xc3\x97\nFor perfectionists: four times the CPU of 4\xc3\x97 and sounds the same."
    };
    for (int i = 0; i < 6; ++i)
    {
        auto& b = osButtons[(size_t) i];
        b.setButtonText (juce::String::fromUTF8 (osNames[i]));
        b.getProperties().set ("style", 2);
        b.setTooltip (utf8 (osTips[i]));
        b.onClick = [this, i] { if (osAtt != nullptr) osAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    // displays on/off
    valuesButton.setButtonText ("VALUES");
    valuesButton.getProperties().set ("style", 1);
    valuesButton.setTooltip (utf8 ("Values\nShows or hides all the numeric read-outs, leaving just the knobs and the needle."));
    valuesButton.onClick = [this] { setShowValues (! showValues); };
    addAndMakeVisible (valuesButton);

    popupsButton.setButtonText ("POP-UPS");
    popupsButton.getProperties().set ("style", 1);
    popupsButton.setTooltip (utf8 ("Pop-ups\nTurns these tips and the value shown while turning a knob on or off. "
                                 "New windows remember the choice."));
    popupsButton.onClick = [this] { setShowPopups (! showPopups); };
    addAndMakeVisible (popupsButton);

    // the drawer pull
    // the screws on the unit's edge open and close it (the rack ears; the board's corners while open)
    for (int i = 0; i < 8; ++i)
    {
        auto* sw = screws.add (new ScrewSwitch (look, i >= 4, i % 4));
        sw->onClick = [this] { setDrawerOpen (! drawerOpen); };
        addAndMakeVisible (sw);
    }

    addAndMakeVisible (vu);

    ratioAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (pid::ratio),
                   [this] (float v) { ratioIndex = juce::roundToInt (v); refreshSelectors(); });
    modeAtt  = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (pid::mode),
                   [this] (float v) { modeIndex = juce::roundToInt (v); refreshSelectors(); });
    revAtt   = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (pid::revision),
                   [this] (float v) { revIndex = juce::roundToInt (v); refreshSelectors(); });
    meterAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (pid::meter),
                   [this] (float v) { meterIndex = juce::roundToInt (v); refreshSelectors(); });
    osAtt    = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (pid::oversampling),
                   [this] (float v) { osIndex = juce::roundToInt (v); refreshSelectors(); });
    ratioAtt->sendInitialUpdate();
    modeAtt->sendInitialUpdate();
    revAtt->sendInitialUpdate();
    meterAtt->sendInitialUpdate();
    osAtt->sendInitialUpdate();

    // a new instance always opens compact; one the user resized reopens at its own size (saved with the project)
    float startScale = (float) (double) proc.apvts.state.getProperty (uiprop::scale, (double) defaultScale);
    startScale = juce::jlimit (minScale, maxScale, startScale > 0.0f ? startScale : defaultScale);

    setResizable (true, true);
    updateSizeLimits();
    setSize (juce::roundToInt (twinui::kW * startScale),
             juce::roundToInt ((drawerOpen ? twinui::kFullH : twinui::kMainH) * startScale));
    updateVisibility();

    proc.uiArtwork = look.cache;   // the plugin keeps the rendered artwork while this window is closed
    addMouseListener (this, true);  // mouse activity anywhere in the window postpones idle rendering
    lastActivityMs = juce::Time::getMillisecondCounter();
    startTimerHz (60);
}

TwinFetEditor::~TwinFetEditor()
{
    stopTimer();
    // keep what is likely to be shown again (the current look and a few more) while the window is closed
    look.cache->panelLayers.trimTo (24u << 20);
    look.cache->vuLayers.trimTo (6u << 20);
    look.cache->grain.trimTo (24u << 20);
    setLookAndFeel (nullptr);
}

void TwinFetEditor::setupKnob (KnobSlider& k, bool big, const juce::String& tooltip, Readout& readout,
                              std::function<void (Readout&, double)> show)
{
    k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.setRotaryParameters (twinui::kStartAngle, twinui::kEndAngle, true);
    k.setMouseDragSensitivity (big ? 320 : 240);
    k.getProperties().set ("big", big);
    k.setTooltip (tooltip);
    k.setPopupDisplayEnabled (showPopups, false, this);
    k.getProperties().set ("seed", (int) (twinui::hashText (tooltip.toRawUTF8()) & 0x7fffffff));
    k.onValueChange = [&k, &readout, show = std::move (show)] { show (readout, k.getValue()); };
    addAndMakeVisible (k);
}

void TwinFetEditor::refreshSelectors()
{
    for (int i = 0; i < 5; ++i) ratioButtons[(size_t) i].setToggleState (i == ratioIndex, juce::dontSendNotification);
    for (int i = 0; i < 3; ++i) modeButtons[(size_t) i].setToggleState (i == modeIndex, juce::dontSendNotification);
    for (int i = 0; i < 2; ++i) revButtons[(size_t) i].setToggleState (i == revIndex, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i) meterButtons[(size_t) i].setToggleState (i == meterIndex, juce::dontSendNotification);
    for (int i = 0; i < 6; ++i) osButtons[(size_t) i].setToggleState (i == osIndex, juce::dontSendNotification);
    valuesButton.setToggleState (showValues, juce::dontSendNotification);
    valuesButton.getProperties().set ("sub", showValues ? "ON" : "OFF");
    popupsButton.setToggleState (showPopups, juce::dontSendNotification);
    popupsButton.getProperties().set ("sub", showPopups ? "ON" : "OFF");

    // revision = panel material; mode = lamp colours, meter lamp and (ANALOG) a well-used unit
    look.palette = twinui::paletteFor (revIndex, modeIndex);
    if (revIndex != paintedRevision || ! juce::approximatelyEqual (look.palette.wear, paintedWear))
    {
        paintedRevision = revIndex;
        paintedWear = look.palette.wear;
        panelArt.invalidate();   // the only expensive repaint
    }
    vu.setMeterIndex (meterIndex);
    vu.repaint();
    const char* modeNames[] = { "FAT", "CLEAN", "ANALOG" };
    jewelLamp.modeName = modeNames[juce::jlimit (0, 2, modeIndex)];
    jewelLamp.repaint();
    noiseButton.setEnabled (modeIndex == 2);
    noiseButton.setAlpha (modeIndex == 2 ? 1.0f : 0.45f);
    repaint();
}

void TwinFetEditor::updateSizeLimits()
{
    const float h = drawerOpen ? twinui::kFullH : twinui::kMainH;
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) twinui::kW / (double) h);
    setResizeLimits (juce::roundToInt (twinui::kW * minScale), juce::roundToInt (h * minScale),
                     juce::roundToInt (twinui::kW * maxScale), juce::roundToInt (h * maxScale));
}

void TwinFetEditor::setDrawerOpen (bool open)
{
    drawerOpen = open;
    proc.apvts.state.setProperty (uiprop::advancedOpen, open, nullptr);
    const float scaleNow = getWidth() > 0 ? (float) getWidth() / twinui::kW : 1.0f;
    updateSizeLimits();
    setSize (juce::roundToInt (twinui::kW * scaleNow),
             juce::roundToInt ((drawerOpen ? twinui::kFullH : twinui::kMainH) * scaleNow));
    updateVisibility();
}

void TwinFetEditor::setShowValues (bool show)
{
    showValues = show;
    proc.apvts.state.setProperty (uiprop::showValues, show, nullptr);
    updateVisibility();
    refreshSelectors();
}

void TwinFetEditor::setShowPopups (bool show)
{
    showPopups = show;
    proc.apvts.state.setProperty (uiprop::popups, show, nullptr);
    {
        juce::PropertiesFile settings (settingsOptions());   // new windows (and new instances) follow this choice
        settings.setValue ("popups", show);
        settings.saveIfNeeded();
    }
    tooltipWindow.enabled = show;
    if (! show)
        tooltipWindow.hideTip();
    for (auto* k : { &inputKnob, &outputKnob, &attackKnob, &releaseKnob, &mixKnob, &hpfKnob })
        k->setPopupDisplayEnabled (show, false, this);
    refreshSelectors();
}

void TwinFetEditor::updateVisibility()
{
    for (auto* sw : screws)
    {
        sw->open = drawerOpen;
        sw->setVisible (! sw->isBoardScrew() || drawerOpen);
        sw->setTooltip (drawerOpen ? utf8 ("Close the unit\nClick any edge screw to put the advanced settings away.")
                                   : utf8 ("Open the unit\nClick any edge screw to reach the advanced settings: mode, revision, "
                                           "oversampling, displays, mix, sidechain HPF and switches."));
        sw->repaint();
    }
    for (auto* r : { &inputRead, &outputRead, &attackRead, &releaseRead, &grRead, &outRead })
        r->setVisible (showValues);
    mixRead.setVisible (showValues && drawerOpen);
    hpfRead.setVisible (showValues && drawerOpen);
    for (auto& b : modeButtons) b.setVisible (drawerOpen);
    for (auto& b : revButtons)  b.setVisible (drawerOpen);
    for (auto& b : osButtons)   b.setVisible (drawerOpen);
    for (juce::Component* c : { (juce::Component*) &valuesButton, (juce::Component*) &popupsButton,
                                (juce::Component*) &mixKnob, (juce::Component*) &hpfKnob,
                                (juce::Component*) &linkButton, (juce::Component*) &compOffButton, (juce::Component*) &noiseButton })
        c->setVisible (drawerOpen);
}

juce::Rectangle<int> TwinFetEditor::scaled (const twinui::Rect& r, float pad) const
{
    return juce::Rectangle<float> ((r.x - pad) * s, (r.y - pad) * s, (r.w + 2.0f * pad) * s, (r.h + 2.0f * pad) * s).toNearestInt();
}

//==============================================================================
void TwinFetEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1c1f));
}

void TwinFetEditor::resized()
{
    namespace L = twinui::layout;
    s = (float) getWidth() / twinui::kW;
    look.scale = s;
    proc.apvts.state.setProperty (uiprop::scale, s, nullptr);   // this instance reopens at this size

    panelArt.setBounds (0, 0, getWidth(), juce::roundToInt (twinui::kFullH * s));
    {
        const auto j = L::jewel(), n = L::modeName();
        const float pd = JewelLamp::pad;
        jewelLamp.setBounds (scaled ({ j.x - pd, j.y - pd, (n.right() - j.x) + 2.0f * pd, j.h + 2.0f * pd }));
    }
    for (auto* sw : screws)
        sw->setBounds (scaled (sw->isBoardScrew() ? L::boardScrew (sw->index()) : L::earScrew (sw->index())));
    vu.setBounds (scaled (L::vu()));

    inputKnob.setBounds   (scaled (L::input(),   twinui::knobPad));
    outputKnob.setBounds  (scaled (L::output(),  twinui::knobPad));
    attackKnob.setBounds  (scaled (L::attack(),  twinui::knobPad));
    releaseKnob.setBounds (scaled (L::release(), twinui::knobPad));
    mixKnob.setBounds     (scaled (L::mix(),     twinui::knobPad));
    hpfKnob.setBounds     (scaled (L::hpf(),     twinui::knobPad));

    inputRead.setBounds   (scaled (L::inputReadout(),   Readout::pad));
    outputRead.setBounds  (scaled (L::outputReadout(),  Readout::pad));
    attackRead.setBounds  (scaled (L::attackReadout(),  Readout::pad));
    releaseRead.setBounds (scaled (L::releaseReadout(), Readout::pad));
    mixRead.setBounds     (scaled (L::mixReadout(),     Readout::pad));
    hpfRead.setBounds     (scaled (L::hpfReadout(),     Readout::pad));
    auto withCaption = [] (twinui::Rect r) { r.h += twinui::layout::readoutCaptionH; return r; };
    grRead.setBounds      (scaled (withCaption (L::grReadout()),  Readout::pad));
    outRead.setBounds     (scaled (withCaption (L::outReadout()), Readout::pad));

    auto placeButton = [this] (LitButton& b, const twinui::Rect& r)
    {
        b.pad = twinui::buttonPad * s;
        b.setBounds (scaled (r, twinui::buttonPad));
    };
    for (int i = 0; i < 5; ++i) placeButton (ratioButtons[(size_t) i], L::ratioButton (i));
    for (int i = 0; i < 3; ++i) placeButton (modeButtons[(size_t) i],  L::modeButton (i));
    for (int i = 0; i < 2; ++i) placeButton (revButtons[(size_t) i],   L::revButton (i));
    for (int i = 0; i < 4; ++i) placeButton (meterButtons[(size_t) i], L::meterButton (i));
    for (int i = 0; i < 6; ++i) placeButton (osButtons[(size_t) i],    L::osButton (i));
    placeButton (valuesButton,  L::valuesButton());
    placeButton (popupsButton,  L::popupsButton());
    placeButton (linkButton,    L::toggle (0));
    placeButton (compOffButton, L::toggle (1));
    placeButton (noiseButton,   L::toggle (2));
}

//==============================================================================
void TwinFetEditor::timerCallback()
{
    const float gr  = proc.meterGrDb.load (std::memory_order_relaxed);
    const float in  = proc.meterInMeanAbs.load (std::memory_order_relaxed);
    const float out = proc.meterOutMeanAbs.load (std::memory_order_relaxed);
    constexpr float halfPi = juce::MathConstants<float>::halfPi;

    // average-responding, sine calibrated: rectified mean * pi/2 = peak; 0 VU = -18 dBFS
    const float outVu = 20.0f * std::log10 (juce::jmax (1.0e-7f, out * halfPi)) + 18.0f;
    const float inVu  = 20.0f * std::log10 (juce::jmax (1.0e-7f, in * halfPi)) + 18.0f;

    float target = 0.0f;
    if (meterIndex == 0)
    {
        target = std::pow (10.0f, (-gr - 3.0f) / 20.0f);    // GR needle rests on 0 VU and swings left
    }
    else
    {
        const float vuDb = meterIndex == 3 ? inVu : (meterIndex == 2 ? outVu - 4.0f : outVu);
        target = std::pow (10.0f, (vuDb - 3.0f) / 20.0f);
    }
    vu.tick (target, 1.0f / 60.0f);

    const int osNow = proc.activeOversampling.load (std::memory_order_relaxed);
    if (osNow != shownOsFactor)
    {
        shownOsFactor = osNow;
        osButtons[0].getProperties().set ("sub", juce::String (osNow) + juce::String::fromUTF8 ("\xc3\x97"));
        osButtons[0].repaint();
    }

    if (++frame % 3 == 0)
    {
        grRead.setValue (juce::String (gr, 1));
        outRead.setValue (outVu > -40.0f ? juce::String (outVu, 1) : juce::String ("---"));   // dashes = no signal

        // while nobody touches the window, render the looks it may switch to next, in small slices
        if (juce::Time::getMillisecondCounter() - lastActivityMs > 700)
            prerenderStep();
    }
}

void TwinFetEditor::prerenderStep()
{
    // Most likely next first: the opened unit in the current look (revision and mode are switched in there);
    // then complete looks (faceplate + opened unit) for the other revision, the other wear (ANALOG or not)
    // and both changed; then the meter faces. Each call renders at most one thin band, so the window never
    // stalls, and a look is skipped when the cache has no room left for it (see PanelArt::prerender).
    const int otherRev = 1 - revIndex, otherWear = modeIndex == 2 ? 0 : 2;
    const int looks[4][2] = { { revIndex, modeIndex }, { otherRev, modeIndex }, { revIndex, otherWear }, { otherRev, otherWear } };
    if (panelArt.prerender (twinui::paletteFor (revIndex, modeIndex), true))
        return;
    for (int i = 1; i < 4; ++i)
        for (const bool internals : { false, true })
            if (panelArt.prerender (twinui::paletteFor (looks[i][0], looks[i][1]), internals))
                return;
    // meter faces, most likely first: other revision, then the other modes of this one, then of the other
    const int meterLooks[5][2] = { { otherRev, modeIndex }, { revIndex, (modeIndex + 1) % 3 }, { revIndex, (modeIndex + 2) % 3 },
                                   { otherRev, (modeIndex + 1) % 3 }, { otherRev, (modeIndex + 2) % 3 } };
    for (const auto& m : meterLooks)
        if (vu.prerender (twinui::paletteFor (m[0], m[1])))
            return;
}
