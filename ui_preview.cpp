// Offline preview of the TwinFET interface: renders Source/TwinPanel.h with cairo + FreeType
// (the plugin renders the very same drawing code through JUCE).
//
// Linux:  g++ -O2 -std=c++17 ui_preview.cpp -I../Source $(pkg-config --cflags --libs cairo freetype2) -o ui_preview
//         ./ui_preview ../fonts/BigShoulders-Bold.ttf out_prefix
#include "TwinPanel.h"

#include <cairo.h>
#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace twinui;

struct CairoCanvas
{
    cairo_t* cr = nullptr;
    cairo_font_face_t* face = nullptr;
    double emPerHeight = 0.8;   // cairo em size per unit of JUCE font height
    double scale = 1.0;         // pixels per base unit

    static void rgba (uint32_t c, double& r, double& g, double& b, double& a)
    {
        c = twinui::grade (c);   // same colour grade as the plugin's canvas
        a = ((c >> 24) & 0xff) / 255.0; r = ((c >> 16) & 0xff) / 255.0;
        g = ((c >> 8) & 0xff) / 255.0;  b = (c & 0xff) / 255.0;
    }

    void save()    { cairo_save (cr); }
    void restore() { cairo_restore (cr); }
    void clipRect (float x, float y, float w, float h) { cairo_new_path (cr); cairo_rectangle (cr, x, y, w, h); cairo_clip (cr); }

    void setColour (uint32_t c) { double r, g, b, a; rgba (c, r, g, b, a); cairo_set_source_rgba (cr, r, g, b, a); }

    void addStops (cairo_pattern_t* p, const GradStop* s, int n)
    {
        for (int i = 0; i < n; ++i) { double r, g, b, a; rgba (s[i].argb, r, g, b, a); cairo_pattern_add_color_stop_rgba (p, s[i].pos, r, g, b, a); }
    }
    void setLinear (float x1, float y1, float x2, float y2, const GradStop* s, int n)
    {
        cairo_pattern_t* p = cairo_pattern_create_linear (x1, y1, x2, y2);
        addStops (p, s, n); cairo_set_source (cr, p); cairo_pattern_destroy (p);
    }
    void setRadial (float cx, float cy, float r, const GradStop* s, int n)
    {
        cairo_pattern_t* p = cairo_pattern_create_radial (cx, cy, 0.0, cx, cy, r);
        addStops (p, s, n); cairo_set_source (cr, p); cairo_pattern_destroy (p);
    }

    void roundRect (float x, float y, float w, float h, float r)
    {
        r = std::fmin (r, 0.5f * std::fmin (w, h));
        cairo_new_sub_path (cr);
        cairo_arc (cr, x + w - r, y + r, r, -M_PI / 2, 0);
        cairo_arc (cr, x + w - r, y + h - r, r, 0, M_PI / 2);
        cairo_arc (cr, x + r, y + h - r, r, M_PI / 2, M_PI);
        cairo_arc (cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
        cairo_close_path (cr);
    }
    void ellipse (float x, float y, float w, float h)
    {
        cairo_save (cr);
        cairo_translate (cr, x + 0.5 * w, y + 0.5 * h);
        cairo_scale (cr, 0.5 * w, 0.5 * h);
        cairo_new_sub_path (cr);
        cairo_arc (cr, 0, 0, 1, 0, 2 * M_PI);
        cairo_restore (cr);
    }

    void fillRect (float x, float y, float w, float h) { cairo_new_path (cr); cairo_rectangle (cr, x, y, w, h); cairo_fill (cr); }
    void fillRoundRect (float x, float y, float w, float h, float r) { cairo_new_path (cr); roundRect (x, y, w, h, r); cairo_fill (cr); }
    void strokeRoundRect (float x, float y, float w, float h, float r, float lw) { cairo_new_path (cr); roundRect (x, y, w, h, r); cairo_set_line_width (cr, lw); cairo_stroke (cr); }
    void fillEllipse (float x, float y, float w, float h) { cairo_new_path (cr); ellipse (x, y, w, h); cairo_fill (cr); }
    void strokeEllipse (float x, float y, float w, float h, float lw) { cairo_new_path (cr); ellipse (x, y, w, h); cairo_set_line_width (cr, lw); cairo_stroke (cr); }
    void line (float x1, float y1, float x2, float y2, float lw)
    {
        cairo_new_path (cr); cairo_move_to (cr, x1, y1); cairo_line_to (cr, x2, y2);
        cairo_set_line_width (cr, lw); cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT); cairo_stroke (cr);
    }

