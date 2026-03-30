#include "bn_core.h"

#include "scene_type.h"
#include "song_type.h"
#include "title_screen.h"
#include "song_select_screen.h"
#include "gameplay_screen.h"

int main()
{
    bn::core::init();

    gha::scene_type scene = gha::scene_type::TITLE;
    gha::song_type selected_song = gha::song_type::COOLIO;

    while (true)
    {
        switch (scene)
        {
            case gha::scene_type::TITLE:
                gha::title_screen();
                scene = gha::scene_type::SONG_SELECT;
                break;

            case gha::scene_type::SONG_SELECT:
                selected_song = gha::song_select_screen();
                scene = gha::scene_type::GAMEPLAY;
                break;

            case gha::scene_type::GAMEPLAY:
                scene = gha::gameplay_screen(selected_song);
                break;

            default:
                scene = gha::scene_type::TITLE;
                break;
        }
    }
}