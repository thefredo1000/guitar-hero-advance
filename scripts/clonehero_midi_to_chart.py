from __future__ import annotations

import argparse
import configparser
from collections import Counter, defaultdict
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path
from typing import Iterable

import mido


DIFFICULTY_BASE = {
    "easy": 60,
    "medium": 72,
    "hard": 84,
    "expert": 96,
}

TRAVEL_TICKS = 136
DESPAWN_EXTRA_TICKS = 26


@dataclass(frozen=True)
class TempoEvent:
    tick: int
    us_per_beat: int

    @property
    def bpm(self) -> float:
        return mido.tempo2bpm(self.us_per_beat)


@dataclass(frozen=True)
class GuitarNote:
    start_tick: int
    end_tick: int
    lane: int


def load_song_ini(song_dir: Path) -> dict[str, str]:
    ini_path = song_dir / "song.ini"
    if not ini_path.exists():
        return {}

    parser = configparser.ConfigParser(strict=False)
    parser.optionxform = str
    parser.read(ini_path, encoding="utf-8")
    return dict(parser["song"]) if parser.has_section("song") else {}


def collect_tempos(midi: mido.MidiFile) -> list[TempoEvent]:
    sync_track = midi.tracks[0] if midi.tracks else []
    abs_tick = 0
    tempos: list[TempoEvent] = [TempoEvent(0, 500000)]

    for msg in sync_track:
        abs_tick += msg.time
        if msg.type == "set_tempo":
            if tempos and tempos[-1].tick == abs_tick:
                tempos[-1] = TempoEvent(abs_tick, msg.tempo)
            else:
                tempos.append(TempoEvent(abs_tick, msg.tempo))

    tempos.sort(key=lambda event: event.tick)
    return tempos


def track_name(track: mido.MidiTrack) -> str:
    for msg in track:
        if msg.type == "track_name":
            return msg.name
    return ""


def find_track(midi: mido.MidiFile, name: str) -> mido.MidiTrack:
    for track in midi.tracks:
        if track_name(track) == name:
            return track
    raise ValueError(f"Track '{name}' not found in MIDI")


def extract_guitar_notes(track: mido.MidiTrack, difficulty: str) -> list[GuitarNote]:
    base = DIFFICULTY_BASE[difficulty]
    abs_tick = 0
    active: dict[int, list[int]] = defaultdict(list)
    notes: list[GuitarNote] = []

    for msg in track:
        abs_tick += msg.time

        if msg.type == "note_on" and msg.velocity > 0:
            if base <= msg.note <= base + 4:
                active[msg.note].append(abs_tick)
        elif msg.type in ("note_off", "note_on") and (msg.type == "note_off" or msg.velocity == 0):
            if base <= msg.note <= base + 4 and active[msg.note]:
                start_tick = active[msg.note].pop(0)
                notes.append(GuitarNote(start_tick, abs_tick, msg.note - base))

    notes.sort(key=lambda note: (note.start_tick, note.lane, note.end_tick))
    return notes


def tick_to_seconds(target_tick: int, tempos: list[TempoEvent], ticks_per_beat: int) -> float:
    total_seconds = 0.0

    for index, tempo in enumerate(tempos):
        segment_start = tempo.tick
        segment_end = tempos[index + 1].tick if index + 1 < len(tempos) else target_tick

        if target_tick <= segment_start:
            break

        clamped_end = min(target_tick, segment_end)
        if clamped_end <= segment_start:
            continue

        delta_ticks = clamped_end - segment_start
        total_seconds += (delta_ticks / ticks_per_beat) * (tempo.us_per_beat / 1_000_000)

        if clamped_end == target_tick:
            break

    return total_seconds


def tick_to_frame(target_tick: int, tempos: list[TempoEvent], ticks_per_beat: int, fps: float) -> int:
    return round(tick_to_seconds(target_tick, tempos, ticks_per_beat) * fps)


def frame_to_fractional_beats(frame: int, frames_per_beat: Fraction) -> tuple[int, int, int]:
    beat_position = Fraction(frame, 1) / frames_per_beat
    whole_beats = beat_position.numerator // beat_position.denominator
    fractional = beat_position - whole_beats

    if fractional == 0:
        return whole_beats, 0, 1

    return whole_beats, fractional.numerator, fractional.denominator


def aligned_note_start_ticks(
    notes: list[GuitarNote],
    tempos: list[TempoEvent],
    ticks_per_beat: int,
    fps: float,
    align_first_note: bool,
) -> list[int]:
    start_frames = [tick_to_frame(note.start_tick, tempos, ticks_per_beat, fps) for note in notes]

    if align_first_note and start_frames:
        first_frame = start_frames[0]
        return [frame - first_frame for frame in start_frames]

    return start_frames


