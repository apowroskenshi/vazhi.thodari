<#
.SYNOPSIS
Stitch a path-tracer convergence sequence into a home-page-gallery MP4.

.DESCRIPTION
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

.PARAMETER InputFolder
Directory containing the PNG sequence.

.PARAMETER OutputFile
Path to write the MP4 to.

.PARAMETER GeneratePoster
Also write poster.webp to the OutputFile's parent directory, sourced from the
denoised PNG. PathTracerHero.astro expects one poster per scene folder shared
across day/night videos - pass this switch only on whichever variant you want
the poster sourced from (typically the day pass, since that's the cycle's
starting state).

.PARAMETER PosterQuality
WebP quality 0-100 (default 85). 85 is visually transparent at ~15-20% the
size of the source PNG; bump higher for max fidelity or lower for bandwidth.

.EXAMPLE
.\encode_gallery.ps1 .\outputs\sm-courtyard-day .\sm-courtyard\day.mp4

.EXAMPLE
# Encode day (with poster) + night (no poster) for one scene
.\encode_gallery.ps1 .\outputs\sm-courtyard-day   .\gallery\sm-courtyard\day.mp4 -GeneratePoster
.\encode_gallery.ps1 .\outputs\sm-courtyard-night .\gallery\sm-courtyard\night.mp4

.EXAMPLE
# Batch all four scenes for both day/night, poster from each day variant
$scenes = 'sm-courtyard','sm-walkway','bistro-exterior','bistro-interior'
foreach ($s in $scenes) {
    .\encode_gallery.ps1 ".\outputs\$s-day"   ".\gallery\$s\day.mp4" -GeneratePoster
    .\encode_gallery.ps1 ".\outputs\$s-night" ".\gallery\$s\night.mp4"
}
#>
param(
    [Parameter(Mandatory=$true)][string]$InputFolder,
    [Parameter(Mandatory=$true)][string]$OutputFile,
    [switch]$GeneratePoster,
    [int]$PosterQuality = 85
)

$ErrorActionPreference = 'Stop'

# Constants - must match PathTracerHero.astro
$FPS                  = 8
$RAW_START            = 5
$RAW_STEP             = 5
$RAW_END_PASS         = 250
$HOLD_BEFORE_DENOISE  = 1.5
$HOLD_DENOISED        = 2.5
$FRAME_DURATION       = 1.0 / $FPS   # 0.125
$EXPECTED_DURATION    = ($RAW_END_PASS / $RAW_STEP) * $FRAME_DURATION + $HOLD_BEFORE_DENOISE + $HOLD_DENOISED  # 10.25

# --- Validate environment ---

if (-not (Get-Command ffmpeg  -ErrorAction SilentlyContinue)) { throw "ffmpeg not found in PATH"  }
if (-not (Get-Command ffprobe -ErrorAction SilentlyContinue)) { throw "ffprobe not found in PATH" }
if (-not (Test-Path $InputFolder -PathType Container))       { throw "Input folder not found: $InputFolder" }

# Resolve to absolute paths BEFORE Push-Location so relative inputs still work
$InputFolder = (Resolve-Path $InputFolder).Path
$OutputFile  = [System.IO.Path]::GetFullPath($OutputFile)

# --- Validate input files ---

$passSequence = @()
for ($n = $RAW_START; $n -le $RAW_END_PASS; $n += $RAW_STEP) { $passSequence += $n }

$missing = @()
foreach ($n in $passSequence) {
    $f = Join-Path $InputFolder "output_${n}.png"
    if (-not (Test-Path $f)) { $missing += $f }
}
$denoised = Join-Path $InputFolder "output_${RAW_END_PASS}_denoised.png"
if (-not (Test-Path $denoised)) { $missing += $denoised }

if ($missing.Count -gt 0) {
    Write-Host "Missing files:" -ForegroundColor Red
    $missing | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    throw "$($missing.Count) required file(s) missing from $InputFolder"
}

Write-Host "Found all $($passSequence.Count) raw frames + 1 denoised frame." -ForegroundColor Green

# --- Build the concat list ---
# Each frame gets one 'file' + 'duration' pair. The final raw frame (250 spp)
# absorbs the 1.5s pre-denoise hold into its duration. The denoised frame gets
# the 2.5s hold. The concat demuxer requires the final file to be repeated
# without a duration so ffmpeg knows how to end the timeline.

$concatPath = Join-Path $InputFolder "_concat.txt"
$lines = @()

foreach ($n in $passSequence) {
    $lines += "file 'output_${n}.png'"
    if ($n -eq $RAW_END_PASS) {
        # Last raw frame: one frame's worth + the pre-denoise hold
        $lines += "duration $($FRAME_DURATION + $HOLD_BEFORE_DENOISE)"
    } else {
        $lines += "duration $FRAME_DURATION"
    }
}
$lines += "file 'output_${RAW_END_PASS}_denoised.png'"
$lines += "duration $HOLD_DENOISED"
$lines += "file 'output_${RAW_END_PASS}_denoised.png'"  # concat-demuxer EOF quirk

$lines | Set-Content $concatPath -Encoding ASCII

# --- Encode ---

$outputDir = Split-Path $OutputFile -Parent
if ($outputDir -and -not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

Write-Host "Encoding -> $OutputFile" -ForegroundColor Cyan

# Run from the input folder so relative paths in concat.txt resolve correctly.
# OutputFile is already absolute, so it's fine to use as-is from a different cwd.
Push-Location $InputFolder
try {
    & ffmpeg -y `
        -f concat -safe 0 `
        -i "_concat.txt" `
        -c:v libx264 -pix_fmt yuv420p `
        -preset slow -crf 22 `
        -movflags +faststart `
        -vsync vfr `
        $OutputFile

    if ($LASTEXITCODE -ne 0) {
        throw "ffmpeg encoding failed (exit $LASTEXITCODE)"
    }
} finally {
    Pop-Location
    Remove-Item $concatPath -ErrorAction SilentlyContinue
}

# --- Verify output ---

$file = Get-Item $OutputFile
$durationStr = & ffprobe -v error -show_entries format=duration -of csv=p=0 $OutputFile
$duration = [double]$durationStr
$sizeMB = [math]::Round($file.Length / 1MB, 2)

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host "  Output:   $OutputFile"
Write-Host "  Size:     $sizeMB MB"
Write-Host "  Duration: $($duration.ToString('F2'))s  (expected $($EXPECTED_DURATION.ToString('F2'))s)"

if ([math]::Abs($duration - $EXPECTED_DURATION) -gt 0.1) {
    Write-Warning "Duration deviates from expected by >0.1s. The hero component's HUD timing relies on exact durations - check the concat list."
}

# --- Optional: generate poster.webp from the denoised PNG ---

if ($GeneratePoster) {
    $posterPath = Join-Path (Split-Path $OutputFile -Parent) "poster.webp"
    Write-Host ""
    Write-Host "Generating poster -> $posterPath" -ForegroundColor Cyan

    & ffmpeg -y -loglevel error `
        -i $denoised `
        -c:v libwebp -quality $PosterQuality `
        $posterPath

    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Poster generation failed (exit $LASTEXITCODE)"
    } else {
        $posterFile = Get-Item $posterPath
        $posterSizeKB = [math]::Round($posterFile.Length / 1KB, 1)
        Write-Host "  Poster:   $posterSizeKB KB (quality $PosterQuality)"
    }
}
