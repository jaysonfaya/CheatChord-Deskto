#include "Layouts.h"

namespace cc::ui {

// The built-in layouts, in the same format as a layout file. Grid: 60 columns x 40 rows of the area under the header.
// Each one holds all 14 panels (complete() adds any that a file leaves out).
static const char* const BUILT_IN[] = {
R"JSON({
  "name": "Studio",
  "about": "Everything for writing at once: progression, ideas, browser, keys, the rest in tabs.",
  "size": [1600, 1000], "min": [1200, 760], "grid": [60, 40],
  "panels": [
    { "id": "progression", "at": [0, 0, 60, 10] },
    { "id": "ideas",       "at": [0, 10, 17, 16] },
    { "id": "browser",     "at": [17, 10, 25, 16] },
    { "tabs": ["history", "presets", "library", "profile"], "at": [42, 10, 18, 16] },
    { "id": "keys",        "at": [0, 26, 25, 14] },
    { "tabs": ["chord", "groove", "sound", "pads", "transport", "help"], "at": [25, 26, 35, 14] }
  ]
})JSON",
R"JSON({
  "name": "Wide",
  "about": "For big and ultrawide screens: almost every panel open at once.",
  "size": [2400, 1100], "min": [1900, 900], "grid": [60, 40],
  "panels": [
    { "id": "progression", "at": [0, 0, 42, 10] },
    { "id": "transport",   "at": [42, 0, 18, 10] },
    { "id": "ideas",       "at": [0, 10, 13, 16] },
    { "id": "browser",     "at": [13, 10, 21, 16] },
    { "id": "chord",       "at": [34, 10, 13, 16] },
    { "tabs": ["history", "presets", "library", "profile", "help"], "at": [47, 10, 13, 16] },
    { "id": "keys",        "at": [0, 26, 20, 14] },
    { "id": "groove",      "at": [20, 26, 20, 14] },
    { "id": "sound",       "at": [40, 26, 12, 14] },
    { "id": "pads",        "at": [52, 26, 8, 14] }
  ]
})JSON",
R"JSON({
  "name": "Compact",
  "about": "For laptops and small windows: the progression on top, everything else in tabs.",
  "size": [1200, 800], "min": [940, 640], "grid": [60, 40],
  "panels": [
    { "id": "progression", "at": [0, 0, 60, 13] },
    { "tabs": ["ideas", "browser", "keys", "chord", "groove", "sound", "pads", "history", "presets", "library", "profile", "transport", "help"], "at": [0, 13, 60, 27] }
  ]
})JSON",
R"JSON({
  "name": "Perform",
  "about": "Big targets for playing live: huge chords and ideas, big transport.",
  "size": [1600, 980], "min": [1200, 760], "grid": [60, 40],
  "panels": [
    { "id": "progression", "at": [0, 0, 60, 13] },
    { "id": "ideas",       "at": [0, 13, 38, 16] },
    { "id": "transport",   "at": [38, 13, 22, 16] },
    { "id": "keys",        "at": [0, 29, 34, 11] },
    { "tabs": ["sound", "history", "presets", "chord", "groove", "browser", "pads", "library", "profile", "help"], "at": [34, 29, 26, 11] }
  ]
})JSON",
R"JSON({
  "name": "Discover",
  "about": "For exploring styles, artists and producers: big lists and the profile library.",
  "size": [1600, 1000], "min": [1200, 760], "grid": [60, 40],
  "panels": [
    { "id": "browser",     "at": [0, 0, 38, 22] },
    { "tabs": ["profile", "library", "presets", "history"], "at": [38, 0, 22, 22] },
    { "id": "ideas",       "at": [0, 22, 22, 18] },
    { "id": "progression", "at": [22, 22, 38, 9] },
    { "tabs": ["keys", "chord", "groove", "sound", "pads", "transport", "help"], "at": [22, 31, 38, 9] }
  ]
})JSON",
};

