#!/usr/bin/env python3
"""Generate a random Stormphrax NNUE file for the current Crazyhouse arch.

This matches the current engine settings:
- Input features: WithPockets<SingleBucket> (928)
- L1 size: 64
- Arch: SingleLayer (arch id 1), ClippedReLU (activation id 0)
- Buckets: input=1, output=1
"""

from __future__ import annotations

import argparse
import random
import struct
import sys
from array import array
from pathlib import Path

MAGIC = b"CBNF"
VERSION = 1
FLAGS = 0
ARCH_ID = 1
ACTIVATION_ID = 0
INPUT_BUCKETS = 1
OUTPUT_BUCKETS = 1
INPUT_SIZE = 928
L1_SIZE = 64


def pack_i16(values: list[int]) -> bytes:
    arr = array("h", values)
    if arr.itemsize != 2:
        raise RuntimeError("i16 size is not 2 bytes on this platform")
    if sys.byteorder != "little":
        arr.byteswap()
    return arr.tobytes()


def write_padded(f, data: bytes) -> None:
    f.write(data)
    pad = (-len(data)) % 64
    if pad:
        f.write(b"\x00" * pad)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate random Stormphrax NNUE weights")
    parser.add_argument("--out", default="random_64.nnue", help="output .nnue path")
    parser.add_argument("--name", default="crazyhouse-rand-64", help="network name (<= 48 ASCII chars)")
    parser.add_argument("--seed", type=int, default=None, help="random seed")
    parser.add_argument(
        "--range",
        dest="weight_range",
        type=int,
        default=4,
        help="abs range for random weights/biases (default: 4)",
    )
    args = parser.parse_args()

    name_bytes = args.name.encode("ascii")
    if len(name_bytes) > 48:
        raise SystemExit("name too long (max 48 bytes)")

    rng = random.Random(args.seed)

    def rand_vals(n: int) -> list[int]:
        r = args.weight_range
        return [rng.randint(-r, r) for _ in range(n)]

    ft_weight_count = INPUT_SIZE * L1_SIZE
    ft_bias_count = L1_SIZE
    l1_weight_count = OUTPUT_BUCKETS * L1_SIZE * 2
    l1_bias_count = OUTPUT_BUCKETS

    header = struct.pack(
        "<4sHHBBBHBBB48s",
        MAGIC,
        VERSION,
        FLAGS,
        0,
        ARCH_ID,
        ACTIVATION_ID,
        L1_SIZE,
        INPUT_BUCKETS,
        OUTPUT_BUCKETS,
        len(name_bytes),
        name_bytes.ljust(48, b"\x00"),
    )

    if len(header) != 64:
        raise RuntimeError("header is not 64 bytes")

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    with out_path.open("wb") as f:
        f.write(header)

        write_padded(f, pack_i16(rand_vals(ft_weight_count)))
        write_padded(f, pack_i16(rand_vals(ft_bias_count)))
        write_padded(f, pack_i16(rand_vals(l1_weight_count)))
        write_padded(f, pack_i16(rand_vals(l1_bias_count)))

    seed_msg = "(random)" if args.seed is None else str(args.seed)
    print(f"wrote {out_path} with seed {seed_msg}")


if __name__ == "__main__":
    main()
