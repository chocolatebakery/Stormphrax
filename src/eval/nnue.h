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
#include <vector>

#include "../util/static_vector.h"
#include "arch.h"
#include "nnue/activation.h"
#include "nnue/arch/multilayer.h"
#include "nnue/input.h"
#include "nnue/network.h"

namespace stormphrax::eval {
    using FeatureTransformer = nnue::FeatureTransformer<i16, kL1Size, InputFeatureSet>;
    using Network = nnue::PerspectiveNetwork<FeatureTransformer, OutputBucketing, LayeredArch>;

    using Accumulator = FeatureTransformer::Accumulator;
    using RefreshTable = FeatureTransformer::RefreshTable;
    using PocketCounts = nnue::PocketCounts;

    extern const Network& g_network;

    void loadDefaultNetwork();
    void loadNetwork(std::string_view name);

    [[nodiscard]] std::string_view defaultNetworkName();

    struct NnueUpdates {
        using PieceSquare = std::pair<Piece, Square>;
        struct PocketUpdate {
            Color color;
            PieceType piece;
            u8 slot;
        };

        // [black, white]
        std::array<bool, 2> refresh{};

        StaticVector<PieceSquare, 2> sub{};
        StaticVector<PieceSquare, 2> add{};
        StaticVector<PocketUpdate, 2> pocketSub{};
        StaticVector<PocketUpdate, 2> pocketAdd{};

        inline void setRefresh(Color c) {
            refresh[static_cast<i32>(c)] = true;
        }

        [[nodiscard]] inline bool requiresRefresh(Color c) const {
            return refresh[static_cast<i32>(c)];
        }

        inline void pushSubAdd(Piece piece, Square src, Square dst) {
            sub.push({piece, src});
            add.push({piece, dst});
        }

        inline void pushSub(Piece piece, Square square) {
            sub.push({piece, square});
        }

        inline void pushAdd(Piece piece, Square square) {
            add.push({piece, square});
        }

        inline void pushPocketAdd(Color color, PieceType piece, u8 slot) {
            assert(InputFeatureSet::kHasPockets);
            assert(piece != PieceType::kNone && piece != PieceType::kKing);
            assert(slot < InputFeatureSet::kPocketSlotsCount);
            pocketAdd.push({color, piece, slot});
        }

        inline void pushPocketSub(Color color, PieceType piece, u8 slot) {
            assert(InputFeatureSet::kHasPockets);
            assert(piece != PieceType::kNone && piece != PieceType::kKing);
            assert(slot < InputFeatureSet::kPocketSlotsCount);
            pocketSub.push({color, piece, slot});
        }
    };

    struct UpdateContext {
        NnueUpdates updates{};
        BitboardSet bbs{};
        KingPair kings{};
        PocketCounts pockets{};
    };

    class NnueState {
    private:
        struct UpdatableAccumulator {
            Accumulator acc{};
            UpdateContext ctx{};
            std::array<bool, 2> dirty{};

            inline void setDirty() {
                dirty[0] = dirty[1] = true;
            }

            inline void setUpdated(Color c) {
                assert(c != Color::kNone);
                dirty[static_cast<i32>(c)] = false;
            }

            [[nodiscard]] inline bool isDirty(Color c) {
                assert(c != Color::kNone);
                return dirty[static_cast<i32>(c)];
            }
        };

    public:
        NnueState() {
            m_accumulatorStack.resize(256);
        }

        inline void reset(const BitboardSet& bbs, KingPair kings, const PocketCounts& pockets) {
            assert(kings.isValid());

            m_refreshTable.init(g_network.featureTransformer());

            m_curr = &m_accumulatorStack[0];

            for (const auto c : {Color::kBlack, Color::kWhite}) {
                const auto king = kings.color(c);
                const auto entry = InputFeatureSet::getRefreshTableEntry(c, king);

                auto& rtEntry = m_refreshTable.table[entry];
                resetAccumulator(rtEntry.accumulator, c, bbs, king, pockets);

                m_curr->acc.copyFrom(c, rtEntry.accumulator);
                rtEntry.colorBbs(c) = bbs;
                rtEntry.colorPockets(c) = pockets;
            }
        }