bool LayoutDef::fromJson (const juce::var& v, LayoutDef& d, juce::String& err)
{
    if (! v.isObject()) { err = "not a JSON object"; return false; }
    d = {};
    d.name = v["name"].toString().trim();
    if (d.name.isEmpty()) { err = "no \"name\""; return false; }
    d.about = v["about"].toString();
    auto pair = [] (const juce::var& a, int& x, int& y)
    {
        if (a.isArray() && a.size() >= 2) { x = (int) a[0]; y = (int) a[1]; }
    };
    pair (v["size"], d.width, d.height);
    pair (v["min"], d.minWidth, d.minHeight);
    pair (v["grid"], d.cols, d.rows);
    d.width = juce::jlimit (600, 4000, d.width);
    d.height = juce::jlimit (400, 3000, d.height);
    d.minWidth = juce::jlimit (500, d.width, d.minWidth);
    d.minHeight = juce::jlimit (360, d.height, d.minHeight);
    d.cols = juce::jlimit (1, 200, d.cols);
    d.rows = juce::jlimit (1, 200, d.rows);
    auto* panels = v["panels"].getArray();
    if (panels == nullptr || panels->isEmpty()) { err = "no \"panels\""; return false; }
    juce::StringArray known;
    for (auto& p : panelList()) known.add (p.id);
    for (auto& p : *panels)
    {
        Item it;
        if (p["tabs"].isArray())
            for (auto& t : *p["tabs"].getArray()) it.ids.add (t.toString());
        else
            it.ids.add (p["id"].toString());
        for (auto& id : it.ids)
            if (! known.contains (id)) { err = "unknown panel \"" + id + "\" (known: " + known.joinIntoString (", ") + ")"; return false; }
        auto at = p["at"];
        if (! at.isArray() || at.size() < 4) { err = "panel " + it.ids[0] + ": \"at\" needs [x, y, w, h]"; return false; }
        it.x = juce::jlimit (0, d.cols - 1, (int) at[0]);
        it.y = juce::jlimit (0, d.rows - 1, (int) at[1]);
        it.w = juce::jlimit (1, d.cols - it.x, (int) at[2]);
        it.h = juce::jlimit (1, d.rows - it.y, (int) at[3]);
        d.items.push_back (it);
    }
    d.complete();
    return true;
}

void LayoutDef::complete()
{
    // a panel listed twice keeps its first place
    juce::StringArray seen;
    for (auto& it : items)
    {
        juce::StringArray keep;
        for (auto& id : it.ids) if (! seen.contains (id)) { keep.add (id); seen.add (id); }
        it.ids = keep;
    }
    items.erase (std::remove_if (items.begin(), items.end(), [] (const Item& i) { return i.ids.isEmpty(); }), items.end());
    juce::StringArray missing;
    for (auto& p : panelList()) if (! seen.contains (p.id)) missing.add (p.id);
    if (missing.isEmpty()) return;
    if (items.empty()) { items.push_back ({ missing, 0, 0, cols, rows }); return; }
    int target = -1;
    for (int i = (int) items.size() - 1; i >= 0 && target < 0; --i) if (items[(size_t) i].ids.size() > 1) target = i;
    if (target < 0)
    {
        int best = 0;
        for (int i = 1; i < (int) items.size(); ++i)
            if (items[(size_t) i].w * items[(size_t) i].h > items[(size_t) best].w * items[(size_t) best].h) best = i;
        target = best;
    }
    items[(size_t) target].ids.addArray (missing);
}

juce::var LayoutDef::toJson() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", name);
    o->setProperty ("about", about);
    o->setProperty ("size", juce::Array<juce::var> { width, height });
    o->setProperty ("min", juce::Array<juce::var> { minWidth, minHeight });
    o->setProperty ("grid", juce::Array<juce::var> { cols, rows });
    juce::Array<juce::var> ps;
    for (auto& it : items)
    {
        auto* p = new juce::DynamicObject();
        if (it.ids.size() == 1) p->setProperty ("id", it.ids[0]);
        else { juce::Array<juce::var> t; for (auto& s : it.ids) t.add (s); p->setProperty ("tabs", t); }
        p->setProperty ("at", juce::Array<juce::var> { it.x, it.y, it.w, it.h });
        ps.add (juce::var (p));
    }
    o->setProperty ("panels", ps);
    return juce::var (o);
}

std::vector<LayoutDef> builtInLayouts()
{
    static std::vector<LayoutDef> v;
    if (v.empty())
        for (auto* j : BUILT_IN)
        {
            LayoutDef d;
            juce::String err;
            if (LayoutDef::fromJson (juce::JSON::parse (j), d, err)) v.push_back (d);
            else jassertfalse;
        }
    return v;
}

juce::File layoutsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("JJ DA PLUG").getChildFile ("CheatChord").getChildFile ("Layouts");
}

