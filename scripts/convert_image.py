#!/usr/bin/env python3
"""Convert renderer PPM output to PNG, or an RGB texture to binary PPM."""
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        from PIL import Image
    except ImportError:
        parser.exit(1, "Install the optional converter dependency: python3 -m pip install Pillow\n")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with Image.open(args.input) as image:
        image.convert("RGB").save(args.output)
    print(args.output)


if __name__ == "__main__":
    main()