    void newPath() { cairo_new_path (cr); }
    void moveTo (float x, float y) { cairo_move_to (cr, x, y); }
    void lineTo (float x, float y) { cairo_line_to (cr, x, y); }
    void arc (float cx, float cy, float r, float a0, float a1, bool newSub)
    {
        if (newSub) cairo_new_sub_path (cr);
        const double c0 = a0 - M_PI / 2, c1 = a1 - M_PI / 2;
        if (a1 >= a0) cairo_arc (cr, cx, cy, r, c0, c1); else cairo_arc_negative (cr, cx, cy, r, c0, c1);
    }
    void closePath() { cairo_close_path (cr); }
    void fillPath() { cairo_fill (cr); }
    void strokePath (float lw) { cairo_set_line_width (cr, lw); cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT); cairo_stroke (cr); }

    /** Per-pixel texture (makeGrain) composited as white/black with |v| * strength alpha. */
    void grain (float x, float y, float w, float h, int kind, uint32_t seed, float strength)
    {
        const int pw = std::max (1, (int) std::lround (w * scale)), ph = std::max (1, (int) std::lround (h * scale));
        std::vector<float> v;
        makeGrain (v, pw, ph, kind, seed, (float) scale);
        cairo_surface_t* img = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, pw, ph);
        cairo_surface_flush (img);
        unsigned char* data = cairo_image_surface_get_data (img);
        const int stride = cairo_image_surface_get_stride (img);
        for (int yy = 0; yy < ph; ++yy)
        {
            auto* row = reinterpret_cast<uint32_t*> (data + (size_t) yy * (size_t) stride);
            for (int xx = 0; xx < pw; ++xx)
            {
                const float t = v[(size_t) yy * (size_t) pw + (size_t) xx] * strength;
                const uint32_t a = (uint32_t) std::lround (255.0f * std::fmin (1.0f, std::fabs (t)));
                row[xx] = t > 0.0f ? ((a << 24) | (a << 16) | (a << 8) | a) : (a << 24);   // premultiplied
            }
        }
        cairo_surface_mark_dirty (img);
        cairo_save (cr);
        cairo_translate (cr, x, y);
        cairo_scale (cr, (double) w / (double) pw, (double) h / (double) ph);
        cairo_set_source_surface (cr, img, 0, 0);
        cairo_new_path (cr);
        cairo_rectangle (cr, 0, 0, pw, ph);
        cairo_fill (cr);
        cairo_restore (cr);
        cairo_surface_destroy (img);
    }

    void text (const char* utf8, float x, float y, float w, float h, float size, int align, float tracking)
    {
        cairo_set_font_face (cr, face);
        cairo_set_font_size (cr, size * emPerHeight);
        cairo_font_extents_t fe; cairo_font_extents (cr, &fe);
        // split into UTF-8 characters
        std::vector<std::string> chars;
        for (const unsigned char* p = (const unsigned char*) utf8; *p != 0;)
        {
            int n = (*p < 0x80) ? 1 : ((*p >> 5) == 6 ? 2 : ((*p >> 4) == 14 ? 3 : 4));
            chars.emplace_back ((const char*) p, (size_t) n);
            p += n;
        }
        const double extra = tracking * size;
        double total = 0.0;
        std::vector<double> adv;
        for (auto& c : chars) { cairo_text_extents_t te; cairo_text_extents (cr, c.c_str(), &te); adv.push_back (te.x_advance); total += te.x_advance; }
        if (! chars.empty()) total += extra * (double) (chars.size() - 1);
        double cx = align < 0 ? x : (align > 0 ? x + w - total : x + 0.5 * (w - total));
        const double baseline = y + 0.5 * h + 0.5 * (fe.ascent - fe.descent);
        for (size_t i = 0; i < chars.size(); ++i)
        {
            cairo_move_to (cr, cx, baseline);
            cairo_show_text (cr, chars[i].c_str());
            cx += adv[i] + extra;
        }
    }
};

