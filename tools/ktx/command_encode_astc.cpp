// Copyright 2022-2023 The Khronos Group Inc.
// Copyright 2022-2023 RasterGrid Kft.
// SPDX-License-Identifier: Apache-2.0

#include "command.h"
#include "platform_utils.h"
#include "metrics_utils.h"
#include "compress_utils.h"
#include "encode_utils.h"
#include "formats.h"
#include "sbufstream.h"
#include "utility.h"
#include "validate.h"
#include "astc_utils.h"
#include "ktx.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include <cxxopts.hpp>
#include <fmt/ostream.h>
#include <fmt/printf.h>

namespace ktx {

/** @page ktx_encode_astc ktx encode_astc
@~English

Encode a KTX2 file.

@section ktx_encode_astc_synopsis SYNOPSIS
    ktx encode-astc [option...] @e input-file @e output-file

@section ktx_encode_astc_description DESCRIPTION
    @b ktx @b encode can encode the KTX file specified as the @e input-file argument,
    optionally supercompress the result, and save it as the @e output-file.
    If the @e input-file is '-' the file will be read from the stdin.
    If the @e output-path is '-' the output file will be written to the stdout.
    The input file must be R8, RG8, RGB8 or RGBA8 (or their sRGB variant).
    If the input file is invalid the first encountered validation error is displayed
    to the stderr and the command exits with the relevant non-zero status code.

    The following options are available:
    <dl>
        @snippet{doc} ktx/astc_utils.h command options_astc
        @snippet{doc} ktx/metrics_utils.h command options_metrics
    </dl>
    @snippet{doc} ktx/compress_utils.h command options_compress
    @snippet{doc} ktx/command.h command options_generic

@section ktx_encode_astc_exitstatus EXIT STATUS
    @snippet{doc} ktx/command.h command exitstatus

@section ktx_encode_astc_history HISTORY

@par Version 4.0
 - Initial version

@section ktx_encode_astc_author AUTHOR
    - Wasim Abbas, Arm Ltd. www.arm.com
*/
class CommandEncodeAstc : public Command {
    enum {
        all = -1,
    };

    struct OptionsEncode {
        void init(cxxopts::Options& opts);
        void process(cxxopts::Options& opts, cxxopts::ParseResult& args, Reporter& report);
    };

