#!/usr/bin/env python3
"""
Convert assets/logo.png into a high-quality multi-resolution Windows .ico file.
"""
from PIL import Image
from pathlib import Path


def generate_ico():
    repo_root = Path(__file__).resolve().parent.parent.parent
    png_path = repo_root / "assets" / "logo.png"
    ico_path = repo_root / "assets" / "git-flux.ico"

    if not png_path.exists():
        raise FileNotFoundError(f"Source image not found: {png_path}")

    img = Image.open(png_path)
    img = img.convert("RGBA")

    icon_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    img.save(ico_path, format="ICO", sizes=icon_sizes)
    print(f"Generated {ico_path} with sizes: {icon_sizes}")


if __name__ == "__main__":
    generate_ico()
