#!/usr/bin/env python3
"""Generate ikeback branded Windows/Qt visual assets from the project logo.

Run once on a machine with Pillow and COMMIT the generated files:

    python desktop/scripts/generate_windows_icons.py

The CI Windows job has no Pillow, so it consumes the committed assets directly.
Source: <repo>/ikeback_logo.png (provided by the repository owner).

Why the PNGs matter: Qt's QMainWindow::windowIcon comes from core.qrc
(:/core/icon64.png), NOT from the .ico embedded in the .exe. Windows therefore
shows the qrc image in the taskbar / Alt-Tab. Only replacing the .ico files is
not enough - all the icon*.png and the splash must be regenerated too.
"""

import os
from PIL import Image, ImageDraw, ImageFont

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LOGO = os.path.join(REPO, "ikeback_logo.png")
IKEBACK = os.path.join(REPO, "ikeback")

ICO_TARGETS = [
    os.path.join(IKEBACK, "nsis", "installer.ico"),
    os.path.join(IKEBACK, "nsis", "uninstaller.ico"),
    os.path.join(IKEBACK, "master", "data", "ikeback-master.ico"),
    os.path.join(IKEBACK, "configurator", "data", "ikeback-configurator.ico"),
    os.path.join(IKEBACK, "scripts", "ikeback.ico"),
]

ICO_SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
ICON_SIZES = [16, 22, 32, 64, 128]

BLUE = (58, 96, 224)
PURPLE = (126, 72, 226)
WHITE = (255, 255, 255)
SUBTEXT = (226, 232, 255)


def font(size, bold=False):
    """Pick a CJK-capable font when available; fall back gracefully."""
    candidates = [
        r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    ]
    for path in candidates:
        if os.path.isfile(path):
            try:
                return ImageFont.truetype(path, size)
            except OSError:
                pass
    return ImageFont.load_default()


def write_png(path, image):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    image.save(path, format="PNG")
    print(f"{path} <- {os.path.basename(LOGO)}")


def square_icon(source, size):
    """Logo on a rounded blue-violet tile, so tiny sizes stay recognisable."""
    scale = 4
    canvas = Image.new("RGBA", (size * scale, size * scale), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle(
        (0, 0, size * scale - 1, size * scale - 1),
        radius=int(size * scale * 0.22),
        fill=BLUE + (255,),
    )
    logo = source.resize((int(size * scale * 0.82),) * 2, Image.LANCZOS)
    offset = (size * scale - logo.width) // 2
    canvas.alpha_composite(logo, (offset, offset))
    return canvas.resize((size, size), Image.LANCZOS)


def splash(source):
    """Replacement for master/resources/splash.png (shown at start-up)."""
    width, height = 720, 420
    image = Image.new("RGB", (width, height), BLUE)
    draw = ImageDraw.Draw(image)
    for y in range(height):
        ratio = y / max(1, height - 1)
        draw.line(
            [(0, y), (width, y)],
            fill=tuple(int(BLUE[i] + (PURPLE[i] - BLUE[i]) * ratio) for i in range(3)),
        )

    logo_size = 132
    logo = source.resize((logo_size, logo_size), Image.LANCZOS)
    image.paste(logo, ((width - logo_size) // 2, 62), logo)

    title = font(40, bold=True)
    centre = lambda txt, fnt: (width - draw.textlength(txt, font=fnt)) / 2
    draw.text((centre("ikeback", title), 214), "ikeback", font=title, fill=WHITE)
    tagline = font(18)
    draw.text((centre("课堂看得见 · 成长记得下", tagline), 272),
              "课堂看得见 · 成长记得下", font=tagline, fill=SUBTEXT)

    draw.line([(56, 340), (width - 56, 340)], fill=(255, 255, 255, 90), width=1)
    footer = font(14)
    left = "基于 Ikeback 4.x · GPL-2.0-or-later"
    right = "github.com/zk26/ikeback"
    draw.text((56, 356), left, font=footer, fill=SUBTEXT)
    draw.text((width - 56 - draw.textlength(right, font=footer), 356), right,
              font=footer, fill=SUBTEXT)
    return image


def main():
    if not os.path.isfile(LOGO):
        raise SystemExit(f"Logo not found: {LOGO}")
    source = Image.open(LOGO).convert("RGBA")

    for path in ICO_TARGETS:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        source.save(path, format="ICO", sizes=ICO_SIZES)

    for size in ICON_SIZES:
        write_png(
            os.path.join(IKEBACK, "core", "resources", f"icon{size}.png"),
            square_icon(source, size),
        )

    for relative in (
        os.path.join("master", "data", "ikeback-master.png"),
        os.path.join("configurator", "data", "ikeback-configurator.png"),
        os.path.join("configurator", "resources", "ikeback-configurator.png"),
    ):
        write_png(os.path.join(IKEBACK, relative), square_icon(source, 64))

    write_png(os.path.join(IKEBACK, "master", "resources", "splash.png"), splash(source))
    print("All ikeback branding assets generated.")


if __name__ == "__main__":
    main()