        template <bool ApplyImmediately>
        inline void pushUpdates(
            const NnueUpdates& updates,
            const BitboardSet& bbs,
            KingPair kings,
            const PocketCounts& pockets
        ) {
            if constexpr (ApplyImmediately) {
                const UpdateContext ctx{updates, bbs, kings, pockets};
                updateBoth(m_curr->acc, *m_curr, m_refreshTable, ctx);
            } else {
                ++m_curr;

                m_curr->ctx = {updates, bbs, kings, pockets};
                m_curr->setDirty();
            }
        }

        inline void pop() {
            assert(m_curr > &m_accumulatorStack[0]);
            --m_curr;
        }

        [[nodiscard]] inline i32 evaluate(
            const BitboardSet& bbs,
            KingPair kings,
            const PocketCounts& pockets,
            Color stm
        ) {
            assert(m_curr >= &m_accumulatorStack[0] && m_curr <= &m_accumulatorStack.back());
            assert(stm != Color::kNone);

            ensureUpToDate(bbs, kings, pockets);

            return evaluate(m_curr->acc, bbs, stm);
        }

        [[nodiscard]] static inline i32 evaluateOnce(
            const BitboardSet& bbs,
            KingPair kings,
            const PocketCounts& pockets,
            Color stm
        ) {
            assert(kings.isValid());
            assert(stm != Color::kNone);

            Accumulator accumulator{};

            accumulator.initBoth(g_network.featureTransformer());

            resetAccumulator(accumulator, Color::kBlack, bbs, kings.black(), pockets);
            resetAccumulator(accumulator, Color::kWhite, bbs, kings.white(), pockets);

            return evaluate(accumulator, bbs, stm);
        }

    private:
        std::vector<UpdatableAccumulator> m_accumulatorStack{};
        UpdatableAccumulator* m_curr{};

        RefreshTable m_refreshTable{};

