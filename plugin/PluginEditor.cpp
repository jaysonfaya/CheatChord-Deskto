#include "PluginEditor.h"

namespace cc {

static constexpr int MARGIN = 8;

static juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "CheatChord";
    o.folderName = "JJ DA PLUG";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

// ================================================================ the bar shown while editing the layout
class Editor::EditBar : public juce::Component
{
public:
    explicit EditBar (Editor& e) : ed (e)
    {
        done.set ("DONE");
        reset.set ("RESET LAYOUT");
        save.set ("SAVE AS FILE");
        done.onClick = [this] (const juce::MouseEvent&) { ed.setEditing (false); };
        reset.onClick = [this] (const juce::MouseEvent&)
        {
            if (auto* d = ed.findLayout (ed.baseName)) { ed.work = *d; ed.edited = false; ed.saveWork(); ed.rebuildView(); }
        };
        save.onClick = [this] (const juce::MouseEvent&)
        {
            auto copy = ed.work;
            copy.name = ed.baseName + " (mine)";
            copy.builtIn = false;
            ui::layoutsFolder().createDirectory();
            auto f = ui::layoutsFolder().getNonexistentChildFile (copy.name, ".json", true);
            copy.name = f.getFileNameWithoutExtension();
            f.replaceWithText (juce::JSON::toString (copy.toJson(), false));
            ed.loadLayouts();
            ed.showHelp ("Saved", "The layout is now a file: " + f.getFullPathName() + ". It is in the LAYOUT menu as \"" + copy.name + "\".");
        };
        for (auto* t : { &done, &reset, &save }) addAndMakeVisible (t);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (ui::pal().good.withAlpha (0.18f));
        g.fillRect (getLocalBounds());
        g.setColour (ui::pal().good);
        g.fillRect (0, 0, getWidth(), 2);
        auto r = getLocalBounds().reduced (14, 0);
        r.removeFromRight (ui::U (520));
        ui::fitText (g, "EDIT LAYOUT:  drag a panel to move it, its corner to resize, drop it on another panel to make tabs. "
                        "Right-click (or MENU) for own place, own window, swap.", r.toFloat(), 15.0f, 1, ui::pal().text,
                     juce::Justification::centredLeft, 9.0f);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 3);
        done.setBounds (r.removeFromRight (ui::U (110)));
        r.removeFromRight (8);
        save.setBounds (r.removeFromRight (ui::U (180)));
        r.removeFromRight (8);
        reset.setBounds (r.removeFromRight (ui::U (190)));
    }
private:
    Editor& ed;
    ui::Tile done { ui::Tile::Primary }, reset { ui::Tile::Action }, save { ui::Tile::Action };
};

