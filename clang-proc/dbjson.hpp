#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wpessimizing-move"

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
#include "clang/Tooling/Syntax/Tokens.h"

using namespace clang;

#include "utils.h"
#include "sha.h"
#include "base64.h"
#include "printers.h"
#include "MacroHandler.h"

#include <stdint.h>

#include <string>
#include <algorithm>
#include <sstream>
#include <vector>
#include <stack>
#include <fstream>
#include <streambuf>
#include <cstdlib>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>

#define DBG(on,...)		do { if (on) { __VA_ARGS__; } } while(0)

//#define DEBUG_PRINT
#define DEBUG_TYPEMAP 0
#define JSON_OUT

#define DEBUG_TYPESTRING 0
#define DEBUG_RECORDSTRING 0
#define DEBUG_HASH 0

#define TTP_DEBUG 0

static inline llvm::raw_ostream& operator<<(llvm::raw_ostream &os,const QualType &T){
  return os<<T.getAsOpaquePtr();
}

extern size_t exprOrd;

class CompoundStmtClassVisitor
  : public RecursiveASTVisitor<CompoundStmtClassVisitor> {
public:
	explicit CompoundStmtClassVisitor(ASTContext &Context, const struct main_opts& opts): Context(Context), _opts(opts) {}
	bool VisitStmt(Stmt *S);
	bool VisitExpr(const Expr *Node);
	bool TraverseStmt(Stmt* S);
	bool WalkUpFromStmt(Stmt *S);
	bool WalkUpFromCompoundStmt(CompoundStmt *S);
	bool shouldTraversePostOrder();

private:
	  ASTContext &Context;
	  const struct main_opts& _opts;
};

struct anonRecord {
  std::vector<std::string> fieldNames;
  std::string hash;
};

static std::string JSONReplacementToken = "\"\"\"$\"\"\"";
static std::string ORDReplacementToken = "\"\"\"@\"\"\"";
static std::string ARGeplacementToken = "\"\"\"&\"\"\"";