std::vector<LayoutDef> userLayouts (juce::StringArray& errors)
{
    std::vector<LayoutDef> v;
    auto dir = layoutsFolder();
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        LayoutDef d;
        juce::String err;
        auto parsed = juce::JSON::parse (f.loadFileAsString());
        if (LayoutDef::fromJson (parsed, d, err))
        {
            d.builtIn = false;
            d.file = f;
            v.push_back (d);
        }
        else errors.add (f.getFileName() + ": " + err);
    }
    std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.name.compareNatural (b.name) < 0; });
    return v;
}

void writeExampleLayouts()
{
    auto ex = layoutsFolder().getChildFile ("Examples");
    ex.createDirectory();
    for (auto& d : builtInLayouts())
        ex.getChildFile (d.name + ".json").replaceWithText (juce::JSON::toString (d.toJson(), false));
    juce::String ids;
    for (auto& p : panelList()) ids << "  " << juce::String (p.id).paddedRight (' ', 13) << p.title << "\n";
    layoutsFolder().getChildFile ("README.txt").replaceWithText (
        "CheatChord layouts\n"
        "==================\n\n"
        "The easy way: EDIT LAYOUT on top of CheatChord. Drag a panel to move it, its corner to resize it, drop it on another\n"
        "panel to put them together as tabs. The menu button of a panel splits tabs apart again. SAVE AS FILE writes it here.\n\n"
        "Every .json file in this folder is a layout in the LAYOUT menu (RELOAD LAYOUT FILES after a change).\n"
        "  \"size\": [width, height]      the window at 100 % zoom\n"
        "  \"min\":  [width, height]      the smallest window\n"
        "  \"grid\": [columns, rows]      the area under the header, cut into cells\n"
        "  \"panels\": a list of { \"id\": ..., \"at\": [x, y, w, h] } in cells,\n"
        "             or { \"tabs\": [id, id, ...], \"at\": [...] } for several panels in one place.\n"
        "A panel a layout leaves out is added as a tab, so every layout reaches everything.\n\n"
        "Panels:\n" + ids);
}

// ================================================================ placeholder for a panel that is open as a window
class FloatingPlaceholder : public juce::Component
{
public:
    FloatingPlaceholder (const juce::String& id, LayoutHost& h) : panelId (id), host (h)
    {
        for (auto& p : panelList()) if (id == p.id) title = p.title;
        setRepaintsOnMouseActivity (true);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (20.0f);
        auto c = panelColour (panelId);
        juce::Path p;
        p.addRoundedRectangle (r, 10.0f);
        juce::Path d;
        const float dash[] = { 8.0f, 6.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (d, p, dash, 2);
        g.setColour (c.withAlpha (isMouseOver() ? 0.9f : 0.5f));
        g.fillPath (d);
        auto t = r.reduced (20.0f);
        fitText (g, title.toUpperCase() + " IS OPEN IN ITS OWN WINDOW", t.removeFromTop (t.getHeight() * 0.55f), 20.0f, 2, c,
                 juce::Justification::centredBottom, 10.0f);
        fitText (g, "Click here to put it back", t, 16.0f, 1, pal().dim, juce::Justification::centredTop, 10.0f);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked()) host.dock (panelId); }
private:
    juce::String panelId, title;
    LayoutHost& host;
};

// ================================================================ TabGroup
TabGroup::TabGroup (Ctx& c, LayoutHost& h, int it, const juce::StringArray& i, juce::ValueTree st, const juce::String& key)
    : Panel ({}), ctx (c), host (h), item (it), ids (i), state (st), stateKey (key)
{
    for (auto& id : ids)
    {
        juce::String n = id;
        for (auto& p : panelList()) if (id == p.id) n = p.title;
        names.add (n);
    }
    panels.resize ((size_t) ids.size());
    setRepaintsOnMouseActivity (true);
    show (juce::jlimit (0, ids.size() - 1, (int) state.getProperty (stateKey, 0)));
}

void TabGroup::show (int i)
{
    cur = juce::jlimit (0, ids.size() - 1, i);
    auto& slot = panels[(size_t) cur];
    const bool floating = host.isFloating (ids[cur]);
    if (slot == nullptr || (floating != (dynamic_cast<FloatingPlaceholder*> (slot.get()) != nullptr)))
    {
        if (floating) slot = std::make_unique<FloatingPlaceholder> (ids[cur], host);
        else
        {
            auto p = makePanel (ids[cur], ctx);
            if (p != nullptr) { p->frame = false; p->title = {}; }
            slot = std::move (p);
        }
        if (slot != nullptr) addChildComponent (*slot);
    }
    for (size_t k = 0; k < panels.size(); ++k)
        if (panels[k] != nullptr) panels[k]->setVisible ((int) k == cur);
    state.setProperty (stateKey, cur, nullptr);
    resized();
    repaint();
    if (ctx.changed) ctx.changed();
}