// ================================================================ construction
Editor::Editor (Processor& p) : AudioProcessorEditor (p), proc (p), ctx (p)
{
    settings = std::make_unique<juce::PropertiesFile> (settingsOptions());
    auto& ui = proc.uiState();
    ui::setTheme ((int) ui.getProperty ("theme", settings->getIntValue ("theme", 0)));
    ui::setTextScale ((float) (double) ui.getProperty ("text", settings->getDoubleValue ("text", 1.25)));
    helperOn = (bool) ui.getProperty ("helper", settings->getBoolValue ("helper", true));
    lnf.refreshColours();
    setLookAndFeel (&lnf);
    ui::writeExampleLayouts();
    loadLayouts();

    ctx.hint = [this] (const juce::String& t, const juce::String& x) { showHelp (t, x); };
    ctx.changed = [this] { triggerAsyncUpdate(); };
    ctx.popOut = [this] (const juce::String& id) { popOut (id); };
    addAndMakeVisible (content);
    tips = std::make_unique<juce::TooltipWindow> (this, 900);
    tips->setLookAndFeel (&lnf);

    // the layout: an edited one from the project, else the chosen one, else Studio or Compact by screen size
    baseName = ui.getProperty ("layout", settings->getValue ("layout", "")).toString();
    const ui::LayoutDef* d = findLayout (baseName);
    if (d == nullptr)
    {
        baseName = "Studio";
        if (auto* s = findLayout ("Studio"); s != nullptr && fitZoom (*s) < 0.9f) baseName = "Compact";
        d = findLayout (baseName);
    }
    work = *d;
    {
        ui::LayoutDef custom;
        juce::String err;
        auto js = ui.getProperty ("custom", "").toString();
        if (js.isNotEmpty() && ui::LayoutDef::fromJson (juce::JSON::parse (js), custom, err)) { work = custom; edited = true; }
    }
    float z = (float) (double) ui.getProperty ("zoom", settings->getDoubleValue ("zoom", 0.0));
    zoom = z >= 0.5f && z <= 2.0f ? z : fitZoom (work);
    const int lw = (int) ui.getProperty ("w", work.width), lh = (int) ui.getProperty ("h", work.height);

    build();
    setResizable (true, true);
    setResizeLimits ((int) (work.minWidth * zoom), (int) (work.minHeight * zoom), 6000, 4000);
    setSize ((int) (juce::jmax (work.minWidth, lw) * zoom), (int) (juce::jmax (work.minHeight, lh) * zoom));
    // panel windows from the project
    auto fl = juce::JSON::parse (ui.getProperty ("floats", "").toString());
    if (auto* arr = fl.getArray())
        for (auto& f : *arr)
        {
            popOut (f["id"].toString());
            if (! floats.empty() && floats.back()->id == f["id"].toString())
                floats.back()->setBounds ((int) f["x"], (int) f["y"], juce::jmax (320, (int) f["w"]), juce::jmax (220, (int) f["h"]));
        }
    setWantsKeyboardFocus (true);
    refreshNow();
    startTimerHz (30);
}

Editor::~Editor()
{
    stopTimer();
    tips = nullptr;
    floats.clear();
    helper = nullptr;
    editBar = nullptr;
    view = nullptr;
    header = nullptr;
    footer = nullptr;
    setLookAndFeel (nullptr);
}

void Editor::loadLayouts()
{
    layouts = ui::builtInLayouts();
    layoutErrors.clear();
    for (auto& u : ui::userLayouts (layoutErrors))
    {
        bool clash = false;
        for (auto& b : layouts) clash |= b.name == u.name;
        if (clash) u.name << " (file)";
        layouts.push_back (u);
    }
}

const ui::LayoutDef* Editor::findLayout (const juce::String& name) const
{
    for (auto& l : layouts) if (l.name == name) return &l;
    return nullptr;
}

