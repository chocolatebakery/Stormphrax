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

#include "../types.h"

#include <array>

#include "nnue/activation.h"
#include "nnue/arch/singlelayer.h"
#include "nnue/features.h"
#include "nnue/output.h"

namespace stormphrax::eval {
    // simple arch: (928->64)x2->1
    // clipped ReLU -> linear

    constexpr u32 kFtQ = 255;
    constexpr u32 kL1Q = 64;

    constexpr u32 kFtScaleBits = 7;

    constexpr u32 kL1Size = 128;

    using L1Activation = nnue::activation::ClippedReLU;

    constexpr i32 kScale = 400;

    // visually flipped upside down, a1 = 0
    using InputFeatureSet = nnue::features::WithPockets<nnue::features::SingleBucket>;

    using OutputBucketing = nnue::output::Single;

    using LayeredArch = nnue::arch::SingleLayer<
        kL1Size,
        kFtQ,
        kL1Q,
        L1Activation,
        OutputBucketing,
        kScale>;
} // namespace stormphrax::eval
