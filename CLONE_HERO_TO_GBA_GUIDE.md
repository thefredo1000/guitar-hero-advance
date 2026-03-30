# Clone Hero to GBA Chart Conversion Guide

This guide documents the current pipeline used in this project to convert Clone Hero charts into playable GBA chart headers.

It is split into two parts:

1. Chart conversion (`notes.mid` -> `ChartNote[]` header)
2. Audio conversion (module creation/playback for Butano + Maxmod)

---

## 1. Project Data Model (What GBA Expects)

In this project, notes are represented by:

- `hit_tick`: frame when the note should reach the hit line (`HALO_Y`)
- `lane`: integer from `0..4`

Defined in:

- `include/chart.h`

A chart is just:

```cpp
constexpr ChartNote SOME_SONG_CHART[] = {
    { 272, 4 },
    { 306, 0 },
    ...
};
```

And selected by gameplay code:

- `src/gameplay_screen.cpp`

---

## 2. Input Format from Clone Hero

From a Clone Hero song folder, we mainly use:

- `notes.mid`
- `song.ini` (optional metadata)

Typical folder:

- `song.opus`
- `guitar.opus`
- `notes.mid`
- `song.ini`

---

## 3. Conversion Script

Primary script:

- `scripts/clonehero_midi_to_chart.py`

### Quick usage

Analyze only:

```bash
/usr/local/bin/python3 scripts/clonehero_midi_to_chart.py \
  "/path/to/CloneHeroSong" \
  --difficulty expert
```

Emit C++ chart block:

```bash
/usr/local/bin/python3 scripts/clonehero_midi_to_chart.py \
  "/path/to/CloneHeroSong" \
  --difficulty expert \
  --align-first-note \
  --emit-chart \
  --symbol EVEN_FLOW
```

Write generated chart to header:

```bash
/usr/local/bin/python3 scripts/clonehero_midi_to_chart.py \
  "/path/to/CloneHeroSong" \
  --difficulty expert \
  --align-first-note \
  --emit-chart \
  --symbol EVEN_FLOW \
| awk 'BEGIN{print "#pragma once\n"} /^constexpr chart_timing/{emit=1} emit{print}' \
> include/even_flow_chart.h
```

---

## 4. Deep Dive: How `clonehero_midi_to_chart.py` Works

### 4.1 Difficulty mapping

Clone Hero/GH MIDI uses note ranges per difficulty:

- Easy: base 60
- Medium: base 72
- Hard: base 84
- Expert: base 96

The script maps notes `base..base+4` to lanes `0..4`.

Code area:

- `DIFFICULTY_BASE`
- `extract_guitar_notes(...)`

### 4.2 Tempo extraction

It parses tempo events from MIDI sync track (`set_tempo`) and stores them as:

- `TempoEvent(tick, us_per_beat)`

Code area:

- `collect_tempos(...)`
- `tick_to_seconds(...)`

### 4.3 Track selection

It finds the chart track by name (default `PART GUITAR`).

Code area:

- `track_name(...)`
- `find_track(...)`

### 4.4 Note extraction

For chosen difficulty + track:

1. Reads `note_on`/`note_off`
2. Pairs start/end ticks
3. Converts MIDI note -> lane index
4. Produces `GuitarNote(start_tick, end_tick, lane)`

Code area:

- `extract_guitar_notes(...)`

### 4.5 Time conversion (MIDI ticks -> GBA frames)

For each note start tick:

1. Convert tick -> seconds using segmented tempo map
2. Convert seconds -> frame using 60 FPS

Code area:

- `tick_to_seconds(...)`
- `tick_to_frame(...)`

### 4.6 First-note alignment

`--align-first-note` subtracts the first note frame from all note frames.

Why: many CH charts contain a count-in gap before first playable note.

Without alignment, first note can spawn too late.

Code area:

- `aligned_note_start_ticks(...)`

### 4.7 Final `hit_tick` assembly

The script computes:

- `lead_in = TRAVEL_TICKS + countdown_beats * frames_per_beat`
- `hit_tick = lead_in + aligned_start_frame`

This matches the game timing model in `chart_timing`.

Code area:

- `emit_chart_initializer_absolute(...)`

### 4.8 Compatibility report

Before emitting chart, script prints useful metrics:

- `note_events`
- `unique_hit_times`
- sustain counts by thresholds
- chord size distribution
- `max_active_notes`

This helps compare against engine limits like:

- `MAX_ACTIVE_NOTES` in `include/chart.h`

Code area:

- `summarize(...)`
- `max_active_notes(...)`
- `print_report(...)`

---

## 5. Integrating Generated Header

1. Generate header, for example:
   - `include/even_flow_chart.h`
2. Include it in `include/chart.h`
3. Add song enum in `include/song_type.h`
4. Add song in selection UI (`src/song_select_screen.cpp`)
5. Add chart selection + playback mapping (`src/gameplay_screen.cpp`)

---

## 6. Audio Notes (Important)

Current project uses Butano + Maxmod module playback.

Observed constraints:

- Very large or malformed modules may stop during runtime.
- Some generated XM content can stall `make` during `mmutil` processing.
- Keep backups outside `audio/` and avoid names with extra dots in that folder.

Known rule:

- Butano audio scanner expects filenames with one dot only in `audio/`.

---

## 7. Recommended Reliable Workflow

1. Convert chart with `clonehero_midi_to_chart.py`
2. Emit header with `--align-first-note`
3. Start with `hard` or `medium` for dense songs if needed
4. Validate `max_active_notes` against `MAX_ACTIVE_NOTES`
5. Build and test in emulator/hardware
6. Only after chart is stable, iterate on audio module strategy

---

## 8. Troubleshooting

### Build hangs at audio listing

Symptom:

```text
beat_it.xm
coolio.xm
...
```

and then no progress.

Likely cause:

- `mmutil` stuck on malformed module.

Action:

1. Move suspect file out of `audio/`
2. Rebuild
3. Re-add a known-good module

### Notes feel late/early

Action:

- Use `--align-first-note`
- Verify first `hit_tick` relative to lead-in

### Notes disappear / don’t spawn in dense sections

Action:

- Increase `MAX_ACTIVE_NOTES` in `include/chart.h`
- Or reduce chart density/difficulty

---

## 9. Script Reference

Main converter arguments:

- `song_dir`: Clone Hero song folder path
- `--difficulty`: `easy|medium|hard|expert`
- `--track`: default `PART GUITAR`
- `--symbol`: C++ symbol prefix (example `EVEN_FLOW`)
- `--countdown-beats`: default `4`
- `--align-first-note`: normalize to first playable note
- `--emit-chart`: print C++ output

---

## 10. Example End-to-End Command Set

```bash
cd /Users/rodrigocasale/Documents/proyects/gba_dev/butano-18.9.0/games/guitar-hero-advance

/usr/local/bin/python3 scripts/clonehero_midi_to_chart.py \
  "/Users/rodrigocasale/Downloads/Pearl Jam - Even Flow (Neversoft)" \
  --difficulty expert \
  --align-first-note \
  --emit-chart \
  --symbol EVEN_FLOW \
| awk 'BEGIN{print "#pragma once\n"} /^constexpr chart_timing/{emit=1} emit{print}' \
> include/even_flow_chart.h

make
open *.gba
```

---

## 11. Future Improvements

- Add optional export to split large charts into song sections.
- Add hold-note support in gameplay (`end_tick` usage).
- Add star power / HOPO interpretation from MIDI markers.
- Add CI check that validates chart density vs engine caps.
