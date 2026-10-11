#!/usr/bin/env python3
"""Check the renderer's output, feature toggles, and CLI failures.

Run from any directory: python3 tests/smoke_test.py [path/to/ray]
Only the Python standard library is required.
"""

import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
WIDTH, HEIGHT = 64, 36


def run_renderer(executable, args, expect_success=True):
    result = subprocess.run(
        [str(executable), *args],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=60,
    )
    if result.returncode < 0:
        raise AssertionError(f"Renderer crashed with signal {-result.returncode}: {' '.join(args)}")
    if (result.returncode == 0) != expect_success:
        expected = "success" if expect_success else "a nonzero exit status"
        raise AssertionError(
            f"Expected {expected} for {' '.join(args)}; got {result.returncode}\n"
            f"{result.stdout}{result.stderr}"
        )
    return result


def read_ppm(path):
    data = path.read_bytes()
    header = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\r?\n", data)
    if not header:
        raise AssertionError(f"{path.name}: invalid binary P6 header")
    width, height, maximum = map(int, header.groups())
    if (width, height, maximum) != (WIDTH, HEIGHT, 255):
        raise AssertionError(f"{path.name}: unexpected dimensions or color range")
    pixels = data[header.end():]
    if len(pixels) != width * height * 3:
        raise AssertionError(f"{path.name}: incomplete or extra pixel data")
    if len({pixels[i:i + 3] for i in range(0, len(pixels), 3)}) < 2:
        raise AssertionError(f"{path.name}: image contains only one color")
    return pixels


def smoke_test(executable):
    common = [
        "--scene", "scenes/demo.json",
        "--texture", "assets/stone.ppm",
        "--width", str(WIDTH), "--height", str(HEIGHT),
        "--aa", "--samples", "4",
    ]
    with tempfile.TemporaryDirectory(prefix="ray-smoke-") as directory:
        temp = Path(directory)

        def render(name, flags=()):
            path = temp / f"{name}.ppm"
            run_renderer(executable, [*common, *flags, "--output", str(path)])
            return read_ppm(path)

        baseline = render("baseline")
        if render("linear", ["--no-bvh"]) != baseline:
            raise AssertionError("BVH and linear traversal produced different pixels")
        if render("no-reflection", ["--no-refl"]) == baseline:
            raise AssertionError("Disabling reflections did not change the image")
        effects = render("effects", [
            "--dof", "--aperture", "0.1",
            "--glossy", "--glossy-samples", "4",
            "--softshadow", "--shadow-samples", "4",
            "--motionblur", "--motion-amount", "0.5",
        ])
        if effects == baseline:
            raise AssertionError("Enabling camera and lighting effects did not change the image")
        print("PASS: P6 output, BVH equivalence, reflections, and combined effects")

        run_renderer(executable, ["--help"])
        failures = [
            ["--unknown-option"],
            ["--width"],
            ["--scene"],
            [*common, "--height", "0"],
            [*common, "--width", "64junk"],
            [*common, "--samples", "3"],
            [*common, "--aperture", "nan"],
            [*common, "--scene", str(temp / "missing-scene.json")],
            [*common, "--texture", str(temp / "missing-texture.ppm")],
            [*common, "--output", str(temp)],
        ]
        for args in failures:
            run_renderer(executable, args, expect_success=False)
        print("PASS: help and invalid arguments, missing inputs, unwritable output")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", nargs="?", default=str(ROOT / "build" / "ray"))
    args = parser.parse_args()
    executable = Path(args.executable).expanduser().resolve()
    if not executable.is_file():
        parser.error(f"Renderer executable not found: {executable}")
    try:
        smoke_test(executable)
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
