#!/usr/bin/env python3
"""Generate Windows .ico resources from the project logo.

Run once on a machine with Pillow and commit the resulting .ico files:
    python veyon/scripts/generate_windows_icons.py

The CI Windows job does not have Pillow, so it consumes the committed icons
directly. Source: G:/veyon/ClassKeep_logo.png (provided by the repository owner).
"""

import os
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LOGO = os.path.join(REPO, "ClassKeep_logo.png")
VEYON = os.path.join(REPO, "veyon")

TARGETS = [
    os.path.join(VEYON, "nsis", "installer.ico"),
    os.path.join(VEYON, "nsis", "uninstaller.ico"),
    os.path.join(VEYON, "master", "data", "veyon-master.ico"),
    os.path.join(VEYON, "configurator", "data", "veyon-configurator.ico"),
    os.path.join(VEYON, "scripts", "ClassKeep.ico"),
]

SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]


def main():
    if not os.path.isfile(LOGO):
        raise SystemExit(f"Logo not found: {LOGO}")
    source = Image.open(LOGO).convert("RGBA")
    for path in TARGETS:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        source.save(path, format="ICO", sizes=SIZES)
        print(f"{path} <- {os.path.basename(LOGO)}")
    print("Windows icons generated.")


if __name__ == "__main__":
    main()
