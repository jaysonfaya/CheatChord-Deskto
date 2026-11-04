#include "Layouts.h"

namespace cc::ui {

// The built-in layouts, in the same format as a layout file. Grid: 60 columns x 40 rows of the area under the header.
static const char* const BUILT_IN[] = {
R"JSON({
  "name": "Studio",
  "about": "Everything for writing at once: progression, ideas, browser, keys, and the rest in tabs.",
  "size": [1560, 980], "min": [1200, 760], "grid": [60, 40],
  "panels": [
    { "id": "progression", "at": [0, 0, 60, 9] },
    { "id": "ideas",       "at": [0, 9, 17, 17] },
    { "id": "browser",     "at": [17, 9, 25, 17] },
    { "tabs": ["history", "presets", "library", "profile"], "at": [42, 9, 18, 17] },
    { "id": "keys",        "at": [0, 26, 26, 14] },
    { "tabs": ["chord", "groove", "sound", "pads", "help"], "at": [26, 26, 34, 14] }
  ]
})JSON",
R"JSON({
  "name": "Wide",
  "about": "For big and ultrawide screens: every panel open, no tabs except the lists.",
  "size": [2400, 1080], "min": [1900, 900], "grid": [60, 40],
  "panels": [
    { "id": "progression", "at": [0, 0, 42, 9] },
    { "id": "transport",   "at": [42, 0, 18, 9] },
    { "id": "ideas",       "at": [0, 9, 13, 17] },
    { "id": "browser",     "at": [13, 9, 21, 17] },
    { "id": "chord",       "at": [34, 9, 13, 17] },
    { "tabs": ["history", "presets", "library", "profile", "help"], "at": [47, 9, 13, 17] },
    { "id": "keys",        "at": [0, 26, 20, 14] },
    { "id": "groove",      "at": [20, 26, 20, 14] },
    { "id": "sound",       "at": [40, 26, 12, 14] },
    { "id": "pads",        "at": [52, 26, 8, 14] }
  ]
})JSON",
R"JSON({
  "name": "Compact",
  "about": "For laptops and small windows: the progression on top, everything else in tabs.",
  "size": [1180, 760], "min": [940, 620], "grid": [60, 40],
  "panels": [
    { "id": "progression", "at": [0, 0, 60, 12] },
    { "tabs": ["ideas", "browser", "keys", "chord", "groove", "sound", "pads", "history", "presets", "library", "profile", "help"], "at": [0, 12, 60, 28] }
  ]
})JSON",
R"JSON({
  "name": "Perform",
  "about": "Big targets for playing live: huge chords and ideas, big transport.",
  "size": [1560, 960], "min": [1200, 760], "grid": [60, 40],
  "panels": [
    { "id": "progression", "at": [0, 0, 60, 13] },
    { "id": "ideas",       "at": [0, 13, 38, 17] },
    { "id": "transport",   "at": [38, 13, 22, 17] },
    { "id": "keys",        "at": [0, 30, 38, 10] },
    { "tabs": ["history", "presets", "sound"], "at": [38, 30, 22, 10] }
  ]
})JSON",
R"JSON({
  "name": "Discover",
  "about": "For exploring styles, artists and producers: big lists and the profile library.",
  "size": [1560, 980], "min": [1200, 760], "grid": [60, 40],
  "panels": [
    { "id": "browser",     "at": [0, 0, 38, 22] },
    { "tabs": ["profile", "library"], "at": [38, 0, 22, 22] },
    { "id": "ideas",       "at": [0, 22, 22, 18] },
    { "id": "progression", "at": [22, 22, 38, 9] },
    { "id": "keys",        "at": [22, 31, 38, 9] }
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
    return true;
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
    if (ex.isDirectory()) return;
    ex.createDirectory();
    for (auto& d : builtInLayouts())
        ex.getChildFile (d.name + ".json").replaceWithText (juce::JSON::toString (d.toJson(), false));
    juce::String ids;
    for (auto& p : panelList()) ids << "  " << juce::String (p.id).paddedRight (' ', 13) << p.about << "\n";
    layoutsFolder().getChildFile ("README.txt").replaceWithText (
        "CheatChord layouts\n"
        "==================\n\n"
        "Every .json file in this folder is a layout. It appears in CheatChord's LAYOUT menu (RELOAD LAYOUT FILES after a change).\n"
        "Start from a copy of a file in Examples, give it its own \"name\", and move the panels.\n\n"
        "  \"size\": [width, height]      the window at 100 % zoom\n"
        "  \"min\":  [width, height]      the smallest window\n"
        "  \"grid\": [columns, rows]      the area under the header, cut into cells\n"
        "  \"panels\": a list of { \"id\": ..., \"at\": [x, y, w, h] } in cells,\n"
        "             or { \"tabs\": [id, id, ...], \"at\": [...] } for several panels in one place.\n\n"
        "Panels:\n" + ids);
}

