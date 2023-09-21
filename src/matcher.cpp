#include "matcher.h"

void displayField(const FieldDecl *field, size_t depth) {
  auto fixDepth = [](size_t d) {
    fmt::print("// ");
    for (int i = 0; i < d; i++) {
      fmt::print("- ");
    }
  };

  auto FieldType = field->getType();
  auto TypeName = field->getType().getAsString();
  auto FieldTypeInfo =
      field->getParent()->getASTContext().getTypeInfo(FieldType);

  fixDepth(depth);
  fmt::println("Field: {:8}, type: {:8}, byte size: {:3}", //
               field->getName().data(), TypeName, FieldTypeInfo.Width / 8);

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

void displayCXXRecord(const CXXRecordDecl *recorddef) {
  for (auto *Field : recorddef->fields()) {
    displayField(Field, 1);
  }
}

void DisplayMatchee::run(const MatchFinder::MatchResult &res) {
  auto RecDef = res.Nodes.getNodeAs<CXXRecordDecl>("cxxdef");
  fmt::println("// Catch CXXRecordDeclare: {}",
               RecDef->getDeclName().getAsString());
  for (auto base : RecDef->bases()) {
    displayCXXRecord(base.getType()->getAsCXXRecordDecl());
  }
  displayCXXRecord(RecDef);
}

void traceRecords(const CXXRecordDecl *root,
                  std::vector<const CXXRecordDecl *> &decls) {
  decls.push_back(root);

  for (auto *Field : root->fields()) {
    auto FieldTy = Field->getType();

    if (FieldTy->isRecordType()) {
      traceRecords(FieldTy->getAsCXXRecordDecl(), decls);
    }

    if (FieldTy->isArrayType()) {
      auto ElemTy = FieldTy->getAsArrayTypeUnsafe()->getElementType();
      if (ElemTy->isRecordType()) {
        traceRecords(ElemTy->getAsCXXRecordDecl(), decls);
      }
    }
  }

  // remove repeated class
  std::sort(decls.begin(), decls.end());
  auto last = std::unique(decls.begin(), decls.end());
  decls.erase(last, decls.end());
  std::reverse(decls.begin(), decls.end());
}