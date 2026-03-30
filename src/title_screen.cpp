#include "title_screen.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_regular_bg_actions.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_palettes.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "bn_sprite_items_halos.h"
#include "bn_sprite_items_notes.h"

#include "common_variable_8x16_sprite_font.h"

#include "bn_regular_bg_items_guitarhero.h"
#include <bn_music_items.h>

namespace
{
    constexpr int lane_count = 5;
    constexpr int lane_spacing = 16;
    constexpr bn::fixed fade_step = 1.0 / 100;

    constexpr int lane_x(int lane)
    {
        return (lane - (lane_count / 2)) * lane_spacing;
    }
}

namespace gha
{
    void title_screen()
    {
        // bn::vector<bn::sprite_ptr, lane_count> halos;
        // for (int lane = 0; lane < lane_count; ++lane)
        // {
        //     halos.push_back(bn::sprite_items::halos.create_sprite(lane_x(lane), 24, lane));
        // }

        // bn::vector<bn::sprite_ptr, 4> notes;
        // for (int i = 0; i < 4; ++i)
        // {
        //     notes.push_back(bn::sprite_items::notes.create_sprite(lane_x(i + 1), -28 + i * 12, i + 1));
        // }
        bn::music_items::evenflow.play(1);

        bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
        text_generator.set_center_alignment();
        bn::regular_bg_ptr title_screen_bg = bn::regular_bg_items::guitarhero.create_bg(8, 48);

        bn::vector<bn::sprite_ptr, 64> text_sprites;
        text_generator.generate(0, 48, "Press A or START", text_sprites);

        bn::fixed fade_intensity = 1;
        const bn::color black_color(0, 0, 0);
        bn::bg_palettes::set_fade(black_color, fade_intensity);
        bn::sprite_palettes::set_fade(black_color, fade_intensity);

        while (!bn::keypad::a_pressed() && !bn::keypad::start_pressed())
        {
            // Cool animation, maybe I'll use later
            // for (int i = 0; i < notes.size(); ++i)
            // {
            //     bn::sprite_ptr& note = notes[i];
            //     note.set_y(note.y() + 0.35);

            //     if (note.y() > 40)
            //     {
            //         note.set_y(-40);
            //     }
            // }

            if (fade_intensity > 0)
            {
                fade_intensity -= fade_step;

                if (fade_intensity < 0)
                {
                    fade_intensity = 0;
                }

                bn::bg_palettes::set_fade_intensity(fade_intensity);
                bn::sprite_palettes::set_fade_intensity(fade_intensity);
            }

            bn::core::update();
        }

        bn::bg_palettes::set_fade_intensity(0);
        bn::sprite_palettes::set_fade_intensity(0);
    }
}
