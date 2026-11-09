#include "Panels.h"
#include "../PluginProcessor.h"

namespace cc::ui {

// engine keys built at run time ("prog_3") need a stable address
static const char* K (const juce::String& s)
{
    static std::set<std::string> pool;
    return pool.insert (s.toStdString()).first->c_str();
}
static const char* K (const char* prefix, int i, const char* suffix = "") { return K (juce::String (prefix) + juce::String (i) + suffix); }

static juce::StringArray nums (int from, int to, const juce::String& pre = {})
{
    juce::StringArray a;
    for (int i = from; i <= to; ++i) a.add (pre + juce::String (i));
    return a;
}

static const char* const CLASS_NAMES[5] = { "IN KEY", "BORROWED", "LEADS TO NEXT", "SUBSTITUTE", "SPICY" };

static void hintAll (Ctx& ctx, std::initializer_list<Tile*> tiles)
{
    for (auto* t : tiles) t->onHint = [&ctx] (const juce::String& title, const juce::String& h) { ctx.help (title, h); };
}

static juce::String pct (int v) { return juce::String (v) + " %"; }

// "> * Fm9 x2 REV" -> name, badge
static void splitChord (juce::String t, juce::String& name, juce::String& badge, bool& target)
{
    target = false;
    t = t.trim();
    while (t.startsWith (">") || t.startsWith ("*"))
    {
        if (t.startsWith ("*")) target = true;
        t = t.substring (1).trimStart();
    }
    juce::StringArray b;
    if (t.endsWith (" REV")) { b.add ("REV"); t = t.dropLastCharacters (4).trimEnd(); }
    if (t.endsWith (" x2")) { b.insert (0, "x2"); t = t.dropLastCharacters (3).trimEnd(); }
    if (t.endsWith (" 1/2")) { b.insert (0, "1/2"); t = t.dropLastCharacters (4).trimEnd(); }
    name = t;
    badge = b.joinIntoString ("  ");
}

// ================================================================ Progression
ProgressionPanel::ProgressionPanel (Ctx& c)
    : Panel ("Progression"), Live (c), len (c, "gen_len", { "4 CHORDS", "8 CHORDS" }), scope (c, "scope", { "EDIT CHORD", "EDIT ALL" }),
      status (c, "status", 15.0f)
{
    for (int i = 0; i < 8; ++i)
    {
        auto& t = chords[i];
        t.style = Tile::Chord;
        ctx.model.want (K ("prog_", i + 1));
        t.onClick = [this, i] (const juce::MouseEvent& e)
        {
            if (e.mods.isPopupMenu()) { chordMenu (i); return; }
            ctx.tap (K ("prog_", i + 1));
        };
        t.setHelp ("Click: hear this chord and select it. Then double-click an idea to replace it. Right-click: length, reverse, delete.");
        addAndMakeVisible (t);
    }
    len.setHelp ("How many chords the loop has. Chords beyond 4 stay kept (grey) when you switch back.");
    scope.setHelp ("EDIT CHORD: key, scale, style and the profiles re-fit only the selected chord. EDIT ALL: every chord follows.");
    cruise.set ("CRUISE");
    cruise.setHelp ("Adds the next chord that fits after the selected one (the suggestion shows on the button).");
    cruise.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("cruise"); };
    ctx.model.want ("cruise_next");
    generate.set ("GENERATE");
    generate.setHelp ("Writes a whole new progression in the key, style and profiles you chose. UNDO brings the old one back.");
    generate.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("generate"); };
    undo.set ("UNDO"); undo.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("undo"); };
    undo.setHelp ("One step back. Nothing is lost: every change stays in HISTORY.");
    redo.set ("REDO"); redo.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("redo"); };
    redo.setHelp ("One step forward again.");
    del.set ("DELETE"); del.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("delete"); };
    del.setHelp ("Removes the selected chord.");
    clear.set ("CLEAR"); clear.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("clear"); };
    clear.setHelp ("Empties the progression (UNDO brings it back).");
    drag.set ("DRAG MIDI", "into your DAW");
    drag.setHelp ("Drag this onto a track in your DAW: the progression as a MIDI clip, with rhythm, strum and voicing.");
    drag.onDragStart = [this] (const juce::MouseEvent&)
    {
        auto f = ctx.proc.writeDragMidi (dragMsg);
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
        ctx.help ("Drag MIDI", dragMsg);
    };
    drag.onClick = [this] (const juce::MouseEvent&) { ctx.help ("Drag MIDI", "Hold the mouse button down on this button and drag it onto a track in your DAW (or the MPC sequence)."); };
    exportMid.set ("EXPORT .MID");
    exportMid.setHelp ("Saves the progression as a .mid file in Documents / JJ DA PLUG / CheatChord / MIDI Exports.");
    exportMid.onClick = [this] (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            juce::File (cc_docs_root()).getChildFile ("MIDI Exports").startAsProcess();
            return;
        }
        ctx.tap ("export");
    };
    ctx.model.want ("export_status");
    hintAll (ctx, { &cruise, &generate, &undo, &redo, &del, &clear, &drag, &exportMid });
    for (int i = 0; i < 8; ++i) hintAll (ctx, { &chords[i] });
    for (auto* x : std::initializer_list<juce::Component*> { &len, &scope, &cruise, &generate, &undo, &redo, &del, &clear, &drag, &exportMid, &status })
        addAndMakeVisible (x);
}

void ProgressionPanel::chordMenu (int i)
{
    if (i >= ctx.model.count) return;
    ctx.setValue ("cursor", i + 1);   // the chord menu acts on this chord
    juce::PopupMenu m;
    m.addSectionHeader ("CHORD " + juce::String (i + 1));
    const int ln = ctx.val ("slot_len"), rv = ctx.val ("slot_rev");
    m.addItem (1, "Half length", true, ln == 0);
    m.addItem (2, "Normal length", true, ln == 1);
    m.addItem (3, "Double length", true, ln == 2);
    m.addSeparator();
    m.addItem (4, "Reverse (swells in)", true, rv == 1);
    m.addSeparator();
    m.addItem (5, "Ideas to replace it (REROLL)");
    m.addItem (6, "Delete chord");
    juce::Component::SafePointer<ProgressionPanel> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&chords[i]), [safe, rv] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto& c = safe->ctx;
        if (r <= 3) c.setValue ("slot_len", r - 1);
        else if (r == 4) c.setValue ("slot_rev", rv ? 0 : 1);
        else if (r == 5) c.tap ("reroll");
        else if (r == 6) c.tap ("delete");
    });
}

void ProgressionPanel::update()
{
    auto& m = ctx.model;
    for (int i = 0; i < 8; ++i)
    {
        auto& t = chords[i];
        juce::String name, badge;
        bool target;
        splitChord (m.text (K ("prog_", i + 1)), name, badge, target);
        const bool filled = i < m.count;
        const bool plus = i == m.count && i < m.loopLen;
        t.setEmpty (plus || (! filled && i < m.loopLen));
        t.set (plus ? "+" : filled ? name : "", filled ? m.slotRoman[i] : plus ? "NEXT CHORD" : "", filled && i == m.cursor && m.playSlot < 0);
        t.setBadge (target && filled ? "IDEAS" + juce::String (badge.isNotEmpty() ? "  " + badge : "") : badge);
        t.setHot (i == m.playSlot);
        t.setDim (i >= m.loopLen);
        t.setAccent (filled && m.slotClass[i] >= 0 ? pal().cls[m.slotClass[i]].withAlpha (0.9f) : juce::Colours::transparentBlack);
        t.setVisible (i < m.loopLen || filled);
    }
    auto nx = m.text ("cruise_next");
    cruise.set ("CRUISE", nx.startsWith ("NEXT") ? nx : juce::String());
    del.setEnabled (m.count > 0);
    clear.setEnabled (m.count > 0);
    drag.setEnabled (m.count > 0);
    exportMid.set ("EXPORT .MID", m.text ("export_status").startsWith ("SAVED") ? m.text ("export_status").substring (6) : juce::String());
    resized();
}

