#include "song_catalog.h"

#include "bn_music_items.h"

namespace
{
    constexpr gha::song_catalog_entry SONGS[] = {
        {
            gha::song_type::EVEN_FLOW,
            "Even Flow",
            EVEN_FLOW_CHART,
            EVEN_FLOW_CHART_SIZE,
            EVEN_FLOW_TIMING,
            0,
            bn::music_items::evenflow,
            true
        },
        {
            gha::song_type::HACKER,
            "Hacker",
            HACKER_CHART,
            HACKER_CHART_SIZE,
            HACKER_TIMING,
            0,
            bn::music_items::hacker,
            true
        }
    };

    constexpr int SONGS_COUNT = static_cast<int>(sizeof(SONGS) / sizeof(SONGS[0]));
}

namespace gha
{
    int song_catalog_size()
    {
        return SONGS_COUNT;
    }

    const song_catalog_entry& song_catalog_entry_at(int index)
    {
        return SONGS[index];
    }

    const song_catalog_entry& song_catalog_entry_for(song_type song)
    {
        for (const song_catalog_entry& entry : SONGS)
        {
            if (entry.type == song)
            {
                return entry;
            }
        }

        return SONGS[0];
    }
}