struct UiState
{
    int revision = 0, mode = 2, ratio = 0, meter = 0;
    bool link = true, compOff = false, noise = false;
    float input = 6.0f, output = 1.5f, attack = 3.0f, release = 5.0f, mix = 100.0f, hpf = 20.0f;
    float vuDeflection = 0.4f;
    const char* grText = "6.3";
    const char* outText = "-1.2";
    uint32_t led = 0, ledFilter = 0;   // 0 = palette default
    bool drawerOpen = true, showValues = true, popups = true;
    int hoverScrew = -1;
    int oversampling = 0;
};

/** One FreeType face for the whole run. cairo keeps scaled fonts (and their glyph caches) alive after
    cairo_font_face_destroy, so freeing the FT_Face between renders let a new face reuse the old address
    and pick up stale glyphs of another size. */
static cairo_font_face_t* loadFace (const char* fontPath, double& emPerHeight)
{
    static FT_Library lib = nullptr;
    static FT_Face ftFace = nullptr;
    static cairo_font_face_t* face = nullptr;
    if (face == nullptr)
    {
        FT_Init_FreeType (&lib);
        FT_New_Face (lib, fontPath, 0, &ftFace);
        face = cairo_ft_font_face_create_for_ft_face (ftFace, 0);
    }
    emPerHeight = (double) ftFace->units_per_EM / (double) (ftFace->ascender - ftFace->descender);
    return face;
}

