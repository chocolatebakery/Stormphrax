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

#include "../types.h"

#include <array>

#include "nnue.h"
#include "../position/position.h"
#include "../core.h"
#include "../see.h"

namespace stormphrax::eval
{
	// black, white
	using Contempt = std::array<Score, 2>;

	inline auto scaleEval(const Position &pos, i32 eval)
	{
		eval = eval * (200 - pos.halfmove()) / 200;
		return eval;
	}

	template <bool Scale = true>
	inline auto adjustEval(const Position &pos, const Contempt &contempt, i32 eval)
	{
		if constexpr (Scale)
			eval = scaleEval(pos, eval);
		eval += contempt[static_cast<i32>(pos.toMove())];
		return std::clamp(eval, -ScoreWin + 1, ScoreWin - 1);
	}

	inline auto simple_eval(const Position& pos) {
    return (see::value(Piece::WhitePawn) * pos.bbs().forPiece(Piece::WhitePawn).popcount()) - (see::value(Piece::BlackPawn) * pos.bbs().forPiece(Piece::BlackPawn).popcount())
         + (see::value(Piece::WhiteQueen) * pos.bbs().forPiece(Piece::WhiteQueen).popcount()) - (see::value(Piece::BlackQueen) * pos.bbs().forPiece(Piece::BlackQueen).popcount())
		 + (see::value(Piece::WhiteKnight) * pos.bbs().forPiece(Piece::WhiteKnight).popcount()) - (see::value(Piece::BlackKnight) * pos.bbs().forPiece(Piece::BlackKnight).popcount())
		 + (see::value(Piece::WhiteRook) * pos.bbs().forPiece(Piece::WhiteRook).popcount()) - (see::value(Piece::BlackRook) * pos.bbs().forPiece(Piece::BlackRook).popcount())
		 + (see::value(Piece::WhiteBishop) * pos.bbs().forPiece(Piece::WhiteBishop).popcount()) - (see::value(Piece::BlackBishop) * pos.bbs().forPiece(Piece::BlackBishop).popcount())
		 ;
	}
	template <bool Scale = true>
	inline auto staticEval(const Position &pos, const NnueState &nnueState, const Contempt &contempt = {})
	{
		const auto nnueEval = nnueState.evaluate(pos.bbs(), pos.toMove());
		//return simple_eval(pos);
		return adjustEval<Scale>(pos, contempt, nnueEval);
	}

	template <bool Scale = true>
	inline auto staticEvalOnce(const Position &pos, const Contempt &contempt = {})
	{
		const auto nnueEval = NnueState::evaluateOnce(pos.bbs(), pos.blackKing(), pos.whiteKing(), pos.toMove());
		//return simple_eval(pos);
		return adjustEval<Scale>(pos, contempt, nnueEval);
	}
}
