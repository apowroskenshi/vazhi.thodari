import numpy as np
import cv2
import sys
import os

def aces_tonemap(color, exposure=0.7):
    # Narkowicz ACES approximation — film-like, aggressively compresses highlights.
    # Right for production hero shots; hides firefly variance.
    color = color * exposure
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    mapped = (color * (a * color + b)) / (color * (c * color + d) + e)
    return np.clip(mapped, 0.0, 1.0)

def reinhard_white_point_tonemap(color, exposure=0.7, white=4.0):
    # Reinhard with explicit white point. Anything above `white` clips; values
    # below it compress less aggressively than ACES. Middle ground between ACES
    # softness and linear's harshness.
    color = color * exposure
    mapped = (color * (1.0 + color / (white * white))) / (1.0 + color)
    return np.clip(mapped, 0.0, 1.0)

def linear_clip_tonemap(color, exposure=0.5):
    # No highlight compression. Bright pixels clip to white; firefly spikes
    # stay at full intensity relative to neighbors. Use for variance/comparison
    # figures where the SIGNAL is what you want to show, not the aesthetic.
    return np.clip(color * exposure, 0.0, 1.0)

def aces_luminance_tonemap(color, exposure=0.7):
    color = color * exposure
    lum = 0.2126 * color[...,0] + 0.7152 * color[...,1] + 0.0722 * color[...,2]
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    mapped = (lum * (a * lum + b)) / (lum * (c * lum + d) + e)
    scale = np.where(lum > 1e-6, mapped / lum, 0.0)
    return np.clip(color * scale[..., np.newaxis], 0.0, 1.0)

OPERATORS = {
    "aces": aces_tonemap,
    "reinhard": reinhard_white_point_tonemap,
    "linear": linear_clip_tonemap,
    "aces_lum": aces_luminance_tonemap
}

def process_hdr(input_path, output_path=None, exposure=0.7, operator="aces", suffix=""):
    if output_path is None:
        base = os.path.splitext(input_path)[0]
        output_path = f"{base}{suffix}.png"

    hdr = cv2.imread(input_path, cv2.IMREAD_ANYDEPTH | cv2.IMREAD_ANYCOLOR)
    if hdr is None:
        print(f"Failed to read {input_path}")
        return

    hdr = cv2.cvtColor(hdr, cv2.COLOR_BGR2RGB)
    hdr = np.maximum(hdr, 0.0).astype(np.float32)

    ldr = OPERATORS[operator](hdr, exposure)
    ldr = np.power(ldr, 1.0 / 2.2)  # gamma encode for sRGB display

    ldr_8bit = (ldr * 255.0).astype(np.uint8)
    ldr_8bit = cv2.cvtColor(ldr_8bit, cv2.COLOR_RGB2BGR)

    cv2.imwrite(output_path, ldr_8bit)
    print(f"Wrote {output_path} [{operator}, exp={exposure}]")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python tonemap.py <input.hdr|folder> [exposure] [operator]")
        print("  operator: aces (default, production), reinhard (middle ground), linear (variance demos)")
        print("  Non-aces operators write <name>_<operator>.png to avoid overwriting production renders.")
        sys.exit(1)

    input_path = sys.argv[1]
    exposure = float(sys.argv[2]) if len(sys.argv) > 2 else 0.7
    operator = sys.argv[3] if len(sys.argv) > 3 else "aces"

    if operator not in OPERATORS:
        print(f"Unknown operator: {operator}. Choose from: {', '.join(OPERATORS)}")
        sys.exit(1)

    # Suffix non-aces variants so they don't overwrite the production renders
    suffix = "" if operator == "aces" else f"_{operator}"

    if os.path.isdir(input_path):
        hdr_files = [f for f in os.listdir(input_path) if f.lower().endswith(".hdr")]
        if not hdr_files:
            print(f"No .hdr files found in {input_path}")
            sys.exit(1)
        for filename in hdr_files:
            process_hdr(os.path.join(input_path, filename),
                        exposure=exposure, operator=operator, suffix=suffix)
    else:
        process_hdr(input_path, exposure=exposure, operator=operator, suffix=suffix)