juce::Rectangle<int> Editor::screenArea() const
{
    if (auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        return d->userArea;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        return d->userArea;
    return { 0, 0, 1920, 1040 };
}

float Editor::fitZoom (const ui::LayoutDef& d) const
{
    auto a = screenArea();
    float z = juce::jmin (1.0f, a.getWidth() * 0.94f / (float) d.width, a.getHeight() * 0.9f / (float) d.height);
    z = std::floor (z * 20.0f) / 20.0f;
    return juce::jlimit (0.5f, 1.0f, z);
}

void Editor::build()
{
    building = true;
    view = nullptr;
    header = nullptr;
    footer = nullptr;
    editBar = nullptr;
    helper = nullptr;
    header = std::make_unique<ui::Header> (ctx);
    footer = std::make_unique<ui::Footer> (ctx);
    view = std::make_unique<ui::LayoutView> (ctx, *this, work, proc.uiState());
    editBar = std::make_unique<EditBar> (*this);
    helper = std::make_unique<ui::HelperBox> ([this] { setHelper (false); }, [this]
    {
        proc.uiState().setProperty ("helperX", helper->getX(), nullptr);
        proc.uiState().setProperty ("helperY", helper->getY(), nullptr);
    });
    content.addAndMakeVisible (*header);
    content.addAndMakeVisible (*view);
    content.addAndMakeVisible (*footer);
    content.addChildComponent (*editBar);
    content.addChildComponent (*helper);
    for (auto& f : floats) { content.addAndMakeVisible (*f); f->toFront (false); }
    header->layoutBtn.set ("LAYOUT: " + baseName.toUpperCase() + (edited ? " *" : ""));
    header->zoomBtn.set (juce::String (juce::roundToInt (zoom * 100)) + " %");
    header->editBtn.setOn (editMode);
    header->helperBtn.set (helperOn ? "HELPER ON" : "HELPER OFF");
    header->helperBtn.setOn (helperOn);
    header->layoutBtn.onClick = [this] (const juce::MouseEvent&) { layoutMenu(); };
    header->zoomBtn.onClick = [this] (const juce::MouseEvent&) { zoomMenu(); };
    header->editBtn.onClick = [this] (const juce::MouseEvent&) { setEditing (! editMode); };
    header->helperBtn.onClick = [this] (const juce::MouseEvent&) { setHelper (! helperOn); };
    header->helpBtn.onClick = [this] (const juce::MouseEvent&) { popOut ("help"); };
    tips->setMillisecondsBeforeTipAppears (helperOn ? 3600000 : 900);
    proc.uiState().setProperty ("layout", baseName, nullptr);
    building = false;
    layoutChildren();
    refreshNow();
}

void Editor::rebuildView()
{
    view = nullptr;
    view = std::make_unique<ui::LayoutView> (ctx, *this, work, proc.uiState());
    content.addAndMakeVisible (*view);
    for (auto& f : floats) f->toFront (false);
    if (helper) helper->toFront (false);
    if (header) header->layoutBtn.set ("LAYOUT: " + baseName.toUpperCase() + (edited ? " *" : ""));
    layoutChildren();
    refreshNow();
}

void Editor::refreshNow()
{
    ctx.model.refresh (proc);
    ctx.updateAll();
}

void Editor::timerCallback()
{
    proc.pumpIfIdle();
    if (ctx.model.refresh (proc))
        ctx.updateAll();
}

void Editor::paint (juce::Graphics& g) { g.fillAll (ui::pal().bg); }

void Editor::resized()
{
    if (building) return;
    layoutChildren();
    auto& ui = proc.uiState();
    ui.setProperty ("w", (int) std::lround (getWidth() / zoom), nullptr);
    ui.setProperty ("h", (int) std::lround (getHeight() / zoom), nullptr);
}

void Editor::layoutChildren()
{
    if (building) return;
    const int lw = (int) std::lround (getWidth() / zoom), lh = (int) std::lround (getHeight() / zoom);
    content.setTransform (juce::AffineTransform::scale (zoom));
    content.setBounds (0, 0, lw, lh);
    const int hh = headerH(), fh = footerH();
    if (header) header->setBounds (0, 0, lw, hh);
    if (footer) footer->setBounds (0, lh - fh, lw, fh);
    if (editBar) { editBar->setBounds (0, lh - fh - ui::U (44), lw, ui::U (44)); editBar->setVisible (editMode); }
    const int eb = editMode ? ui::U (44) : 0;
    if (view) view->setBounds (MARGIN, hh + MARGIN, lw - 2 * MARGIN, lh - hh - fh - eb - 2 * MARGIN);
    if (helper)
    {
        const int hw = juce::jmin (ui::U (380), lw * 38 / 100), hhgt = helper->preferredHeight (hw);
        auto& ui = proc.uiState();
        int x = (int) ui.getProperty ("helperX", lw - hw - 10), y = (int) ui.getProperty ("helperY", lh - hhgt - 4);
        x = juce::jlimit (0, juce::jmax (0, lw - hw), x);
        y = juce::jlimit (0, juce::jmax (0, lh - hhgt), y);
        helper->setBounds (x, y, hw, hhgt);
        helper->setVisible (helperOn);
        helper->toFront (false);
    }
    for (auto& f : floats)
    {
        auto b = f->getBounds();
        f->setTopLeftPosition (juce::jlimit (-b.getWidth() + 120, lw - 120, b.getX()), juce::jlimit (hh, juce::jmax (hh, lh - ui::U (32)), b.getY()));
    }
}

// ================================================================ layouts, zoom, text size, editing, helper
void Editor::selectLayout (const juce::String& name, bool resizeWindow)
{
    auto* d = findLayout (name);
    if (d == nullptr) return;
    baseName = name;
    work = *d;
    edited = false;
    saveWork();
    build();
    setResizeLimits ((int) (d->minWidth * zoom), (int) (d->minHeight * zoom), 6000, 4000);
    if (resizeWindow)
    {
        auto a = screenArea();
        float z = zoom;
        if (d->width * z > a.getWidth() * 0.98f || d->height * z > a.getHeight() * 0.95f)
            z = fitZoom (*d);
        if (z != zoom) setZoom (z);
        setSize ((int) (d->width * zoom), (int) (d->height * zoom));
    }
}

void Editor::setZoom (float z)
{
    z = juce::jlimit (0.5f, 2.0f, z);
    const int lw = (int) std::lround (getWidth() / zoom), lh = (int) std::lround (getHeight() / zoom);
    zoom = z;
    proc.uiState().setProperty ("zoom", zoom, nullptr);
    setResizeLimits ((int) (work.minWidth * zoom), (int) (work.minHeight * zoom), 6000, 4000);
    if (header) header->zoomBtn.set (juce::String (juce::roundToInt (zoom * 100)) + " %");
    setSize ((int) (lw * zoom), (int) (lh * zoom));
    layoutChildren();
}

void Editor::setTextSize (float s)
{
    ui::setTextScale (s);
    proc.uiState().setProperty ("text", ui::textScale(), nullptr);
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> fl;
    for (auto& f : floats) fl.push_back ({ f->id, f->getBounds() });
    floats.clear();
    build();
    for (auto& [id, b] : fl) { popOut (id); if (! floats.empty()) floats.back()->setBounds (b); }
}

void Editor::setEditing (bool e)
{
    editMode = e;
    if (header) header->editBtn.setOn (e);
    if (view) view->setEditing (e);
    layoutChildren();
    if (e) showHelp ("Edit layout", "Drag a panel to move it, drag its corner to resize it, drop it on another panel to put them together "
                                    "as tabs. Right-click a panel (or MENU) to give a tab its own place, open it as a window, or swap places. DONE when you are happy.");
}

void Editor::setHelper (bool on)
{
    helperOn = on;
    proc.uiState().setProperty ("helper", on, nullptr);
    settings->setValue ("helper", on);
    settings->saveIfNeeded();
    if (header) { header->helperBtn.set (on ? "HELPER ON" : "HELPER OFF"); header->helperBtn.setOn (on); }
    if (tips) tips->setMillisecondsBeforeTipAppears (on ? 3600000 : 900);
    layoutChildren();
}

void Editor::showHelp (const juce::String& title, const juce::String& text)
{
    if (helper) helper->set (title, text);
    if (footer) footer->setHint (text.upToFirstOccurrenceOf ("\n", false, false));
}

void Editor::saveSettings()
{
    settings->setValue ("layout", baseName);
    settings->setValue ("zoom", zoom);
    settings->setValue ("theme", ui::theme());
    settings->setValue ("text", ui::textScale());
    settings->setValue ("helper", helperOn);
    settings->saveIfNeeded();
}

void Editor::saveWork()
{
    proc.uiState().setProperty ("custom", edited ? juce::JSON::toString (work.toJson(), true) : juce::String(), nullptr);
}

void Editor::saveFloats()
{
    juce::Array<juce::var> a;
    for (auto& f : floats)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", f->id);
        o->setProperty ("x", f->getX()); o->setProperty ("y", f->getY());
        o->setProperty ("w", f->getWidth()); o->setProperty ("h", f->getHeight());
        a.add (juce::var (o));
    }
    proc.uiState().setProperty ("floats", juce::JSON::toString (juce::var (a), true), nullptr);
}

