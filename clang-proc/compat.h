#include "clang/AST/Expr.h"
#include "clang/Basic/SourceManager.h"

using namespace clang;

namespace compatibility{
  inline void EvaluateAsConstantExpr(const Expr *E,Expr::EvalResult &Res,ASTContext &Context){
#if CLANG_VERSION>=12
    E->EvaluateAsConstantExpr(Res, Context);
#else
    E->EvaluateAsConstantExpr(Res, Expr::EvaluateForCodeGen, Context);
#endif
  }

  inline const auto getBuffer(SourceManager &SM, FileID FID){
#if CLANG_VERSION>=12
    return SM.getBufferOrFake(FID);
#else
    return SM.getBuffer(FID);
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
}