void ProgressionPanel::resized()
{
    auto r = content();
    auto tb = titleBar();
    if (! popButton().isEmpty()) tb.removeFromRight (tb.getHeight() + 8);
    const int segW = juce::jmin (U (220), tb.getWidth() / 4);
    scope.setBounds (tb.removeFromRight (segW).reduced (0, 1));
    tb.removeFromRight (U (8));
    len.setBounds (tb.removeFromRight (segW).reduced (0, 1));

    const bool tall = r.getHeight() > 200;
    const int statusH = r.getHeight() > 150 ? 26 : 0;
    const int actionsH = juce::jlimit (U (34), U (58), r.getHeight() / 4);
    auto st = statusH ? r.removeFromBottom (statusH) : juce::Rectangle<int>();
    if (statusH) r.removeFromBottom (U (6));
    status.setBounds (st);
    status.setVisible (statusH > 0);
    auto act = r.removeFromBottom (actionsH);
    r.removeFromBottom (U (8));

    const int n = ctx.model.loopLen > 4 || ctx.model.count > 4 ? 8 : 4;
    std::vector<juce::Component*> items;
    for (int i = 0; i < n; ++i) items.push_back (&chords[i]);
    for (int i = n; i < 8; ++i) chords[i].setVisible (false);
    grid (items, r, n, 8);
    for (int i = 0; i < n; ++i) chords[i].setTextScale (tall ? 1.15f : 1.0f);

    // actions: CRUISE and GENERATE wide, then the rest
    const int w = act.getWidth();
    auto a = act;
    cruise.setBounds (takeLeft (a, (int) (w * 0.17f), 8));
    generate.setBounds (takeLeft (a, (int) (w * 0.15f), 8));
    std::vector<juce::Component*> small { &undo, &redo, &del, &clear };
    auto sm = takeLeft (a, (int) (w * 0.38f), 8);
    grid (small, sm, 4, 6);
    grid ({ &exportMid, &drag }, a, 2, 6);
}

void ProgressionPanel::paint (juce::Graphics& g) { Panel::paint (g); }

// ================================================================ Ideas
IdeasPanel::IdeasPanel (Ctx& c) : Panel ("Ideas"), Live (c), info (c, "info", 15.0f, juce::Justification::centred, 2)
{
    for (int i = 0; i < 8; ++i)
    {
        auto& t = ideas[i];
        t.style = Tile::Chord;
        ctx.model.want (K ("sug_", i + 1));
        ctx.model.want (K ("sug_", i + 1, "_on"));
        t.onClick = [this, i] (const juce::MouseEvent&)
        {
            if (ctx.model.selIdea == i) ctx.tap ("add");      // the second click keeps it
            else ctx.tap (K ("sug_", i + 1));                 // the first one plays it
        };
        t.setHelp ("Click: hear it. Click again (or double-click): it goes into the progression - or replaces the selected chord.");
        hintAll (ctx, { &t });
        addAndMakeVisible (t);
    }
    reroll.set ("REROLL");
    reroll.setHelp ("Eight different ideas for the same spot (for the selected chord if one is selected).");
    reroll.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("reroll"); };
    add.set ("ADD");
    add.setHelp ("Puts the lit idea (or the first) into the progression.");
    add.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("add"); };
    hintAll (ctx, { &reroll, &add });
    addAndMakeVisible (reroll);
    addAndMakeVisible (add);
    addAndMakeVisible (info);
}

void IdeasPanel::update()
{
    auto& m = ctx.model;
    for (int i = 0; i < 8; ++i)
    {
        auto t = m.text (K ("sug_", i + 1));
        auto name = t.upToFirstOccurrenceOf ("   ", false, false).trim();
        auto roman = t.fromFirstOccurrenceOf ("   ", false, false).trim();
        const int cls = m.ideaClass[i];
        ideas[i].set (name, roman, i == m.selIdea);
        ideas[i].setBadge (cls >= 0 ? CLASS_NAMES[cls] : "");
        ideas[i].setAccent (cls >= 0 ? pal().cls[cls] : juce::Colours::transparentBlack);
        ideas[i].setEnabled (name.isNotEmpty() && name != "...");
    }
    add.set (m.target < m.count ? "REPLACE" : "ADD");
}

void IdeasPanel::resized()
{
    auto r = content();
    const bool showLegend = r.getHeight() > 260;
    legend = showLegend ? r.removeFromBottom (U (18)) : juce::Rectangle<int>();
    if (showLegend) r.removeFromBottom (U (6));
    info.setBounds (r.removeFromBottom (juce::jlimit (U (26), U (46), r.getHeight() / 7)));
    r.removeFromBottom (U (8));
    auto btn = r.removeFromBottom (juce::jlimit (U (32), U (48), r.getHeight() / 6));
    r.removeFromBottom (U (8));
    grid ({ &reroll, &add }, btn, 2, 8);
    const int cols = r.getWidth() > r.getHeight() * 1.6f ? 4 : 2;
    std::vector<juce::Component*> items;
    for (auto& t : ideas) items.push_back (&t);
    grid (items, r, cols, 8);
}

void IdeasPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    if (legend.isEmpty()) return;
    auto l = legend.toFloat();
    const float w = l.getWidth() / 5.0f;
    for (int i = 0; i < 5; ++i)
    {
        auto c = l.removeFromLeft (w);
        g.setColour (pal().cls[i]);
        g.fillRoundedRectangle (c.removeFromLeft (10.0f).withSizeKeepingCentre (8.0f, 8.0f), 2.0f);
        fitText (g, CLASS_NAMES[i], c.reduced (4.0f, 0), 12.0f, 1, pal().dim, juce::Justification::centredLeft, 7.0f);
    }
}

// ================================================================ Browser
BrowserPanel::BrowserPanel (Ctx& c)
    : Panel ("Harmony browser"), ctx (c), key (c, "key", {}, 12), scale (c, "scale", {}, 8),
      style (c, "style", "Style"), artist (c, "artist", "Artist"), producer (c, "producer", "Producer"),
      styleP (c, "style", "Style", true), artistP (c, "artist", "Artist", true), producerP (c, "producer", "Producer", true),
      spice (c, "spice", "Spice", pct), smix (c, "style_mix", "Style", pct), amix (c, "artist_mix", "Artist", pct),
      pmix (c, "producer_mix", "Producer", pct),
      styleInfo (c, "style_info", 14.0f, juce::Justification::centredLeft), artistInfo (c, "artist_info", 14.0f, juce::Justification::centredLeft),
      producerInfo (c, "producer_info", 14.0f, juce::Justification::centredLeft)
{
    key.setHelp ("The key. With EDIT ALL the whole progression moves with it.");
    scale.setHelp ("The scale: which chords count as in key.");
    spice.setHelp ("How far the ideas leave the key: 0 plain, 100 colourful.");
    smix.setHelp ("How strongly the STYLE shapes the ideas (50 % normal, 100 % double).");
    amix.setHelp ("How strongly the ARTIST profile shapes the ideas.");
    pmix.setHelp ("How strongly the PRODUCER profile shapes the ideas.");
    styleInfo.colour = artistInfo.colour = producerInfo.colour = pal().dim;
    for (auto* x : std::initializer_list<juce::Component*> { &key, &scale, &style, &artist, &producer, &styleP, &artistP, &producerP,
                                                             &spice, &smix, &amix, &pmix, &styleInfo, &artistInfo, &producerInfo })
        addAndMakeVisible (x);
}

