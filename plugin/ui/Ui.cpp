#include "Ui.h"
#include "../PluginProcessor.h"

namespace cc::ui {

// ================================================================ Model
bool Model::refresh (cc::Processor& p)
{
    auto& eng = p.engine();
    bool ch = false;
    {
        std::lock_guard<std::mutex> g (eng.mutex());
        for (auto& k : keys)
        {
            auto s = eng.getU (k.c_str());
            auto it = v.find (k);
            if (it == v.end() || it->second != s) { v[k] = std::move (s); ch = true; }
        }
        void* in = eng.raw();
        auto upd = [&ch] (auto& field, auto value) { if (field != value) { field = value; ch = true; } };
        upd (count, cc_count (in));
        upd (loopLen, cc_loop_len (in));
        upd (playSlot, cc_play_slot (in));
        upd (selIdea, cc_selected_idea (in));
        upd (cursor, cc_cursor (in));
        upd (target, cc_target (in));
        upd (kbLo, cc_kb_lo (in));
        upd (busy, cc_busy (in) ? 1 : 0);
        upd (bpm, cc_bpm (in));
        for (int i = 0; i < 8; ++i)
        {
            upd (ideaClass[i], cc_idea_class (in, i));
            upd (slotClass[i], cc_slot_class (in, i));
            char rb[32];
            cc_slot_roman (in, i, rb, (int) sizeof rb);
            juce::String r (rb);
            if (r != slotRoman[i]) { slotRoman[i] = r; ch = true; }
        }
        int notes[16], bass = -1;
        int n = cc_shown_notes (in, notes, 16, &bass);
        if (n != shownN || bass != shownBass || (n > 0 && std::memcmp (notes, shown, sizeof (int) * (size_t) n) != 0))
        {
            shownN = n; shownBass = bass;
            if (n > 0) std::memcpy (shown, notes, sizeof (int) * (size_t) n);
            ch = true;
        }
    }
    {
        bool hp = p.hostPlaying();
        if (hp != hostPlaying) { hostPlaying = hp; ch = true; }
    }
    if (ch) ++revision;
    return ch;
}

juce::String Model::text (const std::string& key) const
{
    auto it = v.find (key);
    return it == v.end() ? juce::String() : desktopText (it->second.c_str());
}

int Model::val (const std::string& key) const
{
    auto it = v.find (key);
    return it == v.end() ? 0 : std::atoi (it->second.c_str());
}

// ================================================================ Ctx
void Ctx::setValue (const char* key, int v) { proc.setValue (key, v); if (changed) changed(); }
void Ctx::tap (const char* key) { proc.engine().trigger (key); if (changed) changed(); }
void Ctx::gesture (const char* key, bool begin) { proc.gesture (key, begin); }
void Ctx::updateAll()
{
    auto copy = lives;   // a widget may be created or destroyed by an update (tab switch)
    for (auto* l : copy)
        if (std::find (lives.begin(), lives.end(), l) != lives.end())
            l->update();
}

// ================================================================ helpers
juce::Rectangle<int> take (juce::Rectangle<int>& r, int h, int gap)
{
    auto a = r.removeFromTop (juce::jmin (h, r.getHeight()));
    r.removeFromTop (juce::jmin (gap, r.getHeight()));
    return a;
}

juce::Rectangle<int> takeLeft (juce::Rectangle<int>& r, int w, int gap)
{
    auto a = r.removeFromLeft (juce::jmin (w, r.getWidth()));
    r.removeFromLeft (juce::jmin (gap, r.getWidth()));
    return a;
}

void grid (std::vector<juce::Component*> items, juce::Rectangle<int> r, int cols, int gap)
{
    if (items.empty() || cols <= 0) return;
    const int rows = ((int) items.size() + cols - 1) / cols;
    const float cw = (r.getWidth() - gap * (cols - 1)) / (float) cols;
    const float rh = (r.getHeight() - gap * (rows - 1)) / (float) rows;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const int c = (int) i % cols, rr = (int) i / cols;
        items[i]->setBounds (juce::Rectangle<float> (r.getX() + c * (cw + gap), r.getY() + rr * (rh + gap), cw, rh).toNearestInt());
    }
}

