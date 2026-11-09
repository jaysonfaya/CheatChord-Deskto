// CheatChord desktop: the groove library. A groove is a complete way of playing the chords: the rhythm pattern and its
// grid, swing, note length (gate), push, strum, velocity and humanize, and for arpeggios their rate and octaves.
// Values follow how the styles are played (tempo, swing amounts of the classic drum machines and players, where the
// chords sit in the bar). styles: the CheatChord styles the groove fits (names as in the STYLE list, "|" between).
//
// Index reference (tools/params.json):
//   rhythm  0 Sustain 1 Whole 2 Half 3 Quarters 4 Eighths 5 Offbeats 6 Charleston 7 Tresillo 8 Funk 16ths 9 Garage
//           10 Neo Soul Lazy 11 Arp Up 12 Arp Down 13 Arp Up/Down 14 Arp Random 15 Broken Chord 16 Boom Bap Stab
//           17 Trap Pad 18 Drill Slide 19 Dembow 20 Afrobeats 21 Amapiano 22 Deep House 23 2-Step 24 Disco Chops
//           25 Funk Hits 26 Gospel Shout 27 Bossa Comp 28 Samba 29 Salsa Montuno 30 Son Clave 31 Reggae One Drop
//           32 Ska Upstroke 33 Dancehall 34 Motown Beat 35 Doo-Wop 12/8 36 Blues Shuffle 37 Country Boom 38 Rock Drive
//           39 Pop Ballad 40 Synthwave 16 41 EDM Pump 42 Future Chops 43 Trance Gate 44 Lo-Fi Late 45 Habanera
//           46 Jersey Club 47 Trap Triplets 48 Afro House
//   rrate   0 Auto (the pattern's own grid) 6 1/16 7 1/8T 8 1/8 10 1/4
//   rate    (arps) 4 1/16T 5 1/16 6 1/8T 7 1/8 9 1/4
//   strumMode 0 Up 1 Down 2 Alternate 3 Guitar 4 Random
#pragma once

