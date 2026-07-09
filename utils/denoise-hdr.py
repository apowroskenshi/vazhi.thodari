#!/usr/bin/env python3
"""denoise-hdr.py — cross-platform port of denoise-hdr.ps1.

Denoises HDR image(s) using Intel OIDN, with FFmpeg handling HDR <-> PFM
conversion. Runs on Windows, macOS, and Linux (the binaries it shells out to
just need to be on PATH or discoverable next to this script).

Usage:
  Single file:
    python denoise-hdr.py input.hdr [--output out.hdr] [--albedo alb.hdr] [--normal nrm.hdr]
  Folder (all *.hdr):
    python denoise-hdr.py folder
  Folder, frame-sequence subset (e.g. frame_0000.hdr .. frame_0300.hdr):
    python denoise-hdr.py movie_frames --frame-pattern "frame_{:04d}.hdr" \
        --frame-start 0 --frame-end 300 --output-dir movie_frames/denoised
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

IS_WINDOWS = os.name == "nt"
OIDN_EXE = "oidnDenoise.exe" if IS_WINDOWS else "oidnDenoise"


def resolve_oidn(explicit):
    """Locate the oidnDenoise binary across platforms.

    Order: explicit path -> a few layouts next to this script -> PATH.
    """
    if explicit:
        found = shutil.which(explicit) or (explicit if os.path.isfile(explicit) else None)
        if not found:
            sys.exit(f"OIDN denoiser not found at: {explicit}")
        return found

    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(here, "oidn", "bin", OIDN_EXE),
        os.path.join(here, "oidn", OIDN_EXE),
        os.path.join(here, OIDN_EXE),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c

    on_path = shutil.which(OIDN_EXE)
    if on_path:
        return on_path

    sys.exit(
        f"Could not find {OIDN_EXE}. Pass --oidn-path, place it under "
        f"utils/oidn/bin/, or add it to PATH."
    )


def resolve_ffmpeg(explicit):
    found = shutil.which(explicit)
    if not found:
        sys.exit(f"FFmpeg not found: '{explicit}' is not on PATH.")
    return found


def convert_pfm(ffmpeg, src, dst):
    """Convert between HDR and PFM via FFmpeg (gbrpf32le, the format OIDN reads).

    The pixel format is symmetric, so the same invocation handles HDR->PFM and
    PFM->HDR; FFmpeg picks the container from each file's extension.
    """
    proc = subprocess.run(
        [ffmpeg, "-y", "-i", src, "-pix_fmt", "gbrpf32le", dst],
        capture_output=True, text=True,
    )
    if proc.returncode != 0:
        sys.exit(f"FFmpeg failed converting {src} -> {dst}:\n{proc.stderr}")


def denoise_file(input_hdr, output_hdr, albedo_hdr, normal_hdr, oidn, ffmpeg):
    print(f"Denoising: {input_hdr} -> {output_hdr}")

    tmp = tempfile.mkdtemp(prefix="oidn_denoise_")
    try:
        input_pfm = os.path.join(tmp, "input_color.pfm")
        output_pfm = os.path.join(tmp, "denoised_output.pfm")

        convert_pfm(ffmpeg, input_hdr, input_pfm)

        args = [oidn, "--hdr", input_pfm, "--output", output_pfm]

        if albedo_hdr and os.path.isfile(albedo_hdr):
            albedo_pfm = os.path.join(tmp, "input_albedo.pfm")
            convert_pfm(ffmpeg, albedo_hdr, albedo_pfm)
            args += ["--alb", albedo_pfm]

        if normal_hdr and os.path.isfile(normal_hdr):
            normal_pfm = os.path.join(tmp, "input_normal.pfm")
            convert_pfm(ffmpeg, normal_hdr, normal_pfm)
            args += ["--nrm", normal_pfm]

        proc = subprocess.run(args)
        if proc.returncode != 0:
            sys.exit(f"OIDN denoiser failed with exit code {proc.returncode}")
        if not os.path.isfile(output_pfm):
            sys.exit(f"OIDN did not produce output: {output_pfm}")

        out_dir = os.path.dirname(output_hdr)
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)

        convert_pfm(ffmpeg, output_pfm, output_hdr)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def batch_output_path(src_file, out_dir, suffix):
    name, ext = os.path.splitext(os.path.basename(src_file))
    if out_dir:
        return os.path.join(out_dir, f"{name}{ext}")
    return os.path.join(os.path.dirname(src_file), f"{name}{suffix}{ext}")


def collect_frames(folder, pattern, start, end):
    if start < 0 or end < 0:
        sys.exit("--frame-pattern requires --frame-start and --frame-end")
    files = []
    for i in range(start, end + 1):
        fpath = os.path.join(folder, pattern.format(i))
        if os.path.isfile(fpath):
            files.append(fpath)
        else:
            print(f"Warning: missing frame: {fpath}", file=sys.stderr)
    return files


def main():
    p = argparse.ArgumentParser(
        description="Denoise HDR image(s) with Intel OIDN (FFmpeg handles HDR<->PFM).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("input", help="Input .hdr file or a folder of them")
    p.add_argument("--output", default="", help="Output file (single-file mode)")
    p.add_argument("--output-dir", default="", help="Output directory (batch mode)")
    p.add_argument("--oidn-path", default="",
                   help="Path to oidnDenoise binary (auto-detected if omitted)")
    p.add_argument("--ffmpeg-path", default="ffmpeg", help="FFmpeg executable")
    p.add_argument("--albedo", default="", help="Albedo HDR aux buffer (single-file mode)")
    p.add_argument("--normal", default="", help="Normal HDR aux buffer (single-file mode)")
    p.add_argument("--frame-pattern", default="",
                   help="Python format pattern, e.g. 'frame_{:04d}.hdr'")
    p.add_argument("--frame-start", type=int, default=-1)
    p.add_argument("--frame-end", type=int, default=-1)
    p.add_argument("--suffix", default="_denoised",
                   help="Suffix for outputs when no explicit output path is given")
    args = p.parse_args()

    if not os.path.exists(args.input):
        sys.exit(f"Input not found: {args.input}")

    oidn = resolve_oidn(args.oidn_path)
    ffmpeg = resolve_ffmpeg(args.ffmpeg_path)

    if os.path.isdir(args.input):
        # --- Folder mode ---
        if args.frame_pattern:
            files = collect_frames(args.input, args.frame_pattern,
                                   args.frame_start, args.frame_end)
        else:
            files = sorted(
                os.path.join(args.input, f)
                for f in os.listdir(args.input)
                if f.lower().endswith(".hdr")
                and not os.path.splitext(f)[0].endswith(args.suffix)
            )

        if not files:
            print(f"No HDR files to process in {args.input}")
            return

        for idx, f in enumerate(files, 1):
            print(f"[{idx}/{len(files)}] {os.path.basename(f)}")
            out_path = batch_output_path(f, args.output_dir, args.suffix)
            denoise_file(f, out_path, "", "", oidn, ffmpeg)

        print(f"\nDone! Processed {len(files)} file(s).")
    else:
        # --- Single file mode ---
        output = args.output
        if not output:
            base, ext = os.path.splitext(args.input)
            output = f"{base}{args.suffix}{ext}"

        denoise_file(args.input, output, args.albedo, args.normal, oidn, ffmpeg)
        print(f"\nDone! Denoised HDR saved to: {output}")


if __name__ == "__main__":
    main()