class DbJSONClassVisitor
  : public RecursiveASTVisitor<DbJSONClassVisitor> {
public:
  explicit DbJSONClassVisitor(ASTContext &Context, const struct main_opts& opts)
    : TypeNum(0), VarNum(0), FuncNum(0), lastFunctionDef(0), CTA(0), missingRefsCount(0), Context(Context), _opts(opts) {}

  static QualType getDeclType(Decl* D) {
    if (TypedefNameDecl* TDD = dyn_cast<TypedefNameDecl>(D))
      return TDD->getUnderlyingType();
    if (ValueDecl* VD = dyn_cast<ValueDecl>(D))
      return VD->getType();
    return QualType();
  }

  static QualType GetBaseType(QualType T) {
    // FIXME: This should be on the Type class!
    QualType BaseType = T;
    while (!BaseType->isSpecifierType()) {
      if (isa<TypedefType>(BaseType))
        break;
      else if (const PointerType* PTy = BaseType->getAs<PointerType>())
        BaseType = PTy->getPointeeType();
      else if (const BlockPointerType *BPy = BaseType->getAs<BlockPointerType>())
        BaseType = BPy->getPointeeType();
      else if (const ArrayType* ATy = dyn_cast<ArrayType>(BaseType))
        BaseType = ATy->getElementType();
      else if (const FunctionType* FTy = BaseType->getAs<FunctionType>())
        BaseType = FTy->getReturnType();
      else if (const VectorType *VTy = BaseType->getAs<VectorType>())
        BaseType = VTy->getElementType();
      else if (const ReferenceType *RTy = BaseType->getAs<ReferenceType>())
        BaseType = RTy->getPointeeType();
      else if (const AutoType *ATy = BaseType->getAs<AutoType>())
        BaseType = ATy->getDeducedType();
      else if (const AttributedType *AtrTy = BaseType->getAs<AttributedType>())
        BaseType = AtrTy->getModifiedType();
      else if (const ParenType *PTy = BaseType->getAs<ParenType>())
        BaseType = PTy->desugar();
      else
        break;
    }
    return BaseType;
  }

  struct ValueHolder {
	  const ValueDecl* value;
	  unsigned pos;
	  ValueHolder(const ValueDecl* value, unsigned pos): value(value), pos(pos) {}
	  const ValueDecl* getValue() { return value; }
	  unsigned getPos() { return pos; }
	  const ValueDecl* getValue() const { return value; }
	  unsigned getPos() const { return pos; }
	  bool operator<(ValueHolder other) const {
		  if (value<other.value) return true;
		  if (value>other.value) return false;
		  return pos<other.pos;
	  }
	  bool operator==(ValueHolder other) const {
		  return (value==other.value) && (pos==other.pos);
	  }
  };

  struct LiteralHolder {
  	enum LiteralType {
  		LiteralString,
  		LiteralInteger,
  		LiteralChar,
		  LiteralFloat,
  	} type;
  	struct PrivLiterals {
  		std::string stringLiteral;
  		llvm::APSInt integerLiteral;
  		unsigned int charLiteral;
  		double floatingLiteral;
  	} prvLiteral;
  	unsigned pos;
  	bool literalLower(const LiteralHolder &other) const {
      if (type < other.type) return true;
      if (type > other.type) return false;
  		if (type==LiteralString) {
  			return prvLiteral.stringLiteral.compare(other.prvLiteral.stringLiteral)<0;
  		}
  		if (type==LiteralInteger) {
  			return prvLiteral.integerLiteral.extOrTrunc(64).getExtValue() < other.prvLiteral.integerLiteral.extOrTrunc(64).getExtValue();
  		}
  		if (type==LiteralChar) {
  			return prvLiteral.charLiteral < other.prvLiteral.charLiteral;
  		}
  		if (type==LiteralFloat) {
  			return prvLiteral.floatingLiteral < other.prvLiteral.floatingLiteral;
  		}
      assert( 0 && "Wrong literal type.");
  	}
  	bool operator<(const LiteralHolder &other) const {
  		return literalLower(other);
  	}
  };

  typedef std::vector<std::string> recordFields_t;
  typedef std::vector<size_t> recordOffsets_t;
  typedef std::pair<recordFields_t,recordOffsets_t> recordInfo_t;

  struct TypeData{
    size_t id;
    recordInfo_t *RInfo;
    std::string hash;
  };

  typedef std::map<QualType,TypeData> TypeArray_t;
  TypeArray_t TypeMap;
  size_t TypeNum;

  size_t getTypeNum() {
	  return TypeNum;
  }

  // use only when iterating over all types
  // unsafe with types obtained from clang api
  TypeArray_t& getTypeMap() {
  	  return TypeMap;
  }

  TypeData& getTypeData(QualType T) {
	  T = typeForMap(T);
	  if (TypeMap.find(T)==TypeMap.end()) {
		  llvm::outs()<<"Type not in map:"<<T<<'\n';
		  T.dump();
		  assert(0);
	  }
	  return TypeMap.at(T);
  }

  typedef std::pair<int64_t,std::string> caseenum_t;
  typedef std::tuple<caseenum_t,std::string,std::string,int64_t> caseinfo_t;

  struct CastExprOrType {
	  enum CastExprOrTypeKind {
		  CastExprOrTypeKindCast,
		  CastExprOrTypeKindType,
	  };
	  CastExprOrType(CastExpr* cast): cast(cast), kind(CastExprOrTypeKindCast) {}
	  CastExprOrType(QualType type): type(type), kind(CastExprOrTypeKindType) {}
	  CastExpr* cast;
	  QualType type;
	  CastExprOrTypeKind kind;
	  CastExprOrTypeKind getKind() {
		  return kind;
	  }
	  CastExpr* getCast() {
		  return cast;
	  }
	  QualType getType() {
		  return type;
	  }
	  QualType getFinalType() {
		  if (kind==CastExprOrTypeKindCast) {
			  return cast->getType();
		  }
		  else {
			  return type;
		  }
	  }
	  bool operator <(const CastExprOrType & otherCT) const {
		if (kind==CastExprOrTypeKindCast) {
			return cast<otherCT.cast;
		}
		else {
			return type<otherCT.type;
		}
	  }

	  bool operator==(const CastExprOrType & otherCT) const {
		if (kind==CastExprOrTypeKindCast) {
			return cast==otherCT.cast;
		}
		else {
			return type==otherCT.type;
		}
	  }
  };

    struct CStyleCastOrType {
    enum CStyleCastOrTypeKind {
        CStyleCastOrTypeKindNone,
        CStyleCastOrTypeKindCast,
        CStyleCastOrTypeKindType,
    };
    CStyleCastExpr* CSCE;
    QualType T;
    CStyleCastOrTypeKind kind;
    CStyleCastOrType(): CSCE(0), T(), kind(CStyleCastOrTypeKindNone) {}
    CStyleCastOrType(CStyleCastExpr* CSCE): CSCE(CSCE), T(), kind(CStyleCastOrTypeKindCast) {}
    CStyleCastOrType(QualType T): CSCE(0), T(T), kind(CStyleCastOrTypeKindType) {}
    bool isValid() {
        return kind!=CStyleCastOrTypeKindNone;
    }
    bool isType() {
        return kind==CStyleCastOrTypeKindType;
    }
    bool isCast() {
        return kind==CStyleCastOrTypeKindCast;
    }
    void setType(QualType _T) {
        CSCE = 0;
        T = _T;
        kind = CStyleCastOrTypeKindType;
    }
    void setCast(CStyleCastExpr* _cast) {
        CSCE = _cast;
        T = QualType();
        kind = CStyleCastOrTypeKindCast;
    }
    QualType getType() {
        return T;
    }
    CStyleCastExpr* getCast() {
        return CSCE;
    }
    QualType getFinalType() {
        if (kind==CStyleCastOrTypeKindType) {
            return getType();
        }
        else {
            return CSCE->getType();
        }
    }
    bool operator <(const CStyleCastOrType & other) const {
      if (CSCE<other.CSCE) return true;
      if (CSCE>other.CSCE) return false;
      if (T<other.T) return true;
      if (other.T<T) return false;
      return kind<other.kind;
    }
    bool operator >(const CStyleCastOrType & other) const {
      if (CSCE>other.CSCE) return true;
      if (CSCE<other.CSCE) return false;
      if (other.T<T) return true;
      if (T<other.T) return false;
      return kind>other.kind;
    }
    bool operator==(const CStyleCastOrType & other) const {
      return (CSCE==other.CSCE)&&(T==other.T)&&(kind==other.kind);
    }
  };

  /* Gets the first c-style cast saved in the cache unless there isn't one. In such case
      takes first cast whatsoever (which must be implicit cast) */
  static inline CStyleCastOrType getMatchingCast(std::vector<CStyleCastOrType>& vC) {
        auto i = vC.begin();
        for (; i!=vC.end(); ++i) {
            if ((*i).isCast()) {
                break;
            }
        }
        if (i==vC.end()) {
            i = vC.begin();
        }
        return *i;
    }

    static inline CStyleCastExpr* getFirstCast(std::vector<CStyleCastOrType>& vC) {
        auto i = vC.begin();
        for (; i!=vC.end(); ++i) {
            if ((*i).isCast()) {
                return (*i).getCast();
            }
        }
        return 0;
    }

    bool isPtrToVoid(QualType T) {
        if (T->getTypeClass()==Type::Pointer) {
            const PointerType *tp = cast<PointerType>(T);
            QualType ptrT = tp->getPointeeType();
            if (ptrT->getTypeClass()==Type::Builtin) {
                const BuiltinType *btp = cast<BuiltinType>(ptrT);
                if (btp->getName(Context.getPrintingPolicy())=="void") {
                    return true;
                }
            }
        }
        return false;
    }

  struct ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS {
	  enum ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKind {
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindNone,
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindValue,
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCall,
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRefCall,
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAddress,
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindInteger,
          ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindFloating,
          ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindString,
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindMemberExpr,
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindUnary,
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAS,
          ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCAO,
          ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindOOE,
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRET,
		  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindParm,
      ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCond,
      ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindLogic,

	  };
	  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS(): value(0), call(0), address(0), floating(0.), strval(""), ME(0), MEIdx(0), MECnt(0),
			  UO(0), AS(0), valuecast(), cao(0), ooe(0), re(0), kind(ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindNone), primary(true) {}
	  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS(const ValueDecl* value): value(value), call(0), address(0), floating(0.), strval(""), ME(0), MEIdx(0), MECnt(0),
			  UO(0), AS(0), valuecast(), cao(0), ooe(0), re(0), kind(ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindValue), primary(true) {}
	  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS(const CallExpr* call): value(0), call(call), address(0), floating(0.), strval(""), ME(0), MEIdx(0), MECnt(0),
			  UO(0), AS(0), valuecast(), cao(0), ooe(0), re(0), kind(ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCall), primary(true) {}
	  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS(int64_t address): value(0), call(0), address(address), floating(0.), strval(""), ME(0), MEIdx(0), MECnt(0),
			  UO(0), AS(0), valuecast(), cao(0), ooe(0), re(0), kind(ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAddress), primary(true) {}
	  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS(const MemberExpr* ME): value(0), call(0), address(0), floating(0.), strval(""), ME(ME), MEIdx(0), MECnt(0),
			  UO(0), AS(0), valuecast(), cao(0), ooe(0), re(0), kind(ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindMemberExpr), primary(true) {}
	  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS(const UnaryOperator* UO): value(0), call(0), address(0), floating(0.), strval(""), ME(0), MEIdx(0), MECnt(0),
			  UO(UO), AS(0), valuecast(), cao(0), ooe(0), re(0), kind(ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindUnary), primary(true) {}
	  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS(const ArraySubscriptExpr* AS): value(0), call(0), address(0), floating(0.), strval(""), ME(0), MEIdx(0), MECnt(0),
			  UO(0), AS(AS), valuecast(), cao(0), ooe(0), re(0), kind(ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAS), primary(true) {}
      ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS(const CompoundAssignOperator* CAO): value(0), call(0), address(0), floating(0.), strval(""), ME(0), MEIdx(0), MECnt(0),
              UO(0), AS(0), valuecast(), cao(CAO), ooe(0), re(0), kind(ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCAO), primary(true) {}
      ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS(const OffsetOfExpr* OOE): value(0), call(0), address(0), floating(0.), strval(""), ME(0), MEIdx(0), MECnt(0),
              UO(0), AS(0), valuecast(), cao(0), ooe(OOE), re(0), kind(ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindOOE), primary(true) {}
      ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS(const ReturnStmt *S): value(0), call(0), address(0), floating(0.), strval(""), ME(0), MEIdx(0), MECnt(0),
    		  UO(0), AS(0), valuecast(), cao(0), ooe(0), re(S->getRetValue()), kind(ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRET), primary(true) {}
	  const ValueDecl* value;
	  const CallExpr* call;
	  int64_t address;
      double floating;
      std::string strval;
	  const MemberExpr* ME;
	  QualType METype;
	  unsigned MEIdx;
	  unsigned MECnt;
	  const UnaryOperator* UO;
	  const ArraySubscriptExpr* AS;
	  CStyleCastOrType valuecast;
      const BinaryOperator* cao;
      const OffsetOfExpr* ooe;
      const Expr* re;
	  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKind kind;
	  bool primary;

	  bool isPrimary() {
		  return primary;
	  }

	  void setPrimaryFlag(bool __primary) {
		  primary = __primary;
	  }

	  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKind getKind() {
		  return kind;
	  }
	  std::string getKindString() {
		  switch(kind) {
		  	  case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindNone:
		  		  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindNone";
		  	  case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindValue:
		  		  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindValue";
		  	  case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCall:
		  		  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCall";
		  	  case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRefCall:
		  		  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRefCall";
		  	  case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAddress:
		  		  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAddress";
              case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindFloating:
                  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindFloating";
              case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindString:
                  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindString";
		  	  case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindInteger:
		  		  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindInteger";
		  	  case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindMemberExpr:
		  		  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindMemberExpr";
		  	  case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindUnary:
		  		  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindUnary";
		  	  case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAS:
		  		  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAS";
              case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCAO:
                  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCAO";
              case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindOOE:
                  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindOOE";
              case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRET:
            	  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRET";
              case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindParm:
            	  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindParm";
              case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCond:
            	  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCond";
              case ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindLogic:
            	  return "ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindLogic";
		  }
		  return "";
	  }
	  const ValueDecl* getValue() {
		  return value;
	  }
	  CStyleCastOrType getValueCast() {
		  return valuecast;
	  }
	  const CallExpr* getCall() {
		  return call;
	  }

	  int64_t getAddress() {
		  return address;
	  }

	  int64_t getInteger() {
		  return address;
	  }

      double getFloating() {
        return floating;
      }

      std::string getString() {
        return strval;
      }

	  void getCompound() {
	  }
	  std::pair<const MemberExpr*,QualType> getME() {
		  return std::pair<const MemberExpr*,QualType>(ME,METype);
	  }

	  unsigned getMeIdx() {
		  return MEIdx;
	  }

	  unsigned getMeCnt() {
		  return MECnt;
	  }

	  const UnaryOperator* getUnary() {
		  return UO;
	  }

	  const ArraySubscriptExpr* getAS() {
		  return AS;
	  }

      const BinaryOperator* getCAO() {
          return cao;
      }

      const BinaryOperator* getLogic() {
          return cao;
      }

      const OffsetOfExpr* getOOE() {
          return ooe;
      }

      const Expr* getRET() {
			return re;
		}

      const Expr* getParm() {
			return re;
		}

      void setCast(CStyleCastOrType cast) {
    	  valuecast = cast;
      }

	  void setValue(const ValueDecl* v, CStyleCastOrType cast = CStyleCastOrType()) {
		  value = v;
		  valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindValue;
	  }

	  void setCall(const CallExpr* c, CStyleCastOrType cast = CStyleCastOrType()) {
		  call = c;
		  valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCall;
	  }

      void setCAO(const BinaryOperator* CAO, CStyleCastOrType cast = CStyleCastOrType()) {
          cao = CAO;
          valuecast = cast;
          kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCAO;
      }

      void setLogic(const BinaryOperator* CAO, CStyleCastOrType cast = CStyleCastOrType()) {
          cao = CAO;
          valuecast = cast;
          kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindLogic;
      }

      void setOOE(const OffsetOfExpr* OOE, CStyleCastOrType cast = CStyleCastOrType()) {
          ooe = OOE;
          valuecast = cast;
          kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindOOE;
      }

      void setRET(const ReturnStmt* S, CStyleCastOrType cast = CStyleCastOrType()) {
			re = S->getRetValue();
			valuecast = cast;
			kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRET;
		}

      void setParm(const Expr* E, CStyleCastOrType cast = CStyleCastOrType()) {
			re = E;
			valuecast = cast;
			kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindParm;
		}
    
    void setCond(const Expr *E,size_t cf_id, CStyleCastOrType cast = CStyleCastOrType()){
      re = E;
      address = cf_id;
      valuecast = cast;
      kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCond;
    }

	  void setRefCall(const CallExpr* c, const UnaryOperator* __UO, CStyleCastOrType cast = CStyleCastOrType()) {
		  call = c;
		  UO = __UO;
		  valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRefCall;
	  }

	  void setRefCall(const CallExpr* c, const ArraySubscriptExpr* __AS, CStyleCastOrType cast = CStyleCastOrType()) {
		  call = c;
		  AS = __AS;
		  valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRefCall;
	  }

	  void setRefCall(const CallExpr* c, const ValueDecl* VD, CStyleCastOrType cast = CStyleCastOrType()) {
		  call = c;
		  value = VD;
		  valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRefCall;
	  }

	  void setRefCall(const CallExpr* c, int64_t a, CStyleCastOrType cast = CStyleCastOrType()) {
		  call = c;
		  address = a;
		  valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRefCall;
	  }

      void setRefCall(const CallExpr* c, const BinaryOperator* CAO, CStyleCastOrType cast = CStyleCastOrType()) {
          call = c;
          cao = CAO;
          valuecast = cast;
          kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRefCall;
      }

      void setRefCall(const CallExpr* c, const OffsetOfExpr* OOE, CStyleCastOrType cast = CStyleCastOrType()) {
          call = c;
          ooe = OOE;
          valuecast = cast;
          kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRefCall;
      }

      void setRefCall(const CallExpr* c, const ReturnStmt* S, CStyleCastOrType cast = CStyleCastOrType()) {
          call = c;
          re = S->getRetValue();
          valuecast = cast;
          kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindRefCall;
      }

	  void setAddress(int64_t a, CStyleCastOrType cast = CStyleCastOrType()) {
		  address = a;
          valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAddress;
	  }

	  void setInteger(int64_t a, CStyleCastOrType cast = CStyleCastOrType()) {
		  address = a;
          valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindInteger;
	  }

      void setFloating(double f, CStyleCastOrType cast = CStyleCastOrType()) {
        floating = f;
        valuecast = cast;
        kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindFloating;
      }

      void setString(std::string s, CStyleCastOrType cast = CStyleCastOrType()) {
        strval = s;
        valuecast = cast;
        kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindString;
      }

	  void setME(const MemberExpr* __ME, QualType __METype, CStyleCastOrType cast = CStyleCastOrType()) {
		  ME = __ME;
		  METype = __METype;
          valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindMemberExpr;
	  }

	  void setMeIdx(unsigned __MEIdx) {
		  MEIdx = __MEIdx;
	  }
	  void setMeCnt(unsigned __MECnt) {
		  MECnt = __MECnt;
	  }

	  void setUnary(const UnaryOperator* __UO, CStyleCastOrType cast = CStyleCastOrType()) {
		  UO = __UO;
          valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindUnary;
	  }

	  void setAS(const ArraySubscriptExpr* __AS, CStyleCastOrType cast = CStyleCastOrType()) {
		  AS = __AS;
          valuecast = cast;
		  kind = ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAS;
	  }

	  bool operator <(const ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS & otherVC) const {
		if (kind<otherVC.kind) return true;
		if (kind>otherVC.kind) return false;
		if (MEIdx<otherVC.MEIdx) return true;
		if (MEIdx>otherVC.MEIdx) return false;
		if (MECnt<otherVC.MECnt) return true;
		if (MECnt>otherVC.MECnt) return false;
		if (value<otherVC.value) return true;
		if (value>otherVC.value) return false;
		if (address<otherVC.address) return true;
		if (address>otherVC.address) return false;
        if (floating<otherVC.floating) return true;
        if (floating>otherVC.floating) return false;
        if (strval<otherVC.strval) return true;
        if (strval>otherVC.strval) return false;
		if (call<otherVC.call) return true;
		if (call>otherVC.call) return false;
		if (ME<otherVC.ME) return true;
		if (ME>otherVC.ME) return false;
		if (METype<otherVC.METype) return true;
		if ((!(METype<otherVC.METype)) && (METype!=otherVC.METype)) return false;
		if (UO<otherVC.UO) return true;
		if (UO>otherVC.UO) return false;
		if (valuecast<otherVC.valuecast) return true;
		if (valuecast>otherVC.valuecast) return false;
        if (cao<otherVC.cao) return true;
        if (cao>otherVC.cao) return false;
        if (ooe<otherVC.ooe) return true;
        if (ooe>otherVC.ooe) return false;
        if (re<otherVC.re) return true;
        if (re>otherVC.re) return false;
		if (primary<otherVC.primary) return true;
		if (primary>otherVC.primary) return false;
		return AS<otherVC.AS;
	  }

	  bool operator >(const ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS & otherVC) const {
		if (kind<otherVC.kind) return false;
		if (kind>otherVC.kind) return true;
		if (MEIdx<otherVC.MEIdx) return false;
		if (MEIdx>otherVC.MEIdx) return true;
		if (MECnt<otherVC.MECnt) return false;
		if (MECnt>otherVC.MECnt) return true;
		if (value<otherVC.value) return false;
		if (value>otherVC.value) return true;
		if (address<otherVC.address) return false;
		if (address>otherVC.address) return true;
        if (floating<otherVC.floating) return false;
        if (floating>otherVC.floating) return true;
        if (strval<otherVC.strval) return false;
        if (strval>otherVC.strval) return true;
		if (call<otherVC.call) return false;
		if (call>otherVC.call) return true;
		if (ME<otherVC.ME) return false;
		if (ME>otherVC.ME) return true;
		if (METype<otherVC.METype) return false;
		if ((!(METype<otherVC.METype)) && (METype!=otherVC.METype)) return true;
		if (UO<otherVC.UO) return false;
		if (UO>otherVC.UO) return true;
		if (valuecast<otherVC.valuecast) return false;
		if (valuecast>otherVC.valuecast) return true;
        if (cao<otherVC.cao) return false;
        if (cao>otherVC.cao) return true;
        if (ooe<otherVC.ooe) return false;
        if (ooe>otherVC.ooe) return true;
        if (re<otherVC.re) return false;
        if (re>otherVC.re) return true;
		if (primary<otherVC.primary) return false;
		if (primary>otherVC.primary) return true;
		return AS>otherVC.AS;
	  }

	  bool operator==(const ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS & otherVC) const {
		return (kind==otherVC.kind)&&(MEIdx==otherVC.MEIdx)&&(MECnt==otherVC.MECnt)&&(value==otherVC.value)&&
				(address==otherVC.address)&&(floating==otherVC.floating)&&(strval==otherVC.strval)&&(call==otherVC.call)&&(ME==otherVC.ME)&&
                (METype==otherVC.METype)&&
				(UO==otherVC.UO)&&(valuecast==otherVC.valuecast)&&(cao==otherVC.cao)&&(ooe==otherVC.ooe)&&(re==otherVC.re)&&
				(primary==otherVC.primary)&&(AS==otherVC.AS);
	  }

	  bool operator <(const ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS & otherVC) {
		if (kind<otherVC.kind) return true;
		if (kind>otherVC.kind) return false;
		if (MEIdx<otherVC.MEIdx) return true;
		if (MEIdx>otherVC.MEIdx) return false;
		if (MECnt<otherVC.MECnt) return true;
		if (MECnt>otherVC.MECnt) return false;
		if (value<otherVC.value) return true;
		if (value>otherVC.value) return false;
		if (address<otherVC.address) return true;
		if (address>otherVC.address) return false;
        if (floating<otherVC.floating) return true;
        if (floating>otherVC.floating) return false;
        if (strval<otherVC.strval) return true;
        if (strval>otherVC.strval) return false;
		if (call<otherVC.call) return true;
		if (call>otherVC.call) return false;
		if (ME<otherVC.ME) return true;
		if (ME>otherVC.ME) return false;
		if (METype<otherVC.METype) return true;
		if ((!(METype<otherVC.METype)) && (METype!=otherVC.METype)) return false;
		if (UO<otherVC.UO) return true;
		if (UO>otherVC.UO) return false;
		if (valuecast<otherVC.valuecast) return true;
		if (valuecast>otherVC.valuecast) return false;
        if (cao<otherVC.cao) return true;
        if (cao>otherVC.cao) return false;
        if (ooe<otherVC.ooe) return true;
        if (ooe>otherVC.ooe) return false;
        if (re<otherVC.re) return true;
        if (re>otherVC.re) return false;
		if (primary<otherVC.primary) return true;
		if (primary>otherVC.primary) return false;
		return AS<otherVC.AS;
	  }

	  bool operator >(const ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS & otherVC) {
		if (kind<otherVC.kind) return false;
		if (kind>otherVC.kind) return true;
		if (MEIdx<otherVC.MEIdx) return false;
		if (MEIdx>otherVC.MEIdx) return true;
		if (MECnt<otherVC.MECnt) return false;
		if (MECnt>otherVC.MECnt) return true;
		if (value<otherVC.value) return false;
		if (value>otherVC.value) return true;
		if (address<otherVC.address) return false;
		if (address>otherVC.address) return true;
        if (floating<otherVC.floating) return false;
        if (floating>otherVC.floating) return true;
        if (strval<otherVC.strval) return false;
        if (strval>otherVC.strval) return true;
		if (call<otherVC.call) return false;
		if (call>otherVC.call) return true;
		if (ME<otherVC.ME) return false;
		if (ME>otherVC.ME) return true;
		if (METype<otherVC.METype) return false;
		if ((!(METype<otherVC.METype)) && (METype!=otherVC.METype)) return true;
		if (UO<otherVC.UO) return false;
		if (UO>otherVC.UO) return true;
		if (valuecast<otherVC.valuecast) return false;
		if (valuecast>otherVC.valuecast) return true;
        if (cao<otherVC.cao) return false;
        if (cao>otherVC.cao) return true;
        if (ooe<otherVC.ooe) return false;
        if (ooe>otherVC.ooe) return true;
        if (re<otherVC.re) return false;
        if (re>otherVC.re) return true;
		if (primary<otherVC.primary) return false;
		if (primary>otherVC.primary) return true;
		return AS>otherVC.AS;
	  }

	  bool operator==(const ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS & otherVC) {
		return (kind==otherVC.kind)&&(MEIdx==otherVC.MEIdx)&&(MECnt==otherVC.MECnt)&&(value==otherVC.value)&&
				(address==otherVC.address)&&(floating==otherVC.floating)&&(strval==otherVC.strval)&&(call==otherVC.call)&&(ME==otherVC.ME)&&
                (METype==otherVC.METype)&&
				(UO==otherVC.UO)&&(valuecast==otherVC.valuecast)&&(cao==otherVC.cao)&&(ooe==otherVC.ooe)&&(re==otherVC.re)&&
				(primary==otherVC.primary)&&(AS==otherVC.AS);
	  }
  };

  struct callfunc_info_t {
	  bool funcproto;
	  void* FunctionDeclOrProtoType;
	  const CompoundStmt* CS;
	  const CallExpr* CE;
	  void* refObj; // ptr
	  QualType classType;
	  unsigned fieldIndex;
	  std::set<ValueHolder> callrefs;
	  std::set<LiteralHolder> literalRefs;
	  size_t ord;

	  bool operator<(callfunc_info_t other) const {
		  return memcmp(this,&other,sizeof(callfunc_info_t));
	  }
  };
  
  struct refvarinfo_t {
	  unsigned long id;
      double fp;
      std::string s;
	  LiteralHolder lh;
	  enum vartype {
		  CALLVAR_NONE,
		  CALLVAR_FUNCTION,
		  CALLVAR_GLOBAL,
		  CALLVAR_LOCAL,
		  CALLVAR_LOCALPARM,
		  CALLVAR_STRINGLITERAL,
		  CALLVAR_CHARLITERAL,
		  CALLVAR_INTEGERLITERAL,
		  CALLVAR_FLOATLITERAL,
		  CALLVAR_CALLREF,
		  CALLVAR_REFCALLREF,
		  CALLVAR_ADDRCALLREF,
		  CALLVAR_ADDRESS,
		  CALLVAR_INTEGER,
          CALLVAR_FLOATING,
          CALLVAR_STRING,
		  CALLVAR_UNARY,
		  CALLVAR_ARRAY,
		  CALLVAR_MEMBER,
          CALLVAR_ASSIGN,
          CALLVAR_OFFSETOF,
          CALLVAR_LOGIC,

	  } type;
	  unsigned long mi;
	  unsigned long di;
	  long castid;
    refvarinfo_t(): id(-1), fp(0.), s(""), type(CALLVAR_NONE), mi(0), di(0), castid(-1) {}
	  refvarinfo_t(unsigned long id, enum vartype type): id(id), fp(0.), s(""), type(type), mi(0), di(0), castid(-1) {}
	  refvarinfo_t(unsigned long id, enum vartype type, unsigned pos): id(id), fp(0.), s(""), type(type), mi(pos), di(0), castid(-1) {}
      refvarinfo_t(unsigned long id, enum vartype type, unsigned pos, double fp): id(id), fp(fp), s(""), type(type), mi(pos), di(0), castid(-1) {}
      refvarinfo_t(unsigned long id, enum vartype type, unsigned pos, std::string s): id(id), fp(0.), s(s), type(type), mi(pos), di(0), castid(-1) {}
	  refvarinfo_t(LiteralHolder lh, enum vartype type): id(-1), fp(0.), s(""), lh(lh), type(type), mi(0), di(0), castid(-1) {}
	  refvarinfo_t(LiteralHolder lh, enum vartype type, unsigned pos): id(-1), fp(0.), s(""), lh(lh), type(type), mi(pos), di(0), castid(-1) {}
    void set(unsigned long _id, enum vartype _type, long _mi=0, long _di=0, long _castid=-1)
    	{ id = _id; type = _type; mi = _mi; di = _di; castid = _castid; }
    void setFp(unsigned long id, enum vartype _type, long _mi, long _di=0, long _castid=-1, double _fp=0.)
        { id = -1; fp = _fp; s = ""; type = _type; mi = _mi; di = _di; castid = _castid; }
    void setString(unsigned long id, enum vartype _type, long _mi, long _di=0, long _castid=-1, std::string _s="")
        { id = -1; fp = 0.; s = _s; type = _type; mi = _mi; di = _di; castid = _castid; }
	  bool isLiteral() {
		  return (type==CALLVAR_STRINGLITERAL) || (type==CALLVAR_CHARLITERAL) ||
				  (type==CALLVAR_INTEGERLITERAL) || (type==CALLVAR_FLOATLITERAL);
	  }
	  enum vartype getType() {
		  return type;
	  }
	  unsigned getPos() {
		  return mi;
	  }
	  long getCastId() {
		  return castid;
	  }

	  std::string typeString() {
		  if (type==CALLVAR_FUNCTION) return "function";
		  if (type==CALLVAR_GLOBAL) return "global";
		  if (type==CALLVAR_LOCAL) return "local";
		  if (type==CALLVAR_LOCALPARM) return "parm";
		  if (type==CALLVAR_STRINGLITERAL) return "string_literal";
		  if (type==CALLVAR_CHARLITERAL) return "char_literal";
		  if (type==CALLVAR_INTEGERLITERAL) return "integer_literal";
		  if (type==CALLVAR_FLOATLITERAL) return "float_literal";
		  if (type==CALLVAR_CALLREF) return "callref";
		  if (type==CALLVAR_REFCALLREF) return "refcallref";
		  if (type==CALLVAR_ADDRCALLREF) return "addrcallref";
		  if (type==CALLVAR_ADDRESS) return "address";
		  if (type==CALLVAR_INTEGER) return "integer";
          if (type==CALLVAR_FLOATING) return "float";
          if (type==CALLVAR_STRING) return "string";
		  if (type==CALLVAR_UNARY) return "unary";
		  if (type==CALLVAR_ARRAY) return "array";
		  if (type==CALLVAR_MEMBER) return "member";
          if (type==CALLVAR_ASSIGN) return "assign";
          if (type==CALLVAR_OFFSETOF) return "offsetof";
          if (type==CALLVAR_LOGIC) return "logic";
		  return "";
	  }

    bool isReferrent() {
      return (type==CALLVAR_UNARY)||(type==CALLVAR_ARRAY)||(type==CALLVAR_MEMBER)||(type==CALLVAR_ASSIGN)||(type==CALLVAR_OFFSETOF)||(type==CALLVAR_LOGIC);
    }

      std::string idString();
	  std::string LiteralString();
  };

  struct VarData{
    const VarDecl *Node;
    size_t id;
    std::set<QualType> g_refTypes;
    std::set<size_t> g_refVars;
    std::set<const FunctionDecl*> g_refFuncs;
    std::set<LiteralHolder> g_literals;
  };

  typedef std::map<std::string,VarData> VarArray_t;
  VarArray_t VarMap;
  std::map<size_t,const VarDecl*> revVarMap;
  std::vector<std::string> VarIndex;
  size_t VarNum;
  typedef std::map<int,std::multimap<int,const VarDecl*>> taintdata_t;

  enum MemberExprKind {
	  MemberExprKindObject,
	  MemberExprKindPointer,
    MemberExprKindInvalid,
  };

  /*
   *  Contains information about variable reference
   *  Normally VarDecl* would be enough (for simple cases like using simple 'int' variable)
   *  However when referenced variable have a record type we need to know offset for the member used, e.g.
   *  struct A {
   *    int Ai;
   *    short As;
   *  };
   *  struct B {
   *    struct A Ba;
   *  };
   *  {
   *    struct B oB;
   *    (oB.Ba.As);
   *    // Here VarRef would containt the following:
   *    // VDCAM: variable declaration for oB;
   *      This might be (...)
   *    // MemberExprList: [1,0] (rightmost member first)
   *  }
   *  We also need cast type information cause you can do the following in member expression:
   *  (...)
   *
   *  Finally we have to know whether any particular member expression in the chain was '.' or '->'
   *  (...)
   *
   *  The base for the member expression doesn't have to be necessarily a ValueDecl. It can also be a CallExpr or
   *  even an integer value (an address).
   *  (...)
   *
   */
  struct VarRef_t {
	ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS VDCAMUAS;
    std::vector<std::pair<size_t,MemberExprKind>> MemberExprList;
    std::vector<QualType> MECastList;
    std::vector<int64_t> OffsetList;
    std::vector<const CallExpr*> MCallList;
    bool operator <(const VarRef_t & otherVR) const {
      if (VDCAMUAS<otherVR.VDCAMUAS) return true;
      if (VDCAMUAS>otherVR.VDCAMUAS) return false;
      if (MemberExprList<otherVR.MemberExprList) return true;
      if (MemberExprList>otherVR.MemberExprList) return false;
      if (MECastList<otherVR.MECastList) return true;
      if (MECastList>otherVR.MECastList) return false;
      if (MCallList<otherVR.MCallList) return true;
      if (MCallList>otherVR.MCallList) return false;
      return OffsetList<otherVR.OffsetList;
    }
    bool operator >(const VarRef_t & otherVR) const {
      if (VDCAMUAS>otherVR.VDCAMUAS) return true;
      if (VDCAMUAS<otherVR.VDCAMUAS) return false;
      if (MemberExprList>otherVR.MemberExprList) return true;
      if (MemberExprList<otherVR.MemberExprList) return false;
      if (MECastList>otherVR.MECastList) return true;
      if (MECastList<otherVR.MECastList) return false;
      if (MCallList>otherVR.MCallList) return true;
      if (MCallList<otherVR.MCallList) return false;
      return OffsetList>otherVR.OffsetList;
    }
    bool operator==(const VarRef_t & otherVR) const {
      return (VDCAMUAS==otherVR.VDCAMUAS)&&(MemberExprList==otherVR.MemberExprList)&&(MECastList==otherVR.MECastList)&&
    		  (OffsetList==otherVR.OffsetList)&&(MCallList==otherVR.MCallList);
    }
  };

  struct VarInfo_t {
	  const VarDecl* VD;
	  const ParmVarDecl* PVD;
	  const CompoundStmt* CSPtr;
	  const CompoundStmt* parentCSPtr;
	  long varId;
  };

  enum ControlFlowKind{
    cf_none,
    cf_if,
    cf_else,
    cf_for,
    cf_while,
    cf_do,
    cf_switch,
  };
  static std::string ControlFlowName(ControlFlowKind kind = cf_none){
    switch(kind){
      case cf_none: return "none";
      case cf_if: return "if";
      case cf_else: return "else";
      case cf_for: return "for";
      case cf_while: return "while";
      case cf_do: return "do";
      case cf_switch: return "switch";
      default: return "";
    }
  }
  struct ControlFlowData{
    ControlFlowData(ControlFlowKind k, CompoundStmt *_CS) : kind(k), CS(_CS) {}
    ControlFlowKind kind;
    CompoundStmt *CS;
    size_t cond = -1;
  };

  struct IfInfo_t {
	  const IfStmt* ifstmt;
	  const CompoundStmt* CSPtr;
	  const CompoundStmt* parentCSPtr;
  };

  struct GCCAsmInfo_t {
  	  const GCCAsmStmt* asmstmt;
  	  const CompoundStmt* CSPtr;
  	  const CompoundStmt* parentCSPtr;
    };


  enum DereferenceKind {
	  DereferenceUnary,
	  DereferenceArray,
	  DereferenceMember,
	  DereferenceFunction,
	  DereferenceAssign,
	  DereferenceInit,
	  DereferenceOffsetOf,
	  DereferenceReturn,
	  DereferenceParm,
    DereferenceCond,
    DereferenceLogic,
  };
  /*
   * VR: variable being dereferenced
   * i: integer offset
   * vVR: list of variables referenced in offset expressions
   * Expr: raw textual expression
   */

  struct DereferenceInfo_t {
    VarRef_t VR;
    int64_t i;
    std::vector<VarRef_t> vVR;
    std::string Expr;
    const CompoundStmt* CSPtr;
    DereferenceKind Kind;
    unsigned baseCnt;
    std::vector<size_t> ord;
    DereferenceInfo_t(VarRef_t VR, int64_t i, std::vector<VarRef_t> vVR, std::string Expr, const CompoundStmt* CSPtr, DereferenceKind Kind):
      VR(VR), i(i), vVR(vVR), Expr(Expr), CSPtr(CSPtr), Kind(Kind), baseCnt(0) {}
    DereferenceInfo_t(VarRef_t VR, int64_t i, unsigned baseCnt, std::vector<VarRef_t> vVR, std::string Expr, const CompoundStmt* CSPtr,
    		DereferenceKind Kind):
          VR(VR), i(i), vVR(vVR), Expr(Expr), CSPtr(CSPtr), Kind(Kind), baseCnt(baseCnt) {}
    void addOrd(size_t __ord) {
		ord.push_back(__ord);
	}
    bool operator <(const DereferenceInfo_t & otherDI) const {
      if (VR<otherDI.VR) return true;
      if (VR>otherDI.VR) return false;
      if (i<otherDI.i) return true;
      if (i>otherDI.i) return false;
      if (vVR<otherDI.vVR) return true;
      if (vVR>otherDI.vVR) return false;
      if (Kind<otherDI.Kind) return true;
      if (Kind>otherDI.Kind) return false;
      if (baseCnt<otherDI.baseCnt) return true;
      if (baseCnt>otherDI.baseCnt) return false;
      if (CSPtr<otherDI.CSPtr) return true;
      if (CSPtr>otherDI.CSPtr) return false;
      return Expr < otherDI.Expr;
    }
    bool operator >(const DereferenceInfo_t & otherDI) const {
	  if (VR>otherDI.VR) return true;
	  if (VR<otherDI.VR) return false;
	  if (i>otherDI.i) return true;
	  if (i<otherDI.i) return false;
	  if (vVR>otherDI.vVR) return true;
	  if (vVR<otherDI.vVR) return false;
	  if (Kind>otherDI.Kind) return true;
	  if (Kind<otherDI.Kind) return false;
	  if (baseCnt>otherDI.baseCnt) return true;
	  if (baseCnt<otherDI.baseCnt) return false;
	  if (CSPtr>otherDI.CSPtr) return true;
	  if (CSPtr<otherDI.CSPtr) return false;
	  return Expr > otherDI.Expr;
	}
    bool operator==(const DereferenceInfo_t & otherDI) const {
      return (VR==otherDI.VR)&&(i==otherDI.i)&&(vVR==otherDI.vVR)&&(Expr==otherDI.Expr)&&(Kind==otherDI.Kind)
    		  &&(baseCnt==otherDI.baseCnt)&&(CSPtr==otherDI.CSPtr);
    }
    std::string KindString() {
    	if (Kind==DereferenceUnary) return "unary";
    	if (Kind==DereferenceArray) return "array";
    	if (Kind==DereferenceMember) return "member";
    	if (Kind==DereferenceFunction) return "function";
    	if (Kind==DereferenceAssign) return "assign";
    	if (Kind==DereferenceInit) return "init";
    	if (Kind==DereferenceOffsetOf) return "offsetof";
    	if (Kind==DereferenceReturn) return "return";
    	if (Kind==DereferenceParm) return "parm";
      if (Kind==DereferenceCond) return "cond";
      if (Kind==DereferenceLogic) return "logic";
    	return "";
    }
  };

  struct FuncData {
    size_t id;
	  const FunctionDecl* this_func;
	  std::set<callfunc_info_t> funcinfo;
	  std::map<const Expr*,std::vector<std::pair<caseinfo_t,caseinfo_t>>> switch_map;
	  taintdata_t taintdata;
	  int declcount;
	  std::set<QualType> refTypes;
	  std::set<size_t>refVars;
	  std::multimap<size_t, const DeclRefExpr *> refVarInfo;
	  std::set<const FunctionDecl*> refFuncs;
	  long CSId;
	  std::map<const CompoundStmt*,long> csIdMap;
	  std::map<const CompoundStmt*,const CompoundStmt*> csParentMap;
    std::map<const CompoundStmt*,size_t> csInfoMap;
    std::vector<ControlFlowData> cfData;
	  long varId;
	  std::map<const VarDecl*,VarInfo_t> varMap;
	  std::map<const IfStmt*,IfInfo_t> ifMap;
	  std::map<const GCCAsmStmt*,GCCAsmInfo_t> asmMap;
      std::string hash;
      std::string cshash;
      std::set<DereferenceInfo_t> derefList;
      std::set<LiteralHolder> literals;
      std::string firstNonDeclStmtLoc;
  };

  typedef std::map<const FunctionDecl*,FuncData> FuncArray_t;
  FuncArray_t FuncMap;
  size_t FuncNum;
  FuncData* lastFunctionDef, *lastFunctionDefCache;
  std::string lastGlobalVarDecl;
  std::vector<bool> inVarDecl;
  std::stack<const RecordDecl*> recordDeclStack;
  std::vector<FuncData*> functionStack;
  typedef std::map<const FunctionDecl*,size_t> FuncDeclArray_t;
  FuncDeclArray_t FuncDeclMap;
  const FunctionDecl* CTA;
  std::set<const FunctionDecl*> CTAList;
  typedef std::map<std::string,size_t> UnresolvedFuncArray_t;
  UnresolvedFuncArray_t UnresolvedFuncMap;
  std::set<const Expr*> MissingCallExpr;
  std::set<const VarDecl*> MissingVarDecl;
  std::vector<recordInfo_t*> recordIdentifierStack;
  std::set<std::pair<Type::TypeClass,std::string>> missingTypes;
  std::set<std::pair<Decl::Kind,std::string>> unsupportedFuncClass;
  std::set<std::pair<Decl::Kind,std::string>> unsupportedDeclClass;
  std::set<std::string> unsupportedExprRefs;
  std::set<std::string> unsupportedExprWithMemberExprRefs;
  std::set<std::string> unsupportedUnaryOrTypeExprRefs;
  std::set<int> unsupportedAttrRefs;
  std::map<const FunctionDecl*,const FunctionTemplateDecl*> functionTemplateMap;
  std::map<CXXRecordDecl*,const ClassTemplateDecl*> classTemplateMap;
  std::map<const ClassTemplateDecl*,const InjectedClassNameType*> InjectedClassNameMap;
  std::map<const ClassTemplatePartialSpecializationDecl*,const InjectedClassNameType*> InjectedClassNamePSMap;
  std::map<const CXXRecordDecl*,const ClassTemplateSpecializationDecl*> classTemplateSpecializationMap;
  std::map<const CXXRecordDecl*,const ClassTemplatePartialSpecializationDecl*> classTemplatePartialSpecializationMap;
  std::map<const void*,const FriendDecl*> friendDeclMap;
  std::map<CXXRecordDecl*,CXXRecordDecl*> recordParentDeclMap;
  std::vector<const CompoundStmt*> csStack;
  std::map<const RecordDecl *,const CompoundStmt*> recordCSMap;
  std::map<const TemplateTypeParmType*,QualType> templateTypeParmTypeMap;
  std::map<const TemplateTypeParmType*,const void*> templateTypeParmDeclMap;
  std::map<const TemplateSpecializationType*,FunctionDecl*> templateSpecializationFunctionMap;
  std::map<const TemplateSpecializationType*,CXXRecordDecl*> templateSpecializationRecordMap;
  std::map<const TemplateSpecializationType*,TypeAliasDecl*> templateSpecializationTypeAliasMap;
  std::map<const TemplateSpecializationType*,TypedefDecl*> templateSpecializationTypedefMap;
  std::map<const TemplateSpecializationType*,FunctionDecl*> templateSpecializationFunctionMemberMap;
  std::map<QualType, QualType> vaTMap;
  unsigned long missingRefsCount;
  std::map<const RecordDecl*,std::set<size_t>> gtp_refVars;
  std::map<const RecordDecl*,std::set<const FunctionDecl*>> gtp_refFuncs;
  std::map<const EnumDecl*,std::set<size_t>> etp_refVars;
  std::map<const MemberExpr*,const CallExpr*> MEHaveParentCE;
  std::unordered_set<TagDecl*>TypedefRecords;

  bool isTypedefRecord(TagDecl *D){
	  return TypedefRecords.find(D) != TypedefRecords.end();
  }

  size_t getVarNum() {
	  return VarMap.size();
  }

  VarArray_t& getVarMap() {
	  return VarMap;
  }

  VarData& getVarData(std::string name){
    return VarMap.at(name);
  }

  VarData& getVarData(const VarDecl *D){
    return getVarData(D->getNameAsString());
  }
  
  std::vector<std::string>& getVarIndex() {
	  return VarIndex;
  }

  size_t getFuncNum() {
	  return FuncMap.size();
  }

  FuncArray_t& getFuncMap() {
	  return FuncMap;
  }
  
  FuncData &getFuncData(const FunctionDecl *D){
    return FuncMap.at(D);
  }

  size_t getFuncDeclNum() {
	  return FuncDeclMap.size();
  }

  FuncDeclArray_t& getFuncDeclMap() {
	  return FuncDeclMap;
  }

  size_t getUnresolvedFuncNum() {
  	  return UnresolvedFuncMap.size();
    }

  UnresolvedFuncArray_t& getUnresolvedFuncMap() {
  	  return UnresolvedFuncMap;
    }

  std::vector<const CompoundStmt*>& getCSStack() { return csStack; }
  std::map<const RecordDecl *,const CompoundStmt*> getRecordCSMap() { return recordCSMap; }
  std::string getAbsoluteLocation(SourceLocation L);
  bool hasNamedFields(RecordDecl* rD);
  bool emptyRecordDecl(RecordDecl* rD);
  bool isNamedField(Decl* D);
  bool declGroupHasNamedFields(Decl** Begin, unsigned NumDecls);
  int fieldIndexInGroup(Decl** Begin, unsigned NumDecls, const FieldDecl* FD, int startIndex);
  bool fieldMatch(Decl* D, const FieldDecl* FD);
  QualType typeForMap(QualType T);
  void noticeTypeClass(QualType T);
  void noticeTemplateParameters(TemplateParameterList* TPL);
  void noticeTemplateArguments(const TemplateArgumentList& Args);
  bool VisitEnumDecl(const EnumDecl *D);
  bool VisitRecordDecl(const RecordDecl *D);
  bool VisitCXXRecordDecl(const CXXRecordDecl* D);
  bool VisitCXXMethodDecl(const CXXMethodDecl* D);
  bool VisitDecl(Decl *D);
  bool TraverseDecl(Decl *D);
  bool VisitFunctionDeclStart(const FunctionDecl *D);
  bool VisitFunctionDeclComplete(const FunctionDecl *D);
  bool handleCallDeclRefOrMemberExpr(const Expr* E, std::set<ValueHolder> callrefs, std::set<LiteralHolder> literalRefs, const QualType* baseType = 0, const CallExpr* CE = 0);
  bool handleCallAddress(int64_t Addr,const Expr* AddressExpr,std::set<ValueHolder> callrefs,std::set<DbJSONClassVisitor::LiteralHolder> literalRefs, const QualType* baseType = 0,
    const CallExpr* CE = 0, const CStyleCastExpr* CSCE = 0);
  bool handleCallStmtExpr(const Expr* E, std::set<ValueHolder> callrefs, std::set<LiteralHolder> literalRefs, const QualType* baseType = 0, const CallExpr* CE = 0);
  const DbJSONClassVisitor::callfunc_info_t* handleCallMemberExpr(const MemberExpr* ME, std::set<ValueHolder> callrefs, std::set<LiteralHolder> literalRefs, const QualType* baseType = 0, const CallExpr* CE = 0);
  const DbJSONClassVisitor::callfunc_info_t* handleCallVarDecl(const VarDecl* VD, const DeclRefExpr* DRE, std::set<ValueHolder> callrefs, std::set<LiteralHolder> literalRefs, const QualType* baseType = 0, const CallExpr* CE = 0);
  bool handleCallConditionalOperator(const ConditionalOperator* CO, std::set<ValueHolder> callrefs, std::set<LiteralHolder> literalRefs, const QualType* baseType = 0, const CallExpr* CE = 0);
  const Expr* stripCasts(const Expr* E, QualType* castType = 0);
  const Expr* stripCastsEx(const Expr* E, std::vector<CStyleCastOrType>& vC);
  const UnaryOperator* lookForUnaryOperatorInCallExpr(const Expr* E);
  bool verifyMemberExprBaseType(QualType T);
  const ArraySubscriptExpr* lookForArraySubscriptExprInCallExpr(const Expr* E);
  size_t ExtractFunctionId(const FunctionDecl *FD);
  bool VisitExpr(const Expr *Node);
  bool VisitClassTemplateDecl(const ClassTemplateDecl *D);
  bool VisitClassTemplateSpecializationDecl(const ClassTemplateSpecializationDecl *D);
  bool VisitClassTemplatePartialSpecializationDecl(const ClassTemplatePartialSpecializationDecl *D);
  bool VisitFunctionTemplateDecl(const FunctionTemplateDecl *D);
  bool VisitStmt(Stmt *S);
  bool VisitTypedefDecl(TypedefDecl *D);
  bool VisitTypedefDeclFromClass(TypedefDecl *D);
  bool VisitTypeAliasDecl(TypeAliasDecl *D);
  bool VisitTypeAliasDeclFromClass(TypeAliasDecl *D);
  bool VisitRecordDeclStart(const RecordDecl *D);
  bool VisitRecordDeclComplete(const RecordDecl *D);
  bool VisitVarDeclStart(const VarDecl *D);
  bool VisitVarDeclComplete(const VarDecl *D);
  bool VisitVarDecl(const VarDecl *VD);
  bool VisitCastExpr(const CastExpr *Node);
  bool VisitFriendDecl(const FriendDecl *D);
  bool TraverseStmt(Stmt* S);
  bool VisitCompoundStmtStart(const CompoundStmt *CS);
  bool VisitCompoundStmtComplete(const CompoundStmt *CS);
  void handleConditionDeref(Expr *Cond,size_t cf_id);
  bool VisitSwitchStmt(SwitchStmt *S);
  bool VisitIfStmt(IfStmt *S);
  bool VisitForStmt(ForStmt *S);
  bool VisitWhileStmt(WhileStmt *S);
  bool VisitDoStmt(DoStmt *S);
  bool VisitGCCAsmStmt(GCCAsmStmt *S);
  // bool VisitInitListExpr(InitListExpr* ILE); [CHECK IF NEEDED - DOES NOTHING]
  bool VisitBinaryOperator(BinaryOperator *BO);
  bool VisitUnaryOperator(UnaryOperator *E);
  bool VisitArraySubscriptExpr(ArraySubscriptExpr *Node);
  bool VisitReturnStmt(const ReturnStmt *S);
  bool VisitMemberExpr(MemberExpr *Node);
  bool VisitCallExpr(CallExpr *CE);
  bool VisitDeclRefExpr(DeclRefExpr *DRE);
  bool VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *UTTE);
  bool VisitCompoundLiteralExpr(CompoundLiteralExpr *CLE);
  bool VisitOffsetOfExpr(OffsetOfExpr *Node);

  bool VisitIntegerLiteral(IntegerLiteral *L);
  bool VisitFloatingLiteral(FloatingLiteral *L);
  bool VisitCharacterLiteral(CharacterLiteral *L);
  bool VisitStringLiteral(StringLiteral *L);

  const Expr* lookForDeclReforMemberExpr(const Expr* E);
  const Expr* lookForStmtExpr(const Expr* E);
  void lookForDeclRefExprsWithStmtExpr(const Expr* E, std::set<ValueHolder>& refs, unsigned pos = 0 );
  void lookForDeclRefExprs(const Expr* E, std::set<ValueHolder>& refs, unsigned pos = 0 );
  bool lookForVarReference(const Expr* E, VarRef_t& VR, bool* ignoreError=0);
  bool lookForVarReferencesInOffsetExpr(const Expr* E, VarRef_t& VR, int64_t* LiteralOffset,
      std::vector<VarRef_t>& OffsetRefs, bool* VarDone, BinaryOperatorKind kind);
  bool computeOffsetExpr(const Expr* E, int64_t* LiteralOffset, std::vector<VarRef_t>& OffsetRefs,
		  BinaryOperatorKind kind, bool stripCastFlag = false);
  bool tryComputeOffsetExpr(const Expr* E, int64_t* LiteralOffset, BinaryOperatorKind kind, bool stripCastFlag = false);
  bool mergeBinaryOperators(const BinaryOperator* BO, int64_t* LiteralOffset, std::vector<VarRef_t>& OffsetRefs,
      BinaryOperatorKind kind);
  const Expr* lookForNonTransitiveExpr(const Expr* E);
  const Expr* lookForNonParenExpr(const Expr* E);
  bool tryEvaluateIntegerConstantExpr(const Expr* E, Expr::EvalResult& Res);
  bool lookForDerefExprs(const Expr* E, int64_t* LiteralOffset, std::vector<VarRef_t>& OffsetRefs);
  bool lookForASExprs(const ArraySubscriptExpr *Node, VarRef_t& VR, int64_t* LiteralOffset, std::vector<VarRef_t>& OffsetRefs,
      bool* ignoreErrors);
  void lookForExplicitCastExprs(const Expr* E, std::vector<QualType>& refs );
  void lookForLiteral(const Expr* E, std::set<LiteralHolder>& refs, unsigned pos = 0 );
  const DeclRefExpr* lookForBottomDeclRef(const Expr* E);
  int fieldToIndex(const FieldDecl* FD, const RecordDecl* RD);
  int fieldToFieldIndex(const FieldDecl* FD, const RecordDecl* RD);
  void setSwitchData(const Expr* caseExpr, int64_t* enumtp, std::string* enumstr, std::string* macroValue, std::string* raw_code, int64_t* exprVal);
  void varInfoForRefs(FuncData &func_data, const std::set<ValueHolder>& refs, std::set<LiteralHolder> literals, std::vector<struct refvarinfo_t>& refvarList);
  bool VR_referenced(VarRef_t& VR, std::set<const MemberExpr*>& MERef,std::set<const UnaryOperator*>& UnaryRef, std::set<const ArraySubscriptExpr*>& ASRef,
        std::set<const BinaryOperator*>& CAORef,std::set<const BinaryOperator*>& LogicRef, std::set<const OffsetOfExpr*>& OOERef);
  bool varInfoForVarRef(FuncData &func_data, VarRef_t VR,  struct refvarinfo_t& refvar,
		  std::map<const CallExpr*,unsigned long>& CEIdxMap,
		  std::map<const MemberExpr*,unsigned>& MEIdxMap,
		  std::map<const UnaryOperator*,unsigned>& UnaryIdxMap,
		  std::map<const ArraySubscriptExpr*,unsigned>& ASIdxMap,
		  std::map<const ValueDecl*,unsigned>& VDIdxMap,
      std::map<const BinaryOperator*,unsigned>& CAOIdxMap,
      std::map<const BinaryOperator*,unsigned>& LogicIdxMap,
      std::map<const OffsetOfExpr*,unsigned>& OOEIdxMap);
  void notice_class_references(RecordDecl* rD);
  void notice_field_attributes(RecordDecl* rD, std::vector<QualType>& QV);
  void notice_template_class_references(CXXRecordDecl* TRD);
  void lookForAttrTypes(const Attr* attr, std::vector<QualType>& QV);
  bool lookForUnaryExprOrTypeTraitExpr(const Expr* e, std::vector<QualType>& QV);
  bool lookForTypeTraitExprs(const Expr* e, std::vector<const TypeTraitExpr*>& TTEV);
  const TemplateSpecializationType* LookForTemplateSpecializationType(QualType T);
  size_t outerFunctionorMethodIdforTagDecl(TagDecl* rD);
  std::string parentFunctionOrMethodString(TagDecl* tD);
  size_t getFunctionDeclId(const FunctionDecl* FD);
  QualType lookForNonPointerType(const PointerType* tp);
  const FunctionProtoType* lookForFunctionType(QualType T);

  typedef std::tuple<MemberExpr*,CastExprOrType,int64_t,const CallExpr*,CStyleCastOrType> lookup_cache_tuple_t;
  typedef std::multimap<ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS,std::vector<lookup_cache_tuple_t>> DREMap_t;
  void lookForDeclRefWithMemberExprsInOffset(const Expr* E, std::vector<VarRef_t>& OffsetRefs);
  bool lookForDeclRefWithMemberExprs(const Expr* E, DREMap_t& refs);
    typedef std::tuple<
    		std::vector<MemberExpr*>,
			std::vector<CastExprOrType>,
			std::vector<int64_t>,
			std::vector<const CallExpr*>,
			std::vector<CStyleCastOrType>> lookup_cache_t;

  std::vector<MemberExpr*>& get_member(lookup_cache_t& cache) {
	  return std::get<0>(cache);
  }

  std::vector<CastExprOrType>& get_type(lookup_cache_t& cache) {
	  return std::get<1>(cache);
  }

  std::vector<int64_t>& get_shift(lookup_cache_t& cache) {
	  return std::get<2>(cache);
  }

  std::vector<const CallExpr*>& get_callref(lookup_cache_t& cache) {
	  return std::get<3>(cache);
  }

  std::vector<CStyleCastOrType>& get_ccast(lookup_cache_t& cache) {
	  return std::get<4>(cache);
  }

  void lookup_cache_clear(lookup_cache_t& cache) {
	  std::get<0>(cache).clear();
	  std::get<1>(cache).clear();
	  std::get<2>(cache).clear();
	  std::get<3>(cache).clear();
	  std::get<4>(cache).clear();
  }

  typedef std::vector<lookup_cache_tuple_t> vMCtuple_t;
  bool DREMap_add(DREMap_t& DREMap, ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS& v, vMCtuple_t& vMCtuple);
  void lookForDeclRefWithMemberExprsInternal(const Expr* E, const Expr* origExpr, DREMap_t& refs, lookup_cache_t& cache,
		  bool* compoundStmtSeen = 0, unsigned MEIdx = 0, unsigned* MECnt = 0, const CallExpr* CE = 0,
		  bool secondaryChain = false, bool IgnoreLiteral = false, bool noticeInitListExpr = false, QualType castType = QualType());
  void lookForDeclRefWithMemberExprsInternalFromStmt(const Stmt* S, DREMap_t& refs, lookup_cache_t& cache,
  		bool* compoundStmtSeen = 0, unsigned MEIdx = 0, const CallExpr* CE = 0, bool secondaryChain = false);

  std::set<MemberExpr*> DoneMEs;
  std::set<InitListExpr*> DoneILEs;

  bool currentWithinCS() { return csStack.size()>0; }
  bool hasParentCS() { return csStack.size()>1; }

  const CompoundStmt* getCurrentCSPtr() {
	  return csStack.back();
  }
  const CompoundStmt* getParentCSPtr() {
	  return *(csStack.end()-2);
  }

  std::string ExprToString(const Expr* E) {
  	std::string Expr;
  	llvm::raw_string_ostream exprstream(Expr);
  	E->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
  	exprstream.flush();
  	return Expr;
  }

