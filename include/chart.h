#pragma once

// ---------------------------------------------------------------------------
// Timing & layout constants
// ---------------------------------------------------------------------------

// GBA locked frame rate.
constexpr int FRAMES_PER_SECOND = 60;

// Butano (0,0) = screen centre. Screen spans y in [-80, 80].
constexpr int SPAWN_Y   = -80;   // note spawns above the top edge
constexpr int HALO_Y    =  56;   // hit-zone / halo row
constexpr int DESPAWN_Y =  82;   // note disappears just below the bottom edge

// Note speed: 1 pixel per frame.
// Travel distance = HALO_Y - SPAWN_Y = 136 pixels → 136 frames.
constexpr int TRAVEL_TICKS = HALO_Y - SPAWN_Y;

// Hit-detection timing windows (frames of tolerance, measured from hit_tick).
constexpr int HIT_WINDOW_PERFECT = 3;
constexpr int HIT_WINDOW_GOOD    = 7;

// Maximum number of note sprites alive at the same time.
constexpr int MAX_ACTIVE_NOTES = 80;

// Lane layout.
constexpr int LANE_COUNT   = 5;
constexpr int LANE_SPACING = 16;

// Score awarded per hit (multiplied by combo tier in the engine).
constexpr int SCORE_PERFECT = 100;
constexpr int SCORE_GOOD    =  50;

// Number of countdown beats shown before music starts (shared by all charts).
constexpr int COUNTDOWN_BEATS = 4;

// ---------------------------------------------------------------------------
// Per-chart timing helper.
// Store one of these per song chart; pass it alongside the note array.
// ---------------------------------------------------------------------------
struct chart_timing
{
    int bpm;
    int frames_per_beat;   // (60 * FRAMES_PER_SECOND) / bpm
    int pre_song_ticks;    // COUNTDOWN_BEATS * frames_per_beat
    int lead_in;           // TRAVEL_TICKS + pre_song_ticks

    constexpr explicit chart_timing(int bpm_val)
        : bpm(bpm_val),
          frames_per_beat((FRAMES_PER_SECOND * 60) / bpm_val),
          pre_song_ticks(COUNTDOWN_BEATS * ((FRAMES_PER_SECOND * 60) / bpm_val)),
          lead_in(TRAVEL_TICKS + COUNTDOWN_BEATS * ((FRAMES_PER_SECOND * 60) / bpm_val))
    {}

    // Convert a beat position to a frame tick.
    //   sub_num/sub_den = intra-beat fraction  (e.g. 1,2 = eighth, 1,4 = 16th)
    constexpr int beat_tick(int beat, int sub_num = 0, int sub_den = 1) const
    {
        return lead_in + beat * frames_per_beat + sub_num * frames_per_beat / sub_den;
    }
};

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

// A single note event in the song chart.
struct ChartNote
{
    int hit_tick; // frame on which the note should be at HALO_Y
    int lane;     // 0..LANE_COUNT-1
};

// Result returned by the hit-detector for a lane key press.
enum class HitResult { NONE, PERFECT, GOOD };

// ---------------------------------------------------------------------------
// Coolio chart  — must be sorted ascending by hit_tick.
// ---------------------------------------------------------------------------
constexpr chart_timing COOLIO_TIMING(82);
constexpr ChartNote COOLIO_CHART[] = {


    { COOLIO_TIMING.beat_tick(0),       2 },
    { COOLIO_TIMING.beat_tick(1),       2 },
    { COOLIO_TIMING.beat_tick(2),       2 },
    { COOLIO_TIMING.beat_tick(3),       2 },
    { COOLIO_TIMING.beat_tick(4),       1 },
    { COOLIO_TIMING.beat_tick(5),       1 },
    { COOLIO_TIMING.beat_tick(6),       4 },
    { COOLIO_TIMING.beat_tick(7),       0 },
    { COOLIO_TIMING.beat_tick(7, 1, 4), 0 },

    { COOLIO_TIMING.beat_tick(8),       2 },
    { COOLIO_TIMING.beat_tick(9),       2 },
    { COOLIO_TIMING.beat_tick(10),      2 },
    { COOLIO_TIMING.beat_tick(11),      2 },
    { COOLIO_TIMING.beat_tick(12),      1 },
    { COOLIO_TIMING.beat_tick(13),      1 },
    { COOLIO_TIMING.beat_tick(14),      4 },
    { COOLIO_TIMING.beat_tick(15),      0 },
    { COOLIO_TIMING.beat_tick(15, 1, 4),0 },

    { COOLIO_TIMING.beat_tick(16),      2 },
    { COOLIO_TIMING.beat_tick(17),      2 },
    { COOLIO_TIMING.beat_tick(18),      2 },
    { COOLIO_TIMING.beat_tick(19),      2 },
    { COOLIO_TIMING.beat_tick(20),      1 },
    { COOLIO_TIMING.beat_tick(21),      1 },
    { COOLIO_TIMING.beat_tick(22),      4 },
    { COOLIO_TIMING.beat_tick(23),      0 },
    { COOLIO_TIMING.beat_tick(23, 1, 4),0 },

    { COOLIO_TIMING.beat_tick(24),      2 },
    { COOLIO_TIMING.beat_tick(25),      2 },
    { COOLIO_TIMING.beat_tick(26),      2 },
    { COOLIO_TIMING.beat_tick(27),      2 },
    { COOLIO_TIMING.beat_tick(28),      1 },
    { COOLIO_TIMING.beat_tick(29),      1 },
    { COOLIO_TIMING.beat_tick(30),      4 },
    { COOLIO_TIMING.beat_tick(31),      0 },
    { COOLIO_TIMING.beat_tick(31, 1, 4),0 }
};

