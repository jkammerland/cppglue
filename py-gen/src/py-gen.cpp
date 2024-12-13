#include "py-gen.h"

#include <fstream>

void generateBindings(const Structs &structs, const Functions &functions, const Headers &headers, const std::string &moduleName) {
    std::ofstream out(moduleName + ".cpp");

    // Write headers - TODO: be smart here, only include what is needed
    out << "#include <pybind11/pybind11.h>\n"
        << "#include <pybind11/stl.h>\n"
        << "#include <pybind11/complex.h>\n"
        << "#include <pybind11/functional.h>\n";

    // Extract all unique headers
    std::set<std::string> userHeaders;
    std::set<std::string> systemHeaders;
    for (const auto &header : headers) {
        if (!header.isSystem) {
            userHeaders.insert(header.name);

        } else {
            systemHeaders.insert(header.name);
        }
    }

    // Write user and system headers
    if (!userHeaders.empty()) {
        out << "\n// User headers\n";
    } else {
        out << "\n// User headers - [none found] \n";
    }
    for (const auto &header : userHeaders) {
        out << "#include \"" << header << "\"\n";
    }

    if (!systemHeaders.empty()) {
        out << "\n// System headers\n";
    } else {
        out << "\n// System headers - [none found] \n";
    }
    for (const auto &header : systemHeaders) {
        out << "// #include <" << header << ">\n";
    }

    out << "\nnamespace py = pybind11;\n\n";
    out << "PYBIND11_MODULE(" << moduleName << ", m) {\n";

    // Helper function to get fully qualified name
    auto getFullName = [](const StructInfo &s) { return s.name.qualified.empty() ? s.name.plain : s.name.qualified; };

    // First, declare all enums and classes
    for (const auto &structInfo : structs) {
        if (structInfo.isEnum) {
            out << fmt::format("    py::enum_<{0}>(m, \"{1}\", py::arithmetic())\n", getFullName(structInfo), structInfo.name.plain);
            for (const auto &member : structInfo.members) {
                out << fmt::format("        .value(\"{0}\", {1}::{0})\n", member.name.plain, getFullName(structInfo));
            }
            out << "        .export_values();\n\n";
        } else {
            out << fmt::format("    py::class_<{0}> {1}(m, \"{2}\");\n", getFullName(structInfo), structInfo.name.plain + "_class",
                               structInfo.name.plain);
        }
    }

    // Then, define the actual bindings for non-enum classes
    for (const auto &structInfo : structs) {
        if (structInfo.isEnum)
            continue;

        std::string className = structInfo.name.plain + "_class";
        std::string fullName  = getFullName(structInfo);

        // Main class definition
        out << fmt::format("    {0}\n", className) << "        .def(py::init<>())\n";

        // Add members
        for (const auto &member : structInfo.members) {
            out << fmt::format("        .def_readwrite(\"{0}\", &{1}::{0})\n", member.name.plain, fullName);
        }

        // Add member functions
        for (const auto &funcInfo : functions) {
            if (funcInfo.parent.has_value() && funcInfo.parent->qualified == fullName) {
                out << fmt::format("        .def(\"{0}\", &{1}::{0})\n", funcInfo.name.plain, fullName);
            }
        }
        // Remove last newline and add semicolon
        out.seekp(-1, std::ios_base::end);
        out << ";\n\n";
    }

    // Generate free function bindings
    for (const auto &funcInfo : functions) {
        if (funcInfo.isMemberFunction)
            continue;

        out << fmt::format("    m.def(\"{0}\", &{1});\n", funcInfo.name.plain,
                           funcInfo.name.qualified.empty() ? funcInfo.name.plain : funcInfo.name.qualified);
    }

    out << "}\n";
}
