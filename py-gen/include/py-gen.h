#include "ast_actions.hpp"
#include "include_tracker.hpp"
#include "visitor.hpp"

/**
 * @brief Generates Python bindings for C++ code
 * 
 * @param structs Collection of struct/class definitions to generate bindings for
 * @param functions Collection of functions to generate bindings for
 * @param headers Collection of headers used by the code
 * @param moduleName Name of the Python module to generate
 * @param out Output stream to write the bindings to
 */
void generateBindings(const Structs &structs, 
                     const Functions &functions, 
                     const Headers &headers, 
                     const std::string &moduleName,
                     std::ostream &out);

/**
 * @brief Generates Python bindings for C++ code and writes to a file
 * 
 * @param structs Collection of struct/class definitions to generate bindings for
 * @param functions Collection of functions to generate bindings for
 * @param headers Collection of headers used by the code
 * @param moduleName Name of the Python module to generate
 */
void generateBindings(const Structs &structs,
                     const Functions &functions,
                     const Headers &headers,
                     const std::string &moduleName);
