#ifndef GHA_SONG_CATALOG_H
#define GHA_SONG_CATALOG_H

#include "bn_music_item.h"

#include "chart.h"
#include "song_type.h"

namespace gha
{
    struct song_catalog_entry
    {
        song_type type;
        const char* title;
        const ChartNote* chart;
        int chart_size;
        chart_timing timing;
        int note_offset_ticks;
        bn::music_item music;
        bool restart_on_drop;
    };

    int song_catalog_size();
    const song_catalog_entry& song_catalog_entry_at(int index);
    const song_catalog_entry& song_catalog_entry_for(song_type song);
}

#endif