        static inline void update(
            const Accumulator& prev,
            UpdatableAccumulator& curr,
            RefreshTable& refreshTable,
            const UpdateContext& ctx,
            Color c
        ) {
            if (ctx.updates.requiresRefresh(c)) {
                refreshAccumulator(curr, c, ctx.bbs, ctx.pockets, refreshTable, ctx.kings.color(c));
                return;
            }

            const auto subCount = ctx.updates.sub.size();
            const auto addCount = ctx.updates.add.size();
            const auto pocketSubCount = ctx.updates.pocketSub.size();
            const auto pocketAddCount = ctx.updates.pocketAdd.size();

            const bool hasBoardUpdates = subCount != 0 || addCount != 0;
            const bool hasPocketUpdates =
                InputFeatureSet::kHasPockets && (pocketSubCount != 0 || pocketAddCount != 0);

            if (!hasBoardUpdates && !hasPocketUpdates) {
                return;
            }

            const auto king = ctx.kings.color(c);

            if (hasBoardUpdates) {
                if (addCount == 1 && subCount == 1) // regular non-capture
                {
                    const auto [subPiece, subSquare] = ctx.updates.sub[0];
                    const auto [addPiece, addSquare] = ctx.updates.add[0];

                    const auto sub = featureIndex(c, subPiece, subSquare, king);
                    const auto add = featureIndex(c, addPiece, addSquare, king);

                    curr.acc.subAddFrom(prev, g_network.featureTransformer(), c, sub, add);
                } else if (addCount == 1 && subCount == 2) // any capture
                {
                    const auto [subPiece0, subSquare0] = ctx.updates.sub[0];
                    const auto [subPiece1, subSquare1] = ctx.updates.sub[1];
                    const auto [addPiece, addSquare] = ctx.updates.add[0];

                    const auto sub0 = featureIndex(c, subPiece0, subSquare0, king);
                    const auto sub1 = featureIndex(c, subPiece1, subSquare1, king);
                    const auto add = featureIndex(c, addPiece, addSquare, king);

                    curr.acc.subSubAddFrom(prev, g_network.featureTransformer(), c, sub0, sub1, add);
                } else if (addCount == 1 && subCount == 0) // drop
                {
                    const auto [addPiece, addSquare] = ctx.updates.add[0];

                    const auto add = featureIndex(c, addPiece, addSquare, king);

                    curr.acc.copyFrom(c, prev);
                    curr.acc.activateFeature(g_network.featureTransformer(), c, add);
                } else if (addCount == 2 && subCount == 2) // castling
                {
                    const auto [subPiece0, subSquare0] = ctx.updates.sub[0];
                    const auto [subPiece1, subSquare1] = ctx.updates.sub[1];
                    const auto [addPiece0, addSquare0] = ctx.updates.add[0];
                    const auto [addPiece1, addSquare1] = ctx.updates.add[1];

                    const auto sub0 = featureIndex(c, subPiece0, subSquare0, king);
                    const auto sub1 = featureIndex(c, subPiece1, subSquare1, king);
                    const auto add0 = featureIndex(c, addPiece0, addSquare0, king);
                    const auto add1 = featureIndex(c, addPiece1, addSquare1, king);

                    curr.acc.subSubAddAddFrom(prev, g_network.featureTransformer(), c, sub0, sub1, add0, add1);
                } else {
                    assert(false && "Materialising a piece from nowhere?");
                }
            } else {
                curr.acc.copyFrom(c, prev);
            }

            if constexpr (InputFeatureSet::kHasPockets) {
                for (const auto& update : ctx.updates.pocketSub) {
                    const auto feature = pocketFeatureIndex(c, update.color, update.piece, update.slot, king);
                    curr.acc.deactivateFeature(g_network.featureTransformer(), c, feature);
                }

                for (const auto& update : ctx.updates.pocketAdd) {
                    const auto feature = pocketFeatureIndex(c, update.color, update.piece, update.slot, king);
                    curr.acc.activateFeature(g_network.featureTransformer(), c, feature);
                }
            }

            curr.setUpdated(c);
        }

        static inline void updateBoth(
            const Accumulator& prev,
            UpdatableAccumulator& curr,
            RefreshTable& refreshTable,
            const UpdateContext& ctx
        ) {
            update(prev, curr, refreshTable, ctx, Color::kBlack);
            update(prev, curr, refreshTable, ctx, Color::kWhite);
        }

        inline void ensureUpToDate(const BitboardSet& bbs, KingPair kings, const PocketCounts& pockets) {
            for (const auto c : {Color::kBlack, Color::kWhite}) {
                if (!m_curr->isDirty(c)) {
                    continue;
                }

                // if the current accumulator needs a refresh, just do it
                if (m_curr->ctx.updates.requiresRefresh(c)) {
                    refreshAccumulator(*m_curr, c, bbs, pockets, m_refreshTable, kings.color(c));
                    continue;
                }

                // scan back to the last non-dirty accumulator, or an accumulator that requires a refresh.
                // root accumulator is always up-to-date
                auto* curr = m_curr - 1;
                for (; curr->isDirty(c) && !curr->ctx.updates.requiresRefresh(c); --curr) {
                }

                assert(curr != &m_accumulatorStack[0] || !curr->ctx.updates.requiresRefresh(c));

                // if the found accumulator requires a refresh, just give up and refresh the current one
                if (curr->ctx.updates.requiresRefresh(c)) {
                    refreshAccumulator(*m_curr, c, bbs, pockets, m_refreshTable, kings.color(c));
                } else {
                    // otherwise go forward and incrementally update all accumulators in between
                    do {
                        const auto& prev = *curr;

                        ++curr;
                        update(prev.acc, *curr, m_refreshTable, curr->ctx, c);
                    } while (curr != m_curr);
                }
            }
        }

        [[nodiscard]] static inline i32 evaluate(const Accumulator& accumulator, const BitboardSet& bbs, Color stm) {
            assert(stm != Color::kNone);
            return stm == Color::kBlack ? g_network.propagate(bbs, accumulator.black(), accumulator.white())[0]
                                        : g_network.propagate(bbs, accumulator.white(), accumulator.black())[0];
        }

