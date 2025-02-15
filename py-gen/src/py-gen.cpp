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
    out << "\n// User headers" << (userHeaders.empty() ? " - [none found] \n" : "\n");
    for (const auto &header : userHeaders) {
        out << "#include \"" << header << "\"\n";
    }

    out << "\n// System headers" << (systemHeaders.empty() ? " - [none found] \n" : "\n");
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

bool shouldWriteFile(const std::filesystem::path &path, const std::string &newContent) {
    if (!std::filesystem::exists(path)) {
        return true;
    }

    std::ifstream file(path);
    if (!file) {
        return true;
    }

    std::string existingContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return existingContent != newContent;
}

void writeFileIfDifferent(const std::filesystem::path &path, const std::string &content) {
    if (shouldWriteFile(path, content)) {
        std::ofstream out(path);
        if (!out) {
            throw std::runtime_error("Failed to open file for writing: " + path.string());
        }
        out << content;
        std::cout << "Generated: " << path << '\n';
    } else {
        std::cout << "Skipped unchanged file: " << path << '\n';
    }
}

std::string generateCMakeLists(const std::string &moduleName, const Headers &headers) {
    std::string templ = readTemplate("CMakeLists.txt.template");

    // Generate header fileset section
    std::stringstream headerFiles;
    headerFiles << "# Direct header dependencies (that you must resolve!):\n";
    for (const auto &header : headers) {
        if (!header.isSystem) {
            headerFiles << "# " << header.fullPath << "\n";
        }
    }
    headerFiles << "\n";

    // Replace placeholders
    size_t            pos;
    const std::string placeholder = "{module_name}";
    while ((pos = templ.find(placeholder)) != std::string::npos) {
        templ.replace(pos, placeholder.length(), moduleName);
    }

    // Insert header files section after the project() line
    if ((pos = templ.find("project(")) != std::string::npos) {
        pos = templ.find('\n', pos) + 1;
        templ.insert(pos, headerFiles.str());
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

std::string generateSetupPy(const std::string &moduleName) {
    std::string       templ = readTemplate("setup.py.template");
    size_t            pos;
    const std::string placeholder = "{module_name}";
    while ((pos = templ.find(placeholder)) != std::string::npos) {
        templ.replace(pos, placeholder.length(), moduleName);
    }
    return templ;
}

std::string generatePyprojectToml(const std::string &moduleName) {
    std::string       templ = readTemplate("pyproject.toml.template");
    size_t            pos;
    const std::string placeholder = "{module_name}";
    while ((pos = templ.find(placeholder)) != std::string::npos) {
        templ.replace(pos, placeholder.length(), moduleName);
    }
    return templ;
}
} // namespace

void generateBindings(const Structs &structs, const Functions &functions, const Headers &headers, const std::string &moduleName,
                      const std::filesystem::path &outputDir) {
    // Create output directory if it doesn't exist
    createDirectory(outputDir);

    // Generate the bindings file
    auto              bindingsPath = outputDir / (moduleName + ".cpp");
    std::stringstream bindingsContent;
    generateBindings(structs, functions, headers, moduleName, bindingsContent);
    writeFileIfDifferent(bindingsPath, bindingsContent.str());

    // Generate CMakeLists.txt
    auto cmakePath    = outputDir / "CMakeLists.txt";
    auto cmakeContent = generateCMakeLists(moduleName, headers);
    writeFileIfDifferent(cmakePath, cmakeContent);

    // Generate CPM.cmake
    auto cpmPath    = outputDir / "CPM.cmake";
    auto cpmContent = generateCPM("0.40.5");
    writeFileIfDifferent(cpmPath, cpmContent);

    // Generate package directory
    createDirectory(outputDir / moduleName);

    // Generate Python packaging files
    auto setupPath    = outputDir / moduleName / "setup.py";
    auto setupContent = generateSetupPy(moduleName);
    writeFileIfDifferent(setupPath, setupContent);

    auto pyprojectPath    = outputDir / moduleName / "pyproject.toml";
    auto pyprojectContent = generatePyprojectToml(moduleName);
    writeFileIfDifferent(pyprojectPath, pyprojectContent);

    // Create package directory and __init__.py
    auto packageDir = outputDir / moduleName / moduleName;
    createDirectory(packageDir);
    auto initPath = packageDir / "__init__.py";
    writeFileIfDifferent(initPath, "from ." + moduleName + " import *");

    std::cout << "Generated files in: " << outputDir << '\n';
}
