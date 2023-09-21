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

template <typename SubProcess>
class DeclFindingAction : public clang::ASTFrontendAction {
public:
  DeclFindingAction(std::string target, std::string file)
      : TargetClassName(std::move(target)), FileName(std::move(file)) {}

  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, clang::StringRef _) final {
    return std::make_unique<SubProcess>(TargetClassName, FileName);
  }

private:
  std::string TargetClassName;
  std::string FileName;
};

void traceRecords(const CXXRecordDecl *root,
                  std::vector<const CXXRecordDecl *> &decls);