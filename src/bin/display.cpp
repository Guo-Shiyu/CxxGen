#include "entry.h"

struct GenState {
  constexpr static auto kFixedHeader = R"(
template<typename T> inline void __display(T t) {}

template <typename T> struct Generator {
  using value_type = T;
  using gen_type = T *;
  virtual gen_type next() { return nullptr; }
};

template<typename T> inline
void __display_all(Generator<T> s) {
  for (T* _p = s.next(); _p != nullptr; _p = s.next()) {
    __display<T>(_p);
  }
}

)";

  GenState(StringRef fileName) {
    FwdDecls.push_back(fmt::format("#include \"{}\" \n#include <iostream> \n",
                                   fileName.data()));
    FwdDecls.push_back(kFixedHeader);
  }

  std::list<std::string> FwdDecls, FieldDisplayers;

  void visitField(const FieldDecl *field, std::list<std::string> &buf) {
    auto FieldName = field->getName().data();
    auto FieldType = field->getType();
    auto TypeName = field->getType().getAsString();
    auto FieldTypeInfo =
        field->getParent()->getASTContext().getTypeInfo(FieldType);

    if (FieldType->isBuiltinType()) {
      buf.push_back(
          fmt::format("std::cout << \"{0}({1}):\" << _p->{0} << \", \"; ",
                      FieldName, TypeName));
    }

    if (const EnumType *Ety = field->getType()->getAs<EnumType>()) {
      EnumDecl *Decl = Ety->getDecl();
      visitEnum(Decl);
      buf.push_back(fmt::format("__display<{}>(_p->{});", //
                                TypeName, FieldName));
      return;
    }

    if (FieldType->isRecordType() and not FieldType->isUnionType()) {
      buf.push_back(
          fmt::format("__display<{}*>(&_p->{});", TypeName, FieldName));
      return;
    }

    if (FieldType->isRecordType() and FieldType->isUnionType()) {
      auto UCName = FieldType->getAsCXXRecordDecl()->getName().data();
      buf.push_back(fmt::format("__display<{}*>(&_p->{});", UCName, FieldName));
      return;
    }

    if (FieldType->isArrayType()) {
      auto ElemTy = FieldType->getAsArrayTypeUnsafe()->getElementType();
      if (ElemTy->isBuiltinType()) {
        buf.push_back(fmt::format(
            "for (auto& v : _p->{}) {{std::cout << v << \", \";}}", FieldName));
      } else if (ElemTy->isRecordType()) {
        buf.push_back(fmt::format(
            "for (auto& v : _p->{}) {{__display<{}*>(&v);}}", FieldName,
            ElemTy->getAsRecordDecl()->getName().data()));
      }
    }
  }

  void visitCxxRecord(const CXXRecordDecl *recDef) {
    auto TypeName = recDef->getName().data();
    fmt::println("\n// Generating CXXRecord: {} at 0x{:x}...", //
                 TypeName, reinterpret_cast<size_t>(recDef));

    FwdDecls.push_back(fmt::format(
        "template<> inline void __display<{0}*>({0}* _p); \n", TypeName));

    std::list<std::string> Buf;
    Buf.push_back(fmt::format("template<> inline void __display<{0}*>({0}* _p) "
                              "\n{{\nstd::cout << \"{0} at: 0x\" << std::hex "
                              "<< (size_t)_p << std::dec;",
                              TypeName));

    for (auto Base : recDef->bases()) {
      auto BaseDecl = Base.getType()->getAsCXXRecordDecl();
      if (BaseDecl != nullptr) {
        for (auto *Field : BaseDecl->fields()) {
          visitField(Field, Buf);
        }
      }
    }
    for (auto *Field : recDef->fields()) {
      visitField(Field, Buf);
    }
    Buf.push_back("std::cout << std::endl;\n}");

    Buf.splice(Buf.begin(), FieldDisplayers);
    FieldDisplayers = Buf;
  }

  void visitEnum(const EnumDecl *eEDecl) {
    std::list<std::string> Buf;

    auto EnumClassName = eEDecl->getName().data();
    Buf.push_back(fmt::format(
        "template<> inline void __display<{0}>({0} e) \n{{ \n  switch (e) \n{{",
        EnumClassName));

    for (auto Enumral : eEDecl->enumerators()) {
      Buf.push_back(
          fmt::format("  case {0}::{1}: std::cout << \"{1}\" << ' '; break;",
                      EnumClassName, Enumral->getName().data()));
    }

    Buf.push_back("  default: std::cout << \"Unknown Enum Case: \" "
                  "<< static_cast<int>(e) << ' '; \n}\n}");

    Buf.splice(Buf.begin(), FwdDecls);
    FwdDecls = Buf;
  }

  void showGenn() {

    fmt::println("");
    for (auto &s : FwdDecls) {
      fmt::println("{}", s);
    }

    fmt::println("");
    for (auto &s : FieldDisplayers) {
      fmt::println("{}", s);
    }
  }

  ~GenState() { showGenn(); }
};

struct DisplayGen : MatchFinder::MatchCallback {
  DisplayGen(StringRef file) : FileName(file) {}

  StringRef FileName;
  std::vector<const CXXRecordDecl *> RecDecls;

  void run(const MatchFinder::MatchResult &res) override {
    auto RecDef = res.Nodes.getNodeAs<CXXRecordDecl>("cxxdef");

    // Record all class in defination
    traceRecords(RecDef, RecDecls);

    // generate
    GenState GS(FileName);
    for (auto *Rec : RecDecls) {
      GS.visitCxxRecord(Rec);
    }
  }
};

struct DisplayGenPass : ASTConsumer {
  DisplayGenPass(StringRef target, StringRef file) : GenPass(file) {
    DeclarationMatcher RecordDeclMatcher =
        cxxRecordDecl(isDefinition(), hasName(target)).bind("cxxdef");

    Finder.addMatcher(RecordDeclMatcher, &Displayer);
    Finder.addMatcher(RecordDeclMatcher, &GenPass);
  }
  void HandleTranslationUnit(ASTContext &Ctx) override { Finder.matchAST(Ctx); }

private:
  MatchFinder Finder;
  DisplayMatchee Displayer;
  DisplayGen GenPass;
};

int main(int argc, char *argv[]) { return entry<DisplayGenPass>(argc, argv); }
