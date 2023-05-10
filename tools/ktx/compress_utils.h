// Copyright 2022-2023 The Khronos Group Inc.
// Copyright 2022-2023 RasterGrid Kft.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "command.h"
#include "utility.h"

// -------------------------------------------------------------------------------------------------

namespace ktx {

struct OptionsCompress {
    std::optional<uint32_t> zstd;
    std::optional<uint32_t> zlib;

    void init(cxxopts::Options& opts) {
        opts.add_options()
            ("zstd", "Supercompress the data with Zstandard. Level range is [1,22]. Default level is 3.",
                cxxopts::value<uint32_t>(), "<level>")
            ("zlib", "Supercompress the data with ZLIB. Level range is [1,9]. Default level is 2.",
                cxxopts::value<uint32_t>(), "<level>");
    }

    void process(cxxopts::Options&, cxxopts::ParseResult& args, Reporter& report) {
        if (args["zstd"].count()) {
            zstd = args["zstd"].as<uint32_t>();
            if (zstd < 1u || zstd > 22u)
                report.fatal_usage("Invalid zstd level: \"{}\". Value must be between 1 and 22 inclusive.", zstd.value());
        }
        if (args["zlib"].count()) {
            zlib = args["zlib"].as<uint32_t>();
            if (zlib < 1u || zlib > 9u)
                report.fatal_usage("Invalid zlib level: \"{}\". Value must be between 1 and 9 inclusive.", zlib.value());
        }
        if (zstd.has_value() && zlib.has_value())
            report.fatal_usage("Conflicting options: zstd and zlib cannot be used at the same time.");
    }
};

} // namespace ktx
