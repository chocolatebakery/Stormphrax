/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2025 Ciekce
 *
 * Stormphrax is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Stormphrax is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Stormphrax. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "types.h"

#include <array>
#include <cmath>
#include <cstring>
#include <utility>

#include "bitboard.h"
#include "move.h"
#include "position/position.h"
#include "tunable.h"
#include "util/multi_array.h"

namespace stormphrax {
    using HistoryScore = i16;

    struct HistoryEntry {
        i16 value{};

        HistoryEntry() = default;
        HistoryEntry(HistoryScore v) :
                value{v} {}

        [[nodiscard]] inline operator HistoryScore() const {
            return value;
        }

        [[nodiscard]] inline HistoryEntry& operator=(HistoryScore v) {
            value = v;
            return *this;
        }

        inline void update(HistoryScore bonus) {
            value += bonus - value * std::abs(bonus) / tunable::maxHistory();
        }

        inline void updateWithBase(HistoryScore bonus, i32 base) {
            value += bonus - base * std::abs(bonus) / tunable::maxHistory();
        }
    };

    inline HistoryScore historyBonus(i32 depth) {
        return static_cast<HistoryScore>(std::clamp(
            depth * tunable::historyBonusDepthScale() - tunable::historyBonusOffset(),
            0,
            tunable::maxHistoryBonus()
        ));
    }

    inline HistoryScore historyPenalty(i32 depth) {
        return static_cast<HistoryScore>(-std::clamp(
            depth * tunable::historyPenaltyDepthScale() - tunable::historyPenaltyOffset(),
            0,
            tunable::maxHistoryPenalty()
        ));
    }

    class ContinuationSubtable {
    public:
        //TODO take two args when c++23 is usable
        inline HistoryScore operator[](std::pair<Piece, Move> move) const {
            const auto [piece, mv] = move;
            return m_data[static_cast<i32>(piece)][static_cast<i32>(mv.toSq())];
        }

        inline HistoryEntry& operator[](std::pair<Piece, Move> move) {
            const auto [piece, mv] = move;
            return m_data[static_cast<i32>(piece)][static_cast<i32>(mv.toSq())];
        }

    private:
        // [piece type][to]
        util::MultiArray<HistoryEntry, 12, 64> m_data{};
    };

    class HistoryTables {
    public:
        inline void clear() {
            std::memset(&m_butterfly, 0, sizeof(m_butterfly));
            std::memset(&m_pieceTo, 0, sizeof(m_pieceTo));
            std::memset(&m_continuation, 0, sizeof(m_continuation));
            std::memset(&m_noisy, 0, sizeof(m_noisy));
            std::memset(&m_drop, 0, sizeof(m_drop));
            std::memset(&m_dropCheck, 0, sizeof(m_dropCheck));
            std::memset(&m_lowPly, 0, sizeof(m_lowPly));
            std::memset(&m_pawn, 0, sizeof(m_pawn));
        }

        [[nodiscard]] inline const ContinuationSubtable& contTable(Piece moving, Square to) const {
            return m_continuation[static_cast<i32>(moving)][static_cast<i32>(to)];
        }

        [[nodiscard]] inline ContinuationSubtable& contTable(Piece moving, Square to) {
            return m_continuation[static_cast<i32>(moving)][static_cast<i32>(to)];
        }

        inline void updateConthist(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Bitboard threats,
            Piece moving,
            Move move,
            HistoryScore bonus
        ) {
            const auto base = getConthist(continuations, ply, moving, move) + getMainHist(threats, moving, move) / 2;

            updateConthist(continuations, ply, moving, move, base, bonus, 1);
            updateConthist(continuations, ply, moving, move, base, bonus, 2);
            updateConthist(continuations, ply, moving, move, base, bonus, 4);
        }

