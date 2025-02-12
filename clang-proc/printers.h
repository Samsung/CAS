#include "clang/AST/Decl.h"
#include <set>

extern bool enable_sa;
extern thread_local int csd;
extern thread_local std::set<const clang::FunctionDecl*> *CTAList;

void setCTAList(std::set<const clang::FunctionDecl*> *List);

void printRecordHead(clang::RecordDecl *D, llvm::raw_ostream &Out, const clang::PrintingPolicy &Policy);
void printUnnamedTag(clang::TagDecl *D,llvm::raw_ostream &Out, const clang::PrintingPolicy &Policy);

void setCustomStructDefs(bool _csd);
void processDeclGroupNoClear(llvm::SmallVectorImpl<clang::Decl*>& Decls, llvm::raw_ostream &Out,
                             const clang::PrintingPolicy &Policy);

void printPrettyAttrModified(clang::Attr *A,llvm::raw_ostream &Out,const clang::PrintingPolicy &Policy);