def emit_chart_initializer_absolute(
    symbol: str,
    notes: list[GuitarNote],
    tempos: list[TempoEvent],
    ticks_per_beat: int,
    countdown_beats: int,
    fps: float,
    align_first_note: bool,
) -> str:
    first_bpm = round(tempos[0].bpm)
    frames_per_beat = round((fps * 60) / first_bpm)
    lead_in = TRAVEL_TICKS + countdown_beats * frames_per_beat
    lines = [f"constexpr chart_timing {symbol}_TIMING({first_bpm});", f"constexpr ChartNote {symbol}_CHART[] = {{"]

    aligned_start_frames = aligned_note_start_ticks(notes, tempos, ticks_per_beat, fps, align_first_note)

    for note, start_frame in zip(notes, aligned_start_frames):
        hit_tick = lead_in + start_frame
        lines.append(f"    {{ {hit_tick}, {note.lane} }},")

    lines.append("};")
    lines.append(f"constexpr int {symbol}_CHART_SIZE = static_cast<int>(sizeof({symbol}_CHART) / sizeof({symbol}_CHART[0]));")
    return "\n".join(lines)


def max_active_notes(hit_ticks: Iterable[int]) -> int:
    ordered = sorted(hit_ticks)
    left = 0
    max_count = 0

    for right, hit_tick in enumerate(ordered):
        while ordered[left] < hit_tick - (TRAVEL_TICKS + DESPAWN_EXTRA_TICKS):
            left += 1

        active_count = right - left + 1
        if active_count > max_count:
            max_count = active_count

    return max_count


def summarize(
    notes: list[GuitarNote],
    tempos: list[TempoEvent],
    ticks_per_beat: int,
    countdown_beats: int,
    fps: float,
    align_first_note: bool,
) -> dict[str, object]:
    by_tick: dict[int, list[int]] = defaultdict(list)
    durations = []

    first_bpm = round(tempos[0].bpm)
    frames_per_beat = round((fps * 60) / first_bpm)
    lead_in = TRAVEL_TICKS + countdown_beats * frames_per_beat
    aligned_start_frames = aligned_note_start_ticks(notes, tempos, ticks_per_beat, fps, align_first_note)
    hit_ticks = []

    for note, start_frame in zip(notes, aligned_start_frames):
        by_tick[note.start_tick].append(note.lane)
        durations.append(note.end_tick - note.start_tick)
        hit_ticks.append(lead_in + start_frame)

    chord_sizes = Counter(len(lanes) for lanes in by_tick.values())
    return {
        "note_events": len(notes),
        "unique_hit_times": len(by_tick),
        "sustains_ge_240": sum(1 for duration in durations if duration >= 240),
        "sustains_ge_480": sum(1 for duration in durations if duration >= 480),
        "sustains_ge_960": sum(1 for duration in durations if duration >= 960),
        "max_active_notes": max_active_notes(hit_ticks),
        "chord_sizes": dict(sorted(chord_sizes.items())),
        "first_hits": [
            (tick, sorted(lanes))
            for tick, lanes in sorted(by_tick.items())[:20]
        ],
    }


def print_report(
    song_dir: Path,
    difficulty: str,
    notes: list[GuitarNote],
    tempos: list[TempoEvent],
    ticks_per_beat: int,
    fps: float,
    align_first_note: bool,
) -> None:
    song_ini = load_song_ini(song_dir)
    summary = summarize(notes, tempos, ticks_per_beat, 4, fps, align_first_note)
    print(f"song={song_ini.get('name', song_dir.name)}")
    print(f"artist={song_ini.get('artist', 'unknown')}")
    print(f"difficulty={difficulty}")
    print(f"align_first_note={align_first_note}")
    print(f"target_fps={fps}")
    print(f"ticks_per_beat={ticks_per_beat}")
    print(f"tempo_events={len(tempos)}")
    print("bpms=" + ", ".join(f"{event.tick}:{event.bpm:.3f}" for event in tempos[:16]))
    print(f"note_events={summary['note_events']}")
    print(f"unique_hit_times={summary['unique_hit_times']}")
    print(f"sustains_ge_240={summary['sustains_ge_240']}")
    print(f"sustains_ge_480={summary['sustains_ge_480']}")
    print(f"sustains_ge_960={summary['sustains_ge_960']}")
    print(f"max_active_notes={summary['max_active_notes']}")
    print(f"chord_sizes={summary['chord_sizes']}")
    print(f"first_hits={summary['first_hits']}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect and convert Clone Hero MIDI charts to guitar-hero-advance ChartNote format.")
    parser.add_argument("song_dir", type=Path, help="Directory containing notes.mid and song.ini")
    parser.add_argument("--difficulty", choices=sorted(DIFFICULTY_BASE), default="expert")
    parser.add_argument("--track", default="PART GUITAR")
    parser.add_argument("--symbol", default="EVEN_FLOW")
    parser.add_argument("--countdown-beats", type=int, default=4)
    parser.add_argument("--fps", type=float, default=59.7275)
    parser.add_argument("--align-first-note", action="store_true")
    parser.add_argument("--emit-chart", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    midi_path = args.song_dir / "notes.mid"
    midi = mido.MidiFile(midi_path)
    tempos = collect_tempos(midi)
    track = find_track(midi, args.track)
    notes = extract_guitar_notes(track, args.difficulty)

    print_report(args.song_dir, args.difficulty, notes, tempos, midi.ticks_per_beat, args.fps, args.align_first_note)

    if args.emit_chart:
        print()
        print(
            emit_chart_initializer_absolute(
                args.symbol,
                notes,
                tempos,
                midi.ticks_per_beat,
                args.countdown_beats,
                args.fps,
                args.align_first_note,
            )
        )


if __name__ == "__main__":
    main()