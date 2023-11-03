#ifndef FTDB_COMPAT_H
#define FTDB_COMPAT_H
#include "clang/AST/Expr.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/DeclTemplate.h"

#if CLANG_VERSION==11
#include "clang/Basic/FileManager.h"
#elif CLANG_VERSION>=12
#include "clang/Basic/FileEntry.h"
#endif

using namespace clang;

namespace compatibility{
  inline void EvaluateAsConstantExpr(const Expr *E,Expr::EvalResult &Res,ASTContext &Context){
#if CLANG_VERSION>=12
    E->EvaluateAsConstantExpr(Res, Context);
#else
    E->EvaluateAsConstantExpr(Res, Expr::EvaluateForCodeGen, Context);
#endif
  }

  inline std::string toString(llvm::APSInt Iv){
#if CLANG_VERSION>=13
    llvm::SmallVector<char,30> tmp;
    Iv.toString(tmp,10);
    return std::string(tmp.begin(),tmp.end());
#else
    return Iv.toString(10);
#endif
  }

  inline CompoundStmt *createEmptyCompoundStmt(ASTContext &Ctx){
#if CLANG_VERSION>=15
    return CompoundStmt::CreateEmpty(Ctx,1,0);
#else
    return CompoundStmt::CreateEmpty(Ctx,1);
#endif
  }

#if CLANG_VERSION<16
#define getUnmodifiedType getUnderlyingType
#endif

  inline const TemplateTypeParmType *getReplacedParmType(const SubstTemplateTypeParmType* tp){
#if CLANG_VERSION>=16
    return cast<TemplateTypeParmType>(tp->getReplacedParameter()->getTypeForDecl());
#else
    return tp->getReplacedParameter();
#endif
  }
}
#endif //FTDB_COMPAT_H