void TabGroup::refreshFloating()
{
    for (size_t k = 0; k < panels.size(); ++k)
        if (panels[k] != nullptr)
        {
            const bool fl = host.isFloating (ids[(int) k]);
            const bool isPh = dynamic_cast<FloatingPlaceholder*> (panels[k].get()) != nullptr;
            if (fl != isPh) panels[k] = nullptr;
        }
    show (cur);
}

juce::Rectangle<float> TabGroup::popBtn() const
{
    const float h = (float) U (32);
    return { (float) getWidth() - h - 8.0f, 5.0f, h, h };
}

juce::Rectangle<float> TabGroup::tab (int i) const
{
    auto r = getLocalBounds().toFloat().reduced (8.0f, 0).withTrimmedTop (5.0f).withHeight ((float) U (32));
    r.removeFromRight ((float) U (32) + 6.0f);
    const int n = juce::jmax (1, ids.size());
    // tabs share the width; very many tabs go to two rows
    const int rowsN = r.getWidth() / n < Uf (78.0f) ? 2 : 1;
    const int perRow = (n + rowsN - 1) / rowsN;
    const float w = juce::jmin (Uf (170.0f), r.getWidth() / (float) perRow);
    const int row = i / perRow, col = i % perRow;
    return { r.getX() + col * w, r.getY() + row * (r.getHeight() + 4.0f), w - 4.0f, r.getHeight() };
}

int TabGroup::tabAt (juce::Point<float> p) const
{
    for (int i = 0; i < ids.size(); ++i) if (tab (i).contains (p)) return i;
    return -1;
}

void TabGroup::resized()
{
    const int rowsN = ids.size() > 0 && tab (ids.size() - 1).getY() > tab (0).getY() ? 2 : 1;
    const int top = 5 + rowsN * (U (32) + 4) + 4;
    if (auto* p = cur < (int) panels.size() ? panels[(size_t) cur].get() : nullptr)
        p->setBounds (getLocalBounds().withTrimmedTop (top));
}

void TabGroup::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const auto active = panelColour (ids[cur]);
    g.setColour (p.panel);
    g.fillRoundedRectangle (r, 8.0f);
    for (int i = 0; i < ids.size(); ++i)
    {
        auto t = tab (i);
        const bool s = i == cur;
        const auto c = panelColour (ids[i]);
        // every tab in its own colour: filled when chosen, tinted otherwise
        g.setColour (s ? c : c.withAlpha (i == hover ? 0.32f : 0.16f));
        g.fillRoundedRectangle (t, 6.0f);
        g.setColour (c.withAlpha (s ? 1.0f : 0.55f));
        g.drawRoundedRectangle (t.reduced (0.5f), 6.0f, 1.0f);
        const auto fg = s ? (theme() == 2 ? juce::Colours::white : juce::Colour (0xff141014)) : c;
        fitText (g, names[i].toUpperCase(), t.reduced (6.0f, 0), 16.0f, 2, fg, juce::Justification::centred, 9.0f);
        if (host.isFloating (ids[i]))
            g.fillEllipse (t.getRight() - 9.0f, t.getY() + 4.0f, 5.0f, 5.0f);
    }
    // pop-out button for the shown tab
    auto b = popBtn();
    const bool hot = b.contains (getMouseXYRelative().toFloat()) && isMouseOver();
    g.setColour (active.withAlpha (hot ? 0.3f : 0.12f));
    g.fillRoundedRectangle (b, 5.0f);
    auto ic = b.reduced (b.getWidth() * 0.26f);
    g.setColour (active);
    g.drawRect (ic.withTrimmedTop (ic.getHeight() * 0.3f).withTrimmedRight (ic.getWidth() * 0.3f), 1.6f);
    juce::Path a;
    a.startNewSubPath (ic.getCentreX(), ic.getCentreY());
    a.lineTo (ic.getRight(), ic.getY());
    a.startNewSubPath (ic.getRight() - ic.getWidth() * 0.4f, ic.getY());
    a.lineTo (ic.getRight(), ic.getY());
    a.lineTo (ic.getRight(), ic.getY() + ic.getHeight() * 0.4f);
    g.strokePath (a, juce::PathStrokeType (1.6f));
    // the frame in the colour of the shown panel
    g.setColour (active.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 8.0f, 1.4f);
    const int rowsN = ids.size() > 0 && tab (ids.size() - 1).getY() > tab (0).getY() ? 2 : 1;
    g.fillRect (8.0f, 5.0f + rowsN * ((float) U (32) + 4.0f), (float) getWidth() - 16.0f, 2.0f);
}

