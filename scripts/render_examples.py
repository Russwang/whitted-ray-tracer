#!/usr/bin/env python3
"""Render feature comparisons from the repository root."""
import argparse
from pathlib import Path
import subprocess


EXAMPLES = {
    "base": [],
    "aa_off": ["--no-aa"],
    "aa_on": ["--aa", "--samples", "4"],
    "shadow_hard": ["--no-softshadow"],
    "shadow_soft": ["--softshadow", "--shadow-samples", "16", "--light-radius", "0.5"],
    "glossy_off": ["--no-glossy"],
    "glossy_on": ["--glossy", "--glossy-samples", "4", "--glossy-spread", "0.12"],
    "dof_off": ["--no-dof"],
    "dof_on": ["--dof", "--aperture", "0.05", "--focus-dist", "36"],
    "motionblur_off": ["--no-motionblur"],
    "motionblur_on": ["--motionblur", "--motion-amount", "0.5"],
    "final_all_effects": ["--softshadow", "--shadow-samples", "16", "--glossy",
                          "--glossy-samples", "4", "--dof", "--focus-dist", "36",
                          "--motionblur"],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ray", type=Path, default=Path("build/ray"))
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=360)
    parser.add_argument("--only", choices=EXAMPLES)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    executable = args.ray.resolve()
    selected = {args.only: EXAMPLES[args.only]} if args.only else EXAMPLES
    for name, flags in selected.items():
        subprocess.run([str(executable), "--width", str(args.width), "--height", str(args.height),
                        "--aa", "--samples", "4", *flags,
                        "--output", f"output/{name}.ppm"], cwd=root, check=True)


if __name__ == "__main__":
    main()
