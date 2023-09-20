#pragma once

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

#include "fmt/format.h"

using namespace clang;
using namespace clang::ast_matchers;

struct DisplayMatchee : MatchFinder::MatchCallback {
  void run(const MatchFinder::MatchResult &Res) override;
};

struct PrintCodeGen : MatchFinder::MatchCallback {
  PrintCodeGen(StringRef FName) : FileName(FName) {}

  StringRef FileName;
  std::vector<const CXXRecordDecl *> RecDecls;

  void run(const MatchFinder::MatchResult &Res) override;
};

struct AnaDeclConsumer : ASTConsumer {
  AnaDeclConsumer(StringRef Target, StringRef File) : DisplayGen(File) {
    DeclarationMatcher RecordDeclMatcher =
        cxxRecordDecl(isDefinition(), hasName(Target)).bind("cxxdef");

    Finder.addMatcher(RecordDeclMatcher, &Displayer);
    Finder.addMatcher(RecordDeclMatcher, &DisplayGen);
  }
  void HandleTranslationUnit(ASTContext &Ctx) override { Finder.matchAST(Ctx); }

private:
  MatchFinder Finder;
  DisplayMatchee Displayer;
  PrintCodeGen DisplayGen;
};

class DeclFindingAction : public clang::ASTFrontendAction {
public:
  DeclFindingAction(std::string Target, std::string File)
      : TargetClassName(std::move(Target)), FileName(std::move(File)) {}

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, clang::StringRef) final {
    return std::make_unique<AnaDeclConsumer>(TargetClassName, FileName);
  }

private:
  std::string TargetClassName;
  std::string FileName;
};
