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
#include <cstdint>

#include "core.h"
#include "opts.h"
#include "util/static_vector.h"

namespace stormphrax {
    enum class MoveType {
        kStandard = 0,
        kPromotion,
        kCastling,
        kEnPassant,
        kDrop,
    };

    class Move {
    public:
        constexpr Move() = default;

        [[nodiscard]] constexpr usize fromSqIdx() const {
            return m_move & kSquareMask;
        }

        [[nodiscard]] constexpr Square fromSq() const {
            return static_cast<Square>(fromSqIdx());
        }

        [[nodiscard]] constexpr i32 fromSqRank() const {
            return static_cast<i32>(fromSqIdx()) >> 3;
        }

        [[nodiscard]] constexpr i32 fromSqFile() const {
            return static_cast<i32>(fromSqIdx()) & 0x7;
        }

        [[nodiscard]] constexpr usize toSqIdx() const {
            return (m_move >> kToShift) & kSquareMask;
        }

        [[nodiscard]] constexpr Square toSq() const {
            return static_cast<Square>(toSqIdx());
        }

        [[nodiscard]] constexpr i32 toSqRank() const {
            return static_cast<i32>(toSqIdx()) >> 3;
        }

        [[nodiscard]] constexpr i32 toSqFile() const {
            return static_cast<i32>(toSqIdx()) & 0x7;
        }

        [[nodiscard]] constexpr usize promoIdx() const {
            return (m_move >> kAuxShift) & kAuxMask;
        }

        [[nodiscard]] constexpr PieceType promo() const {
            const auto moveType = type();
            if (moveType == MoveType::kPromotion || moveType == MoveType::kDrop) {
                return static_cast<PieceType>(promoIdx());
            }
            return PieceType::kNone;
        }

        [[nodiscard]] constexpr MoveType type() const {
            return static_cast<MoveType>((m_move >> kTypeShift) & kTypeMask);
        }

        [[nodiscard]] constexpr bool isNull() const {
            return m_move == 0;
        }

        [[nodiscard]] constexpr u32 data() const {
            return m_move;
        }

        [[nodiscard]] explicit constexpr operator bool() const {
            return !isNull();
        }

        constexpr bool operator==(const Move& other) const = default;

        [[nodiscard]] static constexpr Move standard(Square src, Square dst) {
            return Move{pack(src, dst, MoveType::kStandard)};
        }

        [[nodiscard]] static constexpr Move promotion(Square src, Square dst, PieceType promo) {
            return Move{pack(src, dst, MoveType::kPromotion, static_cast<u32>(promo))};
        }

        [[nodiscard]] static constexpr Move castling(Square src, Square dst) {
            return Move{pack(src, dst, MoveType::kCastling)};
        }

        [[nodiscard]] static constexpr Move enPassant(Square src, Square dst) {
            return Move{pack(src, dst, MoveType::kEnPassant)};
        }

        [[nodiscard]] static constexpr Move drop(PieceType piece, Square dst) {
            return Move{pack(dst, dst, MoveType::kDrop, static_cast<u32>(piece))};
        }

    private:
        static constexpr u32 kSquareMask = 0x3F;
        static constexpr u32 kAuxMask = 0x7;
        static constexpr u32 kTypeMask = 0x7;

        static constexpr u32 kToShift = 6;
        static constexpr u32 kAuxShift = 12;
        static constexpr u32 kTypeShift = 15;

        [[nodiscard]] static constexpr u32 pack(Square src, Square dst, MoveType type, u32 aux = 0) {
            return (static_cast<u32>(src) & kSquareMask) | ((static_cast<u32>(dst) & kSquareMask) << kToShift)
                | ((aux & kAuxMask) << kAuxShift) | ((static_cast<u32>(type) & kTypeMask) << kTypeShift);
        }

        explicit constexpr Move(u32 move) :
                m_move{move} {}

        u32 m_move{};
    };

    constexpr Move kNullMove{};

    // Crazyhouse drops can exceed the classical move upper bound.
    constexpr usize kDefaultMoveListCapacity = 1024;

    using MoveList = StaticVector<Move, kDefaultMoveListCapacity>;
} // namespace stormphrax

template <>
struct fmt::formatter<stormphrax::Move> : fmt::formatter<std::string_view> {
    format_context::iterator format(stormphrax::Move value, format_context& ctx) const;
};
