// Copyright 2022-2023 The Khronos Group Inc.
// Copyright 2022-2023 RasterGrid Kft.
// SPDX-License-Identifier: Apache-2.0

#include "command.h"
#include "utility.h"
#include "validate.h"
#include <iostream>
#include <sstream>
#include <utility>

#include <cxxopts.hpp>
#include <fmt/printf.h>


// -------------------------------------------------------------------------------------------------

namespace ktx {

/** @page ktxtools_validate ktx validate
@~English

Validates a KTX2 file.

@warning TODO Tools P5: This page is incomplete

@section ktxtools_validate_synopsis SYNOPSIS
    ktx validate [options] @e input_file

@section ktxtools_validate_description DESCRIPTION
    @b ktx @b validate validates a Khronos texture format version 2 (KTX2) file given as argument.
    Prints any found error and warning to the stdout.

    @note @b ktx @b validate prints using UTF-8 encoding. If your console is not
    set for UTF-8 you will see incorrect characters in output of the file
    identifier on each side of the "KTX nn".

    The following options are available:
    <dl>
        <dt>--format &lt;text|json|mini-json&gt;</dt>
        <dd>Specifies the output format.
            @b text - Human readable text based format.
            @b json - Formatted JSON.
            @b mini-json - Minified JSON (Every optional formatting is skipped).
            The default format is @b text.
        </dd>
        <dt>-g, --gltf-basisu</dt>
        <dd>Check compatibility with KHR_texture_basisu glTF extension.</dd>
        <dt>-e, --warnings-as-errors</dt>
        <dd>Treat warnings as errors.</dd>
    </dl>
    @snippet{doc} ktx/command.h command options

@section ktxtools_validate_exitstatus EXIT STATUS
    @b ktx @b validate exits
        0 - Success
        1 - Command line error
        2 - IO error

@section ktxtools_validate_history HISTORY

@par Version 4.0
 - Initial version

@section ktxtools_validate_author AUTHOR
    - Mátyás Császár [Vader], RasterGrid www.rastergrid.com
    - Daniel Rákos, RasterGrid www.rastergrid.com
*/
class CommandValidate : public Command {
    struct OptionsValidate {
        bool warningsAsErrors = false;
        bool GLTFBasisU = false;

        void init(cxxopts::Options& opts) {
            opts.add_options()
                ("e,warnings-as-errors", "Treat warnings as errors.")
                ("g,gltf-basisu", "Check compatibility with KHR_texture_basisu glTF extension.");
        }

        void process(cxxopts::Options&, cxxopts::ParseResult& args, Reporter&) {
            warningsAsErrors = args["warnings-as-errors"].as<bool>();
            GLTFBasisU = args["gltf-basisu"].as<bool>();
        }
    };

    Combine<OptionsValidate, OptionsFormat, OptionsSingleIn, OptionsGeneric> options;

public:
    virtual int main(int argc, _TCHAR* argv[]) override;
    virtual void initOptions(cxxopts::Options& opts) override;
    virtual void processOptions(cxxopts::Options& opts, cxxopts::ParseResult& args) override;

private:
    void executeValidate();
};

// -------------------------------------------------------------------------------------------------

int CommandValidate::main(int argc, _TCHAR* argv[]) {
    try {
        parseCommandLine("ktx validate",
                "Validates a KTX2 file and prints any issues to the stdout.",
                argc, argv);
        executeValidate();
        return RETURN_CODE_SUCCESS;
    } catch (const FatalError& error) {
        return error.return_code;
    } catch (const std::exception& e) {
        fmt::print(std::cerr, "{} fatal: {}\n", commandName, e.what());
        return RETURN_CODE_RUNTIME_ERROR;
    }
}

void CommandValidate::initOptions(cxxopts::Options& opts) {
    options.init(opts);
}

void CommandValidate::processOptions(cxxopts::Options& opts, cxxopts::ParseResult& args) {
    options.process(opts, args, *this);
}

void CommandValidate::executeValidate() {
    switch (options.format) {
    case OutputFormat::text: {
        std::ostringstream messagesOS;
        const auto validationResult = validateNamedFile(
                options.inputFilepath,
                options.warningsAsErrors,
                options.GLTFBasisU,
                [&](const ValidationReport& issue) {
            fmt::print(messagesOS, "{}-{:04}: {}\n", toString(issue.type), issue.id, issue.message);
            fmt::print(messagesOS, "    {}\n", issue.details);
        });

        const auto validationMessages = std::move(messagesOS).str();
        if (!validationMessages.empty()) {
            fmt::print("Validation {}\n", validationResult == 0 ? "successful" : "failed");
            fmt::print("\n");
            fmt::print("{}", validationMessages);
        }

        if (validationResult != 0)
            throw FatalError(RETURN_CODE_INVALID_FILE);
        break;
    }
    case OutputFormat::json: [[fallthrough]];
    case OutputFormat::json_mini: {
        const auto base_indent = options.format == OutputFormat::json ? +0 : 0;
        const auto indent_width = options.format == OutputFormat::json ? 4 : 0;
        const auto space = options.format == OutputFormat::json ? " " : "";
        const auto nl = options.format == OutputFormat::json ? "\n" : "";

        std::ostringstream messagesOS;
        PrintIndent pi{messagesOS, base_indent, indent_width};

        bool first = true;
        const auto validationResult = validateNamedFile(
                options.inputFilepath,
                options.warningsAsErrors,
                options.GLTFBasisU,
                [&](const ValidationReport& issue) {
            if (!std::exchange(first, false)) {
                pi(2, "}},{}", nl);
            }
            pi(2, "{{{}", nl);
            pi(3, "\"id\":{}{},{}", space, issue.id, nl);
            pi(3, "\"type\":{}\"{}\",{}", space, toString(issue.type), nl);
            pi(3, "\"message\":{}\"{}\",{}", space, escape_json_copy(issue.message), nl);
            pi(3, "\"details\":{}\"{}\"{}", space, escape_json_copy(issue.details), nl);
        });

        PrintIndent out{std::cout, base_indent, indent_width};
        out(0, "{{{}", nl);
        out(1, "\"$schema\":{}\"https://schema.khronos.org/ktx/validate_v0.json\",{}", space, nl);
        out(1, "\"valid\":{}{},{}", space, validationResult == 0, nl);
        if (!first) {
            out(1, "\"messages\":{}[{}", space, nl);
            fmt::print("{}", std::move(messagesOS).str());
            out(2, "}}{}", nl);
            out(1, "]{}", nl);
        } else {
            out(1, "\"messages\":{}[]{}", space, nl);
        }
        out(0, "}}{}", nl);

        if (validationResult != 0)
            throw FatalError(RETURN_CODE_INVALID_FILE);
        break;
    }
    }
}

} // namespace ktx

KTX_COMMAND_ENTRY_POINT(ktxValidate, ktx::CommandValidate)
