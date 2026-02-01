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

#include "bench.h"
#include "cuckoo.h"
#include "datagen/datagen.h"
#include "eval/nnue.h"
#include "tunable.h"
#include "uci.h"
#include "util/ctrlc.h"
#include "util/parse.h"

#if SP_EXTERNAL_TUNE
    #include "util/split.h"
#endif

using namespace stormphrax;

i32 main(i32 argc, const char* argv[]) {
    tunable::init();
    cuckoo::init();

    eval::loadDefaultNetwork();

    if (argc > 1) {
        const std::string_view mode{argv[1]};

        if (mode == "bench") {
            search::Searcher searcher{bench::kDefaultBenchTtSize};
            bench::run(searcher);

            return 0;
        } else if (mode == "datagen") {
            const auto printUsage = [&]() {
                eprintln(
                    "usage: {} datagen <marlinformat/viriformat/fen> <standard/dfrc/crazyhouse> <path> [threads] [max positions] [syzygy path]",
                    argv[0]
                );
            };

            if (argc < 5) {
                printUsage();
                return 1;
            }

            bool dfrc = false;
            bool crazyhouse = false;

            if (std::string_view{argv[3]} == "dfrc") {
                dfrc = true;
            } else if (std::string_view{argv[3]} == "crazyhouse") {
                crazyhouse = true;
            } else if (std::string_view{argv[3]} != "standard") {
                eprintln("invalid variant {}", argv[3]);
                printUsage();
                return 1;
            }

            u32 threads = 1;
            if (argc > 5 && !util::tryParse<u32>(threads, argv[5])) {
                eprintln("invalid number of threads {}", argv[5]);
                printUsage();
                return 1;
            }

            std::optional<std::string_view> tbPath{};
            std::optional<u64> maxPositions{};

            i32 argIdx = 6;
            if (argc > argIdx) {
                u64 parsed{};
                if (util::tryParse<u64>(parsed, argv[argIdx])) {
                    maxPositions = parsed;
                    if (argc > argIdx + 1) {
                        tbPath = std::string_view{argv[argIdx + 1]};
                    }
                } else {
                    tbPath = std::string_view{argv[argIdx]};
                }
            }

            return datagen::run(
                printUsage,
                argv[2],
                dfrc,
                crazyhouse,
                argv[4],
                static_cast<i32>(threads),
                tbPath,
                maxPositions
            );
        }
#if SP_EXTERNAL_TUNE
        else if (mode == "printwf" || mode == "printctt" || mode == "printob")
        {
            if (argc == 2) {
                return 0;
            }

            std::vector<std::string_view> params{};
            split::split(params, argv[2], ',');

            if (mode == "printwf") {
                uci::printWfTuningParams(params);
            } else if (mode == "printctt") {
                uci::printCttTuningParams(params);
            } else if (mode == "printob") {
                uci::printObTuningParams(params);
            }

            return 0;
        }
#endif
    }

    return uci::run();
}
