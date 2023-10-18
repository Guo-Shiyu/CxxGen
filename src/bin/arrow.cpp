#include "entry.h"

void formatCode(const std::string &code) {
  std::string path = std::to_string((size_t)code.data());

  // 将代码写入临时文件
  std::ofstream out(path);
  out << code;
  out.close();

  // 调用clang-format来格式化代码
  auto cmd = fmt::format("clang-format -style=llvm -i {}", path);
  std::system(cmd.c_str());

  // 读取格式化后的代码
  std::ifstream in(path);
  std::string formattedCode((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());

  // 输出格式化后的代码
  llvm::outs() << formattedCode;
}

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
    auto FDeclTy = fieldDecl->getType();

    if (FDeclTy->isArrayType()) {
      auto ArrTy = FDeclTy->getAsArrayTypeUnsafe();
      auto ElemTy = ArrTy->getElementType();

      // specialize for string type
      if (ElemTy->isAnyCharacterType()) {
        BuilderDecls.push_back(
            fmt::format("// arrow::CTypeTraits<std::add_const_t<"
                        "std::decay_t<decltype({0}::{1})>>>::"
                        "BuilderType _{1}_b;\n arrow::CTypeTraits<const "
                        "char*>::BuilderType _{1}_b;",
                        recName.data(), FDeclName.data()));
        // fall back to common path
        goto COMMON;

      } else if (ElemTy->isBuiltinType()) {
        if (const auto *CAT =
                dyn_cast<ConstantArrayType>(FDeclTy.getTypePtr())) {
          auto Len = CAT->getSize().getZExtValue();

          for (auto i = 1; i <= Len; i++) {
            BuilderDecls.push_back(fmt::format(
                "arrow::CTypeTraits<std::decay_t<decltype({0}::{1}[0])>"
                ">::BuilderType _{1}_b_{2};",
                recName.data(), FDeclName.data(), i));
            BuilderDecls.push_back(fmt::format(R"(
{{
std::vector<std::decay_t<decltype({0}::{1}[0])>*> _map_{2};
std::transform(_obj.begin(), _obj.end(), _map_{2}.begin(),
                 [](auto o) {{ return &(o->{1}[{2}]); }});
for (auto _p : _map_{2}) {{
  _{1}_b_{2}.Append(*_p).ok();
}}
}}
auto _{1}_f_{2} = _{1}_b_{2}.Finish().ValueOrDie();
)",
                                               recName.data(), FDeclName.data(),
                                               i));

            SchemaFieldDecls.push_back(
                fmt::format(" arrow::field(\"{0}{2}\", _{1}_f_{2}->type()),",
                            getMappedName(FDeclName), FDeclName.data(), i));

            ColsDecl.push_back(fmt::format("_{0}_f_{1}", FDeclName.data(), i));
          }

        } else {
          fmt::println("unreachable case, bad array type ");
          ::exit(-1);
        }

      } else {
        fmt::println("unsupport array type: {} of field: {}",
                     fieldDecl->getName().data(), FDeclName.data());
        ::exit(-1);
      }

      return;
    } else if (FDeclTy->isAnyCharacterType()) {
      // specialize for char
      BuilderDecls.push_back(
          fmt::format("arrow::CTypeTraits<const char *>::BuilderType _{1}_b;",
                      recName.data(), FDeclName.data()));
      BuilderDecls.push_back(fmt::format(R"(
{{
std::vector<decltype({0}::{1})*> _map;
std::transform(_obj.begin(), _obj.end(), _map.begin(),
                 [](auto o) {{ return &(o->{1}); }});
for (auto _p : _map) {{
  _{1}_b.Append(std::string_view(_p, 1)).ok();
}}
}}
auto _{1}_f = _{1}_b.Finish().ValueOrDie();
)",
                                         recName.data(), FDeclName.data()));

      SchemaFieldDecls.push_back(
          fmt::format(" arrow::field(\"{0}\", _{1}_f->type()),",
                      getMappedName(FDeclName), FDeclName.data()));

      ColsDecl.push_back(fmt::format("_{0}_f", FDeclName.data()));
      return;
    }

    BuilderDecls.push_back(fmt::format(
        "arrow::CTypeTraits<decltype({0}::{1})>::BuilderType _{1}_b;",
        recName.data(), FDeclName.data()));

  COMMON:

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
auto inline __arrowgen(Generator<{0}> _g) {{

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
    std::string buf;
    buf.reserve(1024 * 128); // 128 kb

    // buf.push_back(' ');
    for (auto &s : Header) {
      buf.append(s);
    }

    buf.push_back(' ');
    for (auto &s : BuilderDecls) {
      buf.append(s);
    }

    buf.push_back(' ');
    for (auto &s : SchemaFieldDecls) {
      buf.append(s);
    }

    buf.push_back(' ');
    for (auto &s : Footer) {
      buf.append(s);
    }

    formatCode(buf);
    // llvm::outs() << buf;
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

    Finder.addMatcher(RecordDeclMatcher, &Generator);
    Finder.addMatcher(RecordDeclMatcher, &Displayer);
  }
  void HandleTranslationUnit(ASTContext &Ctx) override { Finder.matchAST(Ctx); }

private:
  MatchFinder Finder;
  DisplayMatchee Displayer;
  ArrowGen Generator;
};

int main(int argc, char *argv[]) { return entry<ArrowGenPass>(argc, argv); }