private:
  ASTContext &Context;
  const struct main_opts& _opts;
};

typedef std::tuple<std::string,std::string,std::string> MacroDefInfo;

class DbJSONClassConsumer : public clang::ASTConsumer {
public:
  explicit DbJSONClassConsumer(ASTContext &Context, const std::string* sourceFile,
		  const std::string* directory, const struct main_opts& opts, Preprocessor &PP, bool save_exps)
    : Visitor(Context,opts), CSVisitor(Context,opts), _sourceFile(sourceFile), _directory(directory), _opts(opts),
	  TUId(0), Context(Context), Macros(PP,save_exps) {
    }

  static QualType getDeclType(Decl* D) {
    if (TypedefNameDecl* TDD = dyn_cast<TypedefNameDecl>(D))
      return TDD->getUnderlyingType();
    if (ValueDecl* VD = dyn_cast<ValueDecl>(D))
      return VD->getType();
    return QualType();
  }

  static QualType GetBaseType(QualType T) {
    // FIXME: This should be on the Type class!
    QualType BaseType = T;
    while (!BaseType->isSpecifierType()) {
      if (isa<TypedefType>(BaseType))
        break;
      else if (const PointerType* PTy = BaseType->getAs<PointerType>())
        BaseType = PTy->getPointeeType();
      else if (const BlockPointerType *BPy = BaseType->getAs<BlockPointerType>())
        BaseType = BPy->getPointeeType();
      else if (const ArrayType* ATy = dyn_cast<ArrayType>(BaseType))
        BaseType = ATy->getElementType();
      else if (const FunctionType* FTy = BaseType->getAs<FunctionType>())
        BaseType = FTy->getReturnType();
      else if (const VectorType *VTy = BaseType->getAs<VectorType>())
        BaseType = VTy->getElementType();
      else if (const ReferenceType *RTy = BaseType->getAs<ReferenceType>())
        BaseType = RTy->getPointeeType();
      else if (const AutoType *ATy = BaseType->getAs<AutoType>())
        BaseType = ATy->getDeducedType();
      else if (const AttributedType *AtrTy = BaseType->getAs<AttributedType>())
    	  BaseType = AtrTy->getModifiedType();
      else if (const ParenType *PTy = BaseType->getAs<ParenType>())
        BaseType = PTy->desugar();
      else
        llvm_unreachable("Unknown declarator!");
    }
    return BaseType;
  }

