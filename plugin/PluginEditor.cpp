#include "PluginEditor.h"

namespace cc {

static constexpr int HEADER_H = 76, FOOTER_H = 30, MARGIN = 8;

static juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "CheatChord";
    o.folderName = "JJ DA PLUG";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

Editor::Editor (Processor& p) : AudioProcessorEditor (p), proc (p), ctx (p)
{
    settings = std::make_unique<juce::PropertiesFile> (settingsOptions());
    auto& ui = proc.uiState();
    ui::setTheme ((int) ui.getProperty ("theme", settings->getIntValue ("theme", 0)));
    lnf.refreshColours();
    setLookAndFeel (&lnf);
    ui::writeExampleLayouts();
    loadLayouts();

    ctx.hint = [this] (const juce::String& s) { if (footer) footer->setHint (s); };
    ctx.changed = [this] { triggerAsyncUpdate(); };
    addAndMakeVisible (content);
    tips = std::make_unique<juce::TooltipWindow> (this, 900);
    tips->setLookAndFeel (&lnf);

    layoutName = ui.getProperty ("layout", settings->getValue ("layout", "")).toString();
    const ui::LayoutDef* d = current();
    if (d == nullptr || layoutName.isEmpty())
    {
        // first time: Studio if it fits the screen at 80 % or more, else Compact
        layoutName = "Studio";
        if (auto* s = current(); s != nullptr && fitZoom (*s) < 0.8f) layoutName = "Compact";
        d = current();
    }
    float z = (float) (double) ui.getProperty ("zoom", settings->getDoubleValue ("zoom", 0.0));
    zoom = z >= 0.5f && z <= 2.0f ? z : fitZoom (*d);
    const int lw = (int) ui.getProperty ("w", d->width), lh = (int) ui.getProperty ("h", d->height);

    build();
    setResizable (true, true);
    setResizeLimits ((int) (d->minWidth * zoom), (int) (d->minHeight * zoom), 6000, 4000);
    setSize ((int) (juce::jmax (d->minWidth, lw) * zoom), (int) (juce::jmax (d->minHeight, lh) * zoom));
    setWantsKeyboardFocus (true);
    refreshNow();
    startTimerHz (30);
}

Editor::~Editor()
{
    stopTimer();
    tips = nullptr;
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

const ui::LayoutDef* Editor::current() const
{
    for (auto& l : layouts) if (l.name == layoutName) return &l;
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
    float z = juce::jmin (1.0f, a.getWidth() * 0.94f / (float) d.width, a.getHeight() * 0.88f / (float) d.height);
    z = std::floor (z * 20.0f) / 20.0f;
    return juce::jlimit (0.5f, 1.0f, z);
}

void Editor::build()
{
    building = true;
    auto* d = current();
    if (d == nullptr) { layoutName = layouts.front().name; d = current(); }
    view = nullptr;
    header = nullptr;
    footer = nullptr;
    header = std::make_unique<ui::Header> (ctx);
    footer = std::make_unique<ui::Footer> (ctx);
    view = std::make_unique<ui::LayoutView> (ctx, *d, proc.uiState());
    content.addAndMakeVisible (*header);
    content.addAndMakeVisible (*view);
    content.addAndMakeVisible (*footer);
    header->layoutBtn.set ("LAYOUT: " + d->name.toUpperCase());
    header->zoomBtn.set (juce::String (juce::roundToInt (zoom * 100)) + " %");
    header->layoutBtn.onClick = [this] (const juce::MouseEvent&) { layoutMenu(); };
    header->zoomBtn.onClick = [this] (const juce::MouseEvent&) { zoomMenu(); };
    header->helpBtn.onClick = [this] (const juce::MouseEvent&)
    {
        // the help panel: show it where the layout has it, else as a box
        auto help = ui::makePanel ("help", ctx);
        help->setSize (900, 420);
        auto& box = juce::CallOutBox::launchAsynchronously (std::move (help), getLocalArea (header.get(), header->helpBtn.getBounds()), this);
        box.setLookAndFeel (&lnf);
    };
    proc.uiState().setProperty ("layout", d->name, nullptr);
    building = false;
    resized();
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
    const int lw = (int) std::lround (getWidth() / zoom), lh = (int) std::lround (getHeight() / zoom);
    content.setTransform (juce::AffineTransform::scale (zoom));
    content.setBounds (0, 0, lw, lh);
    if (header) header->setBounds (0, 0, lw, HEADER_H);
    if (footer) footer->setBounds (0, lh - FOOTER_H, lw, FOOTER_H);
    if (view) view->setBounds (MARGIN, HEADER_H + MARGIN, lw - 2 * MARGIN, lh - HEADER_H - FOOTER_H - 2 * MARGIN);
    auto& ui = proc.uiState();
    ui.setProperty ("w", lw, nullptr);
    ui.setProperty ("h", lh, nullptr);
}

void Editor::selectLayout (const juce::String& name, bool resizeWindow)
{
    layoutName = name;
    auto* d = current();
    if (d == nullptr) return;
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
    if (auto* d = current())
        setResizeLimits ((int) (d->minWidth * zoom), (int) (d->minHeight * zoom), 6000, 4000);
    if (header) header->zoomBtn.set (juce::String (juce::roundToInt (zoom * 100)) + " %");
    setSize ((int) (lw * zoom), (int) (lh * zoom));
    resized();
}

void Editor::saveSettings()
{
    settings->setValue ("layout", layoutName);
    settings->setValue ("zoom", zoom);
    settings->setValue ("theme", ui::theme());
    settings->saveIfNeeded();
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
                   true, l.name == layoutName);
    }
    for (auto& e : layoutErrors) m.addItem (-1, "! " + e, false);
    m.addSeparator();
    juce::PopupMenu themes;
    auto names = ui::themeNames();
    for (int i = 0; i < names.size(); ++i) themes.addItem (500 + i, names[i], true, i == ui::theme());
    m.addSubMenu ("Colour theme", themes);
    m.addSeparator();
    m.addItem (600, "Use this layout, zoom and theme for new instances");
    m.addItem (601, "Save this layout as a file to edit...");
    m.addItem (602, "Open the layouts folder");
    m.addItem (603, "Reload layout files");
    m.addItem (604, "Reset the window to the layout's size");

