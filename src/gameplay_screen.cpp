#include "gameplay_screen.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_music.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "bn_sstream.h"
#include "bn_utility.h"
#include "bn_vector.h"

#include "bn_sprite_items_halos.h"
#include "bn_sprite_items_notes.h"

#include "common_variable_8x16_sprite_font.h"

#include "song_catalog.h"

namespace
{
    constexpr int lane_x(int lane)
    {
        return (lane - (LANE_COUNT / 2)) * LANE_SPACING;
    }

    bool lane_just_pressed(int lane)
    {
        switch (lane)
        {
            case 0: return bn::keypad::left_pressed();
            case 1: return bn::keypad::up_pressed();
            case 2: return bn::keypad::right_pressed();
            case 3: return bn::keypad::b_pressed();
            case 4: return bn::keypad::a_pressed();
            default: return false;
        }
    }

    constexpr int adjusted_hit_tick(const gha::song_catalog_entry& song_data, const ChartNote& note)
    {
        return note.hit_tick + song_data.note_offset_ticks;
    }

    struct active_note
    {
        bn::sprite_ptr sprite;
        int chart_index;
        bool missed;

        active_note(bn::sprite_ptr sprite_param, int chart_index_param)
            : sprite(bn::move(sprite_param)), chart_index(chart_index_param), missed(false)
        {
        }
    };

    struct runtime
    {
        int current_tick = 0;
        int next_spawn_idx = 0;
        int score = 0;
        int combo = 0;
        int last_music_position = 0;

        bn::vector<bn::sprite_ptr, LANE_COUNT> halo_sprites;
        bn::vector<active_note, MAX_ACTIVE_NOTES> active_notes;

        int halo_flash[LANE_COUNT] = {};

        bn::vector<bn::sprite_ptr, 32> score_sprites;
        bn::vector<bn::sprite_ptr, 32> combo_sprites;
        int last_displayed_score = -1;
        int last_displayed_combo = -1;

        bn::vector<bn::sprite_ptr, 16> feedback_sprites;
        int feedback_timer = 0;
    };

    void init_halos(runtime& game)
    {
        for (int i = 0; i < LANE_COUNT; ++i)
        {
            game.halo_sprites.push_back(
                bn::sprite_items::halos.create_sprite(lane_x(i), HALO_Y, i));
        }
    }

    void spawn_pending_notes(runtime& game, const gha::song_catalog_entry& song_data)
    {
        while (game.next_spawn_idx < song_data.chart_size)
        {
            const ChartNote& note = song_data.chart[game.next_spawn_idx];
            int spawn_tick = adjusted_hit_tick(song_data, note) - TRAVEL_TICKS;

            if (game.current_tick < spawn_tick)
            {
                break;
            }

            if (!game.active_notes.full())
            {
                game.active_notes.emplace_back(
                    bn::sprite_items::notes.create_sprite(bn::fixed(lane_x(note.lane)) * bn::fixed(0.4), SPAWN_Y, note.lane),
                    game.next_spawn_idx);
            }

            ++game.next_spawn_idx;
        }
    }

    // Scale values used for the 4 discrete perspective steps.
    // Using only 4 distinct values means at most 4 OAM affine-matrix slots are
    // consumed by notes at any one time, well within the GBA's 32-slot budget.
    constexpr bn::fixed NOTE_SCALE_TABLE[4] = {
        bn::fixed(0.3), bn::fixed(0.55), bn::fixed(0.8), bn::fixed(1)
    };