        inline void updateQuietScore(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Bitboard threats,
            const Position& pos,
            Piece moving,
            Move move,
            HistoryScore bonus
        ) {
            const auto movingType = pieceType(moving);
            if (movingType == PieceType::kPawn) {
                pawnEntry(moving, move).update(bonus);
            }
            if (ply <= tunable::lowPlyHistoryDepth()) {
                lowPlyEntry(moving, move).update(bonus);
            }

            if (move.type() == MoveType::kDrop) {
                auto& dropHist = dropEntry(move.promo(), move.toSq());
                dropHist.update(bonus);
                if (pos.givesCheck(move)) {
                    dropCheckEntry(move.promo(), move.toSq()).update(bonus);
                }

                const auto base =
                    static_cast<i32>(dropHist) + getConthist(continuations, ply, moving, move) / 2;
                updateConthist(continuations, ply, moving, move, base, bonus, 1);
                updateConthist(continuations, ply, moving, move, base, bonus, 2);
                updateConthist(continuations, ply, moving, move, base, bonus, 4);
                return;
            }

            butterflyEntry(threats, move).update(bonus);
            pieceToEntry(threats, moving, move).update(bonus);
            updateConthist(continuations, ply, threats, moving, move, bonus);
        }

        inline void updateNoisyScore(Move move, Piece captured, Bitboard threats, HistoryScore bonus) {
            noisyEntry(move, captured, threats[move.toSq()]).update(bonus);
        }

        [[nodiscard]] inline i32 getMainHist(Bitboard threats, Piece moving, Move move) const {
            return (butterflyEntry(threats, move) + pieceToEntry(threats, moving, move)) / 2;
        }

        [[nodiscard]] inline i32 getConthist(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            Piece moving,
            Move move
        ) const {
            i32 score{};

            score += conthistScore(continuations, ply, moving, move, 1);
            score += conthistScore(continuations, ply, moving, move, 2);
            score += conthistScore(continuations, ply, moving, move, 4) / 2;

            return score;
        }

        [[nodiscard]] inline i32 quietScore(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            Bitboard threats,
            Piece moving,
            Move move
        ) const {
            i32 score{};
            const auto movingType = pieceType(moving);

            if (move.type() == MoveType::kDrop) {
                score += dropEntry(move.promo(), move.toSq()) * tunable::dropHistoryWeight();
                score += getConthist(continuations, ply, moving, move);
                if (movingType == PieceType::kPawn) {
                    score += pawnEntry(moving, move) * tunable::pawnHistoryWeight();
                }
                if (ply <= tunable::lowPlyHistoryDepth()) {
                    score += lowPlyEntry(moving, move) * tunable::lowPlyHistoryWeight();
                }
                return score;
            }

            score += getMainHist(threats, moving, move);
            score += getConthist(continuations, ply, moving, move);
            if (movingType == PieceType::kPawn) {
                score += pawnEntry(moving, move) * tunable::pawnHistoryWeight();
            }
            if (ply <= tunable::lowPlyHistoryDepth()) {
                score += lowPlyEntry(moving, move) * tunable::lowPlyHistoryWeight();
            }

            return score;
        }

        [[nodiscard]] inline i32 noisyScore(Move move, Piece captured, Bitboard threats) const {
            return noisyEntry(move, captured, threats[move.toSq()]);
        }

    private:
        // [from][to][from attacked][to attacked]
        util::MultiArray<HistoryEntry, 64, 64, 2, 2> m_butterfly{};
        // [piece][to]
        util::MultiArray<HistoryEntry, 12, 64, 2, 2> m_pieceTo{};
        // [prev piece][to][curr piece type][to]
        util::MultiArray<ContinuationSubtable, 12, 64> m_continuation{};

        // [from][to][captured][defended]
        // additional slot for non-capture queen promos
        util::MultiArray<HistoryEntry, 64, 64, 13, 2> m_noisy{};

        // [drop piece type][to]
        util::MultiArray<HistoryEntry, 5, 64> m_drop{};
        // [drop piece type][to] for drop-checks
        util::MultiArray<HistoryEntry, 5, 64> m_dropCheck{};

        // [piece][to] at low ply
        util::MultiArray<HistoryEntry, 12, 64> m_lowPly{};

        // [piece][to] for pawns only
        util::MultiArray<HistoryEntry, 12, 64> m_pawn{};