    juce::Component::SafePointer<Editor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header->layoutBtn).withStandardItemHeight (28), [safe] (int r)
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
            self.build();
            self.repaint();
        }
        else if (r == 600) self.saveSettings();
        else if (r == 601)
        {
            if (auto* d = self.current())
            {
                auto copy = *d;
                copy.name = d->name + " (mine)";
                copy.width = (int) std::lround (self.getWidth() / self.zoom);
                copy.height = (int) std::lround (self.getHeight() / self.zoom);
                auto f = ui::layoutsFolder().getNonexistentChildFile (copy.name, ".json", true);
                ui::layoutsFolder().createDirectory();
                f.replaceWithText (juce::JSON::toString (copy.toJson(), false));
                self.loadLayouts();
                f.revealToUser();
            }
        }
        else if (r == 602) { ui::layoutsFolder().createDirectory(); ui::layoutsFolder().startAsProcess(); }
        else if (r == 603) { self.loadLayouts(); self.build(); }
        else if (r == 604) { if (auto* d = self.current()) self.setSize ((int) (d->width * self.zoom), (int) (d->height * self.zoom)); }
    });
}

void Editor::zoomMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    const int zs[] = { 50, 60, 70, 75, 80, 90, 100, 110, 125, 150, 175, 200 };
    for (int z : zs) m.addItem (z, juce::String (z) + " %", true, juce::roundToInt (zoom * 100) == z);
    m.addSeparator();
    m.addItem (1, "Fit to screen");
    juce::Component::SafePointer<Editor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&header->zoomBtn).withStandardItemHeight (26), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r == 1)
        {
            if (auto* d = safe->current())
            {
                safe->setZoom (safe->fitZoom (*d));
                safe->setSize ((int) (d->width * safe->zoom), (int) (d->height * safe->zoom));
            }
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
    if (c >= '1' && c <= '8') { ctx.tap (c == '1' ? "prog_1" : c == '2' ? "prog_2" : c == '3' ? "prog_3" : c == '4' ? "prog_4"
                                         : c == '5' ? "prog_5" : c == '6' ? "prog_6" : c == '7' ? "prog_7" : "prog_8"); return true; }
    if (k.getKeyCode() == juce::KeyPress::deleteKey || k.getKeyCode() == juce::KeyPress::backspaceKey) { ctx.tap ("delete"); return true; }
    return false;
}

} // namespace cc