// ================================================================ LayoutHost: panel windows and editing
bool Editor::isFloating (const juce::String& id)
{
    for (auto& f : floats) if (f->id == id) return true;
    return false;
}

void Editor::popOut (const juce::String& id)
{
    bool known = false;
    for (auto& p : ui::panelList()) known |= id == p.id;
    if (! known) return;
    for (auto& f : floats) if (f->id == id) { f->toFront (false); return; }
    auto f = std::make_unique<ui::FloatPanel> (ctx, id, [this, id] { dock (id); }, [this] { saveFloats(); });
    const int lw = content.getWidth(), lh = content.getHeight();
    const int w = juce::jmin (ui::U (620), lw - 40), h = juce::jmin (ui::U (480), lh - headerH() - 40);
    const int n = (int) floats.size();
    f->setBounds (juce::jmax (0, lw / 2 - w / 2 + n * 30), juce::jmax (headerH(), lh / 2 - h / 2 + n * 30), w, h);
    content.addAndMakeVisible (*f);
    if (helper) helper->toFront (false);
    floats.push_back (std::move (f));
    if (view) view->refreshFloating();
    saveFloats();
    refreshNow();
}

void Editor::dock (const juce::String& id)
{
    juce::String did = id;   // the lambda that called this lives in the window we delete
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Editor> (this), did]
    {
        if (safe == nullptr) return;
        auto& fl = safe->floats;
        fl.erase (std::remove_if (fl.begin(), fl.end(), [&] (auto& f) { return f->id == did; }), fl.end());
        if (safe->view) safe->view->refreshFloating();
        safe->saveFloats();
        safe->refreshNow();
    });
}

