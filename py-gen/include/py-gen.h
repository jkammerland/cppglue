#include "ast_actions.hpp"
#include "include_tracker.hpp"
#include "visitor.hpp"

#include <filesystem>

/**
 * @brief Generates Python bindings for C++ code
 *
 * @param structs Collection of struct/class definitions to generate bindings for
 * @param functions Collection of functions to generate bindings for
 * @param headers Collection of headers used by the code
 * @param moduleName Name of the Python module to generate
 * @param out Output stream to write the bindings to
 */
void generateBindings(const Structs &structs, const Functions &functions, const Headers &headers, const std::string &moduleName,
                      std::ostream &out);

/**
 * @brief Generates Python bindings for C++ code and writes to a directory
 *
 * Creates a new directory named <moduleName>_bindings containing:
 * - <moduleName>.cpp: The generated bindings code
 * - CMakeLists.txt: A CMake build configuration for the module
 *
 * @param structs Collection of struct/class definitions to generate bindings for
 * @param functions Collection of functions to generate bindings for
 * @param headers Collection of headers used by the code
 * @param moduleName Name of the Python module to generate
 * @throws std::runtime_error if file operations fail
 */
void generateBindings(const Structs &structs, const Functions &functions, const Headers &headers, const std::string &moduleName,
                      const std::filesystem::path &outputDir);
