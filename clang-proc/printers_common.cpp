#include "clang/AST/Attr.h"
#include "clang/AST/Expr.h"

#include "printers.h"
#include "compat.h"


using namespace clang;

thread_local int csd = 0;
thread_local std::set<const FunctionDecl*> *CTAList;

void setCTAList(std::set<const FunctionDecl*> *List){
  CTAList = List;
}

void setCustomStructDefs(bool _csd){
  csd = _csd;
}

static void printPrettySectionAttr(SectionAttr* A, raw_ostream &OS, const PrintingPolicy &Policy) {
  switch (A->getSemanticSpelling()) {
    default:
      llvm_unreachable("Unknown attribute spelling!");
      break;
    case SectionAttr::Spelling::GNU_section: {
      (OS << " __attribute__((section(\"").write_escaped(A->getName()) << "\")))";
      break;
    }
    case SectionAttr::Spelling::CXX11_gnu_section:
    case SectionAttr::Spelling::C2x_gnu_section: {
      (OS << " [[gnu::section(\"").write_escaped(A->getName()) << "\")]]";
      break;
    }
    case SectionAttr::Spelling::Declspec_allocate: {
      (OS << " __declspec(allocate(\"").write_escaped(A->getName()) << "\"))";
      break;
    }
  }
}

static void printPrettyWarnUnusedResultAttr(WarnUnusedResultAttr* A, raw_ostream &OS, const PrintingPolicy &Policy) {
  switch (A->getSemanticSpelling()) {
    default:
      llvm_unreachable("Unknown attribute spelling!");
      break;
    case WarnUnusedResultAttr::Spelling::CXX11_nodiscard:
    case WarnUnusedResultAttr::Spelling::C2x_nodiscard: {
      OS << " [[nodiscard(\"" << A->getMessage() << "\")]]";
      break;
    }
    case WarnUnusedResultAttr::Spelling::CXX11_clang_warn_unused_result: {
      OS << " [[clang::warn_unused_result(\"" << A->getMessage() << "\")]]";
      break;
    }
    case WarnUnusedResultAttr::Spelling::GNU_warn_unused_result : {
      OS << " __attribute__((warn_unused_result))";
      break;
    }
    case WarnUnusedResultAttr::Spelling::CXX11_gnu_warn_unused_result :
    case WarnUnusedResultAttr::Spelling::C2x_gnu_warn_unused_result : {
      OS << " [[gnu::warn_unused_result(\"" << A->getMessage() << "\")]]";
      break;
    }
  }
}

static void printPrettyAssumeAlignedAttr(AssumeAlignedAttr *A, raw_ostream &OS, const PrintingPolicy &Policy) {
  const char *delim;
  switch (A->getAttributeSpellingListIndex()) {
    default:
      llvm_unreachable("Unknown attribute spelling!");
      break;
    case AssumeAlignedAttr::Spelling::GNU_assume_aligned: {
      OS << " __attribute__((assume_aligned";
      delim = "))";
      break;
    }
    case AssumeAlignedAttr::Spelling::CXX11_gnu_assume_aligned:
    case AssumeAlignedAttr::Spelling::C2x_gnu_assume_aligned: {
      OS << " [[gnu::assume_aligned";
      delim = "]]";
      break;
    }
  }
  OS << "(";
  A->getAlignment()->printPretty(OS,nullptr,Policy);
  if(A->getOffset()){
    OS << ", ";
    A->getOffset()->printPretty(OS,nullptr,Policy);
  }
  OS << ")" << delim;
}


void printPrettyAttrModified(Attr *A,raw_ostream &Out,const PrintingPolicy &Policy){
  switch(A->getKind()){
  case attr::Section:
    printPrettySectionAttr(cast<SectionAttr>(A), Out, Policy);
    break;
  case attr::WarnUnusedResult:
    printPrettyWarnUnusedResultAttr(cast<WarnUnusedResultAttr>(A), Out, Policy);
    break;
  case attr::AssumeAligned:
    printPrettyAssumeAlignedAttr(cast<AssumeAlignedAttr>(A), Out, Policy);
    break;
  default:
    A->printPretty(Out, Policy);
    break;
  }
}