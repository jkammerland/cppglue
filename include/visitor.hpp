#pragma once

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceManager.h>
#include <fmt/format.h>
#include <llvm/Support/raw_ostream.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

struct DeclarationName {
    std::string                plain;
    std::string                qualified;
    std::optional<std::string> namespace_;

    [[nodiscard]] bool hasNamespace() const noexcept { return namespace_.has_value(); }
};

struct StructInfo;
struct FunctionInfo;
struct FieldDeclarationInfo;

struct FieldDeclarationInfo {
    DeclarationName type;
    DeclarationName name;
    int64_t         value{0};
    bool            isConst{false};
    bool            isPointer{false};
    bool            isReference{false};
    bool            isFunctional{false};
    bool            isPublic{false};
    bool            spare1{false};

    std::vector<FunctionInfo> functionals;

    [[nodiscard]] constexpr bool isSpecial() const noexcept { return isConst || isPointer || isReference || isFunctional || spare1; }
};

struct StructInfo {
    DeclarationName                   name;
    bool                              isEnum{false};
    std::vector<FieldDeclarationInfo> members;

    [[nodiscard]] bool   empty() const noexcept { return members.empty(); }
    [[nodiscard]] size_t memberCount() const noexcept { return members.size(); }
};

struct FunctionInfo {
    DeclarationName                   name;
    DeclarationName                   returnType;
    std::optional<std::string>        namespace_;
    bool                              isMemberFunction{false};
    bool                              isPureVirtual{false};
    bool                              isStatic{false};
    std::optional<DeclarationName>    parent;
    std::vector<FieldDeclarationInfo> parameters;

    [[nodiscard]] bool hasParameters() const noexcept { return !parameters.empty(); }
};

using Structs   = std::vector<StructInfo>;
using Functions = std::vector<FunctionInfo>;

/**
 * @brief Callback type for when the visit is complete.
 * The reason for this layer is incase of multiple visits, we can aggregate the results and apply mutexes if needed.
 */
using VisitCompleteCallback = std::function<void(Structs &&, Functions &&)>;

static std::optional<std::string> getNamespaceFromContext(const clang::DeclContext *declContext) {
    if (const auto *namespaceDecl = llvm::dyn_cast<clang::NamespaceDecl>(declContext)) {
        return namespaceDecl->getName().str();
    }
    return std::nullopt;
}

template <typename T> static DeclarationName createDeclarationName(const T *declaration) {
    return DeclarationName{.plain      = declaration->getName().str(),
                           .qualified  = declaration->getQualifiedNameAsString(),
                           .namespace_ = getNamespaceFromContext(declaration->getDeclContext())};
}

static FieldDeclarationInfo createFieldInfo(const clang::QualType &type, const std::string &name, const std::string &qualifiedName) {
    return FieldDeclarationInfo{
        .type        = {.plain = type.getAsString(), .qualified = type.getCanonicalType().getAsString(), .namespace_ = std::nullopt},
        .name        = {.plain = name, .qualified = qualifiedName, .namespace_ = std::nullopt},
        .isConst     = type.isConstQualified(),
        .isPointer   = type->isPointerType(),
        .isReference = type->isReferenceType(),
        .functionals = {}};
}

class Visitor : public clang::RecursiveASTVisitor<Visitor> {
  public:
    explicit Visitor(clang::ASTContext *context, VisitCompleteCallback cb) : context_(context), cb_(std::move(cb)) {}

    /**
     * @brief Filters the qualified name of a given declaration.
     *
     * This function takes a declaration and filters its qualified name to determine
     * if it belongs to non-user code (e.g., standard library, compiler-generated, etc.).
     * It returns a fieldInfo consisting of a boolean indicating whether the declaration is
     * non-user code and a string view of the qualified name.
     *
     * @tparam DeclarationType The type of the declaration.
     * @param declaration Pointer to the declaration to be filtered.
     * @return A fieldInfo where the first element is a boolean
     *         indicating if the declaration is non-user code, and the second element is a
     *         string view of the qualified name.
     * @note The qualified name is stored in the stringBuffer_ member variable, and must be consumed before next visit.
     */
    template <typename DeclarationType> auto FilterQualifiedName(const DeclarationType *declaration) {
        auto stringStream_ = getNewStream();
        declaration->printQualifiedName(stringStream_);
        auto &qName = stringBuffer_;

        auto isNonUserCode = qName.starts_with("std") || qName.starts_with("__") || declaration->isImplicit() ||
                             !declaration->isFirstDecl() || declaration->getLocation().isInvalid() || qName.empty() ||
                             context_->getSourceManager().isInSystemHeader(declaration->getLocation());
        return std::make_pair(isNonUserCode, std::string_view{qName});
    }

    inline auto getNewStream() -> llvm::raw_string_ostream {
        stringBuffer_.clear();
        return llvm::raw_string_ostream{stringBuffer_};
    }

    bool VisitCXXRecordDecl(clang::CXXRecordDecl *declaration) {
        auto [isNonUserCode, qName] = FilterQualifiedName(declaration);
        if (isNonUserCode) {
            return true;
        }

        StructInfo info;
        info.name = createDeclarationName(declaration);

        for (const auto *field : declaration->fields()) {
            auto fieldInfo     = createFieldInfo(field->getType(), field->getName().str(), field->getQualifiedNameAsString());
            fieldInfo.isPublic = field->getAccess() == clang::AccessSpecifier::AS_public;
            info.members.emplace_back(fieldInfo);
        }
        structs_.push_back(info);
        return true;
    }