void TabGroup::mouseDown (const juce::MouseEvent& e)
{
    if (popBtn().contains (e.position)) { host.popOut (ids[cur]); return; }
    const int i = tabAt (e.position);
    if (i < 0) return;
    if (e.mods.isPopupMenu()) { host.itemMenu (item, ids[i], this); return; }
    show (i);
}

void TabGroup::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = tabAt (e.position);
    if (i >= 0) host.popOut (ids[i]);
}

void TabGroup::mouseMove (const juce::MouseEvent& e)
{
    const int h = tabAt (e.position);
    if (h != hover) { hover = h; repaint(); }
    if (popBtn().contains (e.position))
        ctx.help ("Open as window", "Opens " + names[cur] + " in its own small window you can move and resize, so you see it next to the other tabs.");
    else if (h >= 0)
        for (auto& p : panelList())
            if (ids[h] == p.id)
                ctx.help (names[h] + " (tab)", juce::String (p.about) + "\n\nDouble-click the tab: own window. Right-click: move it elsewhere.");
}

// ================================================================ LayoutView with its edit frames
class LayoutView::EditFrame : public juce::Component
{
public:
    EditFrame (LayoutView& v, int i) : view (v), item (i) { setRepaintsOnMouseActivity (true); }
    void paint (juce::Graphics& g) override
    {
        auto& it = view.def.items[(size_t) item];
        const auto c = panelColour (it.ids[0]);
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (pal().bg.withAlpha (dragging ? 0.92f : 0.84f));
        g.fillRoundedRectangle (r, 8.0f);
        juce::Path p;
        p.addRoundedRectangle (r, 8.0f);
        juce::Path d;
        const float dash[] = { 10.0f, 6.0f };
        juce::PathStrokeType (2.5f).createDashedStroke (d, p, dash, 2);
        g.setColour (dropTarget ? pal().good : c);
        g.fillPath (d);
        juce::StringArray names;
        for (auto& id : it.ids) for (auto& pi : panelList()) if (id == pi.id) names.add (pi.title);
        auto t = r.reduced (16.0f);
        fitText (g, names.joinIntoString ("  +  ").toUpperCase(), t.removeFromTop (juce::jmin (Uf (34.0f), t.getHeight() * 0.4f)), 22.0f, 2, c,
                 juce::Justification::centred, 10.0f);
        fitText (g, dropTarget ? "DROP: TOGETHER AS TABS" : "DRAG TO MOVE  -  CORNER TO RESIZE  -  RIGHT-CLICK FOR MORE", t.removeFromTop (Uf (26.0f)),
                 15.0f, 1, pal().text, juce::Justification::centred, 9.0f);
        // the menu and resize handles
        auto m = menuBtn().toFloat();
        g.setColour (c.withAlpha (isMouseOver() ? 0.9f : 0.6f));
        g.fillRoundedRectangle (m, 6.0f);
        fitText (g, "MENU", m, 15.0f, 2, juce::Colour (0xff141014), juce::Justification::centred);
        auto rz = grip().toFloat();
        for (int k = 0; k < 3; ++k)
            g.drawLine (rz.getRight() - 4.0f - k * 6.0f, rz.getBottom() - 2.0f, rz.getRight() - 2.0f, rz.getBottom() - 4.0f - k * 6.0f, 2.0f);
    }
    juce::Rectangle<int> menuBtn() const { return getLocalBounds().reduced (10).removeFromTop (U (30)).removeFromRight (U (70)); }
    juce::Rectangle<int> grip() const { return getLocalBounds().removeFromBottom (28).removeFromRight (28); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || menuBtn().contains (e.getPosition()))
        {
            view.host.itemMenu (item, view.def.items[(size_t) item].ids[0], this);
            return;
        }
        mode = grip().contains (e.getPosition()) ? 2 : 1;
        start = getBounds();
        dragging = true;
        toFront (false);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (mode == 0) return;
        auto off = e.getOffsetFromDragStart();
        if (mode == 1) setBounds (start + off);
        else setBounds (start.withSize (juce::jmax (120, start.getWidth() + off.x), juce::jmax (90, start.getHeight() + off.y)));
        // over another panel's middle: they become tabs
        const int other = view.itemAt (getBounds().getCentre());
        for (auto& f : view.frames) f->setDrop (false);
        if (mode == 1 && other >= 0 && other != item)
        {
            auto ob = view.cellRect (view.def.items[(size_t) other]);
            if (ob.reduced (ob.getWidth() / 4, ob.getHeight() / 4).contains (e.getEventRelativeTo (&view).getPosition()))
                view.frames[(size_t) other]->setDrop (true);
        }
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (mode == 0) return;
        dragging = false;
        const int m = mode;
        mode = 0;
        int into = -1;
        for (size_t k = 0; k < view.frames.size(); ++k) if (view.frames[k]->drop) into = (int) k;
        for (auto& f : view.frames) f->setDrop (false);
        if (m == 1 && into >= 0) { view.host.mergeItem (item, into); return; }
        if (e.getDistanceFromDragStart() < 3) { setBounds (start); return; }
        view.host.moveItem (item, view.cellsAt (getBounds()));
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (grip().contains (e.getPosition()) ? juce::MouseCursor::BottomRightCornerResizeCursor
                        : menuBtn().contains (e.getPosition()) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::DraggingHandCursor);
    }
    void setDrop (bool d) { if (d != drop) { drop = dropTarget = d; repaint(); } }
    bool drop = false;
