#include "matcher.h"


void displayField(const FieldDecl *Field, size_t depth) {
  auto fixDepth = [](size_t d) {
    fmt::print("// ");
    for (int i = 0; i < d; i++) {
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
