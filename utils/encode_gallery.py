#!/usr/bin/env python3
"""encode_gallery.py — cross-platform port of encode_gallery.ps1.

Stitch a path-tracer convergence sequence into a home-page-gallery MP4. Runs on
Windows, macOS, and Linux (needs ffmpeg + ffprobe on PATH).

Encodes a 10.25-second clip matching the timing the PathTracerHero component
expects on the portfolio site:

  - 50 raw frames at 5-pass intervals (5, 10, ..., 250 spp) played at 8 fps -> 6.25s
  - 1.5s hold on the 250-spp noisy frame
  - 2.5s hold on the denoised frame

The HUD pass counter in PathTracerHero.astro reads currentTime directly, so the
durations must hit exactly. The pop animation fires at t=7.75s (the noisy ->
denoised transition); the video doesn't need to contain any transition effect.

Required files in the input folder:
  output_5.png, output_10.png, ..., output_250.png  (50 files)
  output_250_denoised.png

Examples:
  python encode_gallery.py ./outputs/sm-courtyard-day ./gallery/sm-courtyard/day.mp4 --poster
  python encode_gallery.py ./outputs/sm-courtyard-night ./gallery/sm-courtyard/night.mp4
"""

import argparse
import os
import shutil
import subprocess
import sys

# Constants - must match PathTracerHero.astro
FPS = 8
RAW_START = 5
RAW_STEP = 5
RAW_END_PASS = 250
HOLD_BEFORE_DENOISE = 1.5
HOLD_DENOISED = 2.5
FRAME_DURATION = 1.0 / FPS  # 0.125
EXPECTED_DURATION = (
    (RAW_END_PASS / RAW_STEP) * FRAME_DURATION + HOLD_BEFORE_DENOISE + HOLD_DENOISED
)  # 10.25


def require(tool):
    if not shutil.which(tool):
        sys.exit(f"{tool} not found in PATH")


def ffmpeg_has_libwebp():
    """True if this ffmpeg build was compiled with the libwebp encoder."""
    out = subprocess.run(
        ["ffmpeg", "-hide_banner", "-encoders"],
        capture_output=True, text=True,
    ).stdout
    return "libwebp" in out


def write_poster(src_png, poster_path, quality):
    """Write poster.webp from a PNG, preferring ffmpeg's libwebp and falling
    back to OpenCV. Returns True on success.

    Not every ffmpeg build ships the libwebp encoder (e.g. Homebrew's default
    lacks --enable-libwebp), so we probe first and use cv2 — already a project
    dependency via tonemap.py — when ffmpeg can't do it.
    """
    if ffmpeg_has_libwebp():
        proc = subprocess.run(
            ["ffmpeg", "-y", "-loglevel", "error",
             "-i", src_png, "-c:v", "libwebp", "-quality", str(quality),
             poster_path],
        )
        if proc.returncode == 0:
            return True
        print(f"Warning: ffmpeg poster encode failed (exit {proc.returncode}), "
              "trying OpenCV...", file=sys.stderr)

    try:
        import cv2
    except ImportError:
        print(
            "Warning: poster skipped — this ffmpeg has no libwebp encoder and "
            "OpenCV is not installed. Install an ffmpeg with --enable-libwebp, "
            "or `pip install opencv-python`.",
            file=sys.stderr,
        )
        return False

    img = cv2.imread(src_png, cv2.IMREAD_UNCHANGED)
    if img is None:
        print(f"Warning: poster skipped — could not read {src_png}", file=sys.stderr)
        return False
    return bool(cv2.imwrite(poster_path, img, [cv2.IMWRITE_WEBP_QUALITY, quality]))


