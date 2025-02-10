#include "py-gen.h"

#include <clang/Tooling/CompilationDatabase.h>
#include <cxxopts.hpp>
#include <exception>
#include <filesystem>
#include <fmt/format.h>
#include <iostream>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <toml++/toml.h>
#include <vector>

struct ProgramOptions {
    std::string              moduleName;
    size_t                   nSourceFiles;
    std::string              outputDir = ".";
    std::filesystem::path    configFile;
    std::filesystem::path    compileCommandsFile;
    std::vector<std::string> clangArgs;
    std::vector<std::string> finalArgs;
};

/**
 * @brief Parses command line arguments to configure the Python binding generator.
 *
 * This function uses the cxxopts library to parse command line arguments and populate
 * a ProgramOptions structure with the provided values. It supports the following options:
 *
 * - `-c, --config <file>`: Specifies a TOML configuration file containing:
 *   - compile_commands: Path to compile_commands.json
 *   - sources: Array of source files to process
 *   - compile_args: Array of compiler arguments
 *   - module_name: Name of the output Python module
 *   - output_dir: Directory for generated files (default: ".")
 * - `-h, --help`: Prints the usage information and exits.
 *
 * @param argc The number of command line arguments
 * @param argv The array of command line arguments
 * @param programOptions Structure to populate with parsed options
 * @return true if parsing was successful, false otherwise
 *
 * Example usage:
 * @code
 * ./py-gen -c config.toml
 * @endcode
 */
bool processCLIargsIntoProgramOptions(int argc, const char **argv, ProgramOptions &programOptions) {
    cxxopts::Options options("py-gen", "Python binding generator for C++");

    options.add_options()("c,config", "Config file", cxxopts::value<std::string>());
    options.add_options()("h,help",
                          "Use -c <file> to specify a .toml config file, containing sources, compile_args, module_name, output_dir");

    // Allow unmatched arguments to be passed through to clang
    options.allow_unrecognised_options();

    try {
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            llvm::outs() << options.help() << "\n";
            return false;
        }

        if (result.count("config")) {
            programOptions.configFile = result["config"].as<std::string>();
            if (!std::filesystem::exists(programOptions.configFile)) {
                llvm::errs() << "Config file does not exist: <" << programOptions.configFile << "> relative to working dir: <"
                             << std::filesystem::current_path() << ">\n";
                return false;
            }
        }
        return true;
    } catch (const std::exception &e) {
        llvm::errs() << "Error parsing options: " << e.what() << "\n";
        return false;
    }
}

std::optional<toml::table> parseToml(const std::filesystem::path &configFile, ProgramOptions &options) {
    llvm::outs() << "Parsing config file: " << configFile.filename().c_str() << "\n";

    std::optional<toml::table> config = std::nullopt;
    if (!std::filesystem::exists(configFile)) {
        llvm::errs() << "Config file does not exist: <" << configFile << ">\n";
        return config;
    }

    try {
        config = toml::parse_file(configFile.c_str());

        auto &table = *config;
        if (table.contains("compile_commands")) {
            auto path = table["compile_commands"]["path"].value<std::string_view>();
            if (!std::filesystem::exists(*path)) {
                llvm::errs() << "compile_commands.json file does not exist: <" << path << ">\n";
                return std::nullopt;
            }
            llvm::outs() << "Using compile_commands.json: " << path << "\n";
            options.compileCommandsFile = *path;
        }

        if (table.contains("sources")) {
            auto sources = table["sources"].as_array();
            llvm::outs() << "sources:\n";
            for (const auto &source : *sources) {
                if (auto str = source.as_string()) {
                    std::cout << "  " << *str << "\n";
                    options.clangArgs.emplace_back(*str);
                }
            }
        }

        if (table.contains("compile_args")) {
            // Seperate clang args from source files
            options.clangArgs.emplace_back("--");

            auto commands = table["compile_args"].as_array();
            llvm::outs() << "compile_args:\n";
            for (const auto &cmd : *commands) {
                if (auto str = cmd.as_string()) {
                    std::cout << "  " << *str << "\n";
                    options.clangArgs.emplace_back(*str);
                }
            }
        }

        options.moduleName = table["module_name"].value_or(std::string(""));
        llvm::outs() << "Module name: " << options.moduleName << "\n";
        options.outputDir = table["output_dir"].value_or(std::string("."));
        llvm::outs() << "Output directory: " << options.outputDir << "\n";
    } catch (const toml::parse_error &e) {
        llvm::errs() << "toml parse error: " << e.what() << "\n";
        config = std::nullopt;
    }

    return config;
}

int main(int argc, const char **argv) {
    // Parse command line options
    ProgramOptions options;
    if (not processCLIargsIntoProgramOptions(argc, argv, options)) {
        llvm::errs() << "Error parsing command line options\n";
        return 1;
    }

    // Parse config file
    auto config = parseToml(options.configFile, options);

    std::unique_ptr<clang::tooling::CompilationDatabase> database;
    if (config) {
        // Print out the config file contents
        if (!options.compileCommandsFile.empty()) {
            auto path = options.compileCommandsFile;
            if (!std::filesystem::exists(path)) {
                llvm::errs() << "compile_commands.json file does not exist: <" << path << ">\n";
                return 1;
            }
            llvm::outs() << "Using compile_commands.json: " << path << "\n";

            auto absolutePath = std::filesystem::absolute(path);

            // Remove file name from path
            auto parentPath = absolutePath.parent_path();
            llvm::outs() << "Parent path: " << parentPath << "\n";

            std::string error;
            database = clang::tooling::CompilationDatabase::loadFromDirectory(parentPath.c_str(), error);
            if (!database) {
                llvm::errs() << "Error loading compilation database: " << error << "\n";
                return 1;
            }
            auto commands = database->getAllCompileCommands();
            for (const auto &command : commands) {
                llvm::outs() << "File: " << command.Filename << "\n";
                for (const auto &arg : command.CommandLine) {
                    llvm::outs() << "    " << arg << "\n";
                }
            }
        }
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
    } else {
        llvm::outs() << "Parsed arguments: \n";
        for (const auto &arg : clangArgv) {
            llvm::outs() << "    " << arg << "\n";
        }
    }

    if (database) {
        throw std::runtime_error("Not implemented");
        expectedParser->getCompilations() = *database;
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
