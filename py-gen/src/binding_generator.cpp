#include "py-gen.h"

#include <cxxopts.hpp>
#include <exception>
#include <fmt/format.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <string>
#include <vector>

struct ProgramOptions {
    std::string              moduleName;
    size_t                   nSourceFiles;
    std::string              outputDir = ".";
    std::string              configFile;
    std::vector<std::string> clangArgs;
};

/**
 * @brief Parses command line arguments to configure the Python binding generator.
 *
 * This function uses the cxxopts library to parse command line arguments and populate
 * a ProgramOptions structure with the provided values. It supports the following options:
 *
 * - `-m, --module <module>`: Specifies the name of the Python module to generate bindings for.
 * - `-s, --sources <files>`: Specifies the source files to generate bindings from.
 * - `-o, --output <directory>`: Specifies the output directory for the generated bindings (default is current directory).
 * - `-c, --config <file>`: Specifies a configuration file (currently unsupported parsing).
 * - `-h, --help`: Prints the usage information and exits.
 *
 * If required options are missing or if there is an error in parsing, the function will print
 * an error message and exit the program.
 *
 * @param argc The number of command line arguments.
 * @param argv The array of command line arguments.
 * @return A ProgramOptions structure populated with the parsed values.
 *
 * Example usage:
 * @code
 * ./py-gen --module MyModule --sources file1.cpp file2.cpp --output ./bindings -- -std=c++11 -I/usr/include
 * @endcode
 */
bool parse2Options(int argc, const char **argv, ProgramOptions &programOptions) {
    cxxopts::Options options("py-gen", "Python binding generator for C++");

    options.add_options()("m,module", "Python module name", cxxopts::value<std::string>());
    options.add_options()("s,sources", "Source files", cxxopts::value<std::vector<std::string>>());
    options.add_options()("o,output", "Output directory", cxxopts::value<std::string>()->default_value("."));
    options.add_options()("c,config", "Config file", cxxopts::value<std::string>());

    // Allow unmatched arguments to be passed through to clang
    options.allow_unrecognised_options();

    try {
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            llvm::outs() << options.help() << "\n";
            exit(0);
        }

        if (!result.count("module")) {
            llvm::errs() << "Missing module name\n";
            exit(1);
        }

        if (!result.count("sources")) {
            llvm::errs() << "Missing source files\n";
            exit(1);
        }

        if (result.count("config")) {
            programOptions.configFile = result["config"].as<std::string>();
            llvm::outs() << "Config file: " << programOptions.configFile << " unsupported parsing\n";
        }

        programOptions.moduleName   = result["module"].as<std::string>();
        programOptions.clangArgs    = result["sources"].as<std::vector<std::string>>();
        programOptions.nSourceFiles = programOptions.clangArgs.size();
        programOptions.outputDir    = result["output"].as<std::string>();

        // List all unmatched arguments
        if (!result.unmatched().empty()) {
            llvm::outs() << "Taking as clang tool arguments (+ source provided files, but not listed here):\n";
            programOptions.clangArgs.reserve(programOptions.clangArgs.size() + result.unmatched().size());

            // Add back "--" separator if we have compiler args, this is lost in cxxopts otherwise
            programOptions.clangArgs.emplace_back("--");
        }
        for (auto &arg : result.unmatched()) {
            llvm::outs() << "   " << arg << "\n";
            programOptions.clangArgs.emplace_back(std::move(arg));
        }
        return true;
    } catch (const std::exception &e) {
        llvm::errs() << "Error parsing options: " << e.what() << "\n";
        return false;
    }
}

int main(int argc, const char **argv) {
    ProgramOptions options;
    if (not parse2Options(argc, argv, options)) {
        return 1;
    }

    // Prepare clang tool
    std::vector<const char *> clangArgv;
    clangArgv.push_back(argv[0]); // Program name
    for (const auto &arg : options.clangArgs) {
        clangArgv.push_back(arg.c_str());
    }

    int  argc_          = clangArgv.size();
    auto expectedParser = clang::tooling::CommonOptionsParser::create(argc_, clangArgv.data(), llvm::cl::getGeneralCategory());

    if (!expectedParser) {
        llvm::errs() << "Error parsing command line arguments: " << expectedParser.takeError() << "\n";
        return 1;
    }

    clang::tooling::ClangTool tool(expectedParser->getCompilations(), expectedParser->getSourcePathList());

    Structs   structs;
    Functions functions;
    Headers   headers;

    auto cb = [&structs, &functions](Structs &&structs_, Functions &&functions_) {
        structs.insert(structs.end(), std::make_move_iterator(structs_.begin()), std::make_move_iterator(structs_.end()));
        functions.insert(functions.end(), std::make_move_iterator(functions_.begin()), std::make_move_iterator(functions_.end()));
    };

    auto hcb = [&headers](Headers &&headers_) {
        headers.insert(headers.end(), std::make_move_iterator(headers_.begin()), std::make_move_iterator(headers_.end()));
    };

    auto factoryPtr = std::make_unique<DeclarationExtractionActionFactory>(cb, hcb);

    if (tool.run(factoryPtr.get()) != 0) {
        llvm::errs() << "Error running tool\n";
        return 1;
    }

    for (const auto &header : headers) {
        llvm::outs() << "Header: " << header.name << " system: <" << (header.isSystem ? "yes" : "no") << "> (" << header.fullPath << ")\n";
    }

    for (const auto &structInfo : structs) {
        llvm::outs() << "Struct: " << fmt::format("{0} ({1})", structInfo.name.plain, structInfo.name.qualified) << "\n";
        for (const auto &info : structInfo.members) {
            llvm::outs() << "    " << fmt::format("{0} {1}", info.type.plain, info.name.plain) << "\n";
        }
    }

    for (const auto &funcInfo : functions) {
        llvm::outs() << "Function: " << fmt::format("{0} ({1})", funcInfo.name.plain, funcInfo.name.qualified) << "\n";
        llvm::outs() << "    Return type: " << fmt::format("{0} ({1})", funcInfo.returnType.plain, funcInfo.returnType.qualified) << "\n";

        if (!funcInfo.parameters.empty()) {
            llvm::outs() << "    Parameters:\n";
        }
        for (const auto &info : funcInfo.parameters) {
            llvm::outs() << "    " << "    "
                         << fmt::format("{0} ({1}) {2} ({3})", info.type.plain, info.type.qualified, info.name.plain, info.name.qualified)
                         << "\n";
        }
    }

    generateBindings(structs, functions, headers, options.moduleName);

    return 0;
}