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

// Imported Clone Hero charts can use absolute frame hit ticks.
#include "even_flow_chart.h"
#include "hacker_chart.h"