private:
    LayoutView& view;
    int item, mode = 0;
    bool dragging = false, dropTarget = false;
    juce::Rectangle<int> start;
};

LayoutView::LayoutView (Ctx& c, LayoutHost& h, const LayoutDef& d, juce::ValueTree uiState) : ctx (c), host (h), def (d)
{
    def.complete();
    for (size_t i = 0; i < def.items.size(); ++i)
    {
        auto& it = def.items[i];
        std::unique_ptr<juce::Component> comp;
        if (it.ids.size() > 1)
            comp = std::make_unique<TabGroup> (c, h, (int) i, it.ids, uiState,
                                               "tab_" + def.name.removeCharacters (" ()") + "_" + it.ids[0] + "_" + juce::String (it.ids.size()));
        else if (h.isFloating (it.ids[0]))
            comp = std::make_unique<FloatingPlaceholder> (it.ids[0], h);
        else
            comp = makePanel (it.ids[0], c);
        if (comp != nullptr) addAndMakeVisible (*comp);
        parts.push_back (std::move (comp));
        frames.push_back (std::make_unique<EditFrame> (*this, (int) i));
        addChildComponent (*frames.back());
    }
    setEditing (h.editing());
}

LayoutView::~LayoutView() = default;

void LayoutView::setEditing (bool e)
{
    for (auto& f : frames) { f->setVisible (e); if (e) f->toFront (false); }
    resized();
    repaint();
}

void LayoutView::refreshFloating()
{
    for (size_t i = 0; i < parts.size(); ++i)
    {
        auto& it = def.items[i];
        if (auto* tg = dynamic_cast<TabGroup*> (parts[i].get())) { tg->refreshFloating(); continue; }
        const bool fl = host.isFloating (it.ids[0]);
        const bool isPh = dynamic_cast<FloatingPlaceholder*> (parts[i].get()) != nullptr;
        if (fl == isPh) continue;
        if (fl) parts[i] = std::make_unique<FloatingPlaceholder> (it.ids[0], host);
        else parts[i] = makePanel (it.ids[0], ctx);
        if (parts[i] != nullptr) addAndMakeVisible (*parts[i]);
    }
    for (auto& f : frames) f->toFront (false);
    resized();
    if (ctx.changed) ctx.changed();
}

juce::Rectangle<int> LayoutView::cellRect (const LayoutDef::Item& it) const
{
    const float gap = 8.0f;
    auto r = getLocalBounds().toFloat();
    const float cw = (r.getWidth() + gap) / (float) def.cols, rh = (r.getHeight() + gap) / (float) def.rows;
    return juce::Rectangle<float> (r.getX() + it.x * cw, r.getY() + it.y * rh, it.w * cw - gap, it.h * rh - gap).toNearestInt();
}

