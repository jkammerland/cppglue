#pragma once

#include "include_tracker.hpp"
#include "visitor.hpp"

inline void printInfo(const Structs &structs, const Functions &functions, const Headers &headers) {

    for (const auto &header : headers) {
        llvm::outs() << "Header: " << header.name << " system: <" << (header.isSystem ? "yes" : "no") << "> (" << header.fullPath << ")\n";
    }

    for (const auto &structInfo : structs) {
        llvm::outs() << "Struct: "
                     << fmt::format("{0} ({1}) isEnum: {2}", structInfo.name.plain, structInfo.name.qualified, structInfo.isEnum) << "\n";
        for (const auto &info : structInfo.members) {
            llvm::outs() << "    " << fmt::format("{0} {1}", info.type.plain, info.name.plain);
            if (structInfo.isEnum) {
                llvm::outs() << fmt::format(" = {}", info.value);
            }
            llvm::outs() << "\n";
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
                         << fmt::format("{0} ({1}) {2} ({3}), isFunctional:{4}", info.type.plain, info.type.qualified, info.name.plain,
                                        info.name.qualified, info.isFunctional)
                         << "\n";
            // TODO: Fix me with recursive printing
            if (info.isFunctional) {
                for (const auto &fInfo : info.functionals) {
                    llvm::outs() << "    " << "    " << "    "
                                 << fmt::format("    {0} ({1}) {2} ({3})", fInfo.returnType.plain, fInfo.returnType.qualified,
                                                fInfo.name.plain, fInfo.name.qualified)
                                 << "\n";
                    for (const auto &pInfo : fInfo.parameters) {
                        llvm::outs() << "    " << "    " << "    " << "    "
                                     << fmt::format("    {0} ({1}) {2} ({3})", pInfo.type.plain, pInfo.type.qualified, pInfo.name.plain,
                                                    pInfo.name.qualified)
                                     << "\n";
                    }
                }
            }
        }
    }
}