// ================================================================ TabGroup
TabGroup::TabGroup (Ctx& c, const juce::StringArray& i, juce::ValueTree st, const juce::String& key)
    : Panel ({}), ctx (c), ids (i), state (st), stateKey (key)
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
    if (panels[(size_t) cur] == nullptr)
    {
        panels[(size_t) cur] = makePanel (ids[cur], ctx);
        if (auto* p = panels[(size_t) cur].get())
        {
            p->frame = false;
            p->title = {};
            addChildComponent (*p);
        }
    }
    for (size_t k = 0; k < panels.size(); ++k)
        if (panels[k] != nullptr) panels[k]->setVisible ((int) k == cur);
    state.setProperty (stateKey, cur, nullptr);
    resized();
    repaint();
    if (ctx.changed) ctx.changed();
}

juce::Rectangle<float> TabGroup::tab (int i) const
{
    auto r = getLocalBounds().toFloat().reduced (8.0f, 0).withTrimmedTop (6.0f).withHeight (28.0f);
    const float w = juce::jmin (150.0f, r.getWidth() / (float) juce::jmax (1, ids.size()));
    return { r.getX() + i * w, r.getY(), w - 4.0f, r.getHeight() };
}

void TabGroup::resized()
{
    if (auto* p = cur < (int) panels.size() ? panels[(size_t) cur].get() : nullptr)
        p->setBounds (getLocalBounds().withTrimmedTop (30));
}

void TabGroup::paint (juce::Graphics& g)
{
    auto& p = pal();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (p.panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (p.border);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
    for (int i = 0; i < ids.size(); ++i)
    {
        auto t = tab (i);
        const bool s = i == cur;
        if (s || i == hover)
        {
            g.setColour (s ? p.tileOn : p.tileHover);
            g.fillRoundedRectangle (t, 5.0f);
        }
        fitText (g, names[i].toUpperCase(), t.reduced (6.0f, 0), 15.0f, 2, s ? p.tileOnText : p.gold, juce::Justification::centred, 8.0f);
    }
    g.setColour (p.goldDim.withAlpha (0.45f));
    g.fillRect (8.0f, 35.0f, (float) getWidth() - 16.0f, 1.0f);
}

void TabGroup::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < ids.size(); ++i)
        if (tab (i).contains (e.position)) { show (i); return; }
}

void TabGroup::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < ids.size(); ++i) if (tab (i).contains (e.position)) h = i;
    if (h != hover) { hover = h; repaint(); }
}

// ================================================================ LayoutView
LayoutView::LayoutView (Ctx& c, const LayoutDef& d, juce::ValueTree uiState) : def (d)
{
    for (auto& it : def.items)
    {
        std::unique_ptr<juce::Component> comp;
        if (it.ids.size() > 1)
            comp = std::make_unique<TabGroup> (c, it.ids, uiState,
                                               "tab_" + def.name.removeCharacters (" ") + "_" + juce::String (it.x) + "_" + juce::String (it.y));
        else
            comp = makePanel (it.ids[0], c);
        if (comp != nullptr) addAndMakeVisible (*comp);
        parts.push_back (std::move (comp));
    }
}

void LayoutView::resized()
{
    const float gap = 8.0f;
    auto r = getLocalBounds().toFloat();
    const float cw = (r.getWidth() + gap) / (float) def.cols, rh = (r.getHeight() + gap) / (float) def.rows;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (parts[i] == nullptr) continue;
        auto& it = def.items[i];
        juce::Rectangle<float> b (r.getX() + it.x * cw, r.getY() + it.y * rh, it.w * cw - gap, it.h * rh - gap);
        parts[i]->setBounds (b.toNearestInt());
    }
}

} // namespace cc::ui
