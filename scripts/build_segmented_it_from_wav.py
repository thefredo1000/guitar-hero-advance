from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
import tempfile
import wave
from pathlib import Path


def run_ffmpeg_to_u8_pcm(source_audio: Path, output_pcm: Path, sample_rate: int) -> None:
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


def read_template_sample_header(template_it: bytes) -> bytes:
    ordnum, insnum, smpnum, _patnum = struct.unpack_from("<4H", template_it, 0x20)
    if smpnum < 1:
        raise ValueError("Template IT must contain at least one sample")

    sample_table_offset = 0xC0 + ordnum + (insnum * 4)
    first_sample_header_ptr = struct.unpack_from("<I", template_it, sample_table_offset)[0]
    sample_header = template_it[first_sample_header_ptr:first_sample_header_ptr + 80]

    if len(sample_header) != 80 or sample_header[0:4] != b"IMPS":
        raise ValueError("Invalid template IT sample header")

    return sample_header


def split_pcm_chunks(pcm_data: bytes, chunk_seconds: int, sample_rate: int) -> list[bytes]:
    chunk_size = chunk_seconds * sample_rate
    if chunk_size <= 0:
        raise ValueError("chunk size must be positive")

    chunks: list[bytes] = []
    start = 0

    while start < len(pcm_data):
        end = min(start + chunk_size, len(pcm_data))
        chunk = pcm_data[start:end]
        if chunk:
            chunks.append(chunk)
        start = end

    if not chunks:
        raise ValueError("No PCM data produced from source audio")

    return chunks


def write_clip_wavs(chunks: list[bytes], clips_dir: Path, sample_rate: int) -> None:
    clips_dir.mkdir(parents=True, exist_ok=True)

    for index, chunk in enumerate(chunks):
        clip_path = clips_dir / f"clip_{index:03}.wav"
        with wave.open(str(clip_path), "wb") as wav_file:
            wav_file.setnchannels(1)
            wav_file.setsampwidth(1)
            wav_file.setframerate(sample_rate)
            wav_file.writeframes(chunk)


def build_pattern_data(sample_index_1_based: int) -> bytes:
    # One note at row 0 on channel 1. Remaining rows are empty row delimiters.
    # This is the same packed event structure used by the existing evenflow template.
    payload = bytearray()
    payload.extend([0x81, 0x03, 60, sample_index_1_based, 0x00])
    payload.extend([0x00] * 63)

    pattern = bytearray()
    pattern.extend(struct.pack("<H", len(payload)))
    pattern.extend(struct.pack("<H", 64))
    pattern.extend(struct.pack("<I", 0))
    pattern.extend(payload)
    return bytes(pattern)


def build_segmented_it(
    template_it_path: Path,
    chunks: list[bytes],
    output_it_path: Path,
    sample_rate: int,
    song_name: str,
) -> None:
    template_data = template_it_path.read_bytes()
    base_sample_header = read_template_sample_header(template_data)

    chunk_count = len(chunks)
    if chunk_count > 254:
        raise ValueError("Too many chunks for IT instrument index limits")

    out = bytearray(template_data[:0xC0])

    # Header core fields.
    name_bytes = song_name.encode("ascii", errors="replace")[:26].ljust(26, b"\0")
    out[4:30] = name_bytes

    ordnum = chunk_count + 1  # include end marker 0xFF
    insnum = 0
    smpnum = chunk_count
    patnum = chunk_count

    struct.pack_into("<4H", out, 0x20, ordnum, insnum, smpnum, patnum)

    # Pattern duration = rows * speed * 2.5 / tempo = 64 * 6 * 2.5 / 32 = 30s.
    out[0x32] = 6
    out[0x33] = 32

    orders = bytearray(range(chunk_count))
    orders.append(0xFF)
    out.extend(orders)

    sample_ptr_table_offset = len(out)
    out.extend(b"\0" * (smpnum * 4))

    pattern_ptr_table_offset = len(out)
    out.extend(b"\0" * (patnum * 4))

    sample_header_ptrs: list[int] = []

    for index, chunk in enumerate(chunks):
        sample_header_ptr = len(out)
        sample_header_ptrs.append(sample_header_ptr)

        sh = bytearray(base_sample_header)
        sh[0:4] = b"IMPS"
        sh[0x11] = 64
        sh[0x12] = 0x01  # sample present, 8-bit mono, no loop
        sh[0x13] = 64
        sh[0x14] = 0x00  # unsigned PCM
        sh[0x15] = 0x20  # center pan

        sample_name = f"seg_{index:03}".encode("ascii", errors="replace")[:26].ljust(26, b"\0")
        sh[0x20:0x20 + 26] = sample_name

        struct.pack_into("<I", sh, 0x30, len(chunk))
        struct.pack_into("<I", sh, 0x34, 0)
        struct.pack_into("<I", sh, 0x38, 0)
        struct.pack_into("<I", sh, 0x3C, sample_rate)
        struct.pack_into("<I", sh, 0x40, 0)
        struct.pack_into("<I", sh, 0x44, 0)
        struct.pack_into("<I", sh, 0x48, 0)

        out.extend(sh)
        struct.pack_into("<I", out, sample_ptr_table_offset + (index * 4), sample_header_ptr)

    for index in range(chunk_count):
        pattern_ptr = len(out)
        pattern = build_pattern_data(index + 1)
        out.extend(pattern)
        struct.pack_into("<I", out, pattern_ptr_table_offset + (index * 4), pattern_ptr)

    for index, chunk in enumerate(chunks):
        sample_data_ptr = len(out)
        out.extend(chunk)
        struct.pack_into("<I", out, sample_header_ptrs[index] + 0x48, sample_data_ptr)

    output_it_path.write_bytes(out)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Split WAV audio into fixed chunks and build an IT that switches samples every chunk.",
    )
    parser.add_argument("--template-it", type=Path, required=True)
    parser.add_argument("--source-audio", type=Path, required=True)
    parser.add_argument("--output-it", type=Path, required=True)
    parser.add_argument("--clips-dir", type=Path, required=True)
    parser.add_argument("--chunk-seconds", type=int, default=30)
    parser.add_argument("--sample-rate", type=int, default=8000)
    parser.add_argument("--song-name", default="Even Flow Segmented")
    parser.add_argument("--backup-output", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if args.backup_output and args.output_it.exists():
        backup_path = args.output_it.with_name(args.output_it.stem + "_orig" + args.output_it.suffix)
        shutil.copy2(args.output_it, backup_path)

    with tempfile.TemporaryDirectory(prefix="segmented_it_") as tmp_dir:
        pcm_path = Path(tmp_dir) / "source_u8.pcm"
        run_ffmpeg_to_u8_pcm(args.source_audio, pcm_path, args.sample_rate)
        pcm_data = pcm_path.read_bytes()

    chunks = split_pcm_chunks(pcm_data, args.chunk_seconds, args.sample_rate)
    write_clip_wavs(chunks, args.clips_dir, args.sample_rate)
    build_segmented_it(
        template_it_path=args.template_it,
        chunks=chunks,
        output_it_path=args.output_it,
        sample_rate=args.sample_rate,
        song_name=args.song_name,
    )

    print(f"Created {len(chunks)} clips in: {args.clips_dir}")
    print(f"Wrote IT: {args.output_it}")


if __name__ == "__main__":
    main()
