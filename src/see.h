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
		constexpr Score King = 0;
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
	inline auto gain(const PositionBoards &boards, Move move) {
		const auto &bbs = boards.bbs();
		
		auto us = pieceColor(boards.pieceAt(move.src()));
		auto them = oppColor(us);

		auto score = 0;
		auto fromTo = Bitboard::fromSquare(move.dst()) | Bitboard::fromSquare(move.src());

		if (move.type() == MoveType::EnPassant) {
			fromTo = Bitboard::fromSquare(move.src());
			score += value(colorPiece(PieceType::Pawn, them));
		}

		auto boom = ((attacks::getKingAttacks(move.dst()) & ~(bbs.pawns())) | fromTo);

		auto ourPieces = bbs.occupancy(us);
		auto theirPieces = bbs.occupancy(them);

		auto boomUs = boom & ourPieces;
		auto boomThem = boom & theirPieces;
			

		if ((boom & bbs.kings(us))) {
			return -ScoreMate;
		}
		if (boom & bbs.kings(them)) {
			return ScoreMate;
		}

		while(boomUs) {
			auto boomsq = static_cast<Square>(util::ctz(boomUs));
			boomUs &= boomUs - 1;
			auto piece_boom = boards.pieceAt(boomsq);
			score -= value(piece_boom);
		}
		while(boomThem) {
			auto boomsq = static_cast<Square>(util::ctz(boomThem));
			boomThem &= boomThem - 1;
			auto piece_boom = boards.pieceAt(boomsq);
			score += value(piece_boom);
		}
		return score;
	}

	inline auto gain_atomic(const Position &pos, Move move) {
		
		const auto &boards = pos.boards();
		const auto &bbs = boards.bbs();

		auto victim = boards.pieceAt(move.src());
		auto stm = pieceColor(victim);
		auto them = oppColor(stm);

		auto fromTo = (Bitboard::fromSquare(move.dst()) | Bitboard::fromSquare(move.src()));
		auto captured = boards.pieceAt(move.dst());
		if (move.type() == MoveType::EnPassant) {
			fromTo = Bitboard::fromSquare(move.src());
			captured = colorPiece(PieceType::Pawn, them);
		}
		auto castle = move.type() == MoveType::Castling;

		auto result = 0;
		if (captured == Piece::None || castle) {

			auto ourPieces = bbs.occupancy(stm);
			auto theirPieces = bbs.occupancy(oppColor(stm));
			auto stmKing = bbs.occupancy(stm) & bbs.forPiece(PieceType::King);
			auto boom = ((attacks::getKingAttacks(move.dst()) & ~(bbs.pawns()) | fromTo & bbs.occupancy()));
			auto boomUs = boom & ourPieces;
			auto boomThem = boom & theirPieces;
			

			auto occupied = bbs.occupancy() ^ fromTo;
			auto attackers = pos.attackersToPos(move.dst(), occupied, oppColor(stm));
      		auto minAttacker = ScoreMaxMate;

			while (attackers) {
				auto sq = static_cast<Square>(util::ctz(attackers));
				attackers &= attackers - 1;
				if (pieceType((boards.pieceAt(sq))) != PieceType::King) {
					minAttacker = std::min(minAttacker, boom & Bitboard::fromSquare(sq) ? 0 : value(boards.pieceAt(sq)));
				}

				if (minAttacker == ScoreMaxMate) {
					return 0;
				}
				result += minAttacker; 
			}

				if (boom & bbs.kings(stm)) {
					result -= ScoreMate;
					goto mateJump;
				}
				if (boom & bbs.kings(them)) {
					result += ScoreMate;
					goto mateJump;
				}
				while(boomUs) {
					auto boomsq = static_cast<Square>(util::ctz(boomUs));
					boomUs &= boomUs - 1;
					auto piece_boom = boards.pieceAt(boomsq);
					result -= value(piece_boom);
				}
				while(boomThem) {
					auto boomsq = static_cast<Square>(util::ctz(boomThem));
					boomThem &= boomThem - 1;
					auto piece_boom = boards.pieceAt(boomsq);
					result += value(piece_boom);
				}
		}

		if (captured != Piece::None && !castle)
		{
			result += gain(boards,move);
			return (result - 1);
		}
		
		mateJump:
		return std::min(result,0);
	}
	

	inline auto gain_move(const Position &pos, Move move) {
		const auto &boards = pos.boards();
		return gain(boards, move);
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
	// Also adapted from https://github.com/fairy-stockfish/Fairy-Stockfish/blob/50adcffd957aaa2b4729409518549fc3107b9c33/src/position.cpp#L2367
	// All credits to https://github.com/ianfab/
	// For the code

	inline auto see(const Position &pos, Move move, Score threshold = 0)
	{
		return gain_atomic(pos,move) >= threshold;
	}
}