    bool VisitLambdaExpr(clang::LambdaExpr *lambda) {
        auto [isNonUserCode, qName] = FilterQualifiedName(lambda->getCallOperator());
        if (isNonUserCode) {
            return true;
        }
        // llvm::outs() << "Lambda: " << qName << " in VisitLambdaExpr, returning immidiately\n";
        return true;
    }

    bool VisitEnumDecl(clang::EnumDecl *declaration) {
        auto [isNonUserCode, qName] = FilterQualifiedName(declaration);
        if (isNonUserCode) {
            return true;
        }

        StructInfo info;
        info.isEnum         = true;
        info.name.plain     = declaration->getName();
        info.name.qualified = declaration->getQualifiedNameAsString();

        for (const auto *enumerator : declaration->enumerators()) {
            FieldDeclarationInfo fieldInfo;
            fieldInfo.type.plain     = declaration->getIntegerType().getAsString();
            fieldInfo.type.qualified = declaration->getIntegerType().getCanonicalType().getAsString();
            fieldInfo.name.plain     = enumerator->getName();
            fieldInfo.name.qualified = enumerator->getQualifiedNameAsString();
            fieldInfo.value          = enumerator->getInitVal().getExtValue();
            info.members.emplace_back(fieldInfo);
        }

        structs_.push_back(info);

        return true;
    }

    bool VisitFunctionDecl(clang::FunctionDecl *declaration) {
        auto [isNonUserCode, qName] = FilterQualifiedName(declaration);
        if (isNonUserCode) {
            return true;
        }

        FunctionInfo info;
        info.name       = createDeclarationName(declaration);
        info.namespace_ = getNamespaceFromContext(declaration->getDeclContext());

        info.returnType = {.plain      = declaration->getReturnType().getAsString(),
                           .qualified  = declaration->getReturnType().getCanonicalType().getAsString(),
                           .namespace_ = std::nullopt};

        if (auto *method = llvm::dyn_cast<clang::CXXMethodDecl>(declaration)) {
            info.isMemberFunction = true;
            info.parent           = createDeclarationName(method->getParent());
        } else {
            info.parent           = std::nullopt;
            info.isMemberFunction = false;
        }

        info.isPureVirtual = declaration->isPureVirtual();
        info.isStatic      = declaration->isStatic();

        for (const auto *param : declaration->parameters()) {
            FieldDeclarationInfo fieldInfo;

            using namespace clang;
            QualType paramType = param->getType();

            // Get the CXXRecordDecl if this is a class/struct type
            if (const CXXRecordDecl *recordDecl = paramType->getAsCXXRecordDecl()) {
                // Check if this is a template specialization
                if (const auto *templateDecl = dyn_cast<ClassTemplateSpecializationDecl>(recordDecl)) {

                    // Check if this is std::function
                    if (templateDecl->getIdentifier()->getName() == "function") {
                        // Get the template arguments
                        const TemplateArgumentList &args = templateDecl->getTemplateArgs();
                        if (args.size() > 0) {
                            // The first argument should be the function type
                            if (args[0].getKind() == TemplateArgument::Type) {
                                QualType functionType = args[0].getAsType();

                                // Get the actual function type
                                if (const auto *protoType = functionType->getAs<FunctionProtoType>()) {
                                    // Get return type
                                    QualType returnType = protoType->getReturnType();

                                    // TODO: fix dirty hack cleaning the type names
                                    // Remove 'struct' or 'class' prefixes if present
                                    auto cleanTypeName = [](std::string &type) {
                                        const std::array<std::string, 2> prefixes = {"struct ", "class "};
                                        for (const auto &prefix : prefixes) {
                                            if (type.substr(0, prefix.length()) == prefix) {
                                                type = type.substr(prefix.length());
                                            }
                                        }
                                    };

                                    FunctionInfo functionalInfo;
                                    functionalInfo.returnType.plain     = returnType.getAsString();
                                    functionalInfo.returnType.qualified = returnType.getCanonicalType().getAsString();

                                    cleanTypeName(functionalInfo.returnType.plain);
                                    cleanTypeName(functionalInfo.returnType.qualified);

                                    // Get parameter types
                                    for (const QualType &argType : protoType->getParamTypes()) {
                                        FieldDeclarationInfo paramInfo;
                                        paramInfo.type.plain     = argType.getAsString();
                                        paramInfo.type.qualified = argType.getCanonicalType().getAsString();

                                        cleanTypeName(paramInfo.type.plain);
                                        cleanTypeName(paramInfo.type.qualified);

                                        paramInfo.isConst     = argType.isConstQualified();
                                        paramInfo.isPointer   = argType->isPointerType();
                                        paramInfo.isReference = argType->isReferenceType();

                                        functionalInfo.parameters.emplace_back(paramInfo);
                                    }

                                    // Store the functional info
                                    fieldInfo.isFunctional = true;
                                    fieldInfo.functionals.emplace_back(functionalInfo);
                                }
                            }
                        }
                    }
                }
            }

            fieldInfo.type.plain     = param->getType().getAsString();
            fieldInfo.type.qualified = param->getType().getCanonicalType().getAsString();
            fieldInfo.name.plain     = param->getName();
            fieldInfo.name.qualified = param->getQualifiedNameAsString();
            info.parameters.emplace_back(fieldInfo);
        }
        functions_.push_back(info);

        return true;
    }

    ~Visitor() { cb_(std::move(structs_), std::move(functions_)); }

  private:
    clang::ASTContext    *context_;
    VisitCompleteCallback cb_;
    std::string           stringBuffer_;

    std::vector<StructInfo>   structs_;
    std::vector<FunctionInfo> functions_;
};
