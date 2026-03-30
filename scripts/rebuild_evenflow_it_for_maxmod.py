from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path


def run_ffmpeg_to_pcm(source_audio: Path, output_pcm: Path, sample_rate: int) -> None:
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
        "u8",
        str(output_pcm),
    ]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def sample_header_offset(it_data: bytes) -> int:
    ordnum, insnum, smpnum, _patnum = struct.unpack_from("<4H", it_data, 0x20)
    if smpnum != 1:
        raise ValueError(f"Expected 1 sample in template IT, found {smpnum}")

    table_offset = 0xC0 + ordnum
    sample_table_offset = table_offset + (insnum * 4)
    return struct.unpack_from("<I", it_data, sample_table_offset)[0]


def rebuild_it(template_it: Path, source_audio: Path, output_it: Path, sample_rate: int) -> None:
    data = bytearray(template_it.read_bytes())
    sh = sample_header_offset(data)

    with tempfile.TemporaryDirectory(prefix="evenflow_pcm_") as tmp_dir:
        pcm_path = Path(tmp_dir) / "audio.pcm"
        run_ffmpeg_to_pcm(source_audio, pcm_path, sample_rate)
        pcm_data = pcm_path.read_bytes()

    # Keep all template pattern/order data, but rewrite sample descriptor and sample bytes.
    sample_ptr = len(data)
    data.extend(pcm_data)

    # IMPS sample header fields:
    # +0x11 gvl
    # +0x12 flags
    # +0x13 volume
    # +0x14 convert
    # +0x15 default pan
    # +0x30 length, loop begin, loop end, C5 speed, sustain begin, sustain end, sample ptr
    data[sh + 0x11] = 64
    data[sh + 0x12] = 0x01  # sample present, 8-bit mono, uncompressed, no loop
    data[sh + 0x13] = 64
    data[sh + 0x14] = 0x00  # unsigned PCM
    data[sh + 0x15] = 0x20  # center default pan

    struct.pack_into("<I", data, sh + 0x30, len(pcm_data))
    struct.pack_into("<I", data, sh + 0x34, 0)
    struct.pack_into("<I", data, sh + 0x38, 0)
    struct.pack_into("<I", data, sh + 0x3C, sample_rate)
    struct.pack_into("<I", data, sh + 0x40, 0)
    struct.pack_into("<I", data, sh + 0x44, 0)
    struct.pack_into("<I", data, sh + 0x48, sample_ptr)

    output_it.write_bytes(data)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Rebuild Even Flow IT into a maxmod-friendlier uncompressed sample module.")
    parser.add_argument("--template-it", type=Path, required=True)
    parser.add_argument("--source-audio", type=Path, required=True)
    parser.add_argument("--output-it", type=Path, required=True)
    parser.add_argument("--sample-rate", type=int, default=8000)
    parser.add_argument("--backup-original", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if args.backup_original and args.output_it.exists():
        backup = args.output_it.with_suffix(args.output_it.suffix + ".orig")
        shutil.copy2(args.output_it, backup)

    rebuild_it(args.template_it, args.source_audio, args.output_it, args.sample_rate)
    print(f"Wrote: {args.output_it}")


if __name__ == "__main__":
    main()