void label (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, juce::Justification j)
{
    fitText (g, t.toUpperCase(), r.toFloat(), 15.0f, 2, pal().dim, j, 10.0f);
}

// ================================================================ Panel
juce::Rectangle<int> Panel::titleBar() const
{
    return title.isEmpty() ? juce::Rectangle<int>() : getLocalBounds().reduced (12, 0).withTrimmedTop (5).withHeight (U (28));
}

juce::Rectangle<int> Panel::popButton() const
{
    if (title.isEmpty() || ! onPopOut) return {};
    auto t = titleBar();
    return t.removeFromRight (t.getHeight()).reduced (2);
}

juce::Rectangle<int> Panel::content() const
{
    auto r = getLocalBounds().reduced (12, 10);
    if (title.isNotEmpty()) r.removeFromTop (U (30));
    return r;
}

static void drawPopIcon (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c, bool hot)
{
    g.setColour (hot ? c.withAlpha (0.25f) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (b, 4.0f);
    auto r = b.reduced (b.getWidth() * 0.24f);
    g.setColour (c);
    g.drawRect (r.withTrimmedTop (r.getHeight() * 0.3f).withTrimmedRight (r.getWidth() * 0.3f), 1.6f);
    juce::Path a;
    a.startNewSubPath (r.getCentreX(), r.getCentreY());
    a.lineTo (r.getRight(), r.getY());
    a.startNewSubPath (r.getRight() - r.getWidth() * 0.4f, r.getY());
    a.lineTo (r.getRight(), r.getY());
    a.lineTo (r.getRight(), r.getY() + r.getHeight() * 0.4f);
    g.strokePath (a, juce::PathStrokeType (1.6f));
}

void Panel::paint (juce::Graphics& g)
{
    if (! frame) return;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (pal().panel);
    g.fillRoundedRectangle (r, 8.0f);
    if (title.isNotEmpty())
    {
        // the panel's colour: a band on top and the title in it
        auto band = r.withHeight ((float) U (30) + 8.0f);
        g.setColour (accent.withAlpha (0.16f));
        g.fillRoundedRectangle (band, 8.0f);
        g.fillRect (band.withTrimmedTop (8.0f));
        g.setColour (accent);
        g.fillRoundedRectangle (r.withHeight (4.0f).reduced (6.0f, 0), 2.0f);
        auto t = titleBar().toFloat();
        if (! popButton().isEmpty()) t.removeFromRight ((float) t.getHeight() + 4);
        fitText (g, title.toUpperCase(), t, 18.0f, 2, accent, juce::Justification::centredLeft);
        if (! popButton().isEmpty())
            drawPopIcon (g, popButton().toFloat(), accent, popButton().contains (getMouseXYRelative()) && isMouseOver());
    }
    g.setColour (accent.withAlpha (0.45f));
    g.drawRoundedRectangle (r, 8.0f, 1.2f);
}

void Panel::mouseDown (const juce::MouseEvent& e)
{
    if (popButton().contains (e.getPosition()) && onPopOut) onPopOut();
}

void Panel::mouseMove (const juce::MouseEvent& e)
{
    repaint (titleBar());
    if (onHint)
    {
        if (popButton().contains (e.getPosition())) onHint ("Open as window", "Opens " + title + " in its own small window you can move and resize. Dock it again with the button in its title bar.");
        else if (titleBar().contains (e.getPosition())) onHint (title, about);
    }
}

// ================================================================ Tile
bool Tile::set (const juce::String& t, const juce::String& s, bool o)
{
    if (t == text && s == sub && o == on) return false;
    text = t; sub = s; on = o;
    repaint();
    return true;
}

void Tile::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool over = isMouseOver (true) && isEnabled(), down = isMouseButtonDown() && isEnabled();
    const float rad = juce::jmin (8.0f, r.getHeight() * 0.25f);

    juce::Colour bg = p.tile, fg = p.text;
    if (style == Primary) { bg = p.gold; fg = p.tileOnText; }
    if (style == Ghost) bg = juce::Colours::transparentBlack;
    if (on) { bg = p.tileOn; fg = p.tileOnText; }
    if (over && ! on) bg = style == Primary ? p.gold.brighter (0.15f) : style == Ghost ? p.tile : p.tileHover;
    if (down) bg = bg.darker (0.15f);
    if (dim) { fg = p.faint; }
    if (! isEnabled()) { fg = p.faint; bg = bg.withMultipliedAlpha (0.5f); }

    if (empty)
    {
        juce::Path pth;
        pth.addRoundedRectangle (r, rad);
        juce::Path dashed;
        const float d[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, pth, d, 2);
        g.setColour ((over ? p.gold : p.faint).withAlpha (0.8f));
        g.fillPath (dashed);
    }
    else
    {
        g.setColour (bg);
        g.fillRoundedRectangle (r, rad);
        if (style == Action || style == Chord)
        {
            g.setColour (on ? bg.darker (0.2f) : p.border);
            g.drawRoundedRectangle (r, rad, 1.0f);
        }
        if (style == Ghost)
        {
            g.setColour (p.border);
            g.drawRoundedRectangle (r, rad, 1.0f);
        }
    }
    if (accent.getAlpha() > 0 && ! empty)
    {
        g.setColour (accent);
        g.fillRoundedRectangle (r.withWidth (5.0f).reduced (0, 3.0f), 2.0f);
    }
    if (hot)
    {
        g.setColour (p.playing);
        g.drawRoundedRectangle (r.reduced (1.0f), rad, 3.0f);
        g.setColour (p.playing.withAlpha (0.18f));
        g.fillRoundedRectangle (r, rad);
    }

    auto tr = r.reduced (juce::jmin (12.0f, r.getWidth() * 0.08f), 3.0f);
    if (accent.getAlpha() > 0) tr.removeFromLeft (4.0f);
    const float big = juce::jmin (r.getHeight() * (sub.isNotEmpty() ? 0.40f : 0.48f), 34.0f) * textScale;
    if (badge.isNotEmpty())
    {
        auto b = r.reduced (6.0f, 5.0f).removeFromTop (juce::jmax (11.0f, r.getHeight() * 0.2f));
        fitText (g, badge, b, b.getHeight(), 2, on ? fg.withAlpha (0.75f) : p.gold.withAlpha (0.9f), juce::Justification::topRight, 8.0f);
    }
    if (sub.isNotEmpty())
    {
        auto top = tr.withTrimmedBottom (tr.getHeight() * 0.42f);
        auto bot = tr.withTrimmedTop (tr.getHeight() * 0.58f);
        fitText (g, text, top, big, 1, fg, juce::Justification::centredBottom, 9.0f);
        fitText (g, sub, bot, juce::jmax (11.0f, big * 0.55f), 1, (on || style == Primary) ? fg.withAlpha (0.78f) : p.dim, juce::Justification::centredTop, 9.0f);
    }
    else
        fitText (g, text, tr, big, style == Primary || style == Action ? 2 : 1, empty ? p.faint : fg, juce::Justification::centred, 9.0f);
}

