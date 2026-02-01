# Stormphrax Crazyhouse

## Como ativar
- **UCI**: `setoption name UCI_Variant value crazyhouse`
- **Datagen**: `stormphrax datagen marlinformat crazyhouse <outdir> [threads] [syzygy path]`

Notas:
- `UCI_Chess960` é desativado automaticamente em Crazyhouse.
- Syzygy (TB) não é suportado em Crazyhouse.

## FEN e notação de drops
- FEN com bolsos no fim da parte do tabuleiro:  
  `r1bqkbnr/pppp1ppp/2n5/4N3/4P3/8/PPPP1PPP/RNBQKB1R[P] b KQkq - 0 3`
- Peças **promovidas** marcadas com `~` após a peça (formato fairy/cutechess).
- Drops em UCI: `P@e4` (a letra indica a peça, a cor é do lado a jogar).
- Peões **não podem** ser dropados na 1ª/8ª fila.

## Formato binário (marlinformat crazyhouse)
Quando `UCI_Variant = crazyhouse`, o `marlinformat` escreve em **.chb** (CrazyHouse Binary).

Layout (56 bytes, little-endian):
- `u64 occupancy`
- `u64 promoted` (bitboard das peças promovidas)
- `u8 pcs[16]` (32 nibbles, mesmo esquema do marlinformat)
- `u8 pockets[2][5]` (índices: `0=preto,1=branco`; ordem: Pawn, Knight, Bishop, Rook, Queen)
- `u8 stm_ep` (bit 7 = side-to-move; bits 0-5 = EP square relativo, igual ao marlinformat)
- `u8 halfmove`
- `u16 fullmove`
- `i16 eval` (score **white POV**, igual ao marlinformat)
- `u8 wdl` (`0=white loss`, `1=draw`, `2=white win`)
- `u8 extra`
- `u8 padding[6]`

## Bullet (trainer)
- **Tipo**: `game::formats::crazyhouse::CrazyhouseMarlinFormat`
- **Adapter**: `inputs::CrazyhouseMarlinFormatAdapter<T>` (para reutilizar inputs de xadrez)
  - Ex.: `CrazyhouseMarlinFormatAdapter(ChessBuckets::new(...))`
- **Inputs Crazyhouse**: `inputs::CrazyhouseBucketsMergedKings` / `inputs::CrazyhouseBucketsMergedKingsMirrored` (864xN)
- **Input simples**: `inputs::CrazyhouseChess768` (928)
- **Loader**: `DirectSequentialDataLoader` com `CrazyhouseMarlinFormat`
- **Exemplo**: `bullet/examples/simple_crazyhouse.rs`

## Notas e limitações atuais
- `viriformat` não suporta drops (Crazyhouse).
- NNUE agora inclui bolsos (5x16 por cor). Redes antigas nao sao compativeis.
- Arquitetura NNUE simples: 928 inputs (768+160), sem buckets, L1=64.
- SEE ajustado para Crazyhouse (threshold/2).

## Script de pesos aleatorios (NNUE)
- Script: `Stormphrax/tools/gen_random_nnue.py`
- Exemplo: `python Stormphrax/tools/gen_random_nnue.py --out Stormphrax/crazyhouse-rand-64.nnue --name crazyhouse-rand-64`
- Para usar no build: atualiza `Stormphrax/network.txt` com o nome base (sem .nnue) e recompila.