void Editor::moveItem (int item, juce::Rectangle<int> cells)
{
    if (item < 0 || item >= (int) work.items.size()) return;
    auto& it = work.items[(size_t) item];
    it.x = cells.getX(); it.y = cells.getY(); it.w = cells.getWidth(); it.h = cells.getHeight();
    edited = true;
    saveWork();
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Editor> (this)] { if (safe) safe->rebuildView(); });
}

void Editor::mergeItem (int from, int into)
{
    if (from == into || from < 0 || into < 0 || from >= (int) work.items.size() || into >= (int) work.items.size()) return;
    work.items[(size_t) into].ids.addArray (work.items[(size_t) from].ids);
    work.items.erase (work.items.begin() + from);
    edited = true;
    saveWork();
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Editor> (this)] { if (safe) safe->rebuildView(); });
}

static juce::String titleOf (const juce::String& id)
{
    for (auto& p : ui::panelList()) if (id == p.id) return p.title;
    return id;
}

void Editor::itemMenu (int item, const juce::String& tabId, juce::Component* target)
{
    if (item < 0 || item >= (int) work.items.size()) return;
    const auto ids = work.items[(size_t) item].ids;
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader (titleOf (tabId).toUpperCase());
    m.addItem (1, isFloating (tabId) ? "Put it back into the layout" : "Open as its own window");
    if (ids.size() > 1)
    {
        m.addItem (2, "Give it its own place (split this area)");
        juce::PopupMenu to;
        for (int i = 0; i < (int) work.items.size(); ++i)
            if (i != item) to.addItem (100 + i, work.items[(size_t) i].ids.joinIntoString (" + ", 0, 3));
        m.addSubMenu ("Move it as a tab to", to);
    }
    juce::PopupMenu with, swap;
    for (int i = 0; i < (int) work.items.size(); ++i)
        if (i != item)
        {
            juce::StringArray n;
            for (auto& id : work.items[(size_t) i].ids) n.add (titleOf (id));
            with.addItem (200 + i, n.joinIntoString (" + ", 0, 4));
            swap.addItem (300 + i, n.joinIntoString (" + ", 0, 4));
        }
    m.addSubMenu ("Put this place together with (tabs)", with);
    m.addSubMenu ("Swap places with", swap);
    m.addSeparator();
    m.addItem (3, editMode ? "Finish editing the layout" : "Edit the layout (move, resize)");
    juce::Component::SafePointer<Editor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target).withMousePosition().withStandardItemHeight (ui::U (26)),
                     [safe, item, tabId] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& self = *safe;
        auto& w = self.work;
        if (item >= (int) w.items.size()) return;
        if (r == 1) { if (self.isFloating (tabId)) self.dock (tabId); else self.popOut (tabId); return; }
        if (r == 3) { self.setEditing (! self.editMode); return; }
        if (r == 2)
        {
            // split: the tab takes the right (or lower) half of this area
            auto& it = w.items[(size_t) item];
            it.ids.removeString (tabId);
            auto px = self.view != nullptr ? self.view->cellRect (it) : juce::Rectangle<int> (it.w * 10, it.h * 10);
            ui::LayoutDef::Item n { juce::StringArray (tabId), it.x, it.y, it.w, it.h };
            if (px.getWidth() >= px.getHeight() && it.w >= 8) { n.w = it.w / 2; it.w -= n.w; n.x = it.x + it.w; }
            else if (it.h >= 8) { n.h = it.h / 2; it.h -= n.h; n.y = it.y + it.h; }
            w.items.push_back (n);
        }
        else if (r >= 100 && r < 200)
        {
            w.items[(size_t) item].ids.removeString (tabId);
            w.items[(size_t) (r - 100)].ids.add (tabId);
            if (w.items[(size_t) item].ids.isEmpty()) w.items.erase (w.items.begin() + item);
        }
        else if (r >= 200 && r < 300) { self.mergeItem (item, r - 200); return; }
        else if (r >= 300)
        {
            auto& a = w.items[(size_t) item];
            auto& b = w.items[(size_t) (r - 300)];
            std::swap (a.ids, b.ids);
        }
        self.edited = true;
        self.saveWork();
        self.rebuildView();
    });
}