  static std::string translateLinkage(clang::Linkage linkage) {

	  switch(linkage) {
		case clang::NoLinkage:
			return "none";
		case clang::InternalLinkage:
			return "internal";
		case clang::UniqueExternalLinkage:
			return "";
		case clang::VisibleNoLinkage:
			return "";
		case clang::ModuleInternalLinkage:
			return "";
		case clang::ModuleLinkage:
			return "";
		case clang::ExternalLinkage:
			return "external";
		default:
			return "";
	  }
  }
  
  enum {
	  EXTRA_BITFIELD = 1,
	  EXTRA_RECORDDECL = 2,
	  EXTRA_ENUMDECL = 4,
	  EXTRA_TEMPLATEDECL = 8,
	  EXTRA_TYPEDEF_BOOL = 16,
	  EXTRA_TYPEDEFDECL = 32,
	  EXTRA_TYPEALIASDECL = 64,
	  EXTRA_TYPEALIASTEMPLATEDECL = 128,
	  EXTRA_METHODDECL = 256,
  };
  
  typedef std::vector<std::pair<int,std::pair<int,unsigned long long>>> TypeGroup_t;
  typedef std::vector<std::pair<int,std::pair<int,unsigned long long>>> MethodGroup_t;
  typedef std::vector<std::pair<QualType,unsigned>> type_args_t;
  typedef std::vector<std::pair<std::string,unsigned>> nontype_args_t;
  typedef std::pair<type_args_t,nontype_args_t> template_args_t;
  typedef std::map<int,QualType> defaut_type_map_t;
  typedef std::map<int,Expr*> default_nontype_map_t;
  typedef std::tuple<defaut_type_map_t,default_nontype_map_t,std::vector<int>> template_default_map_t;
  