        static inline void updateConthist(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            Piece moving,
            Move move,
            i32 base,
            HistoryScore bonus,
            i32 offset
        ) {
            if (offset <= ply) {
                conthistEntry(continuations, ply, offset)[{moving, move}].updateWithBase(bonus, base);
            }
        }

        static inline HistoryScore conthistScore(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            Piece moving,
            Move move,
            i32 offset
        ) {
            if (offset <= ply) {
                return conthistEntry(continuations, ply, offset)[{moving, move}];
            }

            return 0;
        }

        [[nodiscard]] inline const HistoryEntry& butterflyEntry(Bitboard threats, Move move) const {
            return m_butterfly[move.fromSqIdx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline HistoryEntry& butterflyEntry(Bitboard threats, Move move) {
            return m_butterfly[move.fromSqIdx()][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline const HistoryEntry& pieceToEntry(Bitboard threats, Piece moving, Move move) const {
            return m_pieceTo[static_cast<i32>(moving)][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] inline HistoryEntry& pieceToEntry(Bitboard threats, Piece moving, Move move) {
            return m_pieceTo[static_cast<i32>(moving)][move.toSqIdx()][threats[move.fromSq()]][threats[move.toSq()]];
        }

        [[nodiscard]] static inline const ContinuationSubtable& conthistEntry(
            std::span<ContinuationSubtable* const> continuations,
            i32 ply,
            i32 offset
        ) {
            return *continuations[ply - offset];
        }

        [[nodiscard]] static inline ContinuationSubtable& conthistEntry(
            std::span<ContinuationSubtable*> continuations,
            i32 ply,
            i32 offset
        ) {
            return *continuations[ply - offset];
        }

        [[nodiscard]] inline const HistoryEntry& noisyEntry(Move move, Piece captured, bool defended) const {
            return m_noisy[move.fromSqIdx()][move.toSqIdx()][static_cast<i32>(captured)][defended];
        }

        [[nodiscard]] inline HistoryEntry& noisyEntry(Move move, Piece captured, bool defended) {
            return m_noisy[move.fromSqIdx()][move.toSqIdx()][static_cast<i32>(captured)][defended];
        }

        [[nodiscard]] static constexpr i32 dropIndex(PieceType piece) {
            assert(piece != PieceType::kNone && piece != PieceType::kKing);
            return static_cast<i32>(piece);
        }

        [[nodiscard]] inline const HistoryEntry& dropEntry(PieceType piece, Square to) const {
            return m_drop[dropIndex(piece)][static_cast<i32>(to)];
        }

        [[nodiscard]] inline HistoryEntry& dropEntry(PieceType piece, Square to) {
            return m_drop[dropIndex(piece)][static_cast<i32>(to)];
        }

        [[nodiscard]] inline const HistoryEntry& dropCheckEntry(PieceType piece, Square to) const {
            return m_dropCheck[dropIndex(piece)][static_cast<i32>(to)];
        }

        [[nodiscard]] inline HistoryEntry& dropCheckEntry(PieceType piece, Square to) {
            return m_dropCheck[dropIndex(piece)][static_cast<i32>(to)];
        }

    public:
        [[nodiscard]] inline i32 dropCheckScore(PieceType piece, Square to) const {
            return dropCheckEntry(piece, to);
        }

        [[nodiscard]] inline const HistoryEntry& lowPlyEntry(Piece moving, Move move) const {
            return m_lowPly[static_cast<i32>(moving)][move.toSqIdx()];
        }

        [[nodiscard]] inline HistoryEntry& lowPlyEntry(Piece moving, Move move) {
            return m_lowPly[static_cast<i32>(moving)][move.toSqIdx()];
        }

        [[nodiscard]] inline const HistoryEntry& pawnEntry(Piece moving, Move move) const {
            return m_pawn[static_cast<i32>(moving)][move.toSqIdx()];
        }

        [[nodiscard]] inline HistoryEntry& pawnEntry(Piece moving, Move move) {
            return m_pawn[static_cast<i32>(moving)][move.toSqIdx()];
        }
    };
} // namespace stormphrax