static void render (const char* fontPath, const char* outPng, const UiState& st, double scale)
{
    double emPerHeight = 0.8;
    cairo_font_face_t* face = loadFace (fontPath, emPerHeight);

    const float fullH = st.drawerOpen ? kFullH : kMainH;
    const int W = (int) std::lround (kW * scale), H = (int) std::lround (fullH * scale);
    cairo_surface_t* surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, W, H);
    cairo_t* cr = cairo_create (surf);
    cairo_scale (cr, scale, scale);

    CairoCanvas g;
    g.cr = cr; g.face = face; g.scale = scale;
    g.emPerHeight = emPerHeight;

    Palette p = paletteFor (st.revision, st.mode);
    if (st.led != 0) { p.led = st.led; p.ledFilter = st.ledFilter; }
    panel (g, p);
    unitScrews (g, st.drawerOpen, st.hoverScrew);   // in the plugin these are buttons

    char buf[64];
    // VU (static part is cached in the plugin, the needle is drawn every frame)
    vuStatic (g, layout::vu(), p, st.meter);
    vuDynamic (g, layout::vu(), st.vuDeflection);
    vuGlass (g, layout::vu(), p);
    const char* meterNames[] = { "GR", "+4", "+8", "INPUT" };
    for (int i = 0; i < 4; ++i) capButton (g, layout::meterButton (i), i == st.meter, false, false, p, meterNames[i], nullptr, false);

    // mode lamp and its name
    {
        const Rect j = layout::jewel();
        jewel (g, j.cx(), j.cy(), 0.36f * j.w, p.jewel);
        const char* modeNames[] = { "FAT", "CLEAN", "ANALOG" };
        ink (g, p, modeNames[st.mode], layout::modeName(), 18.0f, -1, 0.18f);
    }

    // knobs
    auto drawKnob = [&] (Rect k, float prop, bool big, uint32_t seed)
    { knob (g, k.cx(), k.cy(), knobRadius (k.w), angleFor (prop), big, p.wear, seed); };
    drawKnob (layout::input(),   (st.input + 24.0f) / 60.0f, true, 1);
    drawKnob (layout::output(),  (st.output + 24.0f) / 48.0f, true, 2);
    drawKnob (layout::attack(),  (st.attack - 1.0f) / 6.0f, false, 3);
    drawKnob (layout::release(), (st.release - 1.0f) / 6.0f, false, 4);
    if (st.drawerOpen)
    {
        drawKnob (layout::mix(), st.mix / 100.0f, false, 5);
        drawKnob (layout::hpf(), st.hpf <= 20.5f ? 0.0f : hpfProportion (st.hpf), false, 6);
    }

    if (st.showValues)
    {
        std::snprintf (buf, sizeof (buf), "%.1f", st.input);  ledReadout (g, layout::inputReadout(), buf, "dB", p, 4, true, 0.85f);
        std::snprintf (buf, sizeof (buf), "%.1f", st.output); ledReadout (g, layout::outputReadout(), buf, "dB", p, 4, true, 0.85f);
        ledReadout (g, layout::attackReadout(), "234", "\xC2\xB5s", p, 3, true, 0.85f);
        ledReadout (g, layout::releaseReadout(), "140", "ms", p, 4, true, 0.85f);
        ledReadout (g, layout::grReadout(), st.grText, "dB", p, 4);
        ledReadout (g, layout::outReadout(), st.outText, "VU", p, 4);
        const Rect gr = layout::grReadout(), ol = layout::outReadout();
        readoutCaption (g, p, { gr.x, gr.bottom() + 3.0f, gr.w, 12.0f }, "GAIN REDUCTION");
        readoutCaption (g, p, { ol.x, ol.bottom() + 3.0f, ol.w, 12.0f }, "OUTPUT LEVEL");
        if (st.drawerOpen)
        {
            std::snprintf (buf, sizeof (buf), "%d", (int) st.mix); ledReadout (g, layout::mixReadout(), buf, "%", p, 3, true, 0.85f);
            ledReadout (g, layout::hpfReadout(), st.hpf <= 20.5f ? "oFF" : "120", "Hz", p, 3, st.hpf > 20.5f, 0.85f);
        }
    }

    const char* ratioNames[] = { "4:1", "8:1", "12:1", "20:1", "ALL" };
    for (int i = 0; i < 5; ++i) capButton (g, layout::ratioButton (i), i == st.ratio, false, false, p, ratioNames[i], nullptr, true);

    if (st.drawerOpen)
    {
        const char* modeNames[] = { "FAT", "CLEAN", "ANALOG" };
        const char* modeSubs[]  = { "INFLATE", "PRECISE", "CIRCUIT" };
        for (int i = 0; i < 3; ++i) litButton (g, layout::modeButton (i), i == st.mode, false, false, p, modeNames[i], modeSubs[i]);
        const char* revNames[] = { "BLACK", "BLUE" };
        for (int i = 0; i < 2; ++i) litButton (g, layout::revButton (i), i == st.revision, false, false, p, revNames[i], nullptr);
        const char* osNames[] = { "AUTO", "1\xC3\x97", "2\xC3\x97", "4\xC3\x97", "8\xC3\x97", "16\xC3\x97" };
        for (int i = 0; i < 6; ++i) capButton (g, layout::osButton (i), i == st.oversampling, false, false, p, osNames[i], i == 0 ? "4\xC3\x97" : nullptr, false);
        capButton (g, layout::valuesButton(), st.showValues, false, false, p, "VALUES", st.showValues ? "ON" : "OFF", true);
        capButton (g, layout::popupsButton(), st.popups, false, false, p, "POP-UPS", st.popups ? "ON" : "OFF", true);
        const char* togNames[] = { "LINK", "COMP OFF", "NOISE" };
        const bool tog[] = { st.link, st.compOff, st.noise };
        for (int i = 0; i < 3; ++i) litButton (g, layout::toggle (i), tog[i], i == 1, false, p, togNames[i], nullptr);
    }

    cairo_destroy (cr);
    cairo_surface_write_to_png (surf, outPng);
    cairo_surface_destroy (surf);
    std::printf ("wrote %s\n", outPng);
}

int main (int argc, char** argv)
{
    const char* font = argc > 1 ? argv[1] : "../fonts/BigShoulders-Bold.ttf";
    const std::string prefix = argc > 2 ? argv[2] : "ui";
    const double sc = argc > 3 ? std::atof (argv[3]) : 1.0;
    const char* modeTag[] = { "fat", "clean", "analog" };
    for (int rev = 0; rev < 2; ++rev)
        for (int mode = 0; mode < 3; ++mode)
        {
            UiState st;
            st.revision = rev;
            st.mode = mode;
            if (rev == 1) { st.ratio = 4; st.input = 14.0f; st.vuDeflection = 0.62f; st.mix = 70.0f; st.hpf = 120.0f; st.grText = "11.8"; }
            const std::string name = prefix + (rev == 0 ? "_black_" : "_blue_") + modeTag[mode] + ".png";
            render (font, name.c_str(), st, sc);
        }
    {
        UiState st;   // closed drawer, values hidden
        st.drawerOpen = false; st.showValues = false;
        render (font, (prefix + "_closed_novalues.png").c_str(), st, sc);
        st.showValues = true;
        render (font, (prefix + "_closed.png").c_str(), st, sc);
    }
    return 0;
}