  std::string getAbsoluteLocation(SourceLocation L);
  void printGlobalArray(int Indentation);
  void computeTypeHashes();
  void computeFuncHashes();
  void printFuncArray(int Indentation);
  void printFuncDeclArray(int Indentation);
  void printUnresolvedFuncArray(int Indentation);
  int getDeclId(Decl* D, std::pair<int,unsigned long long>& extraArg);
  TypeGroup_t getTypeGroupIds(Decl** Begin, unsigned NumDecls, const PrintingPolicy &Policy);
  void get_class_references(RecordDecl* rD, TypeGroup_t& Ids, MethodGroup_t& MIds, std::vector<int>& rIds, std::vector<std::string>& rDef);
  void printTemplateParameters(TemplateParameterList* TPL, const std::string& Indent,
		  bool printDefaults = true);
  template_default_map_t getTemplateParameters(TemplateParameterList* TPL, const std::string& Indent,
		  bool getDefaults = true);
  void printTemplateParameters(template_default_map_t template_parms, const std::string& Indent,
		  bool printDefaults = true);
  void printTemplateTypeDefaults(defaut_type_map_t default_type_map, std::map<int,int> type_parms_idx,const std::string& Indent, bool nextJSONitem = true);
  void printTemplateArgs(const TemplateArgumentList&, TemplateParameterList* Params, const std::string& Indent);
  void printTemplateArgs(ArrayRef<TemplateArgument> Args, TemplateParameterList* Params, const std::string& Indent);
  void printTemplateArgs(template_args_t& template_args, const std::string& Indent);
  void printTemplateArgs(template_args_t& template_args, std::vector<int> type_args_idx, const std::string& Indent, bool nextJSONitem=true);
  void getTemplateArguments(const TemplateArgumentList&, const std::string& Indent, template_args_t& template_args);
  void getTemplateArguments(ArrayRef<TemplateArgument> Args, const std::string& Indent, template_args_t& template_args);