void Tile::mouseDown (const juce::MouseEvent&) { dragged = false; repaint(); }
void Tile::mouseUp (const juce::MouseEvent& e)
{
    repaint();
    if (! dragged && isEnabled() && e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick)
        if (e.getNumberOfClicks() < 2 || ! onDouble)
            onClick (e);
}
void Tile::mouseDoubleClick (const juce::MouseEvent& e) { if (isEnabled() && onDouble) onDouble (e); }
void Tile::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragged && onDragStart && e.getDistanceFromDragStart() > 6)
    {
        dragged = true;
        onDragStart (e);
    }
}

// ================================================================ Bar
Bar::Bar (Ctx& c, const char* k, juce::StringArray l, int columns, int valueOffset)
    : Live (c), labels (std::move (l)), key (k), cols (columns), offset (valueOffset)
{
    ctx.model.want (key);
    if (labels.isEmpty())
        if (auto* m = findParam (key))
            for (int i = 0; i < m->nopt; ++i) labels.add (desktopText (m->opt[i]));
    setRepaintsOnMouseActivity (true);
}

void Bar::update()
{
    int s = ctx.val (key) - offset;
    if (s != sel) { sel = s; repaint(); }
}

juce::Rectangle<float> Bar::cell (int i) const
{
    const int n = labels.size();
    const int c = cols > 0 ? cols : n;
    const int rows = (n + c - 1) / c;
    const float gap = 4.0f;
    auto r = getLocalBounds().toFloat();
    const float cw = (r.getWidth() - gap * (c - 1)) / c, rh = (r.getHeight() - gap * (rows - 1)) / rows;
    return { (i % c) * (cw + gap), (i / c) * (rh + gap), cw, rh };
}