void BrowserPanel::resized()
{
    auto r = content();
    lists = r.getHeight() > U (200) && r.getWidth() > 520;
    style.setVisible (lists); artist.setVisible (lists); producer.setVisible (lists);
    styleP.setVisible (! lists); artistP.setVisible (! lists); producerP.setVisible (! lists);

    const bool wideKeys = r.getWidth() > 1100;
    auto top = take (r, wideKeys ? 56 : 104, 10);
    if (wideKeys)
    {
        keyLab = top.removeFromTop (U (18));
        auto kk = top;
        key.setBounds (takeLeft (kk, (int) (kk.getWidth() * 0.6f), 14));
        scale.setBounds (kk);
        scaleLab = keyLab.withX (scale.getX()).withWidth (scale.getWidth());
        keyLab = keyLab.withWidth (key.getWidth());
        scale.labels.size();
    }
    else
    {
        keyLab = top.removeFromTop (U (18));
        key.setBounds (take (top, (top.getHeight() - 6) / 2, 6));
        scaleLab = {};
        scale.setBounds (top);
    }
    scale.repaint();

    // influence knobs on the right (or below when narrow)
    const bool knobsRight = r.getWidth() > 540;
    juce::Rectangle<int> kn;
    if (knobsRight) kn = r.removeFromRight (juce::jlimit (U (150), U (210), r.getWidth() / 4));
    else kn = r.removeFromBottom (juce::jmin (U (110), r.getHeight() / 3));
    if (knobsRight) r.removeFromRight (U (10)); else r.removeFromBottom (U (8));
    grid ({ &spice, &smix, &amix, &pmix }, kn, knobsRight ? 2 : 4, 6);

    const int infoH = r.getHeight() > 260 ? 22 : 0;
    auto info = infoH ? r.removeFromBottom (infoH * 3 + 8) : juce::Rectangle<int>();
    styleInfo.setVisible (infoH > 0); artistInfo.setVisible (infoH > 0); producerInfo.setVisible (infoH > 0);
    if (infoH)
    {
        r.removeFromBottom (U (8));
        styleInfo.setBounds (take (info, infoH, 4));
        artistInfo.setBounds (take (info, infoH, 4));
        producerInfo.setBounds (info);
    }
    if (lists)
        grid ({ &style, &artist, &producer }, r, 3, 10);
    else
    {
        auto p = r;
        const int h = juce::jmin (U (58), (p.getHeight() - 12) / 3);
        styleP.setBounds (take (p, h, 6));
        artistP.setBounds (take (p, h, 6));
        producerP.setBounds (take (p, h, 6));
    }
}

void BrowserPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    label (g, "Key", keyLab);
    if (! scaleLab.isEmpty()) label (g, "Scale", scaleLab);
}

// ================================================================ Keys
KeysPanel::KeysPanel (Ctx& c)
    : Panel ("Keys - every note as it sounds"), kb (c, true), name (c, "kb_name", 18.0f, juce::Justification::centredRight),
      notes (c, "notes", 15.0f, juce::Justification::centredLeft), diff (c, "kb_diff", 14.0f, juce::Justification::centredLeft)
{
    name.boxed = false;
    name.colour = pal().gold;
    name.weight = 2;
    diff.colour = pal().dim;
    for (auto* x : std::initializer_list<juce::Component*> { &kb, &name, &notes, &diff }) addAndMakeVisible (x);
}

void KeysPanel::resized()
{
    auto tb = titleBar();
    if (! popButton().isEmpty()) tb.removeFromRight (tb.getHeight() + 8);
    name.setBounds (tb.removeFromRight (tb.getWidth() / 3));
    auto r = content();
    const int lineH = 24;
    const bool roomy = r.getHeight() > 230, some = r.getHeight() > 150;
    legend = roomy ? r.removeFromBottom (U (16)) : juce::Rectangle<int>();
    if (roomy) r.removeFromBottom (U (4));
    diff.setVisible (roomy);
    if (roomy) { diff.setBounds (r.removeFromBottom (lineH)); r.removeFromBottom (U (4)); }
    notes.setVisible (some);
    if (some) { notes.setBounds (r.removeFromBottom (lineH)); r.removeFromBottom (U (6)); }
    kb.setBounds (r);
}

void KeysPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    auto l = legend.toFloat();
    const char* names[] = { "CHORD NOTE", "BASS", "ADDED BY A KNOB", "TAKEN AWAY" };
    juce::Colour cols[] = { pal().gold, pal().bass, pal().good, pal().bad };
    const float w = juce::jmin (180.0f, l.getWidth() / 4.0f);
    for (int i = 0; i < 4; ++i)
    {
        auto c = l.removeFromLeft (w);
        g.setColour (cols[i]);
        g.fillRoundedRectangle (c.removeFromLeft (12.0f).withSizeKeepingCentre (10.0f, 10.0f), 2.0f);
        fitText (g, names[i], c.reduced (5.0f, 0), 12.0f, 1, pal().dim, juce::Justification::centredLeft, 7.0f);
    }
}

// ================================================================ Chord edit
ChordPanel::ChordPanel (Ctx& c)
    : Panel ("Edit chord"), Live (c), slotName (c, "cursor_name", 17.0f),
      root (c, "slot_root", {}, 12), type (c, "slot_type", {}, 10), bass (c, "slot_bass", {}, 13), var (c, "slot_var", {}, 4),
      slen (c, "slot_len", {}, 3), rev (c, "slot_rev", { "FORWARD", "REVERSE" }, 2), voicing (c, "voicing", {}, 6),
      octave (c, "octave", {}, 5), lead (c, "lead", { "OFF", "ON" }, 2), bassNote (c, "bass", { "OFF", "ON" }, 2)
{
    ctx.model.want ("cursor");
    prevSlot.set ("<"); nextSlot.set (">");
    prevSlot.onClick = [this] (const juce::MouseEvent&) { ctx.setValue ("cursor", juce::jmax (1, ctx.val ("cursor") - 1)); };
    nextSlot.onClick = [this] (const juce::MouseEvent&) { ctx.setValue ("cursor", juce::jmin (juce::jmax (1, ctx.model.count), ctx.val ("cursor") + 1)); };
    slotName.colour = pal().gold;
    slotName.weight = 2;
    root.setHelp ("The root of the selected chord (stopped: you hear it; playing: it morphs).");
    type.setHelp ("The chord type of the selected chord.");
    bass.setHelp ("A slash bass for the selected chord (ROOT = none).");
    var.setHelp ("Other voicings of the same chord: SMOOTHEST follows the chord before it most closely.");
    slen.setHelp ("Half, normal or double length for this chord.");
    rev.setHelp ("REVERSE: the chord swells in like a reversed piano and stops short (MIDI: a CC 11 swell).");
    voicing.setHelp ("How every chord is spread over the keys.");
    octave.setHelp ("Moves every chord by octaves.");
    lead.setHelp ("Voice leading: each chord moves as little as possible from the one before.");
    bassNote.setHelp ("Adds the root (or slash bass) an octave or two below.");
    type.textH = 14.0f;
    for (auto* x : std::initializer_list<juce::Component*> { &prevSlot, &nextSlot, &slotName, &root, &type, &bass, &var, &slen, &rev,
                                                             &voicing, &octave, &lead, &bassNote })
        addAndMakeVisible (x);
}

void ChordPanel::update()
{
    const bool has = ctx.model.count > 0;
    for (auto* b : { &root, &type, &bass, &var, &slen, &rev }) b->setAlpha (has ? 1.0f : 0.45f);
}

