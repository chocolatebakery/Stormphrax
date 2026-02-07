# Stormphrax Crazyhouse

## How to enable
- **UCI**: `setoption name UCI_Variant value crazyhouse`
- **Datagen**: `stormphrax datagen marlinformat crazyhouse <outdir> [threads] [max positions] [syzygy path]`

Notes:
- `UCI_Chess960` is automatically disabled in Crazyhouse.
- Syzygy (TB) is not supported in Crazyhouse.

## Quick workflow (clear steps)
1) Build an AVX2 binary:
   - `make avx2 DEFAULT_NET=zh-03 VERSION=zh03`
2) Generate data (writes .chb files):
   - `stormphrax datagen marlinformat crazyhouse .\data 6 2000000`
3) Train in Bullet:
   - `cargo run --release --example simple_crazyhouse_04 -- .\Stormphrax\data\*.chb`
4) Use the new net in Stormphrax:
   - copy the `.nnue` into `Stormphrax/`
   - update `Stormphrax/network.txt` with the base name (no extension)
   - rebuild with `make avx2 DEFAULT_NET=<name> VERSION=<tag>`

## Build (AVX2, with and without PGO)
No PGO (default):
- `make avx2`
- Optional: `make avx2 DEFAULT_NET=zh-03 VERSION=zh03`

With PGO (uses `bench` to generate a profile automatically):
- `make avx2 PGO=on`
- Optional: `make avx2 PGO=on DEFAULT_NET=zh-03 VERSION=zh03`

Notes:
- PGO requires `llvm-profdata` on PATH (Clang). If missing, PGO is disabled automatically.

## FEN and drop notation
- FEN with pockets at the end of the board part:
  `r1bqkbnr/pppp1ppp/2n5/4N3/4P3/8/PPPP1PPP/RNBQKB1R[P] b KQkq - 0 3`
- Promoted pieces are marked with `~` after the piece (fairy/cutechess format).
- Drops in UCI: `P@e4` (the letter indicates the piece; color is side to move).
- Pawns **cannot** be dropped on the 1st/8th ranks.

## Binary format (marlinformat crazyhouse)
When `UCI_Variant = crazyhouse`, `marlinformat` writes **.chb** (CrazyHouse Binary).

Layout (56 bytes, little-endian):
- `u64 occupancy`
- `u64 promoted` (bitboard of promoted pieces)
- `u8 pcs[16]` (32 nibbles, same scheme as marlinformat)
- `u8 pockets[2][5]` (indices: `0=black,1=white`; order: Pawn, Knight, Bishop, Rook, Queen)
- `u8 stm_ep` (bit 7 = side-to-move; bits 0-5 = EP square relative, same as marlinformat)
- `u8 halfmove`
- `u16 fullmove`
- `i16 eval` (score **white POV**, same as marlinformat)
- `u8 wdl` (`0=white loss`, `1=draw`, `2=white win`)
- `u8 extra`
- `u8 padding[6]`

## Bullet (trainer)
- **Type**: `game::formats::crazyhouse::CrazyhouseMarlinFormat`
- **Adapter**: `inputs::CrazyhouseMarlinFormatAdapter<T>` (to reuse chess inputs)
  - Example: `CrazyhouseMarlinFormatAdapter(ChessBuckets::new(...))`
- **Crazyhouse inputs**: `inputs::CrazyhouseBucketsMergedKings` / `inputs::CrazyhouseBucketsMergedKingsMirrored` (864xN)
- **Simple input**: `inputs::CrazyhouseChess768` (928)
- **Loader**: `DirectSequentialDataLoader` with `CrazyhouseMarlinFormat`
- **Example**: `bullet/examples/simple_crazyhouse.rs`

## Current notes and limitations
- `viriformat` does not support drops (Crazyhouse).
- NNUE now includes pockets (5x16 per color). Old nets are not compatible.
- Simple NNUE architecture: 928 inputs (768+160), no buckets, L1=64.
- SEE adjusted for Crazyhouse (threshold/2).

## Random weights script (NNUE)
- Script: `Stormphrax/tools/gen_random_nnue.py`
- Example: `python Stormphrax/tools/gen_random_nnue.py --out Stormphrax/crazyhouse-rand-64.nnue --name crazyhouse-rand-64`
- To use in build: update `Stormphrax/network.txt` with the base name (without .nnue) and rebuild.

## Rename datagen files
- Script: `Stormphrax/tools/rename_datagen_files.py`
- Example: `python Stormphrax/tools/rename_datagen_files.py Stormphrax/data --prefix zh-04`
- Dry run: `python Stormphrax/tools/rename_datagen_files.py Stormphrax/data --prefix zh-04 --dry-run`

## Interleave / Shuffle .chb (bullet_utils)
Interleave multiple .chb files into a single mixed file:
```
cargo run --release -p bullet_utils -- interleave-chb -o merged.chb file1.chb file2.chb file3.chb
```

Shuffle a single .chb file (full shuffle, record size 56 bytes):
```
cargo run --release -p bullet_utils -- shuffle-chb -i input.chb -o output.chb -m 1024
```