void Bar::paint (juce::Graphics& g)
{
    auto& p = pal();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto r = cell (i).reduced (0.5f);
        const bool s = i == sel;
        g.setColour (s ? p.tileOn : i == hover ? p.tileHover : p.tile);
        g.fillRoundedRectangle (r, 5.0f);
        if (! s)
        {
            g.setColour (p.border);
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
        }
        fitText (g, labels[i], r.reduced (3.0f, 1.0f), juce::jmin (textH, r.getHeight() * 0.62f), s ? 2 : 1,
                 s ? p.tileOnText : p.text, juce::Justification::centred, 8.0f);
    }
}

void Bar::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < labels.size(); ++i)
        if (cell (i).contains (e.position))
        {
            if (onPick) onPick (i);
            else ctx.setValue (key, i + offset);
            sel = i;
            repaint();
            return;
        }
}

void Bar::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < labels.size(); ++i)
        if (cell (i).contains (e.position)) h = i;
    if (h != hover)
    {
        hover = h;
        repaint();
        if (help.isNotEmpty()) ctx.help (helpTitle.isNotEmpty() ? helpTitle : (hover >= 0 ? labels[hover] : juce::String()), help);
    }
}

// ================================================================ Knob
Knob::Knob (Ctx& c, const char* k, const juce::String& l, std::function<juce::String (int)> f)
    : Live (c), key (k), label (l), fmt (std::move (f))
{
    ctx.model.want (key);
    if (auto* m = findParam (key)) { mn = m->min; mx = m->max; def = m->def; }
    if (! fmt) fmt = [] (int x) { return juce::String (x); };
}

void Knob::update()
{
    int nv = ctx.val (key);
    if (nv != v) { v = nv; repaint(); }
}

void Knob::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto r = getLocalBounds().toFloat();
    auto lab = r.removeFromTop (juce::jmin (Uf (22.0f), r.getHeight() * 0.22f));
    auto val = r.removeFromBottom (juce::jmin (Uf (24.0f), r.getHeight() * 0.25f));
    fitText (g, label.toUpperCase(), lab, 15.0f, 2, p.dim, juce::Justification::centred, 9.0f);
    const float d = juce::jmin (r.getWidth(), r.getHeight()) - 4.0f;
    auto k = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
    const float t = mx > mn ? (v - mn) / (float) (mx - mn) : 0.0f;
    const float a = a0 + t * (a1 - a0);
    const float stroke = juce::jmax (3.0f, d * 0.09f);
    juce::Path bgArc, valArc;
    bgArc.addCentredArc (k.getCentreX(), k.getCentreY(), d * 0.5f - stroke, d * 0.5f - stroke, 0, a0, a1, true);
    valArc.addCentredArc (k.getCentreX(), k.getCentreY(), d * 0.5f - stroke, d * 0.5f - stroke, 0, a0, a, true);
    g.setColour (p.tile);
    g.strokePath (bgArc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (isMouseOverOrDragging() ? p.gold.brighter (0.2f) : p.gold);
    g.strokePath (valArc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto cap = k.reduced (stroke * 2.2f);
    g.setColour (p.panelHi);
    g.fillEllipse (cap);
    g.setColour (p.border);
    g.drawEllipse (cap, 1.0f);
    juce::Point<float> c = cap.getCentre();
    juce::Point<float> tip = c + juce::Point<float> (std::sin (a), -std::cos (a)) * (cap.getWidth() * 0.42f);
    g.setColour (p.text);
    g.drawLine ({ c + (tip - c) * 0.35f, tip }, juce::jmax (2.0f, d * 0.05f));
    fitText (g, fmt (v), val, 18.0f, 1, p.text, juce::Justification::centred, 10.0f);
}

void Knob::send (int nv)
{
    nv = juce::jlimit (mn, mx, nv);
    if (nv == v) return;
    v = nv;
    repaint();
    ctx.setValue (key, v);
}

void Knob::mouseDown (const juce::MouseEvent&) { dragStartV = (float) v; ctx.gesture (key, true); }
void Knob::mouseUp (const juce::MouseEvent&) { ctx.gesture (key, false); }
void Knob::mouseDrag (const juce::MouseEvent& e)
{
    const float range = (float) (mx - mn);
    const float px = e.mods.isShiftDown() ? 600.0f : 180.0f;   // pixels for the whole range
    const float dv = (-(float) e.getDistanceFromDragStartY() + (float) e.getDistanceFromDragStartX() * 0.5f) / px * range;
    send ((int) std::lround (dragStartV + dv));
}
void Knob::mouseDoubleClick (const juce::MouseEvent&) { send (def); }
void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int step = juce::jmax (1, (mx - mn) / 50);
    send (v + (w.deltaY > 0 ? step : w.deltaY < 0 ? -step : 0));
}
void Knob::mouseEnter (const juce::MouseEvent&) { if (help.isNotEmpty()) ctx.help (label, help); }