void ChordPanel::resized()
{
    auto r = content();
    auto top = take (r, U (30), 8);
    prevSlot.setBounds (top.removeFromLeft (U (40)));
    nextSlot.setBounds (top.removeFromRight (U (40)));
    slotName.setBounds (top.reduced (6, 0));
    for (auto& l : labs) l = {};

    // a row = a label line + bars side by side (weights), height in units
    struct Row { std::vector<std::pair<Bar*, int>> bars; int units; };
    auto place = [this] (std::vector<Row> rows, juce::Rectangle<int> area, int& li)
    {
        int units = 0;
        for (auto& x : rows) units += x.units;
        const int labH = 17, gap = 7;
        const float uh = (area.getHeight() - (labH + 2 + gap) * (int) rows.size()) / (float) juce::jmax (1, units);
        for (auto& x : rows)
        {
            auto lab = take (area, labH, 2);
            auto bar = take (area, (int) (uh * x.units), gap);
            int wsum = 0;
            for (auto& b : x.bars) wsum += b.second;
            const int avail = bar.getWidth() - 10 * ((int) x.bars.size() - 1);
            for (size_t k = 0; k < x.bars.size(); ++k)
            {
                const int w = k + 1 == x.bars.size() ? bar.getWidth() : avail * x.bars[k].second / wsum;
                x.bars[k].first->setBounds (takeLeft (bar, w, 10));
                labs[li++] = x.bars[k].first->getBounds().withY (lab.getY()).withHeight (labH);
            }
        }
    };
    int li = 0;
    std::vector<Row> a { { { { &root, 1 } }, 1 }, { { { &type, 1 } }, 3 }, { { { &bass, 1 } }, 1 } };
    std::vector<Row> b { { { { &var, 1 } }, 1 }, { { { &slen, 3 }, { &rev, 2 } }, 1 }, { { { &voicing, 1 } }, 1 },
                         { { { &octave, 5 }, { &lead, 2 }, { &bassNote, 2 } }, 1 } };
    if (r.getWidth() > 760)
    {
        auto left = r.removeFromLeft (r.getWidth() * 56 / 100);
        r.removeFromLeft (U (16));
        place (a, left, li);
        place (b, r, li);
    }
    else
    {
        a.insert (a.end(), b.begin(), b.end());
        place (a, r, li);
    }
    repaint();
}

void ChordPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    const char* names[] = { "Root", "Type", "Slash bass", "Variation", "Length", "Reverse", "Voicing - all chords", "Octave", "Voice leading", "Bass note" };
    for (int i = 0; i < 10; ++i)
        if (! labs[i].isEmpty()) label (g, names[i], labs[i]);
}

// ================================================================ Groove
GroovePanel::GroovePanel (Ctx& c)
    : Panel ("Play & groove"),
      loopSync (c, "loop_sync", { "MANUAL", "AUTO", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS", "32 BARS" }, 8),
      direction (c, "direction", {}, 4), push (c, "push", { "OFF", "ON" }, 2), arpOct (c, "arp_oct", { "1", "2", "3" }, 3),
      strumDir (c, "strum_mode", {}, 5), mute (c, "mute", { "SOUND", "MUTED" }, 2),
      bars (c, "bars", "Chord length"), rhythm (c, "rhythm", "Rhythm / genre", true), rrate (c, "rrate", "Rhythm rate"), rate (c, "rate", "Arp rate"),
      swing (c, "swing", "Swing", pct), strum (c, "strum", "Strum", [] (int v) { return juce::String (v) + " ms"; }),
      gate (c, "gate", "Gate", pct), velocity (c, "velocity", "Velocity"), humanize (c, "humanize", "Humanize", pct),
      loopInfo (c, "loop_info", 14.0f)
{
    loopSync.setHelp ("AUTO: the chord lengths fill the DAW's loop. MANUAL: CHORD LENGTH decides. Or a fixed number of bars.");
    direction.setHelp ("The order the chords play in.");
    push.setHelp ("PUSH: every chord comes an 8th early, like a player anticipating the bar.");
    bars.setHelp ("How long each chord lasts (with LOOP SYNC = MANUAL).");
    rhythm.setHelp ("How the chords are played: sustained, stabs, arps or one of 33 genre grooves.");
    rrate.setHelp ("The step of the rhythm (AUTO: the groove's own).");
    rate.setHelp ("The step of the arpeggios.");
    arpOct.setHelp ("How many octaves an arpeggio climbs.");
    swing.setHelp ("Swing on the off-steps (50 % straight).");
    strum.setHelp ("Spreads the notes of each chord like a strummed guitar.");
    strumDir.setHelp ("Which way the strum goes.");
    gate.setHelp ("How long each note holds within its step.");
    velocity.setHelp ("The velocity of the notes.");
    humanize.setHelp ("Small changes in timing, velocity and length, up to a loose human player.");
    mute.setHelp ("MUTED: the progression stays silent (pads and clicks still sound).");
    for (auto* x : std::initializer_list<juce::Component*> { &loopSync, &direction, &push, &arpOct, &strumDir, &mute, &bars, &rhythm, &rrate,
                                                             &rate, &swing, &strum, &gate, &velocity, &humanize, &loopInfo })
        addAndMakeVisible (x);
}

void GroovePanel::resized()
{
    auto r = content();
    for (auto& l : labs) l = {};
    const bool twoCols = r.getWidth() >= 640 && r.getHeight() < 430;
    auto left = r, right = r;
    if (twoCols)
    {
        left = r.removeFromLeft (r.getWidth() * 57 / 100);
        r.removeFromLeft (U (16));
        right = r;
    }
    // left: loop sync, the four lists, direction / push / arp octaves
    auto& L = left;
    labs[0] = take (L, U (17), 2);
    loopSync.setBounds (take (L, U (30), 8));
    const bool showInfo = L.getHeight() > (twoCols ? 210 : 420);
    loopInfo.setVisible (showInfo);
    if (showInfo) loopInfo.setBounds (take (L, U (24), 8));
    const int ph = juce::jlimit (U (44), U (54), L.getHeight() / 5);
    if (L.getWidth() > 900)
    {
        auto a = take (L, ph, 8);
        const int w = (a.getWidth() - 30) / 4;
        bars.setBounds (takeLeft (a, w, 10)); rhythm.setBounds (takeLeft (a, w, 10)); rrate.setBounds (takeLeft (a, w, 10)); rate.setBounds (a);
    }
    else
    {
        auto a = take (L, ph, 8);
        bars.setBounds (takeLeft (a, (a.getWidth() - 10) / 2, 10)); rhythm.setBounds (a);
        auto a2 = take (L, ph, 8);
        rrate.setBounds (takeLeft (a2, (a2.getWidth() - 10) / 2, 10)); rate.setBounds (a2);
    }
    {
        auto lb = take (L, U (17), 2);
        auto bb = take (L, U (30), 8);
        const int unit = (bb.getWidth() - 20) / 6;
        labs[1] = lb.removeFromLeft (unit * 3); direction.setBounds (takeLeft (bb, unit * 3, 10)); lb.removeFromLeft (U (10));
        labs[2] = lb.removeFromLeft (unit + unit / 2); push.setBounds (takeLeft (bb, unit + unit / 2, 10)); lb.removeFromLeft (U (10));
        labs[3] = lb; arpOct.setBounds (bb);
    }
    // right (or below): the feel knobs, strum direction, mute
    auto& R = twoCols ? right : L;
    auto sdl = R.removeFromBottom (U (30));
    auto sl = R.removeFromBottom (U (19));
    R.removeFromBottom (U (6));
    labs[4] = sl.withWidth (sdl.getWidth() * 68 / 100);
    strumDir.setBounds (takeLeft (sdl, sdl.getWidth() * 68 / 100, 10));
    labs[5] = sl.withX (sdl.getX()).withWidth (sdl.getWidth());
    mute.setBounds (sdl);
    const int cols = R.getWidth() > R.getHeight() * 2.2f ? 5 : 3;
    grid ({ &swing, &strum, &gate, &velocity, &humanize }, R, cols, 8);
    repaint();
}

void GroovePanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    const char* names[] = { "Loop sync - chord lengths follow the DAW loop", "Direction", "Push", "Arp octaves", "Strum direction", "Progression" };
    for (int i = 0; i < 6; ++i) if (! labs[i].isEmpty()) label (g, names[i], labs[i]);
}

// ================================================================ Sound
SoundPanel::SoundPanel (Ctx& c)
    : Panel ("Sound & MIDI out"), ctx (c), sound (c, "sound", {}, 4), midiOut (c, "midi_out", { "OFF", "ON" }, 2), synth (c, "synth", { "OFF", "ON" }, 2),
      level (c, "level", "Level", pct), tone (c, "tone", "Tone", pct), release (c, "release", "Release", pct), space (c, "space", "Space", pct),
      channel (c, "channel", "Chord channel"), bassCh (c, "bass_ch", "Bass channel"),
      port (c, [&c] { return c.proc.midiPort().status(); }, 14.0f, juce::Justification::centredLeft, 2)
{
    portBtn.helpTitle = "CheatChord MIDI port";
    portBtn.setHelp ("Sends the chords out of CheatChord as its own MIDI device. In your DAW (MPC, Ableton...), set the MIDI input "
                     "of another track to \"CheatChord\" and that track plays and records every note of the chords. "
                     "Mac: works right away. Windows: install the free loopMIDI once (it brings the driver for the port). "
                     "Set the CheatChord track's own MIDI input to your keyboard, not to \"All\".");
    portBtn.set ("MIDI PORT...");
    portBtn.onClick = [this] (const juce::MouseEvent&) { portMenu(); };
    port.helpTitle = portBtn.helpTitle;
    port.help = portBtn.help;
    channel.options = nums (1, 16, "CH ");
    channel.offset = 1;
    bassCh.options = nums (1, 16, "CH ");
    bassCh.options.insert (0, "SAME AS CHORDS");
    sound.setHelp ("The built-in sound. It plays in every DAW, also where a plugin's MIDI output can't be used.");
    midiOut.setHelp ("MIDI OUT to the DAW: route this track's MIDI to any other instrument (Reaper, Bitwig, Cubase, FL Studio, Logic as AU).");
    synth.setHelp ("Keeps the built-in sound on while MIDI goes out.");
    panic.set ("ALL NOTES OFF");
    panic.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("panic"); };
    port.colour = pal().dim;
    hintAll (ctx, { &portBtn, &panic });
    for (auto* x : std::initializer_list<juce::Component*> { &sound, &midiOut, &synth, &level, &tone, &release, &space, &channel, &bassCh, &panic, &port, &portBtn })
        addAndMakeVisible (x);
}

