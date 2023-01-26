#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wpessimizing-move"
#pragma GCC diagnostic ignored "-Wswitch"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/CommentVisitor.h"
#include "clang/AST/Comment.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/Type.h"

using namespace clang;

#include "utils.h"
#include "sha.h"
#include "base64.h"

#include <stdint.h>

#include <string>
#include <algorithm>
#include <sstream>
#include <vector>
#include <fstream>
#include <streambuf>
#include <cstdlib>
#include <set>
#include <tuple>

#define DEBUG_VISIT 0
#define DEBUG_VISIT_TYPE 1
#define DEBUG_VISIT_DECL 1
#define DEBUG_VISIT_CPPDECL 1
#define DEBUG_VISIT_STMT 1
#define DEBUG_VISIT_EXPR 1

class FOPSClassVisitor
  : public RecursiveASTVisitor<FOPSClassVisitor> {
public:
  explicit FOPSClassVisitor(ASTContext &Context, const struct main_opts& opts)
    : Context(Context), _opts(opts) { (void)Context; }

  void noticeInitListStmt(const Stmt* initExpr, const VarDecl * D);
  void noticeImplicitCastExpr(const ImplicitCastExpr* ICE, const VarDecl * Dref, QualType initExprType, unsigned long index);
  bool VisitVarDecl(const VarDecl *D);
  std::string getFunctionDeclHash(const FunctionDecl *FD);

  struct AssignInfo {
	  std::string name;	// Name of the reference declaration (i.e. X in X=3)
	  const void* D;	// Address of the reference declaration (i.e. X in X=3)
	  std::string type;	// Type of the reference declaration (i.e. U in "struct U x; x.a = 3)
	  unsigned index;	// Index of the assigned field in reference declaration
  };

  bool noticeMemberExprForAssignment(const MemberExpr* ME, struct AssignInfo& AI, unsigned fieldIndex, bool nestedME = false);
  bool VisitBinaryOperator(const BinaryOperator *Node);

  std::string typeMatching(QualType T);
  std::string typeMatchingWithPtr(QualType T);
  const Expr* CollapseToDeclRef(const Stmt *S);

  typedef std::map<const void*,std::set<std::tuple<unsigned long,std::string,std::string,std::string>>> initMap_t;
  initMap_t initMap;
  typedef std::map<const void*,std::pair<std::string,std::set<std::tuple<unsigned long,std::string,std::string>>>> initMapFixedType_t;

  size_t getVarNum() {
	  return initMap.size();
  }

#if DEBUG_VISIT>0
#include "visit.h"
#endif

private:
  ASTContext &Context;
  const struct main_opts& _opts;
};

class FOPSClassConsumer : public clang::ASTConsumer {
public:
  explicit FOPSClassConsumer(ASTContext &Context, const std::string* sourceFile,
		  const std::string* directory, const struct main_opts& opts)
    : Visitor(Context,opts), _sourceFile(sourceFile), _directory(directory), _opts(opts), Context(Context) {}

  void printVarMaps(int Indentation);
  void printVarMap(FOPSClassVisitor::initMapFixedType_t& FTVM, int Indentation);
  std::string getAbsoluteLocation(SourceLocation Loc);

  virtual void HandleTranslationUnit(clang::ASTContext &Context);

  virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
	  return true;
      }
private:
  FOPSClassVisitor Visitor;
  const std::string* _sourceFile;
  const std::string* _directory;
  const struct main_opts& _opts;
  ASTContext &Context;
};

void getFuncDeclSignature(const FunctionDecl* D, std::string& fdecl_sig);

