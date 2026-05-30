# denoise-hdr.ps1
# Denoises HDR image(s) using OIDN, with FFmpeg handling HDR <-> PFM conversion.
#
# Usage:
#   Single file:
#     .\denoise-hdr.ps1 -InputPath "input.hdr" [-Output "out.hdr"] [-AlbedoHDR alb.hdr] [-NormalHDR nrm.hdr]
#   Folder (all *.hdr):
#     .\denoise-hdr.ps1 -InputPath "folder"
#   Folder, frame sequence subset (e.g. frame_0000.hdr .. frame_0300.hdr):
#     .\denoise-hdr.ps1 -InputPath "movie_frames" -FramePattern "frame_{0:D4}.hdr" -FrameStart 0 -FrameEnd 300 -OutputDir "movie_frames/denoised"

param(
    [Parameter(Mandatory=$true)]
    [string]$InputPath,

    [Parameter(Mandatory=$false)]
    [string]$Output = "",

    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "",

    [Parameter(Mandatory=$false)]
    [string]$OdinPath = "odin\bin\oidnDenoise.exe",

    [Parameter(Mandatory=$false)]
    [string]$FFmpegPath = "ffmpeg",

    [Parameter(Mandatory=$false)]
    [string]$AlbedoHDR = "",

    [Parameter(Mandatory=$false)]
    [string]$NormalHDR = "",

    [Parameter(Mandatory=$false)]
    [string]$FramePattern = "",

    [Parameter(Mandatory=$false)]
    [int]$FrameStart = -1,

    [Parameter(Mandatory=$false)]
    [int]$FrameEnd = -1,

    [Parameter(Mandatory=$false)]
    [string]$Suffix = "_denoised"
)

$ErrorActionPreference = "Stop"

# --- FFmpeg helpers ---
function Convert-HDRtoPFM {
    param([string]$SourceFile, [string]$DestFile, [string]$FFmpeg)
    $proc = Start-Process -FilePath $FFmpeg `
        -ArgumentList "-y -i `"$SourceFile`" -pix_fmt gbrpf32le `"$DestFile`"" `
        -NoNewWindow -Wait -PassThru -RedirectStandardError "$env:TEMP\ffmpeg_err.log"
    if ($proc.ExitCode -ne 0) {
        $errLog = Get-Content "$env:TEMP\ffmpeg_err.log" -Raw
        throw "FFmpeg failed converting $SourceFile to PFM:`n$errLog"
    }
}

function Convert-PFMtoHDR {
    param([string]$SourceFile, [string]$DestFile, [string]$FFmpeg)
    $proc = Start-Process -FilePath $FFmpeg `
        -ArgumentList "-y -i `"$SourceFile`" -pix_fmt gbrpf32le `"$DestFile`"" `
        -NoNewWindow -Wait -PassThru -RedirectStandardError "$env:TEMP\ffmpeg_err.log"
    if ($proc.ExitCode -ne 0) {
        $errLog = Get-Content "$env:TEMP\ffmpeg_err.log" -Raw
        throw "FFmpeg failed converting $SourceFile to HDR:`n$errLog"
    }
}