void Editor::layoutMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader ("LAYOUTS");
    int id = 1;
    bool userHeader = false;
    for (auto& l : layouts)
    {
        if (! l.builtIn && ! userHeader) { m.addSeparator(); m.addSectionHeader ("MY LAYOUT FILES"); userHeader = true; }
        m.addItem (id++, l.name + "   -   " + l.about.upToFirstOccurrenceOf (":", false, false).upToFirstOccurrenceOf (".", false, false),
                   true, l.name == baseName);
    }
    for (auto& e : layoutErrors) m.addItem (-1, "! " + e, false);
    m.addSeparator();
    m.addItem (610, editMode ? "Finish editing the layout" : "Edit the layout (move, resize, tabs)...");
    if (edited) m.addItem (611, "Reset \"" + baseName + "\" to how it was");
    juce::PopupMenu themes;
    auto names = ui::themeNames();
    for (int i = 0; i < names.size(); ++i) themes.addItem (500 + i, names[i], true, i == ui::theme());
    m.addSubMenu ("Colour theme", themes);
    juce::PopupMenu text;
    const int ts[] = { 100, 115, 125, 140, 160, 180 };
    for (int t : ts) text.addItem (700 + t, juce::String (t) + " %", true, juce::roundToInt (ui::textScale() * 100) == t);
    m.addSubMenu ("Text size", text);
    juce::PopupMenu win;
    for (auto& p : ui::panelList()) win.addItem (800 + (int) (&p - &ui::panelList()[0]), p.title, true, isFloating (p.id));
    m.addSubMenu ("Open a panel as window", win);
    m.addSeparator();
    m.addItem (600, "Use this layout, zoom, text size and theme for new instances");
    m.addItem (601, "Save this layout as a file to edit...");
    m.addItem (602, "Open the layouts folder");
    m.addItem (603, "Reload layout files");
    m.addItem (604, "Reset the window to the layout's size");

    juce::Component::SafePointer<Editor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header->layoutBtn).withStandardItemHeight (ui::U (26)), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& self = *safe;
        if (r < 500)
        {
            if (r - 1 < (int) self.layouts.size()) self.selectLayout (self.layouts[(size_t) r - 1].name);
        }
        else if (r < 600)
        {
            ui::setTheme (r - 500);
            self.proc.uiState().setProperty ("theme", r - 500, nullptr);
            self.lnf.refreshColours();
            self.setTextSize (ui::textScale());   // rebuilds everything, keeps the windows
            self.repaint();
        }
        else if (r == 600) self.saveSettings();
        else if (r == 601) { if (self.editBar) { self.setEditing (true); } }
        else if (r == 602) { ui::layoutsFolder().createDirectory(); ui::layoutsFolder().startAsProcess(); }
        else if (r == 603) { self.loadLayouts(); self.build(); }
        else if (r == 604) self.setSize ((int) (self.work.width * self.zoom), (int) (self.work.height * self.zoom));
        else if (r == 610) self.setEditing (! self.editMode);
        else if (r == 611) { if (auto* d = self.findLayout (self.baseName)) { self.work = *d; self.edited = false; self.saveWork(); self.rebuildView(); } }
        else if (r >= 700 && r < 800) self.setTextSize ((r - 700) / 100.0f);
        else if (r >= 800) { auto& pl = ui::panelList(); if (r - 800 < (int) pl.size()) self.popOut (pl[(size_t) (r - 800)].id); }
    });
}