    Combine<OptionsEncode, OptionsASTC, OptionsMetrics, OptionsCompress, OptionsSingleInSingleOut, OptionsGeneric> options;

public:
    virtual int main(int argc, char* argv[]) override;
    virtual void initOptions(cxxopts::Options& opts) override;
    virtual void processOptions(cxxopts::Options& opts, cxxopts::ParseResult& args) override;

private:
    void executeEncodeAstc();
};

// -------------------------------------------------------------------------------------------------

int CommandEncodeAstc::main(int argc, char* argv[]) {
    try {
        parseCommandLine("ktx encode-astc",
                "ASTC Encode the KTX file specified as the input-file argument,\n"
                "    optionally supercompress the result, and save it as the output-file.",
                argc, argv);
        executeEncodeAstc();
        return +rc::SUCCESS;
    } catch (const FatalError& error) {
        return +error.returnCode;
    } catch (const std::exception& e) {
        fmt::print(std::cerr, "{} fatal: {}\n", commandName, e.what());
        return +rc::RUNTIME_ERROR;
    }
}

void CommandEncodeAstc::OptionsEncode::init(cxxopts::Options&) {
}

void CommandEncodeAstc::OptionsEncode::process(cxxopts::Options&, cxxopts::ParseResult&, Reporter&) {
}

void CommandEncodeAstc::initOptions(cxxopts::Options& opts) {
    options.init(opts);
}

void CommandEncodeAstc::processOptions(cxxopts::Options& opts, cxxopts::ParseResult& args) {
    options.process(opts, args, *this);
}

void CommandEncodeAstc::executeEncodeAstc() {
    InputStream inputStream(options.inputFilepath, *this);
    validateToolInput(inputStream, fmtInFile(options.inputFilepath), *this);

    KTXTexture2 texture{nullptr};
    StreambufStream<std::streambuf*> ktx2Stream{inputStream->rdbuf(), std::ios::in | std::ios::binary};
    auto ret = ktxTexture2_CreateFromStream(ktx2Stream.stream(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, texture.pHandle());
    if (ret != KTX_SUCCESS)
        fatal(rc::INVALID_FILE, "Failed to create KTX2 texture: {}", ktxErrorString(ret));

    if (texture->supercompressionScheme != KTX_SS_NONE)
        fatal(rc::INVALID_FILE, "Cannot encode KTX2 file with {} supercompression.",
            toString(ktxSupercmpScheme(texture->supercompressionScheme)));

    switch (texture->vkFormat) {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        // Allowed formats
        break;
    default:
        fatal_usage("Only R8, RG8, RGB8, or RGBA8 UNORM and SRGB formats can be encoded, "
            "but format is {}.", toString(VkFormat(texture->vkFormat)));
        break;
    }

    // Convert 1D textures to 2D (we could consider 1D as an invalid input)
    texture->numDimensions = std::max(2u, texture->numDimensions);

    // Modify KTXwriter metadata
    const auto writer = fmt::format("{} {}", commandName, version(options.testrun));
    ktxHashList_DeleteKVPair(&texture->kvDataHead, KTX_WRITER_KEY);
    ktxHashList_AddKVPair(&texture->kvDataHead, KTX_WRITER_KEY,
            static_cast<uint32_t>(writer.size() + 1), // +1 to include the \0
            writer.c_str());

    ktx_uint32_t oetf = ktxTexture2_GetOETF(texture);
    if (options.normalMap && oetf != KHR_DF_TRANSFER_LINEAR)
        fatal(rc::INVALID_FILE,
            "--normal-mode specified but the input file uses non-linear transfer function {}.",
            toString(khr_df_transfer_e(oetf)));

    MetricsCalculator metrics;
    metrics.saveReferenceImages(texture, options, *this);
    ret = ktxTexture2_CompressAstcEx(texture, &options);
    if (ret != KTX_SUCCESS)
        fatal(rc::IO_FAILURE, "Failed to encode KTX2 file with codec \"{}\". KTX Error: {}", ktxErrorString(ret));
    metrics.decodeAndCalculateMetrics(texture, options, *this);

    if (options.zstd) {
        ret = ktxTexture2_DeflateZstd(texture, *options.zstd);
        if (ret != KTX_SUCCESS)
            fatal(rc::IO_FAILURE, "Zstd deflation failed. KTX Error: {}", ktxErrorString(ret));
    }

    if (options.zlib) {
        ret = ktxTexture2_DeflateZLIB(texture, *options.zlib);
        if (ret != KTX_SUCCESS)
            fatal(rc::IO_FAILURE, "ZLIB deflation failed. KTX Error: {}", ktxErrorString(ret));
    }

    // Add KTXwriterScParams metadata
    const auto writerScParams = fmt::format("{}{}", options.astcOptions, options.compressOptions);
    if (writerScParams.size() > 0) {
        // Options always contain a leading space
        assert(writerScParams[0] == ' ');
        ktxHashList_AddKVPair(&texture->kvDataHead, KTX_WRITER_SCPARAMS_KEY,
            static_cast<uint32_t>(writerScParams.size()),
            writerScParams.c_str() + 1); // +1 to exclude leading space
    }

    // Save output file
    const auto outputPath = std::filesystem::path(DecodeUTF8Path(options.outputFilepath));
    if (outputPath.has_parent_path())
        std::filesystem::create_directories(outputPath.parent_path());

    OutputStream outputFile(options.outputFilepath, *this);
    outputFile.writeKTX2(texture, *this);
}

} // namespace ktx

KTX_COMMAND_ENTRY_POINT(ktxEncodeAstc, ktx::CommandEncodeAstc)