  void buildTemplateArgumentsString(const TemplateArgumentList& Args,
	  	  	  std::string& typeString, std::pair<bool,unsigned long long> extraArg = {0,0});

  void buildTemplateArgumentsString(ArrayRef<TemplateArgument> Args,
	  	  	  std::string& typeString, std::pair<bool,unsigned long long> extraArg = {0,0});

  void printTypeInternal(QualType T, const std::string& Indent);

  std::string render_switch_json(const Expr* cond,
		  std::vector<std::pair<DbJSONClassVisitor::caseinfo_t,DbJSONClassVisitor::caseinfo_t>>& caselst,
		  std::string Indent);

  void buildTypeGroupString(Decl** Begin, unsigned NumDecls, std::string& typeString);
  void buildTypeClassString(Decl* D, std::string& typeString);

  void LookForTemplateTypeParameters(QualType T, std::set<const TemplateTypeParmType*>& s);

  void buildRecordTypeString(QualType T, RecordDecl* rD, std::string& qualifierString,
							  std::string& typeString,
							  std::pair<int,unsigned long long> extraArg = std::pair<int,unsigned long long>(0,0));

  void buildTypeString(QualType T, std::string& typeString,
					  std::pair<int,unsigned long long> extraArg = std::pair<int,unsigned long long>(0,0));
  