// ================================================================ Picker
Picker::Picker (Ctx& c, const char* k, const juce::String& l, bool s) : Live (c), key (k), label (l), searchable (s)
{
    ctx.model.want (key);
    if (auto* m = findParam (key))
        for (int i = 0; i < m->nopt; ++i) options.add (desktopText (m->opt[i]));
    prev.set ("<");
    next.set (">");
    prev.onClick = [this] (const juce::MouseEvent&) { step (-1); };
    next.onClick = [this] (const juce::MouseEvent&) { step (1); };
    addAndMakeVisible (prev);
    addAndMakeVisible (next);
    setRepaintsOnMouseActivity (true);
}

void Picker::update()
{
    int nv = ctx.val (key);
    if (nv != v) { v = nv; repaint(); }
}

void Picker::resized()
{
    auto r = getLocalBounds();
    if (label.isNotEmpty()) r.removeFromTop (juce::jmin (U (22), r.getHeight() / 2));
    const int bw = juce::jmin (34, r.getWidth() / 6);
    prev.setBounds (r.removeFromLeft (bw));
    next.setBounds (r.removeFromRight (bw));
    r.reduce (4, 0);
    field = r;
}

void Picker::paint (juce::Graphics& g)
{
    auto& p = pal();
    if (label.isNotEmpty())
        cc::ui::label (g, label, getLocalBounds().removeFromTop (juce::jmin (U (22), getHeight() / 2)));
    auto f = field.toFloat();
    const bool over = isMouseOver (false) && field.contains (getMouseXYRelative());
    g.setColour (over ? p.tileHover : p.tile);
    g.fillRoundedRectangle (f, 5.0f);
    g.setColour (p.border);
    g.drawRoundedRectangle (f.reduced (0.5f), 5.0f, 1.0f);
    const int i = v - offset;
    juce::String t = i >= 0 && i < options.size() ? options[i] : juce::String (v);
    auto tr = f.reduced (10.0f, 2.0f);
    auto arrow = tr.removeFromRight (14.0f);
    fitText (g, t, tr, 18.0f, 1, p.gold, juce::Justification::centredLeft, 10.0f);
    juce::Path tri;
    tri.addTriangle (arrow.getX(), arrow.getCentreY() - 3, arrow.getRight(), arrow.getCentreY() - 3, arrow.getCentreX(), arrow.getCentreY() + 4);
    g.setColour (p.goldDim);
    g.fillPath (tri);
}

void Picker::mouseEnter (const juce::MouseEvent&) { if (help.isNotEmpty()) ctx.help (label, help); }

void Picker::step (int d)
{
    const int n = options.size();
    if (n <= 0) return;
    ctx.setValue (key, ((v - offset) + d + n) % n + offset);
}

void Picker::mouseDown (const juce::MouseEvent& e)
{
    if (field.contains (e.getPosition())) open();
}