void SoundPanel::portMenu()
{
    auto& mp = ctx.proc.midiPort();
    const auto ch = mp.choices();
    juce::PopupMenu m;
    m.addSectionHeader ("CHEATCHORD MIDI PORT");
    for (size_t i = 0; i < ch.size(); ++i)
    {
        if (i == 2) m.addSeparator();
        m.addItem ((int) i + 1, ch[i].name, true, ch[i].id == mp.selected());
    }
    juce::Component::SafePointer<SoundPanel> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&portBtn).withStandardItemHeight (U (26)), [safe, ch] (int r)
    {
        if (safe == nullptr || r <= 0 || r > (int) ch.size()) return;
        if (ch[(size_t) r - 1].id != "off" && safe->ctx.val ("midi_out") == 0) safe->ctx.setValue ("midi_out", 1);
        safe->ctx.proc.selectMidiPort (ch[(size_t) r - 1].id);
        safe->ctx.help ("CheatChord MIDI port", safe->ctx.proc.midiPort().status());
        if (safe->ctx.changed) safe->ctx.changed();
    });
}

void SoundPanel::resized()
{
    auto r = content();
    const bool wide = r.getWidth() > 700;
    for (auto& l : labs) l = {};
    if (wide)
    {
        auto left = r.removeFromLeft (r.getWidth() / 2);
        r.removeFromLeft (U (14));
        labs[0] = take (left, U (17), 2);
        sound.setBounds (take (left, juce::jmin (U (160), left.getHeight() * 55 / 100), 10));
        grid ({ &level, &tone, &release, &space }, left, 4, 8);
        auto a = take (r, U (17), 2);
        auto b = take (r, U (32), 10);
        labs[1] = a.removeFromLeft ((a.getWidth() - 10) / 2);
        a.removeFromLeft (U (10));
        labs[2] = a;
        midiOut.setBounds (takeLeft (b, (b.getWidth() - 10) / 2, 10));
        synth.setBounds (b);
        auto c = take (r, U (54), 10);
        channel.setBounds (takeLeft (c, (c.getWidth() - 10) / 2, 10));
        bassCh.setBounds (c);
        auto e = take (r, U (40), 8);
        portBtn.setBounds (e.removeFromLeft (juce::jmin (U (230), e.getWidth() / 2)));
        e.removeFromLeft (U (8));
        port.setBounds (e);
        auto d = take (r, U (34), 0);
        panic.setBounds (d.removeFromRight (juce::jmin (U (170), d.getWidth() / 2)));
    }
    else
    {
        labs[0] = take (r, U (17), 2);
        sound.setBounds (take (r, juce::jmin (U (150), r.getHeight() * 35 / 100), 8));
        grid ({ &level, &tone, &release, &space }, take (r, juce::jmin (U (110), r.getHeight() / 3), 8), 4, 8);
        auto a = take (r, U (17), 2);
        auto b = take (r, U (30), 8);
        labs[1] = a.removeFromLeft ((a.getWidth() - 10) / 2);
        a.removeFromLeft (U (10));
        labs[2] = a;
        midiOut.setBounds (takeLeft (b, (b.getWidth() - 10) / 2, 10));
        synth.setBounds (b);
        auto c = take (r, U (50), 8);
        channel.setBounds (takeLeft (c, (c.getWidth() - 10) / 2, 10));
        bassCh.setBounds (c);
        auto e = take (r, U (38), 8);
        portBtn.setBounds (e.removeFromLeft (juce::jmin (U (210), e.getWidth() / 2)));
        e.removeFromLeft (U (8));
        port.setBounds (e);
        auto d = take (r, U (30), 0);
        panic.setBounds (d.removeFromRight (U (160)));
    }
    repaint();
}

void SoundPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    label (g, "Built-in sound", labs[0]);
    label (g, "MIDI out to the DAW", labs[1]);
    label (g, "Synth with MIDI", labs[2]);
}

// ================================================================ Pads
PadsPanel::PadsPanel (Ctx& c)
    : Panel ("Pads & keys in"), padBase (c, "pad_base", "Pad 1 note"), padCount (c, "pad_count", "Pads used"),
      velMode (c, "vel_mode", { "AS PLAYED", "FIXED" }, 2), learnInfo (c, "learn_info", 14.0f), lastNote (c, "last_note", 14.0f),
      about (c, [] { return juce::String ("A MIDI keyboard or pad controller plays the chords: the first 8 pads play the progression, the next 8 the ideas. "
                                          "LEARN: hit the first and the last pad you want to use."); }, 14.0f, juce::Justification::centredLeft, 3)
{
    for (int n = 0; n <= 112; ++n)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        padBase.options.add (juce::String (n) + "  (" + names[n % 12] + juce::String (n / 12 - 2) + ")");
    }
    padCount.options = nums (1, 16);
    padCount.offset = 1;
    // pad_count is 1..16: the picker shows index 0 = 1
    learn.set ("LEARN PADS");
    learn.setHelp ("Hit the first pad, then the last. For scale or chord pad layouts, hit the pads between once, then LEARN again.");
    learn.onClick = [&c] (const juce::MouseEvent&) { c.tap ("learn"); };
    velMode.setHelp ("AS PLAYED: the pad's velocity. FIXED: the VELOCITY knob.");
    about.boxed = false;
    about.colour = pal().dim;
    for (auto* x : std::initializer_list<juce::Component*> { &padBase, &padCount, &velMode, &learn, &learnInfo, &lastNote, &about }) addAndMakeVisible (x);
}

