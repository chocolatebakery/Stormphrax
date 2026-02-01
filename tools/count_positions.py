#!/usr/bin/env python3
import argparse
import glob
import os
from pathlib import Path


def iter_paths(patterns):
    seen = set()
    for pat in patterns:
        expanded = glob.glob(pat, recursive=True)
        if not expanded:
            expanded = [pat]
        for p in expanded:
            path = Path(p)
            if path.is_dir():
                for child in path.iterdir():
                    if child.is_file():
                        key = child.resolve()
                        if key not in seen:
                            seen.add(key)
                            yield child
            elif path.is_file():
                key = path.resolve()
                if key not in seen:
                    seen.add(key)
                    yield path


def guess_record_size(path, forced):
    if forced is not None:
        return forced

    ext = path.suffix.lower()
    if ext == ".chb":
        return 56
    if ext == ".bin":
        return 32

    size = path.stat().st_size
    fits_56 = size % 56 == 0
    fits_32 = size % 32 == 0
    if fits_56 and not fits_32:
        return 56
    if fits_32 and not fits_56:
        return 32
    if fits_56 and fits_32:
        return 56

    return None


def main():
    parser = argparse.ArgumentParser(
        description="Count positions in marlinformat files (.bin/.chb) based on record size."
    )
    parser.add_argument(
        "paths",
        nargs="*",
        default=["."],
        help="Files, directories, or glob patterns (default: current directory)",
    )
    parser.add_argument(
        "--record-size",
        type=int,
        choices=(32, 56),
        help="Force record size (32 for .bin, 56 for .chb)",
    )

    args = parser.parse_args()

    total_positions = 0
    total_files = 0

    for path in iter_paths(args.paths):
        rec_size = guess_record_size(path, args.record_size)
        if rec_size is None:
            continue

        size = path.stat().st_size
        positions, rem = divmod(size, rec_size)

        if rem != 0:
            print(
                f"WARNING: {path} size {size} not divisible by record size {rec_size} (remainder {rem})",
                file=os.sys.stderr,
            )

        print(f"{path}  {positions}")
        total_positions += positions
        total_files += 1

    if total_files > 1:
        print(f"TOTAL  {total_positions}")


if __name__ == "__main__":
    main()
