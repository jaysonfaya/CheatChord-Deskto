/* CheatChord reference tracks (see refs.h). Sources were checked in November 2026. Keys and chord sequences are as
 * documented there; where sources disagree it is said in the entry. Degrees: semitones above the tonic of the frame.
 * Profile indices: 1 M. Jackson, 2 Q. Jones, 3 S. Wonder, 4 Beatles, 5 Prince, 6 Drake, 7 K. West, 8 J Dilla,
 * 9 Pharrell, 10 Timbaland, 11 Daft Punk, 12 Bacharach, 13 Babyface, 14 F. Ocean, 15 Weeknd, 16 M. Martin,
 * 17 D'Angelo, 18 B. Mars, 19 SZA, 20 Coldplay, 21 Adele, 22 Dr. Dre, 23 Metro Boomin, 24 N. Rodgers, 25 Jam & Lewis */
#include <stddef.h>
#include "refs.h"

#define M FM_M
#define m FM_m
#define D FM_D
#define o FM_o
#define s FM_s

const ref_t REFS[] = {
    {1, "BILLIE JEAN", "MICHAEL JACKSON", "PROD. Q. JONES / M. JACKSON", "F# MINOR", "F#m  Bm  D  C#",
     "i  iv  bVI  V", "MINOR i-iv VAMP, bVI TO MAJOR V", "MUSIC WILL CHART; UG TAB (ORIGINAL KEY F#m)", 2,
     1, 5, {0, 5, 0, 8, 7}, {m, m, m, M, M | D}},
    {2, "ROCK WITH YOU", "MICHAEL JACKSON", "PROD. QUINCY JONES (WIKIPEDIA)", "Db MAJOR / Eb MINOR AREA",
     "Ebm7  Fm7  Gbmaj7  Ab7", "ii7  iii7  IVmaj7  V7", "STEPWISE 7TH CHORDS, SLASH / 9TH COLOURS",
     "UG TAB, CIFRA CLUB (KEY GIVEN DIFFERENTLY)", 2, 0, 4, {2, 4, 5, 7}, {m, m, M, D}},
    {3, "ISN'T SHE LOVELY", "STEVIE WONDER", "PROD. STEVIE WONDER", "E MAJOR", "C#m7  F#7  B7sus4  E6",
     "vi7  II7 (V/V)  V7sus  I6", "vi-ii-V-I WITH A MAJOR II7, SUS DOMINANT", "PIANO WITH JONNY; PIANO WITH WILLIE",
     2, 0, 4, {9, 2, 7, 0}, {m, D, s | D, M}},
    {4, "LET IT BE", "THE BEATLES", "PROD. PHIL SPECTOR / G. MARTIN", "C MAJOR", "C  G  Am  F  C  G  F  C",
     "I  V  vi  IV  I  V  IV  I", "I-V-vi-IV, V-IV-I, PLAGAL CADENCES", "ALAN W. POLLACK, NOTES ON LET IT BE", 2,
     0, 7, {0, 7, 9, 5, 0, 7, 5}, {M, M, m, M, M, M, M}},
    {5, "PURPLE RAIN", "PRINCE", "PROD. PRINCE", "Bb MAJOR", "Bb  Gm  F  Eb", "I  vi  V  IV",
     "I-vi-V-IV WITH ADD9 COLOUR (Bbadd9/D)", "UG; MUSICNOTES (Bb MAJOR); MUSICRADAR", 2,
     0, 4, {0, 9, 7, 5}, {M | s, m, M, M}},
    {6, "HOTLINE BLING", "DRAKE", "WRITERS INCL. TIMMY THOMAS (SAMPLE)", "D MINOR", "SET: Dm Gm Am Bb C",
     "", "DARK MINOR SAMPLE LOOP (ORDER NOT DOCUMENTED)", "HOOKTHEORY + MUSICNOTES (KEY); MUSIC WILL (CHORDS)", 1,
     1, 0, {0}, {0}},
    {7, "FLASHING LIGHTS", "KANYE WEST", "PROD. KANYE WEST / ERIC HUDSON", "F# MINOR (TAB, CAPO 2)",
     "F#m  D  |  Bm  Bm7", "i  bVI  |  iv  iv7", "MINOR i-bVI LIFT, iv7", "ONE TAB ONLY (GUITARTABSEXPLORER)", 1,
     1, 3, {0, 8, 5}, {m, M, m}},
    {8, "FALL IN LOVE", "SLUM VILLAGE", "PROD. J DILLA", "NOT DOCUMENTED", "", "",
     "GROOVE REFERENCE ONLY: LOOSE, LATE TIMING", "SUNHOUSE BEAT ANALYSIS (RHYTHM, NOT HARMONY)", 1,
     1, 0, {0}, {0}},
    {9, "HAPPY", "PHARRELL WILLIAMS", "PROD. PHARRELL WILLIAMS", "F MINOR (PUBLISHED)", "SET: Db Eb F Bb C Cm",
     "bVI  bVII  I (MAJOR)", "MINOR / MAJOR MIXTURE ENDING ON A MAJOR I", "MUSICNOTES; MUSIC WILL; DUKE (A. KELLEY)", 1,
     1, 3, {8, 10, 0}, {M, M, M}},
    {10, "CRY ME A RIVER", "JUSTIN TIMBERLAKE", "PROD. TIMBALAND", "G HARMONIC MINOR", "SET: i iv VI, DIM, INVERSIONS",
     "i  iv  bVI  V", "HARMONIC MINOR, DIMINISHED PASSING CHORDS", "HOOKTHEORY (KEY, CHORDS USED); WIKIPEDIA", 1,
     1, 4, {0, 5, 8, 7}, {m, m, M, M | D}},
    {11, "GET LUCKY", "DAFT PUNK", "PROD. DAFT PUNK", "B DORIAN", "Bm  D  F#m  E", "i  bIII  v  IV",
     "DORIAN LOOP: i-bIII-v-IV", "HOOKTHEORY (KEY); ULTIMATE GUITAR (CHORDS)", 2, 1, 4, {0, 3, 7, 5}, {m, M, m, M}},
    {12, "WALK ON BY", "DIONNE WARWICK", "BURT BACHARACH / HAL DAVID", "MINOR, SHIFTING KEY CENTRES", "Am7  D6",
     "i7  IV6", "i7-IV6 DORIAN VAMP, KEY CENTRES THAT MOVE", "E-CHORDS; IREAL PRO FORUM ANALYSIS", 2,
     1, 2, {0, 5}, {m, M}},
    {13, "END OF THE ROAD", "BOYZ II MEN", "PROD. L.A. REID / BABYFACE / D. SIMMONS", "Eb MAJOR (MUSICNOTES)", "", "",
     "BALLAD REFERENCE (SEQUENCE NOT DOCUMENTED)", "BILLBOARD CREDITS; MUSICNOTES (KEY)", 1, 0, 0, {0}, {0}},
    {14, "THINKIN BOUT YOU", "FRANK OCEAN", "WRITERS: F. OCEAN / SHEA TAYLOR", "A MINOR (MUSICNOTES)",
     "Dm7  C  G  Am7", "iv7  bIII  bVII  i7", "BORROWED-SOUNDING iv-bIII-bVII-i, FLOATING 7THS",
     "E-CHORDS; CHORDIE; MUSICNOTES (KEY)", 2, 1, 4, {5, 3, 10, 0}, {m, M, M, m}},
    {15, "BLINDING LIGHTS", "THE WEEKND", "WRITERS INCL. MAX MARTIN", "C MINOR", "Fm  Cm  Eb  Bb",
     "iv  i  bIII  bVII", "MINOR SYNTH LOOP iv-i-bIII-bVII", "HOOKTHEORY + MUSICNOTES (KEY); MUSIC WILL; E-CHORDS", 2,
     1, 4, {5, 0, 3, 10}, {m, m, M, M}},
    {16, "BLINDING LIGHTS", "THE WEEKND", "WRITERS INCL. MAX MARTIN", "C MINOR", "Fm  Cm  Eb  Bb",
     "iv  i  bIII  bVII", "FOUR-CHORD LOOP, CLEAR TRIADS", "HOOKTHEORY + MUSICNOTES (KEY); MUSIC WILL; E-CHORDS", 2,
     1, 4, {5, 0, 3, 10}, {m, m, M, M}},
    {17, "UNTITLED (HOW DOES IT FEEL)", "D'ANGELO", "D'ANGELO / RAPHAEL SAADIQ", "D MIXOLYDIAN",
     "Am7  Gmaj7  C7  D", "v7  IVmaj7  bVII7  I", "MIXOLYDIAN v7, BLUESY bVII7", "HOOKTHEORY (KEY); ULTIMATE GUITAR", 2,
     0, 4, {7, 5, 10, 0}, {m, M, D, M}},
    {18, "UPTOWN FUNK", "MARK RONSON FT. BRUNO MARS", "PROD. MARK RONSON / B. MARS / J. BHASKER", "D DORIAN",
     "Dm7  G7", "i7  IV7", "TWO-CHORD DORIAN FUNK VAMP", "HOOKTHEORY (KEY); PIANO WITH WILLIE; UG", 2,
     1, 2, {0, 5}, {m, D}},
    {19, "KILL BILL", "SZA", "PROD. CARTER LANG / ROB BISEL", "Ab MAJOR (MUSICNOTES)", "Bbm7  Eb7  Ab  F7",
     "ii7  V7  I  VI7", "ii-V-I TURNAROUND WITH VI7", "MUSICNOTES (KEY); ULTIMATE GUITAR; E-CHORDS", 2,
     0, 4, {2, 7, 0, 9}, {m, D, M, D}},
    {20, "VIVA LA VIDA", "COLDPLAY", "PROD. B. ENO / M. DRAVS / R. SIMPSON", "Ab MAJOR", "Db  Eb  Ab  Fm",
     "IV  V  I  vi", "ANTHEMIC IV-V-I-vi", "ULTIMATE GUITAR (KEY Ab, CAPO 1)", 2, 0, 4, {5, 7, 0, 9}, {M, M, M, m}},
    {21, "SOMEONE LIKE YOU", "ADELE", "WRITERS: ADELE / DAN WILSON", "A MAJOR", "A  C#m/G#  F#m  D",
     "I  iii/7  vi  IV", "I-V-vi-IV FAMILY WITH A FALLING BASS", "NATIONAL GUITAR ACADEMY (KEY); UG (CAPO 2)", 2,
     0, 4, {0, 4, 9, 5}, {M, m, m, M}},
    {22, "STILL D.R.E.", "DR. DRE FT. SNOOP DOGG", "PROD. DR. DRE", "A MINOR", "Am (INV.)  Esus4  Em",
     "i  vsus4  v", "STACCATO MINOR TRIAD INVERSIONS, SUS4", "MUSICNOTES + RAY HARMONY (KEY); SKOOVE (CHORDS)", 2,
     1, 3, {0, 7, 7}, {m, s, m}},
    {23, "MASK OFF", "FUTURE", "PROD. METRO BOOMIN / SOUTHSIDE (FLUTE SAMPLE)", "D MINOR", "Dm  Gm  C  Dm",
     "i  iv  bVII  i", "DARK MINOR i-iv-bVII LOOP", "ULTIMATE GUITAR; COMPLEX / SONGFACTS (CREDITS)", 2,
     1, 3, {0, 5, 10}, {m, m, M}},
    {24, "GOOD TIMES", "CHIC", "NILE RODGERS / BERNARD EDWARDS", "E MINOR (DORIAN)", "Em7  A7 (A13, E7sus4)",
     "i7  IV7", "i7-IV7 DISCO VAMP WITH SUS AND 13", "TALKINGBASS; SCIENCEBASE (NILE RODGERS LIVE)", 2,
     1, 2, {0, 5}, {m, D | s}},
    {25, "THAT'S THE WAY LOVE GOES", "JANET JACKSON", "PROD. JAM & LEWIS / J. JACKSON", "G MINOR", "", "",
     "SMOOTH 7THS / EXTENDED CHORDS (SEQUENCE NOT DOCUMENTED)", "WIKIPEDIA (KEY, CREDITS); HOOKTHEORY", 1,
     1, 0, {0}, {0}},
};
const int NUM_REFS = (int)(sizeof REFS / sizeof REFS[0]);

const ref_t *ref_of(int profile) {
    for (int i = 0; i < NUM_REFS; i++)
        if (REFS[i].profile == profile) return &REFS[i];
    return NULL;
}
