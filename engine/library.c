#include "library.h"

#define C(d, q) {d, q, -1}
#define CB(d, q, b) {d, q, b}

/* order and names must match TEMPLATES in tools/make_params.py */
const template_t TEMPLATES[] = {
    {"Axis Pop", 0, 0, 4, {C(0, Q_MAJ), C(7, Q_MAJ), C(9, Q_MIN), C(5, Q_MAJ)}},
    {"50s Doo-Wop", 0, 1, 4, {C(0, Q_MAJ), C(9, Q_MIN), C(5, Q_MAJ), C(7, Q_7)}},
    {"Sad Pop", 0, 0, 4, {C(9, Q_MIN), C(5, Q_MAJ), C(0, Q_MAJ), C(7, Q_MAJ)}},
    {"Royal Road", 0, 7, 4, {C(5, Q_MAJ7), C(7, Q_7), C(4, Q_M7), C(9, Q_M7)}},
    {"Jazz ii-V-I", 0, 6, 4, {C(2, Q_M9), C(7, Q_13), C(0, Q_MAJ9), C(9, Q_7B9)}},
    {"Turnaround", 0, 6, 4, {C(0, Q_MAJ7), C(9, Q_M7), C(2, Q_M7), C(7, Q_7)}},
    {"Neo Soul Descent", 0, 4, 4, {C(5, Q_MAJ9), C(4, Q_7S9), C(9, Q_M9), CB(7, Q_M9, -1)}},
    {"Backdoor", 0, 2, 4, {C(5, Q_MAJ7), C(5, Q_M7), C(10, Q_9), C(0, Q_MAJ7)}},
    {"Mixolydian Rock", 0, 11, 4, {C(0, Q_MAJ), C(10, Q_MAJ), C(5, Q_MAJ), C(0, Q_MAJ)}},
    {"Gospel Walk", 0, 5, 8, {C(0, Q_MAJ9), CB(0, Q_7, 4), C(5, Q_MAJ7), C(6, Q_DIM7), CB(0, Q_69, 7), C(9, Q_7),
                              C(2, Q_M9), C(7, Q_13SUS4)}},
    {"Canon", 0, 0, 8, {C(0, Q_MAJ), C(7, Q_MAJ), C(9, Q_MIN), C(4, Q_MIN), C(5, Q_MAJ), C(0, Q_MAJ), C(5, Q_MAJ),
                        C(7, Q_MAJ)}},
    {"Major-Minor IV", 0, 11, 4, {C(0, Q_MAJ), C(4, Q_MAJ), C(5, Q_MAJ), C(5, Q_MIN)}},
    {"70s Soul Lift", 0, 2, 4, {C(0, Q_MAJ9), C(9, Q_M9), C(2, Q_M9), C(7, Q_13SUS4)}},
    {"Chromatic Soul", 0, 2, 4, {C(0, Q_MAJ7), C(3, Q_MAJ7), C(8, Q_MAJ7), C(1, Q_MAJ7)}},
    {"Brit Pop Bridge", 0, 1, 4, {C(0, Q_MAJ), C(2, Q_MAJ), C(5, Q_MAJ), C(0, Q_MAJ)}},
    {"Lo-Fi Sway", 0, 9, 4, {C(5, Q_MAJ9), C(4, Q_M7), C(2, Q_M9), C(0, Q_MAJ9)}},
    {"R&B Slow Jam", 0, 7, 4, {C(0, Q_MAJ9), C(4, Q_M7), C(5, Q_MAJ9), C(7, Q_9SUS4)}},
    {"Andalusian", 1, 10, 4, {C(0, Q_MIN), C(10, Q_MAJ), C(8, Q_MAJ), C(7, Q_MAJ)}},
    {"Epic Minor", 1, 10, 4, {C(0, Q_MIN), C(8, Q_MAJ), C(3, Q_MAJ), C(10, Q_MAJ)}},
    {"Deep House", 1, 8, 4, {C(0, Q_M9), C(8, Q_MAJ7), C(3, Q_MAJ9), C(10, Q_69)}},
    {"Dorian Vamp", 1, 3, 4, {C(0, Q_M9), C(5, Q_9), C(0, Q_M9), C(5, Q_9)}},
    {"Minor ii-V-i", 1, 6, 4, {C(2, Q_M7B5), C(7, Q_7B9), C(0, Q_M9), C(0, Q_M6)}},
    {"Line Cliche", 1, 6, 4, {C(0, Q_MIN), C(0, Q_MMAJ7), C(0, Q_M7), C(0, Q_M6)}},
    {"80s Funk Minor", 1, 3, 4, {C(0, Q_M7), C(8, Q_MAJ7), C(10, Q_9SUS4), C(0, Q_M9)}},
    {"Dark Trap", 1, 10, 4, {C(0, Q_MIN), C(1, Q_MAJ), C(8, Q_MAJ), C(7, Q_MIN)}},
    {"Cinematic Rise", 1, 10, 4, {C(0, Q_MADD9), C(8, Q_ADD9), C(10, Q_MAJ), C(5, Q_MAJ)}},
};
const int NUM_TEMPLATES = (int)(sizeof TEMPLATES / sizeof TEMPLATES[0]);