  void printTypeMap(int Indentation);
  void printDatabase();
  virtual void HandleTranslationUnit(clang::ASTContext &Context);
  
  virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
	  return true;
      }
private:
  void printCallInfo(const DbJSONClassVisitor::callfunc_info_t&, std::stringstream& ss, clang::SourceManager& SM, std::vector<const CallExpr*>& CEv);
  void printCallInfo(const CallExpr* CE, std::stringstream& ss, clang::SourceManager& SM, size_t ord, std::vector<const CallExpr*>& CEv);


  void getFuncDeclSignature(const FunctionDecl* D, std::string& fdecl_sig);

  std::map<int,std::tuple<QualType,std::string,DbJSONClassVisitor::recordInfo_t*>> revTypeMap;
  // Mapping of types to computed type strings (to speed-up type string creation)
  std::map<QualType,std::string> TypeStringMap;
  DbJSONClassVisitor Visitor;
  CompoundStmtClassVisitor CSVisitor;
  const std::string* _sourceFile;
  const std::string* _directory;
  const struct main_opts& _opts;
  uint64_t TUId;
  ASTContext &Context;
  std::vector<MacroDefInfo> mexps;
  MacroHandler Macros;
};
void load_database(std::string filepath);
int internal_declcount(const FunctionDecl *F);
void taint_params(const clang::FunctionDecl *F, DbJSONClassVisitor::FuncData&);
bool isOwnedTagDeclType(QualType DT);
QualType resolve_Typedef_Integer_Type(QualType T);
QualType resolve_Record_Type(QualType T);