        static inline void refreshAccumulator(
            UpdatableAccumulator& accumulator,
            Color c,
            const BitboardSet& bbs,
            const PocketCounts& pockets,
            RefreshTable& refreshTable,
            Square king
        ) {
            const auto tableIdx = InputFeatureSet::getRefreshTableEntry(c, king);

            auto& rtEntry = refreshTable.table[tableIdx];
            auto& prevBoards = rtEntry.colorBbs(c);

            StaticVector<u32, 32> adds;
            StaticVector<u32, 32> subs;
            for (u32 pieceIdx = 0; pieceIdx < static_cast<u32>(Piece::kNone); ++pieceIdx) {
                const auto piece = static_cast<Piece>(pieceIdx);

                const auto prev = prevBoards.forPiece(piece);
                const auto curr = bbs.forPiece(piece);

                auto added = curr & ~prev;
                auto removed = prev & ~curr;

                while (added) {
                    const auto sq = added.popLowestSquare();
                    const auto feature = featureIndex(c, piece, sq, king);
                    adds.push(feature);
                }

                while (removed) {
                    const auto sq = removed.popLowestSquare();
                    const auto feature = featureIndex(c, piece, sq, king);
                    subs.push(feature);
                }
            }

            while (adds.size() >= 4) {
                const auto add0 = adds.pop();
                const auto add1 = adds.pop();
                const auto add2 = adds.pop();
                const auto add3 = adds.pop();
                rtEntry.accumulator.activateFourFeatures(g_network.featureTransformer(), c, add0, add1, add2, add3);
            }

            while (adds.size() >= 1) {
                const auto add = adds.pop();
                rtEntry.accumulator.activateFeature(g_network.featureTransformer(), c, add);
            }

            while (subs.size() >= 4) {
                const auto sub0 = subs.pop();
                const auto sub1 = subs.pop();
                const auto sub2 = subs.pop();
                const auto sub3 = subs.pop();
                rtEntry.accumulator.deactivateFourFeatures(g_network.featureTransformer(), c, sub0, sub1, sub2, sub3);
            }

            while (subs.size() >= 1) {
                const auto sub = subs.pop();
                rtEntry.accumulator.deactivateFeature(g_network.featureTransformer(), c, sub);
            }

            if constexpr (InputFeatureSet::kHasPockets) {
                auto& prevPockets = rtEntry.colorPockets(c);

                for (const auto pocketColor : {Color::kBlack, Color::kWhite}) {
                    const auto colorIdx = static_cast<usize>(pocketColor);
                    const auto& prevPocket = prevPockets[colorIdx];
                    const auto& currPocket = pockets[colorIdx];

                    for (u32 pieceIdx = 0; pieceIdx < InputFeatureSet::kPocketPieceTypes; ++pieceIdx) {
                        const auto prevCount = prevPocket[pieceIdx];
                        const auto currCount = currPocket[pieceIdx];

                        if (prevCount == currCount) {
                            continue;
                        }

                        const auto piece = static_cast<PieceType>(pieceIdx);

                        if (prevCount < currCount) {
                            for (u8 slot = prevCount; slot < currCount; ++slot) {
                                const auto feature = pocketFeatureIndex(c, pocketColor, piece, slot, king);
                                rtEntry.accumulator.activateFeature(g_network.featureTransformer(), c, feature);
                            }
                        } else {
                            for (u8 slot = currCount; slot < prevCount; ++slot) {
                                const auto feature = pocketFeatureIndex(c, pocketColor, piece, slot, king);
                                rtEntry.accumulator.deactivateFeature(g_network.featureTransformer(), c, feature);
                            }
                        }
                    }
                }
            }

            accumulator.acc.copyFrom(c, rtEntry.accumulator);
            prevBoards = bbs;
            rtEntry.colorPockets(c) = pockets;

            accumulator.setUpdated(c);
        }

