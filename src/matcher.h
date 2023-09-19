#pragma once

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include "fmt/format.h"

#include <fstream>

using namespace clang;
using namespace clang::ast_matchers;

struct OnMatchCXXRecord : MatchFinder::MatchCallback {
  void run(const MatchFinder::MatchResult &Res) override {
    auto RecDef = Res.Nodes.getNodeAs<CXXRecordDecl>("cxxdef");
    auto RecDeclName = RecDef->getDeclName().getAsString();
    auto RecType =
        RecDef->getASTContext().getTypeInfo(RecDef->getTypeForDecl());

    fmt::println("Catch CXXRecordDeclare: {}, size: {}", //
                 RecDeclName, RecType.Width / 8);

    for (auto Field : RecDef->fields()) {
      auto DeclName = Field->getDeclName().getAsString();
      auto FieldType = Field->getType();
      auto TypeName = Field->getType().getAsString();
      auto FieldTypeInfo = RecDef->getASTContext().getTypeInfo(FieldType);

      fmt::println(" - Field: {}, type: {:8}, byte size: {:3}", //
                   DeclName, TypeName, FieldTypeInfo.Width / 8);
    }
  }
};

struct AnaDeclConsumer : ASTConsumer {
  AnaDeclConsumer(StringRef Target) {
    DeclarationMatcher RecordDeclMatcher =
        cxxRecordDecl(isDefinition(), hasName(Target)).bind("cxxdef");

    Finder.addMatcher(RecordDeclMatcher, &RecordCallback);
  }
  void HandleTranslationUnit(ASTContext &Ctx) override { Finder.matchAST(Ctx); }

private:
  MatchFinder Finder;
  OnMatchCXXRecord RecordCallback;
};

class DeclFindingAction : public clang::ASTFrontendAction {
public:
  DeclFindingAction(std::string Target) : TargetClassName(std::move(Target)) {}

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, clang::StringRef) final {
    return std::unique_ptr<clang::ASTConsumer>(
        new AnaDeclConsumer(TargetClassName));
  }

private:
  std::string TargetClassName;
};
