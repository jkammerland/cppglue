#include "py-gen.h"

#include <fstream>
#include <iostream>

void generateBindings(const Structs &structs, const Functions &functions, const Headers &headers, const std::string &moduleName,
                      std::ostream &out) {

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
        if (structInfo.isEnum) {
            continue; // Already handled
        }

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
        if (funcInfo.isMemberFunction) {
            continue; // Already handled
        }

        out << fmt::format("    m.def(\"{0}\", &{1});\n", funcInfo.name.plain,
                           funcInfo.name.qualified.empty() ? funcInfo.name.plain : funcInfo.name.qualified);
    }

    out << "}\n";
}

namespace {
std::string readTemplate(const std::string &templateName) {
    // Template file is installed alongside the executable
    std::filesystem::path exePath      = std::filesystem::canonical("/proc/self/exe");
    auto                  templatePath = exePath.parent_path() / "templates" / templateName;

    std::ifstream file(templatePath);
    if (!file) {
        throw std::runtime_error("Failed to open template file: " + templatePath.string());
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void createDirectory(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) {
        if (!std::filesystem::create_directories(path)) {
            throw std::runtime_error("Failed to create directory: " + path.string());
        }
        std::cout << "Created directory: " << path << '\n';
    }
}

std::string generateCMakeLists(const std::string &moduleName) {
    std::string templ = readTemplate("CMakeLists.txt.template");
    // Replace placeholders
    size_t            pos;
    const std::string placeholder = "{module_name}";
    while ((pos = templ.find(placeholder)) != std::string::npos) {
        templ.replace(pos, placeholder.length(), moduleName);
    }
    return templ;
}

std::string generateCPM(const std::string &version) {
    std::string templ = readTemplate("CPM.cmake.template");
    // Replace placeholders
    size_t            pos;
    const std::string placeholder = "{version}";
    while ((pos = templ.find(placeholder)) != std::string::npos) {
        templ.replace(pos, placeholder.length(), version);
    }
    return templ;
}
} // namespace

void generateBindings(const Structs &structs, const Functions &functions, const Headers &headers, const std::string &moduleName,
                      const std::filesystem::path &outputDir) {
    // Create output directory if it doesn't exist
    createDirectory(outputDir);

    // Generate the bindings file
    auto          bindingsPath = outputDir / (moduleName + ".cpp");
    std::ofstream out(bindingsPath);
    if (!out) {
        throw std::runtime_error("Failed to open output file: " + bindingsPath.string());
    }
    generateBindings(structs, functions, headers, moduleName, out);

    // Generate CMakeLists.txt
    auto          cmakePath = outputDir / "CMakeLists.txt";
    std::ofstream cmake(cmakePath);
    if (!cmake) {
        throw std::runtime_error("Failed to create CMakeLists.txt: " + cmakePath.string());
    }
    cmake << generateCMakeLists(moduleName);

    // Generate CPM.cmake
    auto          cpmPath = outputDir / "CPM.cmake";
    std::ofstream cpm(cpmPath);
    if (!cpm) {
        throw std::runtime_error("Failed to create CPM.cmake: " + cpmPath.string());
    }
    cpm << generateCPM("0.40.5");

    std::cout << "Generated files in: " << outputDir << '\n';
}