        static inline void resetAccumulator(
            Accumulator& accumulator,
            Color c,
            const BitboardSet& bbs,
            Square king,
            const PocketCounts& pockets
        ) {
            assert(c != Color::kNone);
            assert(king != Square::kNone);

            // loop through each coloured piece, and activate the features
            // corresponding to that piece on each of the squares it occurs on
            for (u32 pieceIdx = 0; pieceIdx < 12; ++pieceIdx) {
                const auto piece = static_cast<Piece>(pieceIdx);

                auto board = bbs.forPiece(piece);
                while (!board.empty()) {
                    const auto sq = board.popLowestSquare();

                    const auto feature = featureIndex(c, piece, sq, king);
                    accumulator.activateFeature(g_network.featureTransformer(), c, feature);
                }
            }

            if constexpr (InputFeatureSet::kHasPockets) {
                for (const auto pocketColor : {Color::kBlack, Color::kWhite}) {
                    const auto& pocket = pockets[static_cast<usize>(pocketColor)];
                    for (u32 pieceIdx = 0; pieceIdx < InputFeatureSet::kPocketPieceTypes; ++pieceIdx) {
                        const auto count = pocket[pieceIdx];
                        const auto piece = static_cast<PieceType>(pieceIdx);
                        for (u8 slot = 0; slot < count; ++slot) {
                            const auto feature = pocketFeatureIndex(c, pocketColor, piece, slot, king);
                            accumulator.activateFeature(g_network.featureTransformer(), c, feature);
                        }
                    }
                }
            }
        }

        static inline void resetAccumulator(
            UpdatableAccumulator& accumulator,
            Color c,
            const BitboardSet& bbs,
            Square king,
            const PocketCounts& pockets
        ) {
            resetAccumulator(accumulator.acc, c, bbs, king, pockets);
            accumulator.setUpdated(c);
        }

        [[nodiscard]] static inline u32 featureIndex(Color c, Piece piece, Square sq, Square king) {
            assert(c != Color::kNone);
            assert(piece != Piece::kNone);
            assert(sq != Square::kNone);
            assert(king != Square::kNone);

            constexpr u32 kColorStride = 64 * 6;
            constexpr u32 kPieceStride = 64;

            const auto type = static_cast<u32>(pieceType(piece));

            const auto color = [piece, c]() -> u32 {
                if (InputFeatureSet::kMergedKings && pieceType(piece) == PieceType::kKing) {
                    return 0;
                }
                return pieceColor(piece) == c ? 0 : 1;
            }();

            if (c == Color::kBlack) {
                sq = flipSquareRank(sq);
            }

            sq = InputFeatureSet::transformFeatureSquare(sq, king);

            const auto bucketOffset = InputFeatureSet::getBucket(c, king) * InputFeatureSet::kInputSize;
            return bucketOffset + color * kColorStride + type * kPieceStride + static_cast<u32>(sq);
        }

        [[nodiscard]] static inline u32 pocketFeatureIndex(
            Color c,
            Color pocketColor,
            PieceType piece,
            u8 slot,
            Square king
        ) {
            assert(InputFeatureSet::kHasPockets);
            assert(c != Color::kNone);
            assert(pocketColor != Color::kNone);
            assert(piece != PieceType::kNone && piece != PieceType::kKing);
            assert(slot < InputFeatureSet::kPocketSlotsCount);
            assert(king != Square::kNone);

            constexpr u32 kPocketColorStride =
                InputFeatureSet::kPocketPieceTypes * InputFeatureSet::kPocketSlotsCount;
            constexpr u32 kPocketPieceStride = InputFeatureSet::kPocketSlotsCount;

            const auto bucketOffset = InputFeatureSet::getBucket(c, king) * InputFeatureSet::kInputSize;
            const auto color = pocketColor == c ? 0U : 1U;
            const auto type = static_cast<u32>(piece);
            return bucketOffset + InputFeatureSet::kBoardInputSize + color * kPocketColorStride + type * kPocketPieceStride
                 + static_cast<u32>(slot);
        }
    };
} // namespace stormphrax::eval
