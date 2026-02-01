#!/usr/bin/env python3
"""Convert Bullet quantised.bin to a Stormphrax CBNF .nnue file.

Expected input order (from Bullet SavedFormat):
  l0w, l0b, l1w, l1b (all quantised i16, little-endian)
"""

from __future__ import annotations

import argparse
import struct
import sys
from array import array
from pathlib import Path

MAGIC = b"CBNF"
VERSION = 1
FLAGS = 0
ARCH_ID = 1  # SingleLayer
ACTIVATION_ID = 0  # crelu
INPUT_BUCKETS = 1
OUTPUT_BUCKETS = 1


def pack_i16(values: array) -> bytes:
    if values.itemsize != 2:
        raise RuntimeError("i16 size is not 2 bytes on this platform")
    out = values
    if sys.byteorder != "little":
        out = array("h", values)
        out.byteswap()
    return out.tobytes()


def write_padded(f, data: bytes) -> None:
    f.write(data)
    pad = (-len(data)) % 64
    if pad:
        f.write(b"\x00" * pad)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert Bullet quantised.bin to Stormphrax .nnue")
    parser.add_argument("--in", dest="in_path", required=True, help="input Bullet quantised.bin path")
    parser.add_argument("--out", dest="out_path", required=True, help="output .nnue path")
    parser.add_argument("--name", default="crazyhouse-bullet", help="network name (<= 48 ASCII chars)")
    parser.add_argument("--inputs", type=int, default=928, help="input size (default: 928)")
    parser.add_argument("--hidden", type=int, default=64, help="hidden size (default: 64)")
    parser.add_argument("--input-buckets", type=int, default=INPUT_BUCKETS, help="input bucket count")
    parser.add_argument("--output-buckets", type=int, default=OUTPUT_BUCKETS, help="output bucket count")
    args = parser.parse_args()

    name_bytes = args.name.encode("ascii")
    if len(name_bytes) > 48:
        raise SystemExit("name too long (max 48 bytes)")

    input_size = args.inputs
    hidden_size = args.hidden
    input_buckets = args.input_buckets
    output_buckets = args.output_buckets

    l0w_count = input_size * hidden_size * input_buckets
    l0b_count = hidden_size * input_buckets
    l1w_count = output_buckets * hidden_size * 2
    l1b_count = output_buckets
    total_i16 = l0w_count + l0b_count + l1w_count + l1b_count

    data = Path(args.in_path).read_bytes()
    needed_bytes = total_i16 * 2
    if len(data) < needed_bytes:
        raise SystemExit(f"input too small: need {needed_bytes} bytes, got {len(data)}")
    if len(data) > needed_bytes:
        print(f"note: ignoring trailing {len(data) - needed_bytes} bytes", file=sys.stderr)

    weights = array("h")
    weights.frombytes(data[:needed_bytes])
    if sys.byteorder != "little":
        weights.byteswap()

    idx = 0
    l0w = weights[idx : idx + l0w_count]
    idx += l0w_count
    l0b = weights[idx : idx + l0b_count]
    idx += l0b_count
    l1w = weights[idx : idx + l1w_count]
    idx += l1w_count
    l1b = weights[idx : idx + l1b_count]

    header = struct.pack(
        "<4sHHBBBHBBB48s",
        MAGIC,
        VERSION,
        FLAGS,
        0,
        ARCH_ID,
        ACTIVATION_ID,
        hidden_size,
        input_buckets,
        output_buckets,
        len(name_bytes),
        name_bytes.ljust(48, b"\x00"),
    )

    if len(header) != 64:
        raise RuntimeError("header is not 64 bytes")

    out_path = Path(args.out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    with out_path.open("wb") as f:
        f.write(header)
        write_padded(f, pack_i16(l0w))
        write_padded(f, pack_i16(l0b))
        write_padded(f, pack_i16(l1w))
        write_padded(f, pack_i16(l1b))

    print(f"wrote {out_path}")


if __name__ == "__main__":
    main()
