import argparse
import os
from pathlib import Path
import uuid


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Rename datagen output files to a new prefix/id."
    )
    parser.add_argument(
        "directory",
        type=Path,
        help="Directory containing datagen files to rename.",
    )
    parser.add_argument(
        "--prefix",
        required=True,
        help="New id/prefix for renamed files (e.g. zh-04, hetzner-a).",
    )
    parser.add_argument(
        "--ext",
        default=".chb",
        help="File extension filter (default: .chb). Use empty string to rename all files.",
    )
    parser.add_argument(
        "--start",
        type=int,
        default=1,
        help="Starting index for numbering (default: 1).",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print planned renames without changing files.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    directory = args.directory
    if not directory.is_dir():
        print(f"error: directory not found: {directory}")
        return 1

    ext = args.ext
    if ext and not ext.startswith("."):
        ext = f".{ext}"

    files = sorted(
        [p for p in directory.iterdir() if p.is_file() and (not ext or p.suffix == ext)]
    )
    if not files:
        print("no matching files found")
        return 0

    pad = max(3, len(str(args.start + len(files) - 1)))
    planned = []
    for idx, path in enumerate(files, start=args.start):
        new_name = f"{args.prefix}_{str(idx).zfill(pad)}{path.suffix}"
        planned.append((path, path.with_name(new_name)))

    if args.dry_run:
        for src, dst in planned:
            print(f"{src.name} -> {dst.name}")
        return 0

    # Two-phase rename to avoid collisions.
    temp_paths = []
    for src, _ in planned:
        temp_name = f".tmp_{uuid.uuid4().hex}{src.suffix}"
        temp_path = src.with_name(temp_name)
        src.rename(temp_path)
        temp_paths.append(temp_path)

    for temp_path, (_, final_path) in zip(temp_paths, planned):
        if final_path.exists():
            print(f"error: target exists: {final_path.name}")
            return 1
        temp_path.rename(final_path)

    print(f"renamed {len(planned)} files in {directory}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
