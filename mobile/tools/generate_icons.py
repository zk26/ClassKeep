#!/usr/bin/env python3
"""Generate ikeback launcher icons from the project logo.

Source: G:/desktop/ikeback_logo.png (repository root, provided by the owner).
Run from anywhere:  python mobile/tools/generate_icons.py

Produces ic_launcher.png (rounded-square logo) and ic_launcher_round.png
(circular mask) for every density, and removes the Android Studio template
adaptive-icon XML / webp files so no duplicate resource names remain.
"""

import os
from PIL import Image, ImageDraw

TOOLS = os.path.dirname(os.path.abspath(__file__))
MOBILE = os.path.dirname(TOOLS)
REPO = os.path.dirname(MOBILE)
LOGO = os.path.join(REPO, "ikeback_logo.png")
RES = os.path.join(MOBILE, "app", "src", "main", "res")

SIZES = {
    "mdpi": 48,
    "hdpi": 72,
    "xhdpi": 96,
    "xxhdpi": 144,
    "xxxhdpi": 192,
}

# Brand colours sampled from the logo (blue -> violet).
BRAND_BACKGROUND = (30, 104, 214, 255)


def main():
    if not os.path.isfile(LOGO):
        raise SystemExit(f"Logo not found: {LOGO}")

    source = Image.open(LOGO).convert("RGBA")

    # Remove template adaptive icons: we ship explicit raster icons instead.
    anydpi = os.path.join(RES, "mipmap-anydpi")
    for name in ("ic_launcher.xml", "ic_launcher_round.xml"):
        path = os.path.join(anydpi, name)
        if os.path.isfile(path):
            os.remove(path)
    if os.path.isdir(anydpi) and not os.listdir(anydpi):
        os.rmdir(anydpi)

    for density, size in SIZES.items():
        folder = os.path.join(RES, f"mipmap-{density}")
        os.makedirs(folder, exist_ok=True)
        for name in ("ic_launcher.webp", "ic_launcher_round.webp"):
            path = os.path.join(folder, name)
            if os.path.isfile(path):
                os.remove(path)

        square = source.resize((size, size), Image.LANCZOS)
        square.save(os.path.join(folder, "ic_launcher.png"))

        inner_size = max(1, int(size * 0.92))
        inner = source.resize((inner_size, inner_size), Image.LANCZOS)
        round_icon = Image.new("RGBA", (size, size), BRAND_BACKGROUND)
        round_icon.alpha_composite(inner, ((size - inner_size) // 2, (size - inner_size) // 2))

        scale = 4
        mask = Image.new("L", (size * scale, size * scale), 0)
        ImageDraw.Draw(mask).ellipse((0, 0, size * scale, size * scale), fill=255)
        round_icon.putalpha(mask.resize((size, size), Image.LANCZOS))
        round_icon.save(os.path.join(folder, "ic_launcher_round.png"))

        print(f"{density:8s} {size:3d}px  -> {folder}")

    print("ikeback launcher icons generated.")


if __name__ == "__main__":
    main()