static inline bool is_compiletime_assert_decl(const FunctionDecl* D, ASTContext& Context ) {
	  if ((D->getNameAsString().rfind("__compiletime_assert_", 0) == 0) && /* .startswith() */
			  (D->getNameAsString().substr(21).find_first_not_of("0123456789")==std::string::npos)) {
		  QualType rT = D->getReturnType();
		  if (rT->getTypeClass()==Type::Builtin) {
			  const BuiltinType *tp = cast<BuiltinType>(rT);
			  if (tp->getName(Context.getPrintingPolicy()).str()=="void") {
				  if (D->getNumParams()==0) {
					  return true;
				  }
			  }
		  }
	  }
	  return false;
}

static inline bool isCXXTU(clang::ASTContext &Context) {
    LangOptions lang = Context.getLangOpts();
          if (lang.CPlusPlus||lang.CPlusPlus11||lang.CPlusPlus14) return true;
          return false;
}

static inline std::string getQualifierStringInternal(Qualifiers q) {
  std::string qs;
  if (q.hasConst()) {
    qs.append("c");
  }
  if (q.hasRestrict()) {
    qs.append("r");
  }
  if (q.hasVolatile()) {
    qs.append("v");
  }
  return qs;
}

static inline std::string getQualifierString(QualType T) {
  return getQualifierStringInternal(T.getLocalQualifiers());
}

static inline std::string getDecl(std::string fbody) {
    std::size_t loc = fbody.find("\n");
    if (loc!=std::string::npos) {
      return fbody.substr(0,loc-2);
    }
    else {
      return fbody;
    }
  }

static inline QualType walkTypedefType(QualType T) {
  if (T->getTypeClass()==Type::Typedef) {
	  const TypedefType *tp = cast<TypedefType>(T);
	  TypedefNameDecl* D = tp->getDecl();
	  QualType tT = D->getTypeSourceInfo()->getType();
	  return walkTypedefType(tT);
  }
  else {
	  return T;
  }
}

static inline QualType walkToFunctionProtoType(QualType T) {
  if (T->getTypeClass()==Type::Typedef) {
	  const TypedefType *tp = cast<TypedefType>(T);
	  TypedefNameDecl* D = tp->getDecl();
	  QualType tT = D->getTypeSourceInfo()->getType();
	  return walkToFunctionProtoType(tT);
  }
  else if (T->getTypeClass()==Type::Paren) {
	  const ParenType *tp = cast<ParenType>(T);
	  QualType iT = tp->getInnerType();
	  return walkToFunctionProtoType(iT);
  }
  else if (T->getTypeClass()==Type::Pointer) {
	  const PointerType *tp = cast<PointerType>(T);
	  QualType ptrT = tp->getPointeeType();
	  return walkToFunctionProtoType(ptrT);
  }
  else if (T->getTypeClass()==Type::FunctionProto) {
	  return T;
  }
  else {
	  return QualType();
  }
}
