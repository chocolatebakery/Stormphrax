/*
 * Stormphrax, a UCI chess engine
 * Copyright (C) 2024 Ciekce
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

#include "core.h"
#include "position/position.h"
#include "attacks/attacks.h"

namespace stormphrax::see
{
	namespace values
	{
		constexpr Score Pawn = 100;
		constexpr Score Knight = 450;
		constexpr Score Bishop = 450;
		constexpr Score Rook = 650;
		constexpr Score Queen = 1250;
		constexpr Score King = ScoreMate;
	}

	constexpr auto Values = std::array {
		values::Pawn,
		values::Pawn,
		values::Knight,
		values::Knight,
		values::Bishop,
		values::Bishop,
		values::Rook,
		values::Rook,
		values::Queen,
		values::Queen,
		values::King,
		values::King,
		static_cast<Score>(0)
	};

	constexpr auto value(Piece piece)
	{
		return Values[static_cast<i32>(piece)];
	}

	constexpr auto value(PieceType piece)
	{
		return Values[static_cast<i32>(piece) * 2];
	}

	//adapted from MultiVariant Stockfish 
	// https://github.com/ddugovic/Stockfish/blob/eb4c78813370ebaea84724aed9f0b59d5ff3e2f2/src/position.cpp#L2184
	// All credits to https://github.com/ddugovic and https://github.com/ianfab/ and all the MultiVariant-Stockfish contributors
	// For the code
	inline auto gain(const PositionBoards &boards, Move move, Piece nextVictim, Square s) {
		const auto &bbs = boards.bbs();
		auto from = move.src();
		auto us = pieceColor(boards.pieceAt(move.src()));

		auto boom = (attacks::getKingAttacks(move.dst()) & (bbs.occupancy() ^ bbs.pawns())) - Bitboard::fromSquare(move.src());
		if (s != move.src()) {
			boom &= ~(Bitboard::fromSquare(s));
		}

		if (boom & bbs.kings(oppColor(us))) {
			return ScoreMate;
		}
		if (s != move.src() && (boom & bbs.kings(us))) {
			return -ScoreMate;
		}

		auto score = value(nextVictim) + value(boards.pieceAt(s));

		while (boom) {
			auto boom_sq = static_cast<Square>(util::ctz(boom));
			boom &= boom - 1;
			if (pieceColor(boards.pieceAt(boom_sq)) == us) {
				score -= value(boards.pieceAt(boom_sq));
			}
			else {
				score += value(boards.pieceAt(boom_sq));
			}
		}

		return score;
	}

	inline auto gain_move(const Position &pos, Move move) {
		const auto &boards = pos.boards();
		return gain(boards, move, boards.pieceAt(move.src()), move.src());
	}

	[[nodiscard]] inline auto popLeastValuable(const BitboardSet &bbs,
		Bitboard &occ, Bitboard attackers, Color color)
	{
		for (i32 i = 0; i < 6; ++i)
		{
			const auto piece = static_cast<PieceType>(i);
			auto board = attackers & bbs.forPiece(piece, color);

			if (!board.empty())
			{
				occ ^= board.lowestBit();
				return piece;
			}
		}

		return PieceType::None;
	}

	// Adapted from MV-SF https://github.com/ddugovic/Stockfish/blob/eb4c78813370ebaea84724aed9f0b59d5ff3e2f2/src/position.cpp#L2235
	// All credits to https://github.com/ddugovic and https://github.com/ianfab/ and all the Multivariant-Stockfish contributors
	// For the code
	inline auto see(const Position &pos, Move move, Score threshold = 0)
	{
		const auto &boards = pos.boards();
		const auto &bbs = boards.bbs();

		const auto color = pos.toMove();

		auto us = oppColor(color);
		auto victim = boards.pieceAt(move.src());
		auto stm = pieceColor(victim);

		if (boards.pieceAt(move.dst()) != Piece::None) {
			return (gain(boards, move, victim, move.dst()) >= threshold + 1);
		}
		if (threshold > 0) {
			return false;
		}

		auto occupied = bbs.occupancy() ^ Bitboard::fromSquare(move.src());
		auto attackers = pos.allAttackersTo(move.dst(), occupied) & occupied & bbs.forColor(oppColor(stm)) & ~(bbs.forPiece(PieceType::King));

		while(attackers) {
			auto sq = static_cast<Square>(util::ctz(attackers));
			attackers &= attackers - 1;
			if (gain(boards,move,victim,sq) < threshold) {
				return false;
			}
		}
		return true;
	}
}