juce::Rectangle<int> LayoutView::cellsAt (juce::Rectangle<int> px) const
{
    const float gap = 8.0f;
    auto r = getLocalBounds().toFloat();
    const float cw = (r.getWidth() + gap) / (float) def.cols, rh = (r.getHeight() + gap) / (float) def.rows;
    int x = juce::roundToInt (px.getX() / cw), y = juce::roundToInt (px.getY() / rh);
    int w = juce::roundToInt ((px.getWidth() + gap) / cw), h = juce::roundToInt ((px.getHeight() + gap) / rh);
    x = juce::jlimit (0, def.cols - 1, x);
    y = juce::jlimit (0, def.rows - 1, y);
    w = juce::jlimit (4, def.cols - x, w);
    h = juce::jlimit (4, def.rows - y, h);
    return { x, y, w, h };
}

int LayoutView::itemAt (juce::Point<int> p) const
{
    for (size_t i = 0; i < def.items.size(); ++i)
        if (cellRect (def.items[i]).contains (p)) return (int) i;
    return -1;
}

void LayoutView::resized()
{
    for (size_t i = 0; i < parts.size(); ++i)
    {
        auto b = cellRect (def.items[i]);
        if (parts[i] != nullptr) parts[i]->setBounds (b);
        frames[i]->setBounds (b);
    }
}

void LayoutView::paintOverChildren (juce::Graphics& g)
{
    if (! host.editing()) return;
    // overlapping places: a red outline so it is obvious
    for (size_t i = 0; i < def.items.size(); ++i)
        for (size_t k = i + 1; k < def.items.size(); ++k)
        {
            auto a = cellRect (def.items[i]), b = cellRect (def.items[k]);
            auto o = a.getIntersection (b);
            if (! o.isEmpty())
            {
                g.setColour (pal().bad.withAlpha (0.35f));
                g.fillRect (o);
            }
        }
}

// ================================================================ FloatPanel
FloatPanel::FloatPanel (Ctx& c, const juce::String& i, std::function<void()> d, std::function<void()> m)
    : id (i), ctx (c), onDock (std::move (d)), onMoved (std::move (m))
{
    panel = makePanel (id, ctx);
    if (panel != nullptr)
    {
        panel->frame = false;
        panel->title = {};
        addAndMakeVisible (*panel);
    }
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<int> FloatPanel::dockBtn() const
{
    auto t = titleBar().reduced (6, 4);
    return t.removeFromRight (U (150));
}

void FloatPanel::paint (juce::Graphics& g)
{
    const auto c = panelColour (id);
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0, 4.0f), 10.0f);
    g.setColour (pal().panel);
    g.fillRoundedRectangle (r.reduced (0.5f), 10.0f);
    auto t = titleBar().toFloat();
    g.setColour (c.withAlpha (0.9f));
    g.fillRoundedRectangle (t, 10.0f);
    g.fillRect (t.withTrimmedTop (10.0f));
    juce::String title = id;
    for (auto& p : panelList()) if (id == p.id) title = p.title;
    const auto fg = theme() == 2 ? juce::Colours::white : juce::Colour (0xff141014);
    fitText (g, title.toUpperCase(), t.reduced (12.0f, 0).withTrimmedRight ((float) U (160)), 18.0f, 2, fg, juce::Justification::centredLeft);
    auto b = dockBtn().toFloat();
    const bool hot = b.contains (getMouseXYRelative().toFloat()) && isMouseOver();
    g.setColour (fg.withAlpha (hot ? 0.3f : 0.15f));
    g.fillRoundedRectangle (b, 6.0f);
    fitText (g, "BACK INTO LAYOUT", b, 14.0f, 2, fg, juce::Justification::centred, 9.0f);
    g.setColour (c);
    g.drawRoundedRectangle (r.reduced (1.0f), 10.0f, 2.0f);
    auto gr = grip().toFloat();
    for (int k = 0; k < 3; ++k)
        g.drawLine (gr.getRight() - 4.0f - k * 5.0f, gr.getBottom() - 2.0f, gr.getRight() - 2.0f, gr.getBottom() - 4.0f - k * 5.0f, 1.5f);
}

void FloatPanel::resized()
{
    if (panel != nullptr) panel->setBounds (getLocalBounds().withTrimmedTop (U (32)).reduced (2, 0).withTrimmedBottom (14));
}

void FloatPanel::mouseDown (const juce::MouseEvent& e)
{
    toFront (false);
    if (dockBtn().contains (e.getPosition())) { mode = 0; if (onDock) onDock(); return; }
    if (grip().contains (e.getPosition())) { mode = 2; startBounds = getBounds(); return; }
    if (titleBar().contains (e.getPosition())) { mode = 1; dragger.startDraggingComponent (this, e); return; }
    mode = 0;
}

void FloatPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (mode == 1)
    {
        dragger.dragComponent (this, e, nullptr);
        if (auto* p = getParentComponent())   // keep the title bar reachable
            setTopLeftPosition (juce::jlimit (-getWidth() + 120, p->getWidth() - 120, getX()), juce::jlimit (0, p->getHeight() - U (32), getY()));
    }
    else if (mode == 2)
    {
        auto off = e.getOffsetFromDragStart();
        setSize (juce::jmax (320, startBounds.getWidth() + off.x), juce::jmax (220, startBounds.getHeight() + off.y));
    }
}

void FloatPanel::mouseUp (const juce::MouseEvent&)
{
    if (mode != 0 && onMoved) onMoved();
    mode = 0;
}

void FloatPanel::mouseMove (const juce::MouseEvent& e)
{
    repaint (titleBar());
    setMouseCursor (grip().contains (e.getPosition()) ? juce::MouseCursor::BottomRightCornerResizeCursor
                    : titleBar().contains (e.getPosition()) && ! dockBtn().contains (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor
                                                                                                     : juce::MouseCursor::NormalCursor);
    if (dockBtn().contains (e.getPosition())) ctx.help ("Back into layout", "Closes this window and shows the panel in its place in the layout again.");
    else if (titleBar().contains (e.getPosition())) ctx.help ("Panel window", "Drag the title bar to move it, the corner at the bottom right to resize it.");
}

// ================================================================ HelperBox
HelperBox::HelperBox (std::function<void()> c, std::function<void()> m) : onClose (std::move (c)), onMoved (std::move (m))
{
    setRepaintsOnMouseActivity (true);
    setInterceptsMouseClicks (true, false);
}

void HelperBox::set (const juce::String& t, const juce::String& x)
{
    if (t == title && x == text) return;
    title = t;
    text = x;
    repaint();
}

juce::Rectangle<int> HelperBox::closeBtn() const { return getLocalBounds().removeFromTop (U (28)).removeFromRight (U (28)).reduced (4); }

int HelperBox::preferredHeight (int width) const
{
    juce::ignoreUnused (width);
    return U (132);
}

void HelperBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0, 3.0f), 10.0f);
    g.setColour (pal().panelHi.withAlpha (0.97f));
    g.fillRoundedRectangle (r.reduced (0.5f), 10.0f);
    g.setColour (pal().good);
    g.drawRoundedRectangle (r.reduced (1.0f), 10.0f, 2.0f);
    auto head = r.removeFromTop ((float) U (28)).reduced (12.0f, 0);
    fitText (g, "HELPER", head, 14.0f, 2, pal().good, juce::Justification::centredLeft);
    auto cb = closeBtn().toFloat();
    g.setColour (pal().dim.withAlpha (isMouseOver() && cb.contains (getMouseXYRelative().toFloat()) ? 1.0f : 0.6f));
    g.drawLine (cb.getX() + 4, cb.getY() + 4, cb.getRight() - 4, cb.getBottom() - 4, 2.0f);
    g.drawLine (cb.getRight() - 4, cb.getY() + 4, cb.getX() + 4, cb.getBottom() - 4, 2.0f);
    auto body = r.reduced (14.0f, 8.0f);
    if (title.isEmpty() && text.isEmpty())
    {
        fitText (g, "Point at anything and I explain it. Drag me anywhere; HELPER on top switches me off.",
                 body, 15.0f, 0, pal().dim, juce::Justification::topLeft, 10.0f, 4);
        return;
    }
    if (title.isNotEmpty())
        fitText (g, title.toUpperCase(), body.removeFromTop (Uf (26.0f)), 18.0f, 2, pal().gold, juce::Justification::centredLeft, 10.0f);
    fitText (g, text, body, 15.0f, 0, pal().text, juce::Justification::topLeft, 9.0f, 6);
}

void HelperBox::mouseDown (const juce::MouseEvent& e)
{
    if (closeBtn().contains (e.getPosition())) { if (onClose) onClose(); return; }
    dragger.startDraggingComponent (this, e);
}

void HelperBox::mouseDrag (const juce::MouseEvent& e)
{
    dragger.dragComponent (this, e, nullptr);
    if (auto* p = getParentComponent())
        setTopLeftPosition (juce::jlimit (0, juce::jmax (0, p->getWidth() - getWidth()), getX()), juce::jlimit (0, juce::jmax (0, p->getHeight() - getHeight()), getY()));
}

void HelperBox::mouseUp (const juce::MouseEvent&) { if (onMoved) onMoved(); }

} // namespace cc::ui