void Picker::open()
{
    if (searchable)
    {
        auto list = std::make_unique<SearchList> (ctx, key, label);
        list->setSize (juce::jmax (300, field.getWidth()), 460);
        auto* raw = list.get();
        auto* top = getTopLevelComponent();
        auto& box = juce::CallOutBox::launchAsynchronously (std::move (list), top->getLocalArea (this, field), top);
        raw->onPicked = [&box] { box.dismiss(); };
        raw->update();
        raw->focusSearch();
        return;
    }
    juce::PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    const int n = options.size();
    const int cols = n > 12 ? (n + 23) / 24 : 1;
    const int per = (n + cols - 1) / cols;
    for (int i = 0; i < n; ++i)
    {
        if (cols > 1 && i > 0 && i % per == 0) m.addColumnBreak();
        m.addItem (i + 1, options[i], true, i == v - offset);
    }
    juce::Component::SafePointer<Picker> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (field.getWidth() / juce::jmax (1, cols)).withStandardItemHeight (26),
                     [safe] (int r) { if (safe != nullptr && r > 0) safe->ctx.setValue (safe->key, r - 1 + safe->offset); });
}

// ================================================================ SearchList
SearchList::SearchList (Ctx& c, const char* k, const juce::String& t, std::function<juce::String (int)> d)
    : Live (c), key (k), title (t), detail (std::move (d))
{
    ctx.model.want (key);
    if (auto* m = findParam (key))
        for (int i = 0; i < m->nopt; ++i) options.add (desktopText (m->opt[i]));
    search.setTextToShowWhenEmpty ("Search " + title.toLowerCase() + "...", pal().faint);
    search.setFont (font (17.0f, 0));
    search.addListener (this);
    search.setIndents (8, 5);
    list.setModel (this);
    list.setRowHeight (28);
    list.setOutlineThickness (0);
    addAndMakeVisible (search);
    addAndMakeVisible (list);
    filter();
}

void SearchList::filter()
{
    rows.clear();
    auto q = search.getText().trim();
    for (int i = 0; i < options.size(); ++i)
        if (q.isEmpty() || options[i].containsIgnoreCase (q)) rows.push_back (i);
    list.updateContent();
    list.repaint();
    showCurrent();
}

void SearchList::showCurrent()
{
    for (size_t r = 0; r < rows.size(); ++r)
        if (rows[r] == v)
        {
            list.selectRow ((int) r, true, true);
            return;
        }
    list.deselectAllRows();
}

void SearchList::update()
{
    int nv = ctx.val (key);
    if (nv != v) { v = nv; showCurrent(); list.repaint(); }
}

void SearchList::resized()
{
    auto r = getLocalBounds();
    if (title.isNotEmpty() && ! onPicked) r.removeFromTop (U (24));
    search.setBounds (take (r, juce::jmin (U (34), r.getHeight() / 4), 6));
    list.setBounds (r);
    list.setRowHeight (juce::jlimit (U (26), U (34), getHeight() / 12));
}

void SearchList::paint (juce::Graphics& g)
{
    if (title.isNotEmpty() && ! onPicked)
    {
        juce::String t = title + "  (" + juce::String (options.size() - (options.size() > 0 && options[0].startsWith ("No ") ? 1 : 0)) + ")";
        label (g, t, getLocalBounds().removeFromTop (U (22)));
    }
    g.setColour (pal().bg.withAlpha (0.35f));
    g.fillRoundedRectangle (list.getBounds().toFloat(), 5.0f);
}

void SearchList::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (row < 0 || row >= (int) rows.size()) return;
    auto& p = pal();
    const int i = rows[(size_t) row];
    const bool cur = i == v;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (3.0f, 1.5f);
    if (cur) { g.setColour (p.tileOn); g.fillRoundedRectangle (r, 4.0f); }
    else if (row % 2) { g.setColour (p.tile.withAlpha (0.5f)); g.fillRoundedRectangle (r, 4.0f); }
    auto tr = r.reduced (10.0f, 0);
    if (detail)
    {
        auto d = detail (i);
        if (d.isNotEmpty())
            fitText (g, d, tr.removeFromRight (tr.getWidth() * 0.45f), h * 0.45f, 0, cur ? p.tileOnText.withAlpha (0.7f) : p.faint,
                     juce::Justification::centredRight, 8.0f);
    }
    fitText (g, options[i], tr, h * 0.58f, cur ? 2 : 1, cur ? p.tileOnText : p.text, juce::Justification::centredLeft, 9.0f);
}

void SearchList::pick (int row)
{
    if (row < 0 || row >= (int) rows.size()) return;
    v = rows[(size_t) row];
    ctx.setValue (key, v);
    list.repaint();
    if (onPicked) onPicked();
}

