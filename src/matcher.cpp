#include "matcher.h"
#include <algorithm>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <cstdio>
#include <fmt/core.h>
#include <llvm/Support/raw_ostream.h>
#include <vector>

void displayField(const FieldDecl *Field, size_t depth) {
  auto fixDepth = [](size_t d) {
    fmt::print("// ");
    for (int i = 0; i < d - 1; i++) {
      fmt::print("- ");
    }
  };

  auto FieldType = Field->getType();
  auto TypeName = Field->getType().getAsString();
  auto FieldTypeInfo =
      Field->getParent()->getASTContext().getTypeInfo(FieldType);

  fixDepth(depth);
  fmt::println("Field: {:8}, type: {:8}, byte size: {:3}", //
               Field->getName().data(), TypeName, FieldTypeInfo.Width / 8);

  if (const EnumType *Ety = FieldType->getAs<EnumType>()) {
    EnumDecl *Decl = Ety->getDecl();
    for (auto Enumral : Decl->enumerators()) {
      fixDepth(depth + 1);
      fmt::println("Enumeral Name: {:8}, init Val: {:3}", //
                   Enumral->getName().data(),
                   Enumral->getInitVal().getSExtValue());
    }
  }

  if (FieldType->isUnionType()) {
    RecordDecl *Uty = FieldType->getAsUnionType()->getDecl();
    for (auto *Field : Uty->fields()) {
      displayField(Field, depth + 1);
    }
  }

  if (FieldType->isRecordType() and not FieldType->isUnionType()) {
    RecordDecl *Uty = FieldType->getAsCXXRecordDecl();
    for (auto *Field : Uty->fields()) {
      displayField(Field, depth + 1);
    }
  }
}

void displayCXXRecord(const CXXRecordDecl *RecDef) {
  for (auto *Field : RecDef->fields()) {
    displayField(Field, 1);
  }
}

void DisplayMatchee::run(const MatchFinder::MatchResult &Res) {
  auto RecDef = Res.Nodes.getNodeAs<CXXRecordDecl>("cxxdef");
  fmt::println("// Catch CXXRecordDeclare: {}",
               RecDef->getDeclName().getAsString());
  for (auto base : RecDef->bases()) {
    displayCXXRecord(base.getType()->getAsCXXRecordDecl());
  }
  displayCXXRecord(RecDef);
}

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

  //   constexpr static auto kFixedHeader = R"(
  // template<typename T> inline void __display(T t) {}
  // )";

  GenState(StringRef FileName) {
    FwdDecls.push_back(fmt::format("#include \"{}\" \n#include <iostream> \n",
                                   FileName.data()));
    FwdDecls.push_back(kFixedHeader);
  }

  std::list<std::string> FwdDecls, FieldDisplayers;

  void visitField(const FieldDecl *Field, std::list<std::string> &Buf) {
    auto FieldName = Field->getName().data();
    auto FieldType = Field->getType();
    auto TypeName = Field->getType().getAsString();
    auto FieldTypeInfo =
        Field->getParent()->getASTContext().getTypeInfo(FieldType);

    if (FieldType->isBuiltinType()) {
      Buf.push_back(
          fmt::format("std::cout << \"{0}({1}):\" << _p->{0} << \", \"; ",
                      FieldName, TypeName));
    }

    if (const EnumType *Ety = Field->getType()->getAs<EnumType>()) {
      EnumDecl *Decl = Ety->getDecl();
      visitEnum(Decl);
      Buf.push_back(fmt::format("__display<{}>(_p->{});", //
                                TypeName, FieldName));
      return;
    }

    if (FieldType->isRecordType() and not FieldType->isUnionType()) {
      Buf.push_back(
          fmt::format("__display<{}*>(&_p->{});", TypeName, FieldName));
      return;
    }

    if (FieldType->isRecordType() and FieldType->isUnionType()) {
      auto UCName = FieldType->getAsCXXRecordDecl()->getName().data();
      Buf.push_back(fmt::format("__display<{}*>(&_p->{});", UCName, FieldName));
      return;
    }

    if (FieldType->isArrayType()) {
      auto ElemTy = FieldType->getAsArrayTypeUnsafe()->getElementType();
      if (ElemTy->isBuiltinType()) {
        Buf.push_back(fmt::format(
            "for (auto& v : _p->{}) {{std::cout << v << \", \";}}", FieldName));
      } else if (ElemTy->isRecordType()) {
        Buf.push_back(fmt::format(
            "for (auto& v : _p->{}) {{__display<{}*>(&v);}}", FieldName,
            ElemTy->getAsRecordDecl()->getName().data()));
      }
    }
  }

  void visitCxxRecord(const CXXRecordDecl *RecDef) {
    auto TypeName = RecDef->getName().data();
    fmt::println("\n// Generating CXXRecord: {} at 0x{:x}...", //
                 TypeName, reinterpret_cast<size_t>(RecDef));

    FwdDecls.push_back(fmt::format(
        "template<> inline void __display<{0}*>({0}* _p); \n", TypeName));

    std::list<std::string> Buf;
    Buf.push_back(fmt::format("template<> inline void __display<{0}*>({0}* _p) "
                              "\n{{\n std::cout << \"{0} at: 0x\" << std::hex "
                              "<< (size_t)_p << std::dec;",
                              TypeName));
    for (auto *Field : RecDef->fields()) {
      visitField(Field, Buf);
    }
    Buf.push_back("std::cout << std::endl;\n}");

    Buf.splice(Buf.begin(), FieldDisplayers);
    FieldDisplayers = Buf;
  }

  void visitEnum(const EnumDecl *EDecl) {
    std::list<std::string> Buf;

    auto EnumClassName = EDecl->getName().data();
    Buf.push_back(fmt::format(
        "template<> inline void __display<{0}>({0} e) \n{{ \n  switch (e) \n{{",
        EnumClassName));

    for (auto Enumral : EDecl->enumerators()) {
      Buf.push_back(
          fmt::format("  case {0}::{1}: std::cout << \"{0}\" << ' '; break;",
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

void traceRecords(const CXXRecordDecl *Rec,
                  std::vector<const CXXRecordDecl *> &vec) {
  vec.push_back(Rec);
  for (auto *Field : Rec->fields()) {
    auto FieldTy = Field->getType();

    if (FieldTy->isRecordType()) {
      traceRecords(FieldTy->getAsCXXRecordDecl(), vec);
    }

    if (FieldTy->isArrayType()) {
      auto ElemTy = FieldTy->getAsArrayTypeUnsafe()->getElementType();
      if (ElemTy->isRecordType()) {
        traceRecords(ElemTy->getAsCXXRecordDecl(), vec);
      }
    }
  }
}

void PrintCodeGen::run(const MatchFinder::MatchResult &Res) {
  auto RecDef = Res.Nodes.getNodeAs<CXXRecordDecl>("cxxdef");

  // Record all class in defination
  traceRecords(RecDef, this->RecDecls);

  // remove repeated class
  std::sort(this->RecDecls.begin(), this->RecDecls.end());
  auto last = std::unique(this->RecDecls.begin(), this->RecDecls.end());
  this->RecDecls.erase(last, this->RecDecls.end());
  std::reverse(this->RecDecls.begin(), this->RecDecls.end());

  // generate
  GenState GS(FileName);
  for (auto *Rec : this->RecDecls) {
    GS.visitCxxRecord(Rec);
  }
}