constexpr int COOLIO_CHART_SIZE = static_cast<int>(sizeof(COOLIO_CHART) / sizeof(COOLIO_CHART[0]));

// ---------------------------------------------------------------------------
// Beat It chart  (BPM 140) — must be sorted ascending by hit_tick.
// ---------------------------------------------------------------------------
constexpr chart_timing BEAT_IT_TIMING(140);
constexpr ChartNote BEAT_IT_CHART[] = {


    { BEAT_IT_TIMING.beat_tick(0),        0 },
    { BEAT_IT_TIMING.beat_tick(1),        1 },
    { BEAT_IT_TIMING.beat_tick(1,  1, 2), 2 },
    { BEAT_IT_TIMING.beat_tick(2),        4 },
    { BEAT_IT_TIMING.beat_tick(2,  1, 2), 2 },
    { BEAT_IT_TIMING.beat_tick(4),        2 },
    { BEAT_IT_TIMING.beat_tick(4,  1, 4), 3 },
    { BEAT_IT_TIMING.beat_tick(5),        2 },
    { BEAT_IT_TIMING.beat_tick(6),        1 },
    { BEAT_IT_TIMING.beat_tick(7),        1 },

    { BEAT_IT_TIMING.beat_tick(8),        0 },
    { BEAT_IT_TIMING.beat_tick(9),        1 },
    { BEAT_IT_TIMING.beat_tick(9,  1, 2), 2 },
    { BEAT_IT_TIMING.beat_tick(10),       4 },
    { BEAT_IT_TIMING.beat_tick(10,  1, 2), 2 },
    { BEAT_IT_TIMING.beat_tick(12),       2 },
    { BEAT_IT_TIMING.beat_tick(12,  1, 4), 3 },
    { BEAT_IT_TIMING.beat_tick(13),        2 },
    { BEAT_IT_TIMING.beat_tick(14),        1 },


    { BEAT_IT_TIMING.beat_tick(16),        0 },
    { BEAT_IT_TIMING.beat_tick(17),        1 },
    { BEAT_IT_TIMING.beat_tick(17,  1, 2), 2 },
    { BEAT_IT_TIMING.beat_tick(18),        4 },
    { BEAT_IT_TIMING.beat_tick(18,  1, 2), 2 },
    { BEAT_IT_TIMING.beat_tick(20),        2 },
    { BEAT_IT_TIMING.beat_tick(20,  1, 4), 3 },
    { BEAT_IT_TIMING.beat_tick(21),        2 },
    { BEAT_IT_TIMING.beat_tick(22),        1 },
    { BEAT_IT_TIMING.beat_tick(23),        1 },

    { BEAT_IT_TIMING.beat_tick(24),        0 },
    { BEAT_IT_TIMING.beat_tick(25),        1 },
    { BEAT_IT_TIMING.beat_tick(25,  1, 2), 2 },
    { BEAT_IT_TIMING.beat_tick(26),       4 },
    { BEAT_IT_TIMING.beat_tick(26,  1, 2), 2 },
    { BEAT_IT_TIMING.beat_tick(28),       2 },
    { BEAT_IT_TIMING.beat_tick(28,  1, 4), 3 },
    { BEAT_IT_TIMING.beat_tick(29),        2 },
    { BEAT_IT_TIMING.beat_tick(30),        1 },
};

constexpr int BEAT_IT_CHART_SIZE = static_cast<int>(sizeof(BEAT_IT_CHART) / sizeof(BEAT_IT_CHART[0]));

// Imported Clone Hero charts can use absolute frame hit ticks.
#include "even_flow_chart.h"
#include "hacker_chart.h"