void PadsPanel::resized()
{
    auto r = content();
    auto a = take (r, U (54), 10);
    padBase.setBounds (takeLeft (a, (a.getWidth() - 10) / 2, 10));
    padCount.setBounds (a);
    labs[0] = take (r, U (17), 2);
    velMode.setBounds (take (r, U (30), 10));
    learn.setBounds (take (r, U (44), 10));
    learnInfo.setBounds (take (r, U (26), 6));
    lastNote.setBounds (take (r, U (26), 8));
    about.setBounds (r);
}

void PadsPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    label (g, "Pad velocity", labs[0]);
}

// ================================================================ History
HistoryPanel::HistoryPanel (Ctx& c) : Panel ("History"), Live (c), pos (c, "hist_pos", 14.0f)
{
    for (int i = 0; i < 8; ++i)
    {
        ctx.model.want (K ("hist_", i + 1));
        ctx.model.want (K ("hist_", i + 1, "_on"));
        rows[i].style = Tile::Chord;
        rows[i].onClick = [this, i] (const juce::MouseEvent&) { ctx.tap (K ("hist_", i + 1)); };
        rows[i].setHelp ("Click: this becomes the progression again. Nothing is lost.");
        hintAll (ctx, { &rows[i] });
        addAndMakeVisible (rows[i]);
    }
    undo.set ("UNDO"); undo.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("undo"); };
    redo.set ("REDO"); redo.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("redo"); };
    addAndMakeVisible (undo);
    addAndMakeVisible (redo);
    addAndMakeVisible (pos);
}

void HistoryPanel::update()
{
    for (int i = 0; i < 8; ++i)
    {
        auto t = ctx.text (K ("hist_", i + 1));
        rows[i].set (t, {}, ctx.val (K ("hist_", i + 1, "_on")) == 1);
        rows[i].setVisible (t.isNotEmpty());
    }
}

void HistoryPanel::resized()
{
    auto r = content();
    auto b = r.removeFromBottom (U (36));
    r.removeFromBottom (U (8));
    undo.setBounds (takeLeft (b, b.getWidth() / 3, 8));
    redo.setBounds (takeLeft (b, b.getWidth() / 2, 8));
    pos.setBounds (b);
    const int fit = juce::jlimit (U (1), U (8), (r.getHeight() + 5) / 31);   // rows of 26 px or more
    const float h = juce::jmin (44.0f, (r.getHeight() - (fit - 1) * 5) / (float) fit);
    for (int i = 0; i < 8; ++i)
    {
        rows[i].setBounds (juce::Rectangle<float> ((float) r.getX(), r.getY() + i * (h + 5), (float) r.getWidth(), juce::jmin (h, 44.0f)).toNearestInt());
        if (i >= fit) rows[i].setBounds ({});
    }
    for (auto& t : rows) t.setTextScale (0.95f);
}

// ================================================================ Presets
PresetsPanel::PresetsPanel (Ctx& c)
    : Panel ("Presets"), Live (c), slot (c, "save_slot", ""), slotName (c, "save_slot_name", 14.0f), info (c, "preset_info", 14.0f, juce::Justification::centred, 2)
{
    for (int i = 0; i < 24; ++i)
    {
        ctx.model.want (K ("pre_", i + 1));
        ctx.model.want (K ("pre_", i + 1, "_on"));
        fac[i].onClick = [this, i] (const juce::MouseEvent&) { ctx.tap (K ("pre_", i + 1)); };
        addAndMakeVisible (fac[i]);
    }
    for (int i = 0; i < 16; ++i)
    {
        ctx.model.want (K ("usr_", i + 1));
        ctx.model.want (K ("usr_", i + 1, "_on"));
        usr[i].onClick = [this, i] (const juce::MouseEvent&) { ctx.tap (K ("usr_", i + 1)); };
        addAndMakeVisible (usr[i]);
    }
    slot.options = nums (1, 16, "USER ");
    slot.offset = 1;
    save.set ("SAVE");
    save.setHelp ("Saves the progression and every setting into the user slot chosen on the left.");
    save.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("save_preset"); };
    openDir.set ("FOLDER");
    openDir.setHelp ("Opens Documents / JJ DA PLUG / CheatChord / Presets.");
    openDir.onClick = [] (const juce::MouseEvent&) { juce::File (cc_docs_root()).getChildFile ("Presets").startAsProcess(); };
    hintAll (ctx, { &save, &openDir });
    for (auto* x : std::initializer_list<juce::Component*> { &slot, &save, &openDir, &slotName, &info }) addAndMakeVisible (x);
}

void PresetsPanel::update()
{
    for (int i = 0; i < 24; ++i) fac[i].set (ctx.text (K ("pre_", i + 1)), {}, ctx.val (K ("pre_", i + 1, "_on")) == 1);
    for (int i = 0; i < 16; ++i)
    {
        auto t = ctx.text (K ("usr_", i + 1));
        usr[i].set (t.isEmpty() ? juce::String ("USER ") + juce::String (i + 1) : t, {}, ctx.val (K ("usr_", i + 1, "_on")) == 1);
        usr[i].setDim (t.startsWith ("USER") || t.isEmpty() || t.containsIgnoreCase ("EMPTY"));
    }
}

void PresetsPanel::resized()
{
    auto r = content();
    info.setBounds (r.removeFromBottom (juce::jlimit (U (28), U (44), r.getHeight() / 8)));
    r.removeFromBottom (U (8));
    auto sv = r.removeFromBottom (U (34));
    r.removeFromBottom (U (8));
    slot.setBounds (takeLeft (sv, juce::jmin (U (200), sv.getWidth() / 3), 8));
    openDir.setBounds (sv.removeFromRight (juce::jmin (U (100), sv.getWidth() / 4)));
    sv.removeFromRight (U (8));
    save.setBounds (sv.removeFromRight (juce::jmin (U (120), sv.getWidth() / 3)));
    sv.removeFromRight (U (8));
    slotName.setBounds (sv);

    const bool wide = r.getWidth() > 700;
    const int fcols = wide ? 6 : 4, ucols = wide ? 8 : 4;
    const int frows = 24 / fcols, urows = 16 / ucols;
    const int avail = r.getHeight() - 17 * 2 - 10;
    const int fh = avail * frows / (frows + urows);
    facLab = take (r, U (17), 2);
    std::vector<juce::Component*> f, u;
    for (auto& t : fac) f.push_back (&t);
    for (auto& t : usr) u.push_back (&t);
    grid (f, take (r, fh, 10), fcols, 5);
    usrLab = take (r, U (17), 2);
    grid (u, r, ucols, 5);
    for (auto& t : fac) t.setTextScale (0.72f);
    for (auto& t : usr) t.setTextScale (0.72f);
}

void PresetsPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    label (g, "Factory", facLab);
    label (g, "My presets", usrLab);
}

// ================================================================ Library (progression templates)
LibraryPanel::LibraryPanel (Ctx& c) : Panel ("Progression library"), list (c, "template", "Formula"), chords (c, "tpl_info", 17.0f)
{
    chords.colour = pal().gold;
    load.set ("LOAD INTO PROGRESSION");
    load.setHelp ("Writes the formula's chords in your key into the progression (UNDO brings the old one back).");
    load.onClick = [&c] (const juce::MouseEvent&) { c.tap ("load"); };
    addAndMakeVisible (list);
    addAndMakeVisible (chords);
    addAndMakeVisible (load);
}