// ================================================================ Line
Line::Line (Ctx& c, const char* k, float hh, juce::Justification j, int l) : Live (c), key (k), h (hh), just (j), lines (l)
{
    ctx.model.want (key);
}
Line::Line (Ctx& c, std::function<juce::String()> s, float hh, juce::Justification j, int l)
    : Live (c), src (std::move (s)), h (hh), just (j), lines (l) {}

void Line::update()
{
    juce::String n = src ? src() : ctx.text (key);
    if (n != t) { t = n; repaint(); }
}

void Line::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (boxed)
    {
        g.setColour (pal().bg.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 5.0f);
        r = r.reduced (10.0f, 2.0f);
    }
    fitText (g, t, r, juce::jmin (h, r.getHeight() * (lines > 1 ? 0.45f : 0.7f)), weight, colour, just, 9.0f, lines);
}

// ================================================================ Keyboard
Keyboard::Keyboard (Ctx& c, bool b) : Live (c), big (b)
{
    const int n = big ? 72 : 12;
    state.assign ((size_t) n, 0);
    for (int i = 1; i <= n; ++i)
        ctx.model.want ((big ? "kbx_" : "kb_") + std::to_string (i));
}

void Keyboard::update()
{
    bool ch = false;
    for (int i = 0; i < (int) state.size(); ++i)
    {
        int s = ctx.model.val ((big ? "kbx_" : "kb_") + std::to_string (i + 1));
        if (s != state[(size_t) i]) { state[(size_t) i] = s; ch = true; }
    }
    int l = big ? ctx.model.kbLo : 0;
    if (l != lo) { lo = l; ch = true; }
    if (ch) repaint();
}

void Keyboard::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b0a0c));
    g.fillRoundedRectangle (r, 6.0f);
    r = r.reduced (big ? 6.0f : 3.0f);
    const int n = (int) state.size();
    const int first = big ? lo : 0;   // the mini keyboard is one octave from C
    static const bool black[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
    int whites = 0;
    for (int i = 0; i < n; ++i) if (! black[(first + i) % 12]) ++whites;
    const float ww = r.getWidth() / (float) juce::jmax (1, whites);
    auto col = [&] (int s, bool isBlack) -> juce::Colour
    {
        switch (s)
        {
            case 1: return p.gold;
            case 2: return p.bass;
            case 3: return p.good;
            case 4: return p.bad;
            default: return isBlack ? juce::Colour (0xff17151a) : juce::Colour (0xffece6da);
        }
    };
    // white keys
    int wi = 0;
    std::vector<float> wx ((size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const int pc = (first + i) % 12;
        if (black[pc]) { wx[(size_t) i] = r.getX() + wi * ww; continue; }
        auto k = juce::Rectangle<float> (r.getX() + wi * ww, r.getY(), ww, r.getHeight()).reduced (0.75f, 0);
        g.setColour (col (state[(size_t) i], false));
        g.fillRoundedRectangle (k, 3.0f);
        if (state[(size_t) i] == 4)   // gone: hatched dark red
        {
            g.setColour (juce::Colour (0x55000000));
            for (float y = k.getY(); y < k.getBottom(); y += 6.0f)
                g.drawLine (k.getX(), y, k.getRight(), y + 4.0f, 1.0f);
        }
        if (big && pc == 0)
        {
            const int note = first + i;
            fitText (g, "C" + juce::String (note / 12 - 2), k.removeFromBottom (juce::jmin (18.0f, k.getHeight() * 0.2f)),
                     12.0f, 1, state[(size_t) i] ? p.tileOnText : juce::Colour (0xff7a7268), juce::Justification::centred, 7.0f);
        }
        ++wi;
    }
    // black keys on top
    const float bw = ww * 0.62f, bh = r.getHeight() * 0.62f;
    for (int i = 0; i < n; ++i)
    {
        const int pc = (first + i) % 12;
        if (! black[pc]) continue;
        auto k = juce::Rectangle<float> (wx[(size_t) i] - bw * 0.5f, r.getY(), bw, bh);
        g.setColour (col (state[(size_t) i], true));
        g.fillRoundedRectangle (k, 2.5f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (k, 2.5f, 1.0f);
    }
}

} // namespace cc::ui