def main():
    p = argparse.ArgumentParser(
        description="Stitch a path-tracer convergence sequence into a gallery MP4.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("input_folder", help="Directory containing the PNG sequence")
    p.add_argument("output_file", help="Path to write the MP4 to")
    p.add_argument("--poster", action="store_true",
                   help="Also write poster.webp (from the denoised PNG) beside the MP4")
    p.add_argument("--poster-quality", type=int, default=85,
                   help="WebP quality 0-100 (default 85)")
    args = p.parse_args()

    # Flush prints before each shelled-out ffmpeg/ffprobe call so our progress
    # lines stay interleaved with their stderr in the right order.
    sys.stdout.reconfigure(line_buffering=True)

    # --- Validate environment ---
    require("ffmpeg")
    require("ffprobe")
    if not os.path.isdir(args.input_folder):
        sys.exit(f"Input folder not found: {args.input_folder}")

    # Resolve to absolute paths so relative inputs still work once we run from
    # the input folder (the concat list uses relative names).
    input_folder = os.path.abspath(args.input_folder)
    output_file = os.path.abspath(args.output_file)

    # --- Validate input files ---
    pass_sequence = list(range(RAW_START, RAW_END_PASS + 1, RAW_STEP))

    missing = [
        os.path.join(input_folder, f"output_{n}.png")
        for n in pass_sequence
        if not os.path.isfile(os.path.join(input_folder, f"output_{n}.png"))
    ]
    denoised = os.path.join(input_folder, f"output_{RAW_END_PASS}_denoised.png")
    if not os.path.isfile(denoised):
        missing.append(denoised)

    if missing:
        print("Missing files:", file=sys.stderr)
        for m in missing:
            print(f"  {m}", file=sys.stderr)
        sys.exit(f"{len(missing)} required file(s) missing from {input_folder}")

    print(f"Found all {len(pass_sequence)} raw frames + 1 denoised frame.")

    # --- Build the concat list ---
    # Each frame gets one 'file' + 'duration' pair. The final raw frame (250 spp)
    # absorbs the 1.5s pre-denoise hold into its duration. The denoised frame gets
    # the 2.5s hold. The concat demuxer requires the final file to be repeated
    # without a duration so ffmpeg knows how to end the timeline.
    concat_path = os.path.join(input_folder, "_concat.txt")
    lines = []
    for n in pass_sequence:
        lines.append(f"file 'output_{n}.png'")
        if n == RAW_END_PASS:
            lines.append(f"duration {FRAME_DURATION + HOLD_BEFORE_DENOISE}")
        else:
            lines.append(f"duration {FRAME_DURATION}")
    lines.append(f"file 'output_{RAW_END_PASS}_denoised.png'")
    lines.append(f"duration {HOLD_DENOISED}")
    lines.append(f"file 'output_{RAW_END_PASS}_denoised.png'")  # concat-demuxer EOF quirk

    with open(concat_path, "w", encoding="ascii", newline="\n") as fh:
        fh.write("\n".join(lines) + "\n")

    # --- Encode ---
    output_dir = os.path.dirname(output_file)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    print(f"Encoding -> {output_file}")

    # Run from the input folder so relative paths in _concat.txt resolve.
    # output_file is already absolute, so it's fine from a different cwd.
    try:
        proc = subprocess.run(
            [
                "ffmpeg", "-y",
                "-f", "concat", "-safe", "0",
                "-i", "_concat.txt",
                "-c:v", "libx264", "-pix_fmt", "yuv420p",
                "-preset", "slow", "-crf", "22",
                "-movflags", "+faststart",
                "-vsync", "vfr",
                output_file,
            ],
            cwd=input_folder,
        )
        if proc.returncode != 0:
            sys.exit(f"ffmpeg encoding failed (exit {proc.returncode})")
    finally:
        try:
            os.remove(concat_path)
        except OSError:
            pass

    # --- Verify output ---
    duration_str = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "csv=p=0", output_file],
        capture_output=True, text=True,
    ).stdout.strip()
    duration = float(duration_str)
    size_mb = round(os.path.getsize(output_file) / (1024 * 1024), 2)

    print()
    print("Done.")
    print(f"  Output:   {output_file}")
    print(f"  Size:     {size_mb} MB")
    print(f"  Duration: {duration:.2f}s  (expected {EXPECTED_DURATION:.2f}s)")

    if abs(duration - EXPECTED_DURATION) > 0.1:
        print(
            "Warning: duration deviates from expected by >0.1s. The hero "
            "component's HUD timing relies on exact durations - check the concat list.",
            file=sys.stderr,
        )

    # --- Optional: generate poster.webp from the denoised PNG ---
    if args.poster:
        poster_path = os.path.join(os.path.dirname(output_file), "poster.webp")
        print()
        print(f"Generating poster -> {poster_path}")

        if write_poster(denoised, poster_path, args.poster_quality):
            poster_kb = round(os.path.getsize(poster_path) / 1024, 1)
            print(f"  Poster:   {poster_kb} KB (quality {args.poster_quality})")


if __name__ == "__main__":
    main()