void LibraryPanel::resized()
{
    auto r = content();
    load.setBounds (r.removeFromBottom (U (40)));
    r.removeFromBottom (U (8));
    chords.setBounds (r.removeFromBottom (U (34)));
    r.removeFromBottom (U (8));
    list.setBounds (r);
}

void LibraryPanel::paint (juce::Graphics& g) { Panel::paint (g); }

// ================================================================ Profile (the artist / producer library)
ProfilePanel::ProfilePanel (Ctx& c) : Panel ("Artist & producer profile"), kind (c, "lib_kind", { "ARTIST", "PRODUCER" }, 2)
{
    kind.setHelp ("Which profile the lines below describe: the chosen artist or the chosen producer.");
    addAndMakeVisible (kind);
    for (int i = 0; i < 7; ++i)
    {
        lines[i] = std::make_unique<Line> (c, K ("prof_", i + 1), i == 0 ? 18.0f : 15.0f, juce::Justification::centredLeft);
        if (i == 0) { lines[i]->colour = pal().gold; lines[i]->weight = 2; }
        addAndMakeVisible (*lines[i]);
    }
}

void ProfilePanel::resized()
{
    auto r = content();
    kind.setBounds (take (r, U (32), 10));
    const float h = (r.getHeight() - 6 * 5) / 7.0f;
    for (int i = 0; i < 7; ++i)
        lines[i]->setBounds (juce::Rectangle<float> ((float) r.getX(), r.getY() + i * (h + 5), (float) r.getWidth(), juce::jmin (h, 40.0f)).toNearestInt());
}

// ================================================================ Transport (big)
TransportPanel::TransportPanel (Ctx& c)
    : Panel ("Play"), Live (c), mode (c, "listen", { "OFF", "HOST", "LISTEN" }, 3), now (c, "now", 17.0f),
      help (c, "mode_help", 14.0f, juce::Justification::centred, 2)
{
    ctx.model.want ("run");
    mode.setHelp ("OFF: only clicks and pads sound. HOST: the progression plays with the DAW's transport. LISTEN: it plays on its own at the DAW's tempo.");
    play.set ("PLAY");
    play.onClick = [this] (const juce::MouseEvent&) { ctx.setValue ("run", ctx.val ("run") ? 0 : 1); };
    play.setHelp ("LISTEN: plays the progression in a loop here, without starting the DAW.");
    stop.set ("STOP"); stop.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("stop"); };
    cruise.set ("CRUISE"); cruise.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("cruise"); };
    generate.set ("GENERATE"); generate.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("generate"); };
    undo.set ("UNDO"); undo.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("undo"); };
    redo.set ("REDO"); redo.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("redo"); };
    reroll.set ("REROLL IDEAS"); reroll.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("reroll"); };
    now.colour = pal().gold;
    help.colour = pal().dim;
    hintAll (ctx, { &play });
    for (auto* x : std::initializer_list<juce::Component*> { &mode, &play, &stop, &cruise, &generate, &undo, &redo, &reroll, &now, &help })
        addAndMakeVisible (x);
}

void TransportPanel::update()
{
    play.setHot (ctx.model.playSlot >= 0);
    play.set (ctx.val ("run") ? "PLAYING" : "PLAY");
}

void TransportPanel::resized()
{
    auto r = content();
    const int H = r.getHeight();
    mode.setBounds (take (r, juce::jlimit (U (28), U (44), H / 7), 8));
    const bool showHelp = H > 260;
    help.setVisible (showHelp);
    if (showHelp) help.setBounds (take (r, U (38), 8));
    auto nowR = r.removeFromBottom (juce::jlimit (U (24), U (34), H / 9));
    r.removeFromBottom (U (8));
    now.setBounds (nowR);
    const int rowH = (r.getHeight() - 16) / 3;
    grid ({ &play, &stop }, take (r, rowH, 8), 2, 10);
    grid ({ &cruise, &generate }, take (r, rowH, 8), 2, 10);
    grid ({ &undo, &redo, &reroll }, r, 3, 10);
}

// ================================================================ Help
HelpPanel::HelpPanel (Ctx&) : Panel ("How it works") {}

void HelpPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    auto r = content();
    auto img = jfImage();
    auto side = r.removeFromRight (juce::jmin (U (170), r.getWidth() / 4));
    if (img.isValid())
        g.drawImageWithin (img, side.getX(), side.getY(), side.getWidth(), side.getHeight() - 40, juce::RectanglePlacement::centred);
    fitText (g, "CHEATCHORD by JJ DA PLUG", side.removeFromBottom (U (36)).toFloat(), 14.0f, 2, pal().gold, juce::Justification::centred);
    const juce::String t =
        "1  Pick a KEY, SCALE and STYLE - add an ARTIST and a PRODUCER for their harmonic habits.\n"
        "2  IDEAS: click one to hear it, click again to keep it. REROLL deals new ones.\n"
        "3  CRUISE adds the next chord that fits. GENERATE writes a whole progression.\n"
        "4  Click a chord in the PROGRESSION to select it; the ideas then replace it. Right-click: length, reverse.\n"
        "5  PLAY MODE HOST plays along with your DAW (LOOP SYNC AUTO fills its loop); LISTEN plays on its own.\n"
        "6  DRAG MIDI onto a track - or use MIDI OUT to play any instrument. The built-in sound plays everywhere.\n"
        "7  A MIDI keyboard plays the chords: pads 1-8 the progression, 9-16 the ideas (LEARN for any layout).\n\n"
        "Keys: G generate, C cruise, R reroll, A add, Ctrl+Z undo, Ctrl+Y redo, 1-8 select a chord, Del delete.\n"
        "Layouts: the LAYOUT menu on top. Your own layouts: Documents / JJ DA PLUG / CheatChord / Layouts.";
    fitText (g, t, r.reduced (4).toFloat(), 16.0f, 0, pal().text, juce::Justification::topLeft, 9.0f, 30);
}

// ================================================================ Header
Header::Header (Ctx& c)
    : Live (c), sel1 (c, "sel1", 24.0f, juce::Justification::centredLeft), sel2 (c, "sel2", 15.0f, juce::Justification::centredLeft),
      chord (c, "shown", 26.0f, juce::Justification::centred), mini (c, false), mode (c, "listen", { "OFF", "HOST", "LISTEN" }, 3)
{
    ctx.model.want ("run");
    sel1.boxed = sel2.boxed = chord.boxed = false;
    sel1.colour = pal().gold; sel1.weight = 2;
    sel2.colour = pal().dim;
    chord.colour = pal().text; chord.weight = 2;
    mode.setHelp ("OFF: only clicks and pads sound. HOST: plays with the DAW's transport. LISTEN: plays on its own at the DAW's tempo.");
    play.set ("PLAY");
    play.setHelp ("LISTEN: plays the progression in a loop here, without starting the DAW.");
    play.onClick = [this] (const juce::MouseEvent&) { ctx.setValue ("run", ctx.val ("run") ? 0 : 1); };
    stop.set ("STOP");
    stop.onClick = [this] (const juce::MouseEvent&) { ctx.tap ("stop"); };
    layoutBtn.setHelp ("Choose a layout, a colour theme, or load your own layout files.");
    zoomBtn.setHelp ("The size of everything (the window follows).");
    helpBtn.set ("?");
    helpBtn.setHelp ("Help: how CheatChord works, and the keyboard shortcuts.");
    editBtn.set ("EDIT");
    editBtn.helpTitle = "Edit layout";
    editBtn.setHelp ("Arrange the panels yourself: drag a panel to move it, drag its corner to resize it, drop it on another panel "
                     "to put them together as tabs. Right-click a panel or tab for more (own place, own window, swap).");
    helperBtn.helpTitle = "Helper";
    helperBtn.setHelp ("Switches this small help box on or off. It explains whatever the mouse points at.");
    hintAll (ctx, { &play, &layoutBtn, &zoomBtn, &helpBtn, &editBtn, &helperBtn });
    mode.setHelp ("Play mode", "OFF: only clicks and pads sound. HOST: the progression plays with the DAW's transport. LISTEN: it plays on its own at the DAW's tempo.");
    for (auto* x : std::initializer_list<juce::Component*> { &sel1, &sel2, &chord, &mini, &mode, &play, &stop, &layoutBtn, &zoomBtn, &helpBtn, &editBtn, &helperBtn })
        addAndMakeVisible (x);
}

