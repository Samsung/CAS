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

#if CLANG_VERSION>=10
#define COMPAT_VERSION_GE_10(code) code
#else
#define COMPAT_VERSION_GE_10(code)
#endif

#if CLANG_VERSION>=14
#define COMPAT_VERSION_GE_14(code) code
#else
#define COMPAT_VERSION_GE_14(code)
#endif

#if CLANG_VERSION>=15
#define COMPAT_VERSION_GE_15(code) code
#else
#define COMPAT_VERSION_GE_15(code)
#endif

#define COMPAT_VERSION_GE(v,code) COMPAT_VERSION_GE_##v(code)

#if CLANG_VERSION<11
#define FTDB_COMPAT_MACRO_OFFSET -2
#else
#define FTDB_COMPAT_MACRO_OFFSET 0
#endif

#if CLANG_VERSION<16
#define getUnmodifiedType getUnderlyingType
#endif

#if CLANG_VERSION>=16
#define compatGetValue value
#else
#define compatGetValue getValue
#endif

#if CLANG_VERSION>=18
#define compatNoLinkage             Linkage::None
#define compatInternalLinkage       Linkage::Internal
#define compatUniqueExternalLinkage Linkage::UniqueExternal
#define compatExternalLinkage       Linkage::External
#else
#define compatNoLinkage             NoLinkage
#define compatInternalLinkage       InternalLinkage
#define compatUniqueExternalLinkage UniqueExternalLinkage
#define compatExternalLinkage       ExternalLinkage
#endif

#if CLANG_VERSION>=18
#define C2x_gnu_section  C23_gnu_section
#define C2x_nodiscard C23_nodiscard
#define C2x_gnu_warn_unused_result C23_gnu_warn_unused_result
#define C2x_gnu_assume_aligned C23_gnu_assume_aligned
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

  inline const TemplateTypeParmType *getReplacedParmType(const SubstTemplateTypeParmType* tp){
#if CLANG_VERSION>=16
    return cast<TemplateTypeParmType>(tp->getReplacedParameter()->getTypeForDecl());
#else
    return tp->getReplacedParameter();
#endif
  }

  inline const std::string translateLinkageImpl(clang::Linkage linkage){
    switch(linkage){
#if CLANG_VERSION>=18
    case clang::Linkage::None:
      return "none";
		case clang::Linkage::Internal:
			return "internal";
    case clang::Linkage::External:
      return "external";
#else
		case clang::NoLinkage:
			return "none";
		case clang::InternalLinkage:
			return "internal";
		case clang::ExternalLinkage:
			return "external";
#endif
		default:
			return "";
	  }
  }
}


#endif //FTDB_COMPAT_H