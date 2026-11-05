// CheatChord desktop look: red velvet and renaissance gold on near-black, Titillium Web.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace cc::ui {

struct Palette
{
    juce::Colour bg, panel, panelHi, border, tile, tileHover, tileOn, tileOnText, text, dim, faint,
                 gold, goldDim, velvet, velvetHi, playing, good, bad, bass;
    juce::Colour cls[5]; // in key, borrowed, leads to next, substitute, spicy
};

const Palette& pal();
void setTheme (int index);           // 0 Velvet, 1 Graphite, 2 Daylight
int theme();
juce::StringArray themeNames();

juce::Font font (float h, int weight = 1);   // 0 regular, 1 semibold, 2 bold; h is multiplied by the text size
// TEXT SIZE (layout menu): every font and every fixed row height grows with it
float textScale();
void setTextScale (float s);
inline int U (int px) { return (int) std::lround (px * textScale()); }
inline float Uf (float px) { return px * textScale(); }
// each panel has its own colour (tabs, title bar, pop-out window)
juce::Colour panelColour (const juce::String& id);
juce::Image logoImage();      // the medallion + CHEATCHORD wordmark
juce::Image medallionImage();
juce::Image jfImage();        // the JF logo in red velvet

// a text that shrinks to fit its box (down to minH), centred unless just says otherwise
void fitText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float h, int weight,
              juce::Colour c, juce::Justification just = juce::Justification::centred, float minH = 9.0f, int lines = 1);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void refreshColours();
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    juce::Font getPopupMenuFont() override { return font (16.0f, 1); }
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize, bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override { return 10; }
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
};

} // namespace cc::ui
