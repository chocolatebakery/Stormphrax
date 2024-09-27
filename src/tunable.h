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

#include <string>
#include <array>
#include <functional>

#include "util/range.h"

#ifndef SP_EXTERNAL_TUNE
	#define SP_EXTERNAL_TUNE 0
#endif

namespace stormphrax::tunable
{
	extern std::array<std::array<i32, 256>, 256> g_lmrTable;
	auto updateLmrTable() -> void;

	auto init() -> void;

#define SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
	static_assert(Default >= Min); \
	static_assert(Default <= Max); \
	static_assert(Min < Max); \
	static_assert(Min + Step <= Max);

#if SP_EXTERNAL_TUNE
	struct TunableParam
	{
		std::string name;
		i32 defaultValue;
		i32 value;
		util::Range<i32> range;
		f64 step;
		std::function<void()> callback;
	};

	auto addTunableParam(const std::string &name, i32 value,
		i32 min, i32 max, f64 step, std::function<void()> callback) -> TunableParam &;

	#define SP_TUNABLE_PARAM(Name, Default, Min, Max, Step) \
	    SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
		inline TunableParam &param_##Name = addTunableParam(#Name, Default, Min, Max, Step, nullptr); \
		inline auto Name() { return param_##Name.value; }

	#define SP_TUNABLE_PARAM_CALLBACK(Name, Default, Min, Max, Step, Callback) \
	    SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
		inline TunableParam &param_##Name = addTunableParam(#Name, Default, Min, Max, Step, Callback); \
		inline auto Name() { return param_##Name.value; }

#else
	#define SP_TUNABLE_PARAM(Name, Default, Min, Max, Step) \
		SP_TUNABLE_ASSERTS(Default, Min, Max, Step) \
		constexpr auto Name() -> i32 { return Default; }
	#define SP_TUNABLE_PARAM_CALLBACK(Name, Default, Min, Max, Step, Callback) \
		SP_TUNABLE_PARAM(Name, Default, Min, Max, Step)
#endif

	SP_TUNABLE_PARAM(defaultMovesToGo, 39, 12, 40, 1)
	SP_TUNABLE_PARAM(incrementScale, 86, 50, 100, 5)
	SP_TUNABLE_PARAM(softTimeScale, 51, 50, 100, 5)
	SP_TUNABLE_PARAM(hardTimeScale, 59, 20, 100, 5)

	SP_TUNABLE_PARAM(nodeTimeBase, 146, 100, 250, 10)
	SP_TUNABLE_PARAM(nodeTimeScale, 157, 100, 250, 10)
	SP_TUNABLE_PARAM(nodeTimeScaleMin, 102, 1, 1000, 100)

	SP_TUNABLE_PARAM(timeScaleMin, 3, 1, 1000, 100)

	SP_TUNABLE_PARAM(minAspDepth, 9, 1, 10, 1)

	SP_TUNABLE_PARAM(maxAspReduction, 3, 0, 5, 1)

	SP_TUNABLE_PARAM(initialAspWindow, 9, 8, 50, 4)
	SP_TUNABLE_PARAM(maxAspWindow, 533, 100, 1000, 100)
	SP_TUNABLE_PARAM(aspWideningFactor, 3, 1, 24, 1)

	SP_TUNABLE_PARAM(minNmpDepth, 6, 3, 8, 0.5)

	SP_TUNABLE_PARAM(nmpReductionBase, 3, 2, 5, 0.5)
	SP_TUNABLE_PARAM(nmpReductionDepthScale, 7, 1, 8, 1)
	SP_TUNABLE_PARAM(nmpReductionEvalScale, 195, 50, 300, 25)
	SP_TUNABLE_PARAM(maxNmpEvalReduction, 4, 2, 5, 1)

	SP_TUNABLE_PARAM(minNmpVerifDepth, 17, 6, 18, 1)
	SP_TUNABLE_PARAM(nmpVerifDepthFactor, 13, 8, 14, 1)

	SP_TUNABLE_PARAM(minLmrDepth, 2, 2, 5, 1)

	SP_TUNABLE_PARAM(lmrMinMovesPv, 2, 0, 5, 1)
	SP_TUNABLE_PARAM(lmrMinMovesNonPv, 5, 0, 5, 1)

	SP_TUNABLE_PARAM(maxRfpDepth, 4, 4, 12, 0.5)
	SP_TUNABLE_PARAM(rfpMarginNonImproving, 67, 25, 150, 5)
	SP_TUNABLE_PARAM(rfpMarginImproving, 36, 25, 150, 5)
	SP_TUNABLE_PARAM(rfpHistoryMargin, 351, 64, 1024, 64)

	SP_TUNABLE_PARAM(maxSeePruningDepth, 7, 4, 15, 1)

	SP_TUNABLE_PARAM(quietSeeThreshold, -63, -120, -20, 10)
	SP_TUNABLE_PARAM(noisySeeThreshold, -65, -120, -20, 10)

	SP_TUNABLE_PARAM(minSingularityDepth, 11, 4, 12, 0.5)

	SP_TUNABLE_PARAM(singularityDepthMargin, 1, 1, 4, 1)
	SP_TUNABLE_PARAM(singularityDepthScale, 15, 8, 32, 2)

	SP_TUNABLE_PARAM(doubleExtensionMargin, 25, 2, 30, 2)
	SP_TUNABLE_PARAM(tripleExtensionMargin, 90, 50, 300, 15)
	SP_TUNABLE_PARAM(multiExtensionLimit, 4, 4, 16, 1)

	SP_TUNABLE_PARAM(maxFpDepth, 12, 4, 12, 0.5)

	SP_TUNABLE_PARAM(fpMargin, 218, 120, 350, 15)
	SP_TUNABLE_PARAM(fpScale, 78, 40, 80, 5)

	SP_TUNABLE_PARAM(minIirDepth, 5, 3, 6, 0.5)

	SP_TUNABLE_PARAM(maxLmpDepth, 10, 4, 12, 1)
	SP_TUNABLE_PARAM(lmpMinMovesBase, 3, 2, 5, 1)

	SP_TUNABLE_PARAM(maxHistory, 16107, 8192, 32768, 256)

	SP_TUNABLE_PARAM(maxHistoryBonus, 2305, 1024, 3072, 256)
	SP_TUNABLE_PARAM(historyBonusDepthScale, 275, 128, 512, 32)
	SP_TUNABLE_PARAM(historyBonusOffset, 522, 128, 768, 64)

	SP_TUNABLE_PARAM(maxHistoryPenalty, 1130, 1024, 3072, 256)
	SP_TUNABLE_PARAM(historyPenaltyDepthScale, 403, 128, 512, 32)
	SP_TUNABLE_PARAM(historyPenaltyOffset, 217, 128, 768, 64)

	SP_TUNABLE_PARAM(historyLmrDivisor, 9154, 4096, 16384, 512)

	SP_TUNABLE_PARAM(evalDeltaLmrDiv, 407, 100, 1000, 50)
	SP_TUNABLE_PARAM(maxEvalDeltaReduction, 3, 1, 4, 1)

	SP_TUNABLE_PARAM(lmrDeeperBase, 70, 32, 96, 8)
	SP_TUNABLE_PARAM(lmrDeeperScale, 9, 2, 12, 1)

	SP_TUNABLE_PARAM(kingValue, 450, 250, 1450, 25)

	SP_TUNABLE_PARAM_CALLBACK(lmrBase, 69, 50, 120, 5, updateLmrTable)
	SP_TUNABLE_PARAM_CALLBACK(lmrDivisor, 221, 100, 300, 10, updateLmrTable)

	SP_TUNABLE_PARAM(qsearchFpMargin, 146, 50, 400, 10);

#undef SP_TUNABLE_PARAM
#undef SP_TUNABLE_PARAM_CALLBACK
#undef SP_TUNABLE_ASSERTS
}
