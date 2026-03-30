from __future__ import annotations

import argparse
import struct
import subprocess
import tempfile
from pathlib import Path


def run_ffmpeg_to_s8_mono(source_audio: Path, output_pcm: Path, sample_rate: int) -> None:
    cmd = [
        "ffmpeg",
        "-y",
        "-i",
        str(source_audio),
        "-ac",
        "1",
        "-ar",
        str(sample_rate),
        "-f",
        "s8",
        str(output_pcm),
    ]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def delta_encode_s8(samples: bytes) -> bytes:
    out = bytearray(len(samples))
    prev = 0

    for index, raw_byte in enumerate(samples):
        current = raw_byte if raw_byte < 128 else raw_byte - 256
        delta = current - prev
        delta_wrapped = ((delta + 128) & 0xFF) - 128
        out[index] = delta_wrapped & 0xFF
        prev = current

    return bytes(out)


def write_xm(source_audio: Path, output_xm: Path, song_name: str, sample_rate: int) -> None:
    with tempfile.TemporaryDirectory(prefix="xm_audio_") as tmp_dir:
        pcm_path = Path(tmp_dir) / "audio_s8.pcm"
        run_ffmpeg_to_s8_mono(source_audio, pcm_path, sample_rate)
        sample_data = pcm_path.read_bytes()

    sample_data_delta = delta_encode_s8(sample_data)

    # XM pattern with 1 channel, 256 rows, one note at row 0.
    rows = 256
    pattern_data = bytearray(rows * 5)
    pattern_data[0] = 49  # C-4
    pattern_data[1] = 1   # instrument 1
    pattern_data[2] = 0   # volume column
    pattern_data[3] = 0   # effect
    pattern_data[4] = 0   # effect parameter

    module_name = song_name[:20].ljust(20, "\0").encode("ascii", errors="replace")
    tracker_name = "GHA XM BUILDER"[:20].ljust(20, "\0").encode("ascii", errors="replace")

    # Main XM header.
    header = bytearray()
    header.extend(b"Extended Module: ")
    header.extend(module_name)
    header.append(0x1A)
    header.extend(tracker_name)
    header.extend(struct.pack("<H", 0x0104))
    header.extend(struct.pack("<I", 276))          # header size
    header.extend(struct.pack("<H", 1))            # song length
    header.extend(struct.pack("<H", 0))            # restart position
    header.extend(struct.pack("<H", 1))            # channels
    header.extend(struct.pack("<H", 1))            # patterns
    header.extend(struct.pack("<H", 1))            # instruments
    header.extend(struct.pack("<H", 1))            # linear frequency table
    header.extend(struct.pack("<H", 31))           # default speed (ticks per row)
    header.extend(struct.pack("<H", 32))           # default BPM
    order_table = bytearray(256)
    order_table[0] = 0
    header.extend(order_table)

    # Pattern header.
    pattern_header = bytearray()
    pattern_header.extend(struct.pack("<I", 9))
    pattern_header.append(0)
    pattern_header.extend(struct.pack("<H", rows))
    pattern_header.extend(struct.pack("<H", len(pattern_data)))

    # Instrument header (with one sample).
    instrument_header = bytearray()
    instrument_header.extend(struct.pack("<I", 263))
    instrument_name = "evenflow"[:22].ljust(22, "\0").encode("ascii", errors="replace")
    instrument_header.extend(instrument_name)
    instrument_header.append(0)                      # instrument type
    instrument_header.extend(struct.pack("<H", 1))
    instrument_header.extend(struct.pack("<I", 40))
    instrument_header.extend(bytes([0] * 96))
    instrument_header.extend(bytes([0] * 48))
    instrument_header.extend(bytes([0] * 48))
    instrument_header.extend(bytes([0, 0]))          # num points
    instrument_header.extend(bytes([0, 0, 0]))       # vol sustain/loop
    instrument_header.extend(bytes([0, 0, 0]))       # pan sustain/loop
    instrument_header.extend(bytes([0, 0]))          # vol/pan type
    instrument_header.extend(bytes([0, 0, 0, 0]))    # vibrato
    instrument_header.extend(struct.pack("<H", 0))
    instrument_header.extend(struct.pack("<H", 0))

    # Sample header.
    sample_header = bytearray()
    sample_header.extend(struct.pack("<I", len(sample_data_delta)))
    sample_header.extend(struct.pack("<I", 0))
    sample_header.extend(struct.pack("<I", 0))
    sample_header.append(64)                         # volume
    sample_header.append(0)                          # finetune
    sample_header.append(0)                          # type: no loop, 8-bit
    sample_header.append(128)                        # panning center
    sample_header.append(0)                          # relative note
    sample_header.append(0)
    sample_name = "evenflow"[:22].ljust(22, "\0").encode("ascii", errors="replace")
    sample_header.extend(sample_name)

    xm = bytearray()
    xm.extend(header)
    xm.extend(pattern_header)
    xm.extend(pattern_data)
    xm.extend(instrument_header)
    xm.extend(sample_header)
    xm.extend(sample_data_delta)

    output_xm.write_bytes(xm)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build a simple one-sample XM module from an audio file.")
    parser.add_argument("--source-audio", type=Path, required=True)
    parser.add_argument("--output-xm", type=Path, required=True)
    parser.add_argument("--song-name", default="Even Flow")
    parser.add_argument("--sample-rate", type=int, default=8363)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    write_xm(args.source_audio, args.output_xm, args.song_name, args.sample_rate)
    print(f"Wrote: {args.output_xm}")


if __name__ == "__main__":
    main()