void Editor::zoomMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    m.addSectionHeader ("ZOOM (EVERYTHING)");
    const int zs[] = { 50, 60, 70, 75, 80, 90, 100, 110, 125, 150, 175, 200 };
    for (int z : zs) m.addItem (z, juce::String (z) + " %", true, juce::roundToInt (zoom * 100) == z);
    m.addItem (1, "Fit to screen");
    m.addSeparator();
    m.addSectionHeader ("TEXT SIZE");
    const int ts[] = { 100, 115, 125, 140, 160, 180 };
    for (int t : ts) m.addItem (1000 + t, "Text " + juce::String (t) + " %", true, juce::roundToInt (ui::textScale() * 100) == t);
    juce::Component::SafePointer<Editor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header->zoomBtn).withStandardItemHeight (ui::U (26)), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r >= 1000) { safe->setTextSize ((r - 1000) / 100.0f); return; }
        if (r == 1)
        {
            safe->setZoom (safe->fitZoom (safe->work));
            safe->setSize ((int) (safe->work.width * safe->zoom), (int) (safe->work.height * safe->zoom));
        }
        else safe->setZoom (r / 100.0f);
    });
}

bool Editor::keyPressed (const juce::KeyPress& k)
{
    const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    const bool cmd = k.getModifiers().isCommandDown();
    if (cmd && (c == 'z' || k.getKeyCode() == 'Z')) { ctx.tap (k.getModifiers().isShiftDown() ? "redo" : "undo"); return true; }
    if (cmd && (c == 'y' || k.getKeyCode() == 'Y')) { ctx.tap ("redo"); return true; }
    if (cmd) return false;
    if (c == 'g') { ctx.tap ("generate"); return true; }
    if (c == 'c') { ctx.tap ("cruise"); return true; }
    if (c == 'r') { ctx.tap ("reroll"); return true; }
    if (c == 'a') { ctx.tap ("add"); return true; }
    if (c == 'h') { setHelper (! helperOn); return true; }
    if (c >= '1' && c <= '8')
    {
        static const char* const P[8] = { "prog_1", "prog_2", "prog_3", "prog_4", "prog_5", "prog_6", "prog_7", "prog_8" };
        ctx.tap (P[c - '1']);
        return true;
    }
    if (k.getKeyCode() == juce::KeyPress::deleteKey || k.getKeyCode() == juce::KeyPress::backspaceKey) { ctx.tap ("delete"); return true; }
    if (k.getKeyCode() == juce::KeyPress::escapeKey && editMode) { setEditing (false); return true; }
    return false;
}

} // namespace cc