# --- Core: denoise a single HDR file ---
function Invoke-DenoiseFile {
    param(
        [string]$InputHDR,
        [string]$OutputHDR,
        [string]$AlbedoHDR,
        [string]$NormalHDR,
        [string]$OdinPath,
        [string]$FFmpegPath
    )

    Write-Host "Denoising: $InputHDR -> $OutputHDR"

    $TempDir = Join-Path $env:TEMP "odin_denoise_$([guid]::NewGuid().ToString('N'))"
    New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

    try {
        $InputPFM  = Join-Path $TempDir "input_color.pfm"
        $OutputPFM = Join-Path $TempDir "denoised_output.pfm"

        Convert-HDRtoPFM -SourceFile $InputHDR -DestFile $InputPFM -FFmpeg $FFmpegPath

        $AllArgs = @("--hdr", $InputPFM, "--output", $OutputPFM)

        if ($AlbedoHDR -and (Test-Path $AlbedoHDR)) {
            $AlbedoPFM = Join-Path $TempDir "input_albedo.pfm"
            Convert-HDRtoPFM -SourceFile $AlbedoHDR -DestFile $AlbedoPFM -FFmpeg $FFmpegPath
            $AllArgs += "--alb", $AlbedoPFM
        }

        if ($NormalHDR -and (Test-Path $NormalHDR)) {
            $NormalPFM = Join-Path $TempDir "input_normal.pfm"
            Convert-HDRtoPFM -SourceFile $NormalHDR -DestFile $NormalPFM -FFmpeg $FFmpegPath
            $AllArgs += "--nrm", $NormalPFM
        }

        & $OdinPath @AllArgs
        if ($LASTEXITCODE -ne 0) { throw "OIDN denoiser failed with exit code $LASTEXITCODE" }
        if (-not (Test-Path $OutputPFM)) { throw "OIDN did not produce output: $OutputPFM" }

        $OutDir = [System.IO.Path]::GetDirectoryName($OutputHDR)
        if ($OutDir -and -not (Test-Path $OutDir)) {
            New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
        }

        Convert-PFMtoHDR -SourceFile $OutputPFM -DestFile $OutputHDR -FFmpeg $FFmpegPath
    } finally {
        Remove-Item -Recurse -Force $TempDir -ErrorAction SilentlyContinue
    }
}

# --- Resolve output path for batch mode ---
function Get-BatchOutputPath {
    param([string]$SrcFile, [string]$OutDir, [string]$Suffix)
    $name = [System.IO.Path]::GetFileNameWithoutExtension($SrcFile)
    $ext  = [System.IO.Path]::GetExtension($SrcFile)
    if ($OutDir) {
        return Join-Path $OutDir "${name}${ext}"
    } else {
        $dir = [System.IO.Path]::GetDirectoryName($SrcFile)
        return Join-Path $dir "${name}${Suffix}${ext}"
    }
}

# --- Validate ---
if (-not (Test-Path $InputPath)) {
    Write-Error "Input not found: $InputPath"
    exit 1
}

$item = Get-Item $InputPath

if ($item.PSIsContainer) {
    # --- Folder mode ---
    if ($FramePattern) {
        if ($FrameStart -lt 0 -or $FrameEnd -lt 0) {
            Write-Error "-FramePattern requires -FrameStart and -FrameEnd"
            exit 1
        }
        $files = @()
        for ($i = $FrameStart; $i -le $FrameEnd; $i++) {
            $fname = [string]::Format($FramePattern, $i)
            $fpath = Join-Path $item.FullName $fname
            if (Test-Path $fpath) {
                $files += Get-Item $fpath
            } else {
                Write-Warning "Missing frame: $fpath"
            }
        }
    } else {
        $files = Get-ChildItem -Path $item.FullName -Filter "*.hdr" -File |
                 Where-Object { $_.BaseName -notlike "*$Suffix" }
    }

    if (-not $files) {
        Write-Warning "No HDR files to process in $($item.FullName)"
        exit 0
    }

    $count = 0
    foreach ($f in $files) {
        $count++
        Write-Host "[$count/$($files.Count)] $($f.Name)"
        $outPath = Get-BatchOutputPath -SrcFile $f.FullName -OutDir $OutputDir -Suffix $Suffix
        Invoke-DenoiseFile -InputHDR $f.FullName -OutputHDR $outPath `
            -AlbedoHDR "" -NormalHDR "" -OdinPath $OdinPath -FFmpegPath $FFmpegPath
    }

    Write-Host "`nDone! Processed $count file(s)."
} else {
    # --- Single file mode ---
    if (-not $Output) {
        $dir  = [System.IO.Path]::GetDirectoryName($item.FullName)
        $name = [System.IO.Path]::GetFileNameWithoutExtension($item.FullName)
        $ext  = [System.IO.Path]::GetExtension($item.FullName)
        $Output = Join-Path $dir "${name}${Suffix}${ext}"
    }

    Invoke-DenoiseFile -InputHDR $item.FullName -OutputHDR $Output `
        -AlbedoHDR $AlbedoHDR -NormalHDR $NormalHDR `
        -OdinPath $OdinPath -FFmpegPath $FFmpegPath

    Write-Host "`nDone! Denoised HDR saved to: $Output"
}