    void update_notes(runtime& game, const gha::song_catalog_entry& song_data)
    {
        for (auto it = game.active_notes.begin(); it != game.active_notes.end(); )
        {
            active_note& note = *it;
            const ChartNote& chart_note = song_data.chart[note.chart_index];
            int chart_hit_tick = adjusted_hit_tick(song_data, chart_note);

            note.sprite.set_y(note.sprite.y() + 1);

            // --- 3D perspective (trapezoid) ---
            // t = 0  at VANISH_Y (top): narrow spread, small scale
            // t = 1  at HALO_Y   (hit zone): full spread, full scale
            bn::fixed t = (note.sprite.y() - bn::fixed(VANISH_Y)) / PERSPECTIVE_TRAVEL;
            if (t < 0) t = bn::fixed(0);
            if (t > 1) t = bn::fixed(1);

            // X: interpolate from SPREAD_MIN_FACTOR (narrow) to 1.0 (full lane spacing)
            // e.g. at the top lanes are 40% of their final distance apart
            constexpr bn::fixed SPREAD_MIN = bn::fixed(0.4);
            bn::fixed x_factor = SPREAD_MIN + (bn::fixed(1) - SPREAD_MIN) * t;
            note.sprite.set_x(bn::fixed(lane_x(chart_note.lane)) * x_factor);

            // Scale: 4 discrete levels to share OAM affine matrix slots
            int scale_idx = (t * 4).floor_integer();
            if (scale_idx < 0) scale_idx = 0;
            if (scale_idx > 3) scale_idx = 3;
            note.sprite.set_scale(NOTE_SCALE_TABLE[scale_idx]);
            // --- end perspective ---

            if (!note.missed && game.current_tick > chart_hit_tick + HIT_WINDOW_GOOD)
            {
                note.missed = true;
                game.combo = 0;
            }

            if (note.sprite.y() > bn::fixed(DESPAWN_Y))
            {
                it = game.active_notes.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    HitResult try_hit_lane(runtime& game, const gha::song_catalog_entry& song_data, int lane)
    {
        auto best_it = game.active_notes.end();
        int best_dist = HIT_WINDOW_GOOD + 1;

        for (auto it = game.active_notes.begin(); it != game.active_notes.end(); ++it)
        {
            active_note& note = *it;
            if (note.missed)
            {
                continue;
            }

            const ChartNote& chart_note = song_data.chart[note.chart_index];
            if (chart_note.lane != lane)
            {
                continue;
            }

            int dist = game.current_tick - adjusted_hit_tick(song_data, chart_note);
            if (dist < -HIT_WINDOW_GOOD)
            {
                continue;
            }

            int abs_dist = dist < 0 ? -dist : dist;
            if (abs_dist < best_dist)
            {
                best_dist = abs_dist;
                best_it = it;
            }
        }

        if (best_it == game.active_notes.end())
        {
            return HitResult::NONE;
        }

        game.active_notes.erase(best_it);
        return best_dist <= HIT_WINDOW_PERFECT ? HitResult::PERFECT : HitResult::GOOD;
    }

    void process_input(runtime& game, const gha::song_catalog_entry& song_data, bn::sprite_text_generator& text_generator)
    {
        for (int lane = 0; lane < LANE_COUNT; ++lane)
        {
            if (!lane_just_pressed(lane))
            {
                continue;
            }

            game.halo_flash[lane] = 6;
            HitResult hit = try_hit_lane(game, song_data, lane);

            if (hit == HitResult::PERFECT)
            {
                ++game.combo;
                game.score += SCORE_PERFECT * (1 + game.combo / 10);

                game.feedback_sprites.clear();
                text_generator.generate(0, 20, "PERFECT!", game.feedback_sprites);
                game.feedback_timer = 40;
            }
            else if (hit == HitResult::GOOD)
            {
                ++game.combo;
                game.score += SCORE_GOOD * (1 + game.combo / 10);

                game.feedback_sprites.clear();
                text_generator.generate(0, 20, "GOOD", game.feedback_sprites);
                game.feedback_timer = 30;
            }
        }

        if (game.feedback_timer > 0 && --game.feedback_timer == 0)
        {
            game.feedback_sprites.clear();
        }
    }

    void update_halos(runtime& game)
    {
        for (int i = 0; i < LANE_COUNT; ++i)
        {
            if (game.halo_flash[i] > 0)
            {
                --game.halo_flash[i];
                game.halo_sprites[i].set_scale(1.25);
            }
            else
            {
                game.halo_sprites[i].set_scale(1);
            }
        }
    }

    void update_hud(runtime& game, bn::sprite_text_generator& text_generator)
    {
        if (game.score != game.last_displayed_score)
        {
            game.last_displayed_score = game.score;
            game.score_sprites.clear();

            bn::string<16> text;
            bn::ostringstream stream(text);
            stream << game.score;
            text_generator.generate(-112, -72, text, game.score_sprites);
        }

        if (game.combo != game.last_displayed_combo)
        {
            game.last_displayed_combo = game.combo;
            game.combo_sprites.clear();

            if (game.combo > 1)
            {
                bn::string<16> text;
                bn::ostringstream stream(text);
                stream << game.combo << "x";
                text_generator.generate(80, -72, text, game.combo_sprites);
            }
        }
    }
}

namespace gha
{
    scene_type gameplay_screen(song_type song)
    {
        runtime game;
        const song_catalog_entry& song_data = song_catalog_entry_for(song);
        const int music_start_tick = song_data.timing.lead_in;
        const int countdown_start_tick = music_start_tick - song_data.timing.pre_song_ticks;
        bool music_started = false;
        int last_countdown_number = -1;

        bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
        text_generator.set_left_alignment();

        bn::vector<bn::sprite_ptr, 32> top_text_sprites;
        text_generator.generate(-112, 72, song_data.title, top_text_sprites);

        bn::vector<bn::sprite_ptr, 16> countdown_sprites;

        init_halos(game);
        update_hud(game, text_generator);

        while (!bn::keypad::start_pressed())
        {
            if (!music_started && game.current_tick >= music_start_tick)
            {
                song_data.music.play(1);
                music_started = true;
                countdown_sprites.clear();
            }

            if (music_started)
            {
                if (bn::music::playing())
                {
                    game.last_music_position = bn::music::position();
                }
                else if (song_data.restart_on_drop)
                {
                    // Maxmod can occasionally drop this imported module; resume near last known position.
                    song_data.music.play(1);

                    if (game.last_music_position > 0)
                    {
                        bn::music::set_position(game.last_music_position);
                    }
                }
            }

            if (!music_started && game.current_tick >= countdown_start_tick)
            {
                int countdown_elapsed_ticks = game.current_tick - countdown_start_tick;
                int beat_index = countdown_elapsed_ticks / song_data.timing.frames_per_beat;
                int countdown_number = COUNTDOWN_BEATS - beat_index;

                if (countdown_number != last_countdown_number && countdown_number >= 1)
                {
                    last_countdown_number = countdown_number;
                    countdown_sprites.clear();

                    bn::string<4> countdown_text;
                    bn::ostringstream countdown_stream(countdown_text);
                    countdown_stream << countdown_number;
                    text_generator.generate(0, -18, countdown_text, countdown_sprites);
                }
            }

            spawn_pending_notes(game, song_data);
            process_input(game, song_data, text_generator);
            update_notes(game, song_data);
            update_halos(game);
            update_hud(game, text_generator);

            ++game.current_tick;
            bn::core::update();
        }

        bn::music::stop();
        return scene_type::SONG_SELECT;
    }
}