void Header::update()
{
    play.setHot (ctx.model.playSlot >= 0);
    play.set (ctx.val ("run") ? "PLAYING" : "PLAY");
}

void Header::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    const int h = r.getHeight();
    logo = takeLeft (r, h * 560 / 168, 16);
    auto right = r.removeFromRight (juce::jmin (U (560), r.getWidth() * 36 / 100));
    const int bh = juce::jmin (h, U (44));
    auto row = right.withSizeKeepingCentre (right.getWidth(), bh);
    const int unit = (row.getWidth() - 4 * 6) / 13;
    helpBtn.setBounds (row.removeFromRight (unit * 1 + unit / 2)); row.removeFromRight (6);
    zoomBtn.setBounds (row.removeFromRight (unit * 2)); row.removeFromRight (6);
    layoutBtn.setBounds (row.removeFromRight (unit * 4)); row.removeFromRight (6);
    helperBtn.setBounds (row.removeFromRight (unit * 3)); row.removeFromRight (6);
    editBtn.setBounds (row);
    r.removeFromRight (U (12));
    auto tr = r.removeFromRight (juce::jmin (U (420), r.getWidth() * 45 / 100));
    r.removeFromRight (U (14));
    auto modeR = takeLeft (tr, tr.getWidth() * 48 / 100, 8);
    mode.setBounds (modeR.withSizeKeepingCentre (modeR.getWidth(), juce::jmin (U (38), h)));
    const int pw = (tr.getWidth() - 8) / 2;
    play.setBounds (tr.removeFromLeft (pw).withSizeKeepingCentre (pw, bh));
    tr.removeFromLeft (8);
    stop.setBounds (tr.withSizeKeepingCentre (tr.getWidth(), bh));
    auto summary = takeLeft (r, juce::jmax (U (150), r.getWidth() - (h * 2 + U (120))), 12);
    sel1.setBounds (summary.removeFromTop (summary.getHeight() * 55 / 100));
    sel2.setBounds (summary);
    mini.setBounds (takeLeft (r, juce::jmin (h * 2 + 30, r.getWidth() / 2), 10));
    chord.setBounds (r);
}

void Header::paint (juce::Graphics& g)
{
    g.setColour (pal().panel);
    g.fillRect (getLocalBounds());
    g.setColour (pal().goldDim.withAlpha (0.5f));
    g.fillRect (0, getHeight() - 1, getWidth(), 1);
    auto img = logoImage();
    if (img.isValid())
        g.drawImageWithin (img, logo.getX(), logo.getY(), logo.getWidth(), logo.getHeight(), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
}

// ================================================================ Footer
Footer::Footer (Ctx& c) : Live (c) {}

void Footer::update()
{
    const int b = ctx.model.busy;
    juce::String e = "BPM " + juce::String (ctx.model.bpm, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + "   " + JucePlugin_VersionString;
    if (b != busy || e != extra) { busy = b; extra = e; repaint(); }
}

void Footer::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (14, 0).toFloat();
    g.setColour (pal().panel);
    g.fillRect (getLocalBounds());
    g.setColour (pal().border);
    g.fillRect (0, 0, getWidth(), 1);
    auto right = r.removeFromRight (U (220));
    fitText (g, extra, right, 13.0f, 1, pal().faint, juce::Justification::centredRight);
    if (busy)
    {
        auto b = r.removeFromRight (U (110));
        fitText (g, "THINKING...", b, 13.0f, 2, pal().gold, juce::Justification::centredRight);
    }
    fitText (g, hint.isNotEmpty() ? hint : juce::String ("Hover over anything for help."), r, 14.0f, 0, pal().dim, juce::Justification::centredLeft);
}

// ================================================================ factory
const std::vector<PanelInfo>& panelList()
{
    static const std::vector<PanelInfo> v {
        { "progression", "Progression", "Your chords, 4 or 8. Click one to hear and select it; the ideas then replace it. CRUISE adds the next chord that fits, GENERATE writes a whole progression, DRAG MIDI puts it into your DAW." },
        { "ideas", "Ideas", "Eight chords that fit the selected spot, coloured by family (in key, borrowed, leading to the next, substitute, spicy). Click to hear, click again to keep." },
        { "browser", "Browser", "Key, scale, style, artist and producer, with search. The four knobs set how strongly each one shapes the ideas." },
        { "keys", "Keys", "The big keyboard: every note of the shown chord as it sounds. Gold = chord note, orange = bass, green = added by a knob, red = taken away." },
        { "chord", "Chord", "Edit the selected chord: root, type, slash bass, voicing variation, length, reverse; voicing and octave for all chords." },
        { "groove", "Groove", "How the progression plays: loop sync with the DAW, chord length, rhythm or genre groove, arps, swing, strum, humanize." },
        { "sound", "Sound", "The built-in instrument (16 sounds) and MIDI out to the DAW." },
        { "pads", "Pads", "A MIDI keyboard or pad controller: pads 1-8 play the progression, 9-16 the ideas. LEARN maps any pad layout." },
        { "history", "History", "Every change you made. Click a line to go back to it; nothing is ever lost." },
        { "presets", "Presets", "24 factory presets and 16 slots of your own." },
        { "library", "Library", "26 classic progression formulas, written into your key." },
        { "profile", "Profile", "What the chosen artist or producer typically does with chords." },
        { "transport", "Play", "Big buttons: play mode, play, stop, cruise, generate, undo." },
        { "help", "Help", "How CheatChord works and the keyboard shortcuts." },
    };
    return v;
}

static std::unique_ptr<Panel> makeRaw (const juce::String& id, Ctx& c)
{
    if (id == "progression") return std::make_unique<ProgressionPanel> (c);
    if (id == "ideas") return std::make_unique<IdeasPanel> (c);
    if (id == "browser") return std::make_unique<BrowserPanel> (c);
    if (id == "keys") return std::make_unique<KeysPanel> (c);
    if (id == "chord") return std::make_unique<ChordPanel> (c);
    if (id == "groove") return std::make_unique<GroovePanel> (c);
    if (id == "sound") return std::make_unique<SoundPanel> (c);
    if (id == "pads") return std::make_unique<PadsPanel> (c);
    if (id == "history") return std::make_unique<HistoryPanel> (c);
    if (id == "presets") return std::make_unique<PresetsPanel> (c);
    if (id == "library") return std::make_unique<LibraryPanel> (c);
    if (id == "profile") return std::make_unique<ProfilePanel> (c);
    if (id == "transport") return std::make_unique<TransportPanel> (c);
    if (id == "help") return std::make_unique<HelpPanel> (c);
    return nullptr;
}

std::unique_ptr<Panel> makePanel (const juce::String& id, Ctx& c)
{
    auto p = makeRaw (id, c);
    if (p == nullptr) return p;
    p->id = id;
    p->accent = panelColour (id);
    for (auto& i : panelList())
        if (id == i.id) { p->title = i.title; p->about = i.about; }
    p->onHint = [&c] (const juce::String& t, const juce::String& h) { c.help (t, h); };
    p->onPopOut = [&c, id] { if (c.popOut) c.popOut (id); };
    return p;
}

} // namespace cc::ui
