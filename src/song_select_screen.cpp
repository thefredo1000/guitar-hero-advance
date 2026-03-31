#include "song_select_screen.h"

#include "song_catalog.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

#include "bn_sprite_items_notes.h"

#include "common_variable_8x16_sprite_font.h"
#include <bn_music_items.h>
#include <bn_music.h>

namespace
{
    constexpr int SONG_ROW_SPACING = 24;

    int song_row_y(int index, int song_count)
    {
        int top_y = -((song_count - 1) * SONG_ROW_SPACING) / 2;
        return top_y + index * SONG_ROW_SPACING;
    }

    void draw_song_select_ui(bn::sprite_text_generator& text_generator,
                             bn::vector<bn::sprite_ptr, 64>& text_sprites,
                             bn::vector<bn::sprite_ptr, 48>& song_sprites)
    {
        text_sprites.clear();
        song_sprites.clear();

        const int song_count = gha::song_catalog_size();

        text_generator.generate(0, -56, "Select a Song", text_sprites);
        text_generator.generate(0, 48, "SELECT: How to play  A/START: Play", text_sprites);

        for (int index = 0; index < song_count; ++index)
        {
            const gha::song_catalog_entry& song = gha::song_catalog_entry_at(index);
            text_generator.generate(0, song_row_y(index, song_count), song.title, song_sprites);
        }
    }

    void how_to_play_screen(bn::sprite_text_generator& text_generator)
    {
        bn::vector<bn::sprite_ptr, 96> text_sprites;
        text_generator.generate(0, -56, "How To Play", text_sprites);
        text_generator.generate(0, -32, "Left: Green", text_sprites);
        text_generator.generate(0, -16, "Down: Red", text_sprites);
        text_generator.generate(0, 0, "Right: Yellow", text_sprites);
        text_generator.generate(0, 16, "B: Blue", text_sprites);
        text_generator.generate(0, 32, "A: Orange", text_sprites);

        // Avoid instantly leaving because SELECT is still pressed.
        while (bn::keypad::select_pressed())
        {
            bn::core::update();
        }

        while (!bn::keypad::b_pressed() && !bn::keypad::select_pressed())
        {
            bn::core::update();
        }

        while (bn::keypad::b_pressed() || bn::keypad::select_pressed())
        {
            bn::core::update();
        }
    }
}

namespace gha
{
    song_type song_select_screen()
    {

        bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
        text_generator.set_center_alignment();

        const int song_count = song_catalog_size();
        int selected = 0;

        bn::vector<bn::sprite_ptr, 64> text_sprites;
        bn::vector<bn::sprite_ptr, 48> song_sprites;
        draw_song_select_ui(text_generator, text_sprites, song_sprites);

        bn::sprite_ptr cursor = bn::sprite_items::notes.create_sprite(-70, song_row_y(0, song_count), 2);
        cursor.set_scale(0.8);

        // Flush the A/START press that exited the previous screen.
        bn::core::update();

        while (!bn::keypad::a_pressed() && !bn::keypad::start_pressed())
        {
            if (bn::keypad::select_pressed())
            {
                text_sprites.clear();
                song_sprites.clear();
                cursor.set_visible(false);

                how_to_play_screen(text_generator);

                draw_song_select_ui(text_generator, text_sprites, song_sprites);
                cursor.set_visible(true);
            }

            if (bn::keypad::up_pressed() && selected > 0)
                --selected;
            else if (bn::keypad::down_pressed() && selected < song_count - 1)
                ++selected;

            // Cursor sits beside the selected row
            cursor.set_y(song_row_y(selected, song_count));

            bn::core::update();
        }
        bn::music::stop();
        return song_catalog_entry_at(selected).type;
    }
}
