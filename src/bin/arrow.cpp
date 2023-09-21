#include "entry.h"
#include <clang/AST/Decl.h>
#include <fmt/core.h>
#include <functional>
#include <llvm/ADT/StringRef.h>

struct GenState {
  constexpr static auto kPrelude = R"(
#include <arrow/api.h>

template <typename T> struct Generator {
  using value_type = T;
  using gen_type = T *;
  virtual gen_type next() { return nullptr; }
};

// template <typename C> using _ACT = arrow::CTypeTraits<C>;
)";

  GenState(StringRef file) : File(file) {}

  StringRef File;
  std::list<std::string> Header, Footer;
  std::list<std::string> BuilderDecls, SchemaFieldDecls, ColsDecl;

  std::map<std::string, std::string, std::less<>> RenameMap;

  std::string getMappedName(StringRef fieldName) {
    if (RenameMap.contains(fieldName.data())) {
      return RenameMap[fieldName.data()];
    } else {
      return fieldName.str();
    }
  }

  void visitField(const FieldDecl *fieldDecl, StringRef recName) {
    if (fieldDecl->getType()->isRecordType()) {
      auto SubRec = fieldDecl->getType()->getAsCXXRecordDecl();
      for (auto Field : SubRec->fields()) {
        visitField(Field, SubRec->getName());
      }
    }

    auto FDeclName = fieldDecl->getName();

    if (fieldDecl->getType()->isArrayType() && fieldDecl->getType()
                                                   ->getAsArrayTypeUnsafe()
                                                   ->getElementType()
                                                   ->isAnyCharacterType()) {
      BuilderDecls.push_back(
          fmt::format("// arrow::CTypeTraits<std::add_const_t<"
                      "std::decay_t<decltype({0}::{1})>>>::"
                      "BuilderType _{1}_b;\n arrow::CTypeTraits<const "
                      "char*>::BuilderType _{1}_b;",
                      recName.data(), FDeclName.data()));

    } else {
      BuilderDecls.push_back(fmt::format(
          "arrow::CTypeTraits<decltype({0}::{1})>::BuilderType _{1}_b;",
          recName.data(), FDeclName.data()));
    }

    BuilderDecls.push_back(fmt::format(R"(
{{
std::vector<decltype({0}::{1})*> _map;
std::transform(_obj.begin(), _obj.end(), _map.begin(),
                 [](auto o) {{ return &(o->{1}); }});
for (auto _p : _map) {{
  _{1}_b.Append(*_p).ok();
}}
}}
auto _{1}_f = _{1}_b.Finish().ValueOrDie();
)",
                                       recName.data(), FDeclName.data()));

    SchemaFieldDecls.push_back(
        fmt::format(" arrow::field(\"{0}\", _{1}_f->type()),",
                    getMappedName(FDeclName), FDeclName.data()));

    ColsDecl.push_back(fmt::format("_{0}_f", FDeclName.data()));
  }

  void visitCxxRecord(const CXXRecordDecl *recDef) {
    auto TypeName = recDef->getName();

    Header.push_back(fmt::format("#include \"{0}\"\n", File.data()));
    Header.push_back(kPrelude);
    Header.push_back(fmt::format(R"(
auto __arrowgen(Generator<{0}> _g) {{

std::vector<{0}*> _obj = {{}};
for (auto *_p = _g.next(); _p != nullptr; _p = _g.next()) {{
  _obj.push_back(_p);
}}

)",
                                 TypeName.data()));

    for (auto Base : recDef->bases()) {
      auto BaseDecl = Base.getType()->getAsCXXRecordDecl();
      if (BaseDecl != nullptr) {
        for (auto *Field : BaseDecl->fields()) {
          visitField(Field, TypeName);
        }
      }
    }
    for (auto *Field : recDef->fields()) {
      visitField(Field, TypeName);
    }

    SchemaFieldDecls.push_front("auto _s = arrow::schema({{");
    SchemaFieldDecls.push_back("}});");

    auto end = std::string(R"(std::vector _c = {)");
    for (auto &decl : ColsDecl) {
      end.append(decl);
      end.append(", ");
    }

    end.append("}; \n return arrow::RecordBatch::Make(_s, _c.size(), _c); \n}");
    Footer.push_back(std::move(end));
  }

  void showGen() {
    fmt::println("");
    for (auto &s : Header) {
      fmt::println("{}", s);
    }

    fmt::println("");
    for (auto &s : BuilderDecls) {
      fmt::println("{}", s);
    }

    fmt::println("");
    for (auto &s : SchemaFieldDecls) {
      fmt::println("{}", s);
    }

    fmt::println("");
    for (auto &s : Footer) {
      fmt::println("{}", s);
    }
  }

  ~GenState() { showGen(); }
};

struct ArrowGen : MatchFinder::MatchCallback {
  ArrowGen(StringRef file) : File(file) {}

  StringRef File;

  void run(const MatchFinder::MatchResult &Res) override {
    auto RecDef = Res.Nodes.getNodeAs<CXXRecordDecl>("cxxdef");
    GenState GS(File);
    GS.visitCxxRecord(RecDef);
  }
};

struct ArrowGenPass : ASTConsumer {
  ArrowGenPass(StringRef Target, StringRef File) : Generator(File) {
    DeclarationMatcher RecordDeclMatcher =
        cxxRecordDecl(isDefinition(), hasName(Target)).bind("cxxdef");

    Finder.addMatcher(RecordDeclMatcher, &Displayer);
    Finder.addMatcher(RecordDeclMatcher, &Generator);
  }
  void HandleTranslationUnit(ASTContext &Ctx) override { Finder.matchAST(Ctx); }

private:
  MatchFinder Finder;
  DisplayMatchee Displayer;
  ArrowGen Generator;
};

int main(int argc, char *argv[]) { return entry<ArrowGenPass>(argc, argv); }
