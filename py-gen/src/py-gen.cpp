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
                // Build parameter documentation string
                std::string params;
                if (funcInfo.hasParameters()) {
                    for (const auto &param : funcInfo.parameters) {
                        if (!params.empty())
                            params += ", ";
                        params += fmt::format("{}: {}", param.name.plain, param.type.plain);
                    }
                }

                // Add function with documentation
                out << fmt::format("        .def(\"{}\", &{}::{}", funcInfo.name.plain, fullName, funcInfo.name.plain);

                // Add parameter names if present
                if (funcInfo.hasParameters()) {
                    out << ", ";
                    bool first = true;
                    for (const auto &param : funcInfo.parameters) {
                        if (!first)
                            out << ", ";
                        out << "py::arg(\"" << param.name.plain << "\")";
                        first = false;
                    }
                }

                // Add docstring with type information
                out << fmt::format(", \"{}({}){}\"{}", funcInfo.name.plain, params,
                                   funcInfo.returnType.plain.empty() ? "" : " -> " + funcInfo.returnType.plain,
                                   funcInfo.isPureVirtual ? ", py::is_method()" : "");

                out << ")\n";
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

        // Build parameter documentation string
        std::string params;
        if (funcInfo.hasParameters()) {
            for (const auto &param : funcInfo.parameters) {
                if (!params.empty())
                    params += ", ";
                params += fmt::format("{}: {}", param.name.plain, param.type.plain);
            }
        }

        out << fmt::format("    m.def(\"{}\", &{}", funcInfo.name.plain,
                           funcInfo.name.qualified.empty() ? funcInfo.name.plain : funcInfo.name.qualified);

        // Add parameter names if present
        if (funcInfo.hasParameters()) {
            out << ", ";
            bool first = true;
            for (const auto &param : funcInfo.parameters) {
                if (!first)
                    out << ", ";
                out << "py::arg(\"" << param.name.plain << "\")";
                first = false;
            }
        }

        // Add docstring with type information
        out << fmt::format(", \"{}({}){}\");\n", funcInfo.name.plain, params,
                           funcInfo.returnType.plain.empty() ? "" : " -> " + funcInfo.returnType.plain);
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

std::string toPythonType(const std::string &cppType) {
    // Handle complex types
    if (cppType == "std::complex<double>" || cppType == "std::complex<float>")
        return "Complex";
    if (cppType == "Complex")
        return "Complex";

    // Handle basic types
    if (cppType == "int" || cppType == "long")
        return "int";
    if (cppType == "float" || cppType == "double")
        return "float";
    if (cppType == "bool")
        return "bool";
    if (cppType == "std::string")
        return "str";

    // Handle templated types
    if (cppType.starts_with("std::vector<")) {
        std::string innerType = cppType.substr(12, cppType.length() - 13);
        return "List[" + toPythonType(innerType) + "]";
    }
    if (cppType.starts_with("std::optional<")) {
        std::string innerType = cppType.substr(14, cppType.length() - 15);
        return "Optional[" + toPythonType(innerType) + "]";
    }
    if (cppType.starts_with("std::function<")) {
        std::string signature = cppType.substr(14, cppType.length() - 15);
        size_t returnPos = signature.find("(");
        std::string returnType = signature.substr(0, returnPos);
        std::string args = signature.substr(returnPos + 1, signature.length() - returnPos - 2);
        
        // Parse argument types
        std::vector<std::string> argTypes;
        size_t start = 0;
        int bracketCount = 0;
        std::string currentArg;
        
        for (size_t i = 0; i < args.length(); ++i) {
            if (args[i] == '<') bracketCount++;
            else if (args[i] == '>') bracketCount--;
            else if (args[i] == ',' && bracketCount == 0) {
                if (!currentArg.empty()) {
                    argTypes.push_back(currentArg);
                    currentArg.clear();
                }
                continue;
            }
            
            if (!std::isspace(args[i])) {
                currentArg += args[i];
            }
        }
        if (!currentArg.empty()) {
            argTypes.push_back(currentArg);
        }

        // Build the Callable type
        std::string callableType = "Callable[[";
        for (size_t i = 0; i < argTypes.size(); ++i) {
            if (i > 0) callableType += ", ";
            callableType += toPythonType(argTypes[i]);
        }
        callableType += "], " + toPythonType(returnType) + "]";
        
        return callableType;
    }

    // Handle scoped types (remove namespace qualifiers)
    size_t scopePos = cppType.find("::");
    if (scopePos != std::string::npos) {
        return cppType.substr(scopePos + 2);
    }

    // For custom types, use the type as-is
    return cppType;
}

std::string generatePyi(const Structs &structs, const Functions &functions) {
    std::stringstream out;

    // Add common imports
    out << "from typing import Optional, Callable, List, Dict, Set, Tuple, Union, overload\n"
        << "from typing import TypeVar, Generic, Complex\n" // Added Complex import
        << "from enum import Enum\n"                        // Added Enum import
        << "import numpy.typing as npt\n"
        << "import numpy as np\n\n";

    // Generate enum definitions
    for (const auto &structInfo : structs) {
        if (structInfo.isEnum) {
            out << "class " << structInfo.name.plain << "(Enum):\n"; // Make it inherit from Enum
            for (const auto &member : structInfo.members) {
                out << "    " << member.name.plain << " = " // Add value assignment
                    << member.value << "\n";                // Assuming you have value information in your Member struct
            }
            out << "\n";
        }
    }

    // First generate forward declarations for all classes
    for (const auto &structInfo : structs) {
        if (!structInfo.isEnum) {
            out << "class " << structInfo.name.plain << ":\n    ...\n\n";
        }
    }

    // Then generate full class definitions
    for (const auto &structInfo : structs) {
        if (!structInfo.isEnum) {
            out << "class " << structInfo.name.plain << ":\n";
            out << "    def __init__(self) -> None: ...\n\n";

            // Properties
            for (const auto &member : structInfo.members) {
                out << "    " << member.name.plain << ": " << toPythonType(member.type.plain) << "\n";
            }

            // Member functions
            for (const auto &funcInfo : functions) {
                if (funcInfo.parent.has_value() && funcInfo.parent->qualified == structInfo.name.qualified) {
                    out << "    def " << funcInfo.name.plain << "(self";
                    if (funcInfo.hasParameters()) {
                        for (const auto &param : funcInfo.parameters) {
                            out << ", " << param.name.plain << ": " << toPythonType(param.type.plain);
                        }
                    }
                    out << ") -> " << (funcInfo.returnType.plain.empty() ? "None" : toPythonType(funcInfo.returnType.plain)) << ": ...\n";
                }
            }
            out << "\n";
        }
    }

    // Rest of the function remains the same...
    return out.str();
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
    // Generate __init__.py that properly imports and re-exports symbols
    std::stringstream initContent;
    initContent << "from ." << moduleName << " import *  # type: ignore\n\n"
                << "# Re-export all symbols defined in the .pyi stub file\n"
                << "__all__ = []  # Will be populated by type hints from .pyi\n";
    writeFileIfDifferent(initPath, initContent.str());

    // Generate .pyi stub file
    auto pyiPath    = packageDir / (moduleName + ".pyi");
    auto pyiContent = generatePyi(structs, functions);
    writeFileIfDifferent(pyiPath, pyiContent);

    std::cout << "Generated files in: " << outputDir << '\n';
}