namespace cc {

struct Groove
{
    const char* name;
    const char* family;
    const char* about;    // what it is, the tempo it lives at
    const char* styles;
    int rhythm, rrate, swing, gate, push, strum, strumMode, velocity, humanize, rate, arpOct;
};

static const Groove GROOVES_LIB[] = {
    // ---------------------------------------------------------------- Hip-hop
    { "Dilla Drunk", "Hip-Hop", "Behind the beat and off the grid, like an unquantized MPC 3000. 86-95 BPM.",
      "Neo Soul|Lo-Fi|Boom Bap|90s R&B", 10, 0, 62, 86, 0, 16, 0, 92, 45, 5, 0 },
    { "Boom Bap 90s", "Hip-Hop", "Sample stabs on the one and the and-of-three, MPC 60 swing around 58 %. 85-95 BPM.",
      "Boom Bap|Jazz|Lo-Fi", 16, 0, 58, 62, 0, 4, 0, 104, 14, 5, 0 },
    { "Premier Chops", "Hip-Hop", "Short, hard-cut chops on a tight grid, little swing. 88-96 BPM.",
      "Boom Bap", 16, 0, 54, 32, 0, 0, 0, 112, 8, 5, 0 },
    { "G-Funk Bounce", "Hip-Hop", "West-coast 16ths with a light swing, synth-lead lazy. 90-100 BPM.",
      "Boom Bap|70s Soul|90s R&B", 8, 0, 56, 55, 0, 6, 2, 100, 12, 5, 0 },
    { "Lo-Fi Tape", "Hip-Hop", "Late, soft hits with a drag, the study-beats feel. 70-85 BPM.",
      "Lo-Fi|Neo Soul|Jazz", 44, 0, 60, 94, 0, 24, 0, 80, 35, 5, 0 },
    { "Lo-Fi Rhodes Hold", "Hip-Hop", "Long held Rhodes chords, slowly rolled. 70-85 BPM.",
      "Lo-Fi|Ambient|Jazz", 0, 0, 50, 100, 0, 32, 0, 76, 25, 5, 0 },
    { "Trap Pad", "Hip-Hop", "Half-time pads that hold, a re-hit before the turn. Straight, 130-150 BPM.",
      "Trap|Cinematic", 17, 0, 50, 100, 0, 0, 0, 98, 5, 5, 0 },
    { "Trap Triplets", "Hip-Hop", "Triplet hits under triplet flows. 130-150 BPM (felt half).",
      "Trap", 47, 0, 50, 70, 0, 0, 0, 102, 6, 5, 0 },
    { "Drill Slide", "Hip-Hop", "Syncopated UK/NY drill hits around the sliding 808. 140-145 BPM.",
      "Drill|Trap", 18, 0, 50, 80, 0, 0, 0, 100, 6, 5, 0 },
    { "Rage Arp", "Hip-Hop", "Bright synth arps in 16ths over two octaves. 150-165 BPM.",
      "Trap|EDM", 11, 0, 50, 50, 0, 0, 0, 110, 4, 5, 1 },
    // ---------------------------------------------------------------- R&B and soul
    { "Neo Soul Pocket", "R&B & Soul", "Pushed chords that land just before the bar, laid back. 80-95 BPM.",
      "Neo Soul|90s R&B|Gospel", 10, 0, 60, 90, 1, 12, 0, 90, 30, 5, 0 },
    { "90s R&B Glide", "R&B & Soul", "Slow-jam held chords, changes pushed an eighth early. 65-90 BPM.",
      "90s R&B|Neo Soul|Pop", 39, 0, 56, 96, 1, 14, 0, 92, 18, 5, 0 },
    { "Gospel Shout", "R&B & Soul", "Church shout, swung hits on every beat with a push. 120-140 BPM.",
      "Gospel", 26, 0, 66, 70, 0, 6, 0, 116, 14, 5, 0 },
    { "Gospel 6/8 Ballad", "R&B & Soul", "Broken chords in triplets, a slow worship ballad. 60-75 BPM.",
      "Gospel|Romantic|90s R&B", 15, 0, 50, 100, 0, 0, 0, 88, 16, 6, 0 },
    { "Motown Beat", "R&B & Soul", "Four-on-the-floor piano pulse with backbeat accents. 120-130 BPM.",
      "Motown|70s Soul|60s Brit Pop", 34, 0, 50, 70, 0, 4, 1, 104, 10, 5, 0 },
    { "70s Soul Strut", "R&B & Soul", "Funky short chords with a little swing, Philly and Stax. 95-110 BPM.",
      "70s Soul|Motown|Disco", 25, 0, 54, 48, 0, 6, 2, 102, 14, 5, 0 },
    { "Doo-Wop 12/8", "R&B & Soul", "Triplet piano on every eighth of a 12/8 bar. 60-80 BPM.",
      "Motown|60s Brit Pop|Blues", 35, 0, 50, 75, 0, 0, 0, 92, 12, 5, 0 },
    // ---------------------------------------------------------------- Funk and disco
    { "Funk 16ths", "Funk & Disco", "Scratchy 16th chops, heavy on the one. 100-115 BPM.",
      "80s Pop Funk|70s Soul|Disco", 8, 0, 52, 40, 0, 8, 2, 108, 10, 5, 0 },
    { "Funk Hits", "Funk & Disco", "Sharp short horn-section stabs. 100-118 BPM.",
      "80s Pop Funk|70s Soul|City Pop", 25, 0, 52, 30, 0, 0, 0, 112, 8, 5, 0 },
    { "Disco Chops", "Funk & Disco", "Off-beat guitar-style chops over four-on-the-floor. 116-124 BPM.",
      "Disco|House|80s Pop Funk", 24, 0, 50, 45, 0, 6, 2, 104, 8, 5, 0 },
    { "80s Pop Funk", "Funk & Disco", "Even 8ths, short and bright, alternating strums. 110-120 BPM.",
      "80s Pop Funk|Pop|City Pop|K-Pop", 4, 0, 50, 55, 0, 6, 2, 100, 8, 5, 0 },
    { "City Pop Glide", "Funk & Disco", "Smooth 16th guitar-and-keys groove, slightly swung. 100-115 BPM.",
      "City Pop|80s Pop Funk|Disco", 8, 0, 53, 52, 0, 10, 3, 96, 12, 5, 0 },
    // ---------------------------------------------------------------- House and dance
    { "Deep House Stabs", "House & Dance", "Syncopated organ and Rhodes stabs, a touch of swing. 120-124 BPM.",
      "House|UK Garage", 22, 0, 54, 45, 0, 0, 0, 102, 8, 5, 0 },
    { "Piano House", "House & Dance", "Off-beat piano stabs, the 90s house anthem feel. 122-126 BPM.",
      "House|Disco|EDM", 5, 0, 50, 60, 0, 0, 0, 112, 6, 5, 0 },
    { "UK Garage 2-Step", "House & Dance", "Skippy, swung 16ths with gaps where the kick drops. 130-135 BPM.",
      "UK Garage|House|90s R&B", 23, 0, 62, 55, 0, 0, 0, 100, 10, 5, 0 },
    { "Speed Garage", "House & Dance", "Shuffled stabs with a heavy swing. 130-135 BPM.",
      "UK Garage|House", 9, 0, 64, 50, 0, 0, 0, 104, 8, 5, 0 },
    { "Afro House", "House & Dance", "Rolling percussive keys on the offbeats. 120-124 BPM.",
      "House|Afrobeats|Amapiano", 48, 0, 54, 55, 0, 0, 0, 100, 12, 5, 0 },
    { "Jersey Club", "House & Dance", "Bouncy hits after the kick pattern of Jersey club. 130-140 BPM.",
      "EDM|Trap|K-Pop", 46, 0, 50, 45, 0, 0, 0, 108, 6, 5, 0 },
    // ---------------------------------------------------------------- Electronic
    { "EDM Pump", "Electronic", "Chords that breathe with a sidechained kick, off-beat swells. 124-128 BPM.",
      "EDM|House|Pop", 41, 0, 50, 90, 0, 0, 0, 108, 4, 5, 0 },
    { "Trance Gate", "Electronic", "Gated 16th pads, the classic trance chop. 136-140 BPM.",
      "EDM|Synthwave", 43, 0, 50, 50, 0, 0, 0, 104, 2, 5, 0 },
    { "Future Bass Chops", "Electronic", "Chopped supersaw chords in a half-time bounce. 140-160 BPM.",
      "Future Bass|EDM|K-Pop", 42, 0, 50, 60, 0, 0, 0, 106, 4, 5, 0 },
    { "Synthwave 16", "Electronic", "Driving 16ths with accents on the beat, 80s style. 85-110 BPM.",
      "Synthwave|80s Pop Funk|Cinematic", 40, 0, 50, 60, 0, 0, 0, 100, 4, 5, 0 },
    { "Synthwave Arp", "Electronic", "Up and down 16th arpeggios over two octaves. 85-110 BPM.",
      "Synthwave|Cinematic|EDM", 13, 0, 50, 70, 0, 0, 0, 96, 4, 5, 1 },
    { "Ambient Swell", "Electronic", "Long held chords, slowly rolled in, soft. 60-90 BPM.",
      "Ambient|Cinematic|Lo-Fi", 0, 0, 50, 100, 0, 80, 0, 70, 20, 5, 0 },
    { "Arp Rain", "Electronic", "Falling 16th-triplet arpeggios over three octaves.",
      "Ambient|Cinematic|Future Bass", 12, 0, 50, 80, 0, 0, 0, 86, 10, 4, 2 },
    // ---------------------------------------------------------------- Cinematic and classical
    { "Cinematic Ostinato", "Cinematic & Classical", "Broken-chord 8ths that drive a film cue. 90-130 BPM.",
      "Cinematic|Classical|Ambient", 15, 0, 50, 90, 0, 0, 0, 96, 6, 7, 0 },
    { "Alberti Bass", "Cinematic & Classical", "Low-high-middle-high 16ths, the Mozart left hand.",
      "Classical|Romantic", 15, 0, 50, 90, 0, 0, 0, 88, 10, 5, 0 },
    { "Nocturne Arpeggio", "Cinematic & Classical", "Wide triplet arpeggios over three octaves, rubato. 50-70 BPM.",
      "Romantic|Classical|Cinematic", 11, 0, 50, 100, 0, 0, 0, 80, 25, 6, 2 },
    { "Whole-Note Choir", "Cinematic & Classical", "One chord per bar, held, gently rolled.",
      "Cinematic|Classical|Ambient|Gospel", 1, 0, 50, 100, 0, 20, 0, 84, 10, 5, 0 },
    // ---------------------------------------------------------------- Latin and Caribbean
    { "Bossa Comp", "Latin & Caribbean", "Joao Gilberto's syncopated guitar comping, felt in two. 120-140 BPM.",
      "Bossa Nova|Jazz|City Pop", 27, 0, 50, 55, 0, 12, 3, 88, 14, 5, 0 },
    { "Samba Partido", "Latin & Caribbean", "Busy 16th samba comping with the tamborim feel. 95-105 BPM (in 2).",
      "Bossa Nova|Salsa / Son", 28, 0, 52, 45, 0, 8, 3, 96, 12, 5, 0 },
    { "Salsa Montuno", "Latin & Caribbean", "The piano montuno over the 2-3 clave. 180-200 BPM.",
      "Salsa / Son", 29, 0, 50, 50, 0, 0, 0, 104, 8, 5, 0 },
    { "Son Clave 3-2", "Latin & Caribbean", "Chords on the five strokes of the son clave.",
      "Salsa / Son|Afrobeats", 30, 0, 50, 60, 0, 0, 0, 100, 8, 5, 0 },
    { "Habanera", "Latin & Caribbean", "Dotted 8th, 16th, two 8ths: Cuban habanera and tango. 70-90 BPM.",
      "Flamenco|Salsa / Son|Classical", 45, 0, 50, 75, 0, 6, 1, 96, 10, 5, 0 },
    { "Flamenco Rumba", "Latin & Caribbean", "3-3-2 rumba strums, fast and loose. 100-120 BPM.",
      "Flamenco", 7, 0, 50, 55, 0, 30, 4, 112, 18, 5, 0 },
    { "Reggaeton Dembow", "Latin & Caribbean", "The dembow 3+3+2 under every bar. 88-100 BPM.",
      "Reggaeton|Reggae|Afrobeats", 19, 0, 50, 60, 0, 0, 0, 104, 6, 5, 0 },
    { "Reggae One Drop", "Latin & Caribbean", "Short skank chords on two and four, bass on the one. 70-80 BPM.",
      "Reggae", 31, 0, 54, 35, 0, 0, 0, 100, 10, 5, 0 },
    { "Ska Upstroke", "Latin & Caribbean", "Choppy upstrokes on every offbeat. 140-170 BPM.",
      "Reggae|Rock", 32, 0, 50, 30, 0, 0, 0, 106, 8, 5, 0 },
    { "Dancehall Bounce", "Latin & Caribbean", "Steady syncopated dancehall hits. 90-105 BPM.",
      "Reggae|Reggaeton|Afrobeats", 33, 0, 50, 55, 0, 0, 0, 104, 8, 5, 0 },
    { "Tresillo Pop", "Latin & Caribbean", "The 3-3-2 tresillo that runs through pop and dancehall. 95-115 BPM.",
      "Pop|K-Pop|Reggaeton|Afrobeats", 7, 0, 50, 70, 0, 0, 0, 100, 8, 5, 0 },
    // ---------------------------------------------------------------- African
    { "Afrobeats Bounce", "African", "Syncopated afro-pop keys with a light swing. 100-110 BPM.",
      "Afrobeats|Amapiano|Reggae", 20, 0, 56, 55, 0, 0, 0, 100, 12, 5, 0 },
    { "Amapiano Log", "African", "Off-beat keys around the log drum, soft and lazy. 110-115 BPM.",
      "Amapiano|Afrobeats|House", 21, 0, 55, 60, 0, 6, 0, 94, 14, 5, 0 },
    // ---------------------------------------------------------------- Rock, pop and country
    { "Pop Ballad", "Rock, Pop & Country", "Held chords with a pick-up on beats three and four. 60-80 BPM.",
      "Pop|Romantic|90s R&B|K-Pop", 39, 0, 50, 95, 0, 15, 0, 90, 12, 5, 0 },
    { "Pop Piano 8ths", "Rock, Pop & Country", "Steady 8th-note piano, the modern pop drive. 100-120 BPM.",
      "Pop|K-Pop|Rock|60s Brit Pop", 4, 0, 50, 75, 0, 0, 0, 96, 10, 5, 0 },
    { "Rock Drive", "Rock, Pop & Country", "Driving 8ths with accents, guitar-band energy. 110-140 BPM.",
      "Rock|60s Brit Pop|Metal", 38, 0, 50, 70, 0, 4, 1, 110, 8, 5, 0 },
    { "Metal Chug", "Rock, Pop & Country", "Palm-muted 8ths, short and loud.",
      "Metal|Rock", 4, 0, 50, 35, 0, 2, 1, 124, 4, 5, 0 },
    { "Country Boom-Chick", "Rock, Pop & Country", "Bass on one and three, chord on two and four. 100-120 BPM.",
      "Country|Blues|60s Brit Pop", 37, 0, 50, 50, 0, 8, 1, 98, 10, 5, 0 },
    { "Country Strum", "Rock, Pop & Country", "Down-up guitar 8ths, strummed. 90-120 BPM.",
      "Country|Pop|Rock", 4, 0, 50, 80, 0, 25, 3, 96, 14, 5, 0 },
    // ---------------------------------------------------------------- Jazz and blues
    { "Charleston Comp", "Jazz & Blues", "Comping on one and the and-of-two, swung. 140-200 BPM.",
      "Jazz|Blues", 6, 0, 66, 60, 0, 8, 0, 92, 18, 5, 0 },
    { "Jazz Ballad", "Jazz & Blues", "Held, gently rolled chords, played loose. 55-75 BPM.",
      "Jazz|Neo Soul|Romantic", 0, 0, 50, 100, 0, 25, 0, 82, 25, 5, 0 },
    { "Blues Shuffle", "Jazz & Blues", "Long-short triplet shuffle. 90-120 BPM.",
      "Blues|Rock|Country", 36, 0, 50, 70, 0, 4, 0, 100, 12, 5, 0 },
    { "Half-Note Walk", "Jazz & Blues", "Two chords per bar, warm and simple.",
      "Jazz|Gospel|Pop|Classical", 2, 0, 50, 95, 0, 10, 0, 90, 12, 5, 0 },
};

static constexpr int NUM_GROOVES_LIB = (int) (sizeof (GROOVES_LIB) / sizeof (GROOVES_LIB[0]));

} // namespace cc
