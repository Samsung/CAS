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

#define DEBUG_TYPESTRING 0
#define DEBUG_HASH 0

extern thread_local size_t exprOrd;

struct anonRecord {
  std::vector<std::string> fieldNames;
  std::string hash;
};

static std::string JSONReplacementToken = "\"\"\"$\"\"\"";
static std::string ORDReplacementToken = "\"\"\"@\"\"\"";
static std::string ARGeplacementToken = "\"\"\"&\"\"\"";

class DbJSONClassVisitor
  : public RecursiveASTVisitor<DbJSONClassVisitor> {
  using Base = RecursiveASTVisitor<DbJSONClassVisitor>;
public:
  int tab = 0;
  explicit DbJSONClassVisitor(ASTContext &Context)
    : TypeNum(0), VarNum(0), FuncNum(0), lastFunctionDef(0), CTA(0), missingRefsCount(0), Context(Context) {}

    bool shouldVisitImplicitCode() const {return true;}
    bool shouldVisitTemplateInstantiations() const {return true;}

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
      COMPAT_VERSION_GE(14,
      if (isa<UsingType>(BaseType))
        break;
      )
      else if (const PointerType* PTy = BaseType->getAs<PointerType>())
        BaseType = PTy->getPointeeType();
      else if (const BlockPointerType *BPy = BaseType->getAs<BlockPointerType>())
        BaseType = BPy->getPointeeType();
      else if (const MemberPointerType *MPy = BaseType->getAs<MemberPointerType>())
        BaseType = MPy->getPointeeType();
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
      COMPAT_VERSION_GE(15,
      else if (const BTFTagAttributedType *AtrTy = BaseType->getAs<BTFTagAttributedType>())
        BaseType = AtrTy->getWrappedType();
      )
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
  	struct {
  		std::string stringLiteral;
  		llvm::APSInt integerLiteral;
  		unsigned int charLiteral;
  		llvm::APFloat floatingLiteral{llvm::APFloat::IEEEdouble()};
  	};
  	unsigned pos;
  	bool literalLower(const LiteralHolder &other) const {
      if (type < other.type) return true;
      if (type > other.type) return false;
  		if (type==LiteralString) {
  			return stringLiteral.compare(other.stringLiteral)<0;
  		}
  		if (type==LiteralInteger) {
  			return integerLiteral.extOrTrunc(63).getExtValue() < other.integerLiteral.extOrTrunc(63).getExtValue();
  		}
  		if (type==LiteralChar) {
  			return charLiteral < other.charLiteral;
  		}
  		if (type==LiteralFloat) {
  			return floatingLiteral.convertToDouble() < other.floatingLiteral.convertToDouble();
  		}
      assert( 0 && "Wrong literal type.");
  	}
  	bool operator<(const LiteralHolder &other) const {
  		return literalLower(other);
  	}
  };

  std::string LiteralString(LiteralHolder &lh);

  typedef std::vector<std::string> recordFields_t;
  typedef std::vector<size_t> recordOffsets_t;
  typedef std::pair<recordFields_t,recordOffsets_t> recordInfo_t;

  class ObjectID{
      size_t _id = 0;
      bool is_set = false;
    public:
      ObjectID(){}
      void setID(size_t id){_id = id;}
      void setIDProper(size_t id){_id = id; is_set = true;}
      operator size_t(){
        assert(is_set);
        return _id;
      }
  };
  struct TypeData{
    ObjectID id;
    QualType T;
    size_t size;
    recordInfo_t *RInfo;
    std::string hash;
    std::shared_ptr<std::string> output;
    void *usedrefs;
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
		  llvm::outs()<<"Type not in map:"<<T.getAsOpaquePtr()<<'\n';
		  T.dump();
		  assert(0);
	  }
	  return TypeMap.at(T);
  }

  struct FopsObject{
    const QualType T;
    const VarDecl *V;
    const FunctionDecl *F;
    enum FopsKind{
      FObjGlobal,
      FObjLocal,
      FObjFunction,
    } kind;
    std::string FopsKindName() const {
      switch(kind){
        case FObjGlobal:
          return "global";
        case FObjLocal:
          return "local";
        case FObjFunction:
          return "function";
        default:
          assert(0 && "Invalid Fops kind");
      }
    }

    FopsObject(QualType Type, const VarDecl *Var, const FunctionDecl *Func = nullptr) : T(Type), V(Var), F(Func) {
      if(!Var){
        // fallback case without associated variable
        kind = FObjFunction;
      }
      else{
        if(Var->isDefinedOutsideFunctionOrMethod()){
          // global
          kind = FObjGlobal;
        }
        else{
          // local
          kind = FObjLocal;
        }
      }
    }

    bool operator<(const FopsObject &other) const {
      if(kind != other.kind){
        return kind < other.kind;
      }
      else if(T != other.T){
        return T <other.T;
      }
      else if(V){
        return V <other.V;
      }
      else{
        return F <other.F;
      }
    }
    operator bool() const {
      return bool(kind);
    }
  };

  struct FopsData{
    FopsData(FopsObject &FObj) : obj(FObj) {}
    FopsObject obj;
    std::string hash;
    size_t type_id;
    size_t var_id;
    size_t func_id;
    std::string loc;
    std::map <size_t,std::set<const FunctionDecl*>> fops_info; //member_id : function id
    std::shared_ptr<std::string>output;

    bool operator<(const FopsData &other) const {
      return obj <other.obj;
    }
  };

  typedef std::map<FopsObject,FopsData> FopsArray_t;
  FopsArray_t FopsMap;

  FopsArray_t &getFopsMap(){
    return FopsMap;
  }

  typedef std::pair<int64_t,std::string> caseenum_t;
  typedef std::tuple<caseenum_t,std::string,std::string,int64_t> caseinfo_t;

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
    if(vC.empty()) return {};
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

  struct ExprRef_t {
	  enum ExprRefKind {
		  ExprRefKindNone,
		  ExprRefKindAddress,     //internal
		  ExprRefKindInteger,     //internal
		  ExprRefKindFloating,    //internal
		  ExprRefKindString,      //internal
		  ExprRefKindValue,       //main: function, init; internal
		  ExprRefKindCall,        //main: function; internal; special handling
		  ExprRefKindRefCall,     //main: function; internal; special handling
		  ExprRefKindMemberExpr,  //main: member; internal
		  ExprRefKindUnary,       //main: unary; internal
		  ExprRefKindAS,          //main: array; internal
		  ExprRefKindCAO,         //main: assign; internal
		  ExprRefKindOOE,         //main: offsetof; internal
		  ExprRefKindLogic,       //main: logic; internal
			External,
		  ExprRefKindRET = External,         //main: return
		  ExprRefKindParm,        //main: parm
		  ExprRefKindCond,        //main: cond
      ExprRefKindUnhandled,    //main: missing cpp support
	  };

	  ExprRef_t(): value(0), call(0), address(0), floating(0.), strval(""), MEIdx(0), MECnt(0),
			  expr(0), valuecast(), kind(ExprRefKindNone) {}
	  const ValueDecl* value;
	  const CallExpr* call;
	  int64_t address;
		double floating;
		std::string strval;
	  unsigned MEIdx;
	  unsigned MECnt;
		const Expr *expr;
	  CStyleCastOrType valuecast;
	  ExprRefKind kind;
		bool isSet = false;


		bool isRefKind(){
			return kind >= ExprRefKindMemberExpr && kind < External;
		}

	  ExprRefKind getKind() {
		  return kind;
	  }
	  std::string getKindString() const {
		  switch(kind) {
				case ExprRefKindNone:
					return "ExprRefKindNone";
				case ExprRefKindValue:
					return "ExprRefKindValue";
				case ExprRefKindCall:
					return "ExprRefKindCall";
				case ExprRefKindRefCall:
					return "ExprRefKindRefCall";
				case ExprRefKindAddress:
					return "ExprRefKindAddress";
				case ExprRefKindFloating:
					return "ExprRefKindFloating";
				case ExprRefKindString:
					return "ExprRefKindString";
				case ExprRefKindInteger:
					return "ExprRefKindInteger";
				case ExprRefKindMemberExpr:
					return "ExprRefKindMemberExpr";
				case ExprRefKindUnary:
					return "ExprRefKindUnary";
				case ExprRefKindAS:
					return "ExprRefKindAS";
				case ExprRefKindCAO:
					return "ExprRefKindCAO";
				case ExprRefKindOOE:
					return "ExprRefKindOOE";
				case ExprRefKindRET:
					return "ExprRefKindRET";
				case ExprRefKindParm:
					return "ExprRefKindParm";
				case ExprRefKindCond:
					return "ExprRefKindCond";
				case ExprRefKindLogic:
					return "ExprRefKindLogic";
        case ExprRefKindUnhandled:
					return "ExprRefKindUnhandled";
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

	  const MemberExpr* getME() {
		  return dyn_cast_or_null<MemberExpr>(expr);
	  }

	  unsigned getMeIdx() {
		  return MEIdx;
	  }

	  unsigned getMeCnt() {
		  return MECnt;
	  }

		const Expr* getExpr(){
			return expr;
		}
	  const UnaryOperator* getUnary() {
		  return dyn_cast_or_null<UnaryOperator>(expr);
	  }

	  const ArraySubscriptExpr* getAS() {
		  return dyn_cast_or_null<ArraySubscriptExpr>(expr);
	  }

		const BinaryOperator* getCAO() {
			return dyn_cast_or_null<BinaryOperator>(expr);
		}

		const BinaryOperator* getLogic() {
			return dyn_cast_or_null<BinaryOperator>(expr);
		}

		const OffsetOfExpr* getOOE() {
			return dyn_cast_or_null<OffsetOfExpr>(expr);
		}

		const Expr* getRET() {
			return (expr);
		}

		const Expr* getParm() {
			return (expr);
		}

		void setCast(CStyleCastOrType cast) {
			valuecast = cast;
		}

	  void setValue(const ValueDecl* v, CStyleCastOrType cast = CStyleCastOrType()) {
		  value = v;
		  valuecast = cast;
		  kind = ExprRefKindValue;
	  }

	  void setCall(const CallExpr* c, CStyleCastOrType cast = CStyleCastOrType()) {
		  call = c;
		  valuecast = cast;
		  kind = ExprRefKindCall;
	  }

	  void setAddress(int64_t a, CStyleCastOrType cast = CStyleCastOrType()) {
		  address = a;
			valuecast = cast;
		  kind = ExprRefKindAddress;
	  }

	  void setInteger(int64_t a, CStyleCastOrType cast = CStyleCastOrType()) {
		  address = a;
			valuecast = cast;
		  kind = ExprRefKindInteger;
	  }

		void setFloating(double f, CStyleCastOrType cast = CStyleCastOrType()) {
			floating = f;
			valuecast = cast;
			kind = ExprRefKindFloating;
		}

		void setString(std::string s, CStyleCastOrType cast = CStyleCastOrType()) {
			strval = s;
			valuecast = cast;
			kind = ExprRefKindString;
		}


		void setCAO(const BinaryOperator* CAO, CStyleCastOrType cast = CStyleCastOrType()) {
			expr = CAO;
			expr = llvm::cast<BinaryOperator>(expr);
			valuecast = cast;
			kind = ExprRefKindCAO;
		}

		void setLogic(const BinaryOperator* CAO, CStyleCastOrType cast = CStyleCastOrType()) {
			expr = CAO;
			valuecast = cast;
			kind = ExprRefKindLogic;
		}

		void setOOE(const OffsetOfExpr* OOE, CStyleCastOrType cast = CStyleCastOrType()) {
			expr = OOE;
			valuecast = cast;
			kind = ExprRefKindOOE;
		}

	  void setUnary(const UnaryOperator* __UO, CStyleCastOrType cast = CStyleCastOrType()) {
			expr = __UO;
			valuecast = cast;
		  kind = ExprRefKindUnary;
	  }

	  void setAS(const ArraySubscriptExpr* __AS, CStyleCastOrType cast = CStyleCastOrType()) {
			expr = __AS;
			valuecast = cast;
		  kind = ExprRefKindAS;
	  }

		void setRET(const ReturnStmt* S, CStyleCastOrType cast = CStyleCastOrType()) {
			expr = S->getRetValue();
			valuecast = cast;
			kind = ExprRefKindRET;
		}

		void setParm(const Expr* E, CStyleCastOrType cast = CStyleCastOrType()) {
			expr = E;
			valuecast = cast;
			kind = ExprRefKindParm;
		}
    
    void setCond(const Expr *E,size_t cf_id, CStyleCastOrType cast = CStyleCastOrType()){
			expr = E;
      address = cf_id;
      valuecast = cast;
      kind = ExprRefKindCond;
    }

    void setUnhandled(const Expr *E, const CallExpr* c, CStyleCastOrType cast = CStyleCastOrType()){
      expr = E;
      call = c;
		  valuecast = cast;
		  kind = ExprRefKindUnhandled;
    }

	  void setRefCall(const CallExpr* c, const ValueDecl* VD, CStyleCastOrType cast = CStyleCastOrType()) {
		  call = c;
		  value = VD;
		  valuecast = cast;
		  kind = ExprRefKindRefCall;
	  }

	  void setRefCall(const CallExpr* c, int64_t a, CStyleCastOrType cast = CStyleCastOrType()) {
		  call = c;
		  address = a;
		  valuecast = cast;
		  kind = ExprRefKindRefCall;
	  }

	  void setRefCall(const CallExpr* c, const Expr* E, CStyleCastOrType cast = CStyleCastOrType()) {
		  call = c;
			expr = E;
		  valuecast = cast;
		  kind = ExprRefKindRefCall;
	  }

	  void setME(const MemberExpr* __ME, CStyleCastOrType cast = CStyleCastOrType()) {
			expr = __ME;
          valuecast = cast;
		  kind = ExprRefKindMemberExpr;
	  }

	  void setMeIdx(unsigned __MEIdx) {
		  MEIdx = __MEIdx;
	  }
	  void setMeCnt(unsigned __MECnt) {
		  MECnt = __MECnt;
	  }

	  bool operator <(const ExprRef_t & otherVC) const {
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
		if (expr<otherVC.expr) return true;
		if (expr>otherVC.expr) return false;
		return valuecast<otherVC.valuecast;

	  }

	  bool operator >(const ExprRef_t & otherVC) const {
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
		if (expr<otherVC.expr) return false;
		if (expr>otherVC.expr) return true;
		return valuecast>otherVC.valuecast;
	  }

	  bool operator==(const ExprRef_t & otherVC) const {
		  return (kind==otherVC.kind)&&(MEIdx==otherVC.MEIdx)&&(MECnt==otherVC.MECnt)&&(value==otherVC.value)&&
				(address==otherVC.address)&&(floating==otherVC.floating)&&(strval==otherVC.strval)&&(call==otherVC.call)&&
				(expr==otherVC.expr)&&(valuecast==otherVC.valuecast);
	  }
  };

  struct callfunc_info_t {
	  bool funcproto;
	  void* FunctionDeclOrProtoType;
	  size_t csid;
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
	  refvarinfo_t(unsigned long id, enum vartype type, unsigned pos): id(id), fp(0.), s(""), type(type), mi(pos), di(0), castid(-1) {}
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
    };
    
  struct VarData{
    ObjectID id;
    const VarDecl *Node;
    std::string hash;
    std::shared_ptr<std::string>output;
    std::set<QualType> g_refTypes;
    std::set<const VarDecl*> g_refVars;
    std::set<const FunctionDecl*> g_refFuncs;
    std::set<LiteralHolder> g_literals;
  };
  
  std::set<std::string> unique_name;
  typedef std::map<const VarDecl*,VarData> VarArray_t;
  VarArray_t VarMap;
  size_t VarNum;
  typedef std::map<int,std::multimap<int,const VarDecl*>> taintdata_t;

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

  struct MemberInfo_t {
    const void *ME = 0;
    QualType T = {};
    int64_t offset = 0;
    const CallExpr *Call = 0;
    bool operator<(const MemberInfo_t &other) const {
      if(ME<other.ME) return true;
      if(ME>other.ME) return false;
      if(T<other.T) return true;
      if(other.T<T) return false;
      if(offset<other.offset) return true;
      if(offset>other.offset) return false;
      return Call<other.Call;
    }
    bool operator==(const MemberInfo_t &other) const {
      return (ME==other.ME)&&(T==other.T)&&(offset==other.offset)&&(Call==other.Call);
    }
  };

  struct VarInfo_t {
	  const VarDecl* VD;
	  const ParmVarDecl* PVD;
	  const CompoundStmt* CSPtr;
	  const CompoundStmt* parentCSPtr;
	  size_t varId;
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
		DereferenceInternal,
	  DereferenceUnary = DereferenceInternal, // unary
	  DereferenceArray, // as
	  DereferenceMember, // me
	  DereferenceFunction, // call/refcall
	  DereferenceAssign, // cao
    DereferenceLogic, // logic
	  DereferenceOffsetOf, // ooe
		DereferenceExternal,
	  DereferenceInit = DereferenceExternal, // value
	  DereferenceReturn, // ret
	  DereferenceParm, // parm
    DereferenceCond, // cond
  };
  /*
   * VR: variable being dereferenced
   * i: integer offset
   * vVR: list of variables referenced in offset expressions
   * Expr: raw textual expression
   */
  
  struct DereferenceInfo_t {
    ExprRef_t VR;
    std::vector<MemberInfo_t> MInfoList;
    int64_t i;
    std::vector<ExprRef_t> vVR;
    mutable std::string Expr;
    std::function<void(const DereferenceInfo_t*)> evalExprInner;
    const CompoundStmt* CSPtr;
    DereferenceKind Kind;
    unsigned baseCnt;
    std::vector<size_t> ord;
    DereferenceInfo_t(ExprRef_t VR, std::vector<MemberInfo_t> MI, int64_t i, std::vector<ExprRef_t> vVR, std::string Expr, const CompoundStmt* CSPtr, DereferenceKind Kind):
      VR(VR), MInfoList(MI), i(i), vVR(vVR), Expr(Expr), CSPtr(CSPtr), Kind(Kind), baseCnt(0) {}
    DereferenceInfo_t(ExprRef_t VR, std::vector<MemberInfo_t> MI, int64_t i, unsigned baseCnt, std::vector<ExprRef_t> vVR, std::string Expr, const CompoundStmt* CSPtr, DereferenceKind Kind):
          VR(VR), MInfoList(MI), i(i), vVR(vVR), Expr(Expr), CSPtr(CSPtr), Kind(Kind), baseCnt(baseCnt) {}
    void addOrd(size_t __ord) {
      ord.push_back(__ord);
    }
    void evalExpr() const {evalExprInner(this);}
    bool operator <(const DereferenceInfo_t & otherDI) const {
      if (VR<otherDI.VR) return true;
      if (VR>otherDI.VR) return false;
      if (MInfoList<otherDI.MInfoList) return true;
      if (MInfoList>otherDI.MInfoList) return false;
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
    if (MInfoList>otherDI.MInfoList) return true;
    if (MInfoList<otherDI.MInfoList) return false;
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
      return (VR==otherDI.VR)&&(MInfoList==otherDI.MInfoList)&&(i==otherDI.i)&&(vVR==otherDI.vVR)&&(Expr==otherDI.Expr)
          &&(Kind==otherDI.Kind)&&(baseCnt==otherDI.baseCnt)&&(CSPtr==otherDI.CSPtr);
    }
    std::string KindString() const {
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

  void evalExpr(const Stmt *E,const DereferenceInfo_t*d){
    llvm::raw_string_ostream exprstream(d->Expr);
    auto &SM = Context.getSourceManager();
    int last_tok_len = Lexer::MeasureTokenLength(SM.getSpellingLoc(E->getEndLoc()),SM,Context.getLangOpts());
    exprstream << "[" << getAbsoluteLocation(E->getBeginLoc());
    if(isa<BinaryOperator>(E)){
      exprstream << ':' << SM.getFileOffset(cast<BinaryOperator>(E)->getOperatorLoc());
    }
    exprstream << ':' << SM.getFileOffset(E->getEndLoc()) + last_tok_len
    << ':' << SM.getFileOffset(E->getBeginLoc())
    << ':' << E->getBeginLoc().isMacroID()
    << "]: ";
    E->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
    exprstream.flush();
  }

  void evalExpr(const ValueDecl *E,const DereferenceInfo_t*d){
    llvm::raw_string_ostream exprstream(d->Expr);
    auto &SM = Context.getSourceManager();
    int last_tok_len = Lexer::MeasureTokenLength(SM.getSpellingLoc(E->getEndLoc()),SM,Context.getLangOpts());
    exprstream << "[" << getAbsoluteLocation(E->getBeginLoc())
    << ':' << SM.getFileOffset(E->getEndLoc()) + last_tok_len
    << ':' << SM.getFileOffset(E->getBeginLoc())
    << ':' << E->getBeginLoc().isMacroID()
    << "]: ";
    E->print(exprstream);
    exprstream.flush();
  }

  struct FuncDeclData{
    ObjectID id;
    const FunctionDecl *this_func;
    std::string declhash;
    std::string signature;
    std::string templatePars;
    std::string nms;
    int fid;
    std::shared_ptr<std::string> output;
  };

  struct FuncData : public FuncDeclData{
    size_t weak_id;
    std::set<callfunc_info_t> funcinfo;
    std::map<const Expr*,std::vector<std::pair<caseinfo_t,caseinfo_t>>> switch_map;
    taintdata_t taintdata;
    int declcount;
    std::set<QualType> refTypes;
    std::map<const VarDecl*,std::set<const DeclRefExpr*>>refVars;
    std::set<const FunctionDecl*> refFuncs;
    size_t CSId;
    std::map<const CompoundStmt*,long> csIdMap;
    std::map<const CompoundStmt*,const CompoundStmt*> csParentMap;
    std::map<const CompoundStmt*,size_t> csInfoMap;
    std::vector<ControlFlowData> cfData;
    size_t varId;
    std::map<const VarDecl*,VarInfo_t> varMap;
    std::map<const IfStmt*,IfInfo_t> ifMap;
    std::map<const GCCAsmStmt*,GCCAsmInfo_t> asmMap;
    std::string body;
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
  const VarDecl* lastGlobalVarDecl = 0;
  std::vector<bool> inVarDecl;
  std::vector<const VarDecl*>fopsVarDecl;
  std::stack<const RecordDecl*> recordDeclStack;
  std::vector<FuncData*> functionStack;
  typedef std::map<const FunctionDecl*,FuncDeclData> FuncDeclArray_t;
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
  std::map<const CXXRecordDecl*,const ClassTemplateSpecializationDecl*> classTemplateSpecializationMap;
  std::map<const CXXRecordDecl*,const ClassTemplatePartialSpecializationDecl*> classTemplatePartialSpecializationMap;
  std::map<CXXRecordDecl*,CXXRecordDecl*> recordParentDeclMap;
  std::vector<const CompoundStmt*> csStack;
  std::map<const RecordDecl *,const CompoundStmt*> recordCSMap;
  std::map<QualType, QualType> vaTMap;
  unsigned long missingRefsCount;
  std::map<const RecordDecl*,std::set<const VarDecl*>> gtp_refVars;
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

  const VarDecl* VarForMap(const VarDecl*D){
    return D->getCanonicalDecl();
  }
  VarData& getVarData(const VarDecl *D){
    D = VarForMap(D);
    
	  if (VarMap.find(D)==VarMap.end()) {
		  llvm::outs()<<"Var not in map:"<<D<<'\n';
		  D->dump();
		  assert(0);
	  }
    return VarMap.at(D);
  }

  size_t getFuncNum() {
	  return FuncMap.size();
  }

  FuncArray_t& getFuncMap() {
	  return FuncMap;
  }
  
  FuncData &getFuncData(const FunctionDecl *D){
	  if (FuncMap.find(D)==FuncMap.end()) {
		  llvm::outs()<<"Func not in map:"<<D<<'\n';
		  D->dump();
		  assert(0);
	  }
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
  const VarDecl* findFopsVar(const Expr *E);
  void noticeFopsFunction(FopsObject &FObj, int field_index,const Expr *E);
  bool VisitEnumDecl(const EnumDecl *D);
  bool VisitRecordDecl(const RecordDecl *D);
  bool VisitCXXRecordDecl(const CXXRecordDecl* D);
  bool VisitCXXMethodDecl(const CXXMethodDecl* D);
  bool VisitDecl(Decl *D);
  bool TraverseDecl(Decl *D);
  // bypass Visitor incorrectly visiting requires-expr parameters twice
  bool TraverseRequiresExprBodyDecl(RequiresExprBodyDecl *D){return true;}
  bool VisitFunctionDeclStart(const FunctionDecl *D);
  bool VisitFunctionDeclComplete(const FunctionDecl *D);
  bool handleCallDeclRefOrMemberExpr(const Expr* E, std::set<ValueHolder> callrefs, std::set<LiteralHolder> literalRefs, const QualType* baseType = 0, const CallExpr* CE = 0);
  bool handleCallAddress(int64_t Addr,const Expr* AddressExpr,std::set<ValueHolder> callrefs,std::set<DbJSONClassVisitor::LiteralHolder> literalRefs, const QualType* baseType = 0,
    const CallExpr* CE = 0, const CStyleCastExpr* CSCE = 0);
  bool handleCallStmtExpr(const Expr* E, std::set<ValueHolder> callrefs, std::set<LiteralHolder> literalRefs, const QualType* baseType = 0, const CallExpr* CE = 0);
  const DbJSONClassVisitor::callfunc_info_t* handleCallMemberExpr(const MemberExpr* ME, std::set<ValueHolder> callrefs, std::set<LiteralHolder> literalRefs, const QualType* baseType = 0, const CallExpr* CE = 0);
  const DbJSONClassVisitor::callfunc_info_t* handleCallVarDecl(const VarDecl* VD, const DeclRefExpr* DRE, std::set<ValueHolder> callrefs, std::set<LiteralHolder> literalRefs, const QualType* baseType = 0, const CallExpr* CE = 0);
  bool handleCallConditionalOperator(const ConditionalOperator* CO, std::set<ValueHolder> callrefs, std::set<LiteralHolder> literalRefs, const QualType* baseType = 0, const CallExpr* CE = 0);
  const Expr* stripCasts(const Expr* E);
  const Expr* stripCastsEx(const Expr* E, std::vector<CStyleCastOrType>& vC);
  const UnaryOperator* lookForUnaryOperatorInCallExpr(const Expr* E);
  const ArraySubscriptExpr* lookForArraySubscriptExprInCallExpr(const Expr* E);
  bool VisitAlignedAttr(AlignedAttr *A);
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
  bool TraverseStmt(Stmt* S);
  bool TraverseInitListExpr(InitListExpr *S);
  bool VisitCompoundStmtStart(const CompoundStmt *CS);
  bool VisitCompoundStmtComplete(const CompoundStmt *CS);
  void handleConditionDeref(Expr *Cond,size_t cf_id);
  bool VisitSwitchStmt(SwitchStmt *S);
  bool VisitIfStmt(IfStmt *S);
  bool VisitForStmt(ForStmt *S);
  bool VisitWhileStmt(WhileStmt *S);
  bool VisitDoStmt(DoStmt *S);
  bool VisitGCCAsmStmt(GCCAsmStmt *S);
  bool VisitInitListExpr(InitListExpr* ILE);
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
  void computeOffsetExpr(const Expr* E, int64_t* LiteralOffset, std::vector<ExprRef_t>& OffsetRefs,
		  BinaryOperatorKind kind, bool stripCastFlag = false);
  bool tryComputeOffsetExpr(const Expr* E, int64_t* LiteralOffset, BinaryOperatorKind kind, bool stripCastFlag = false);
  void mergeBinaryOperators(const BinaryOperator* BO, int64_t* LiteralOffset, std::vector<ExprRef_t>& OffsetRefs,
      BinaryOperatorKind kind);
  const Expr* lookForNonParenExpr(const Expr* E);
  bool tryEvaluateIntegerConstantExpr(const Expr* E, Expr::EvalResult& Res);
  void lookForDerefExprs(const Expr* E, int64_t* LiteralOffset, std::vector<ExprRef_t>& OffsetRefs);
  void lookForExplicitCastExprs(const Expr* E, std::vector<QualType>& refs );
  void lookForLiteral(const Expr* E, std::set<LiteralHolder>& refs, unsigned pos = 0 );
  const DeclRefExpr* lookForBottomDeclRef(const Expr* E);
  int fieldToIndex(const MemberExpr *ME);
  int fieldToIndex(const FieldDecl* FD);
  void setSwitchData(const Expr* caseExpr, int64_t* enumtp, std::string* enumstr, std::string* macroValue, std::string* raw_code, int64_t* exprVal);
  void varInfoForRefs(FuncData &func_data, const std::set<ValueHolder>& refs, std::set<LiteralHolder> literals, std::vector<struct refvarinfo_t>& refvarList);
  bool VR_referenced(ExprRef_t& VR, std::unordered_set<const Expr*> DExpRef);
  bool varInfoForVarRef(FuncData &func_data, ExprRef_t VR,  struct refvarinfo_t& refvar, std::map<const CallExpr*,unsigned long>& CEIdxMap,
	      std::unordered_map<const Expr*,unsigned> DExpIdxMap, std::unordered_map<const ValueDecl*,unsigned> DValIdxMap);
  void notice_class_references(RecordDecl* rD);
  void notice_field_attributes(RecordDecl* rD, std::vector<QualType>& QV);
  void notice_template_class_references(CXXRecordDecl* TRD);
  void lookForAttrTypes(const Attr* attr, std::vector<QualType>& QV);
  bool lookForUnaryExprOrTypeTraitExpr(const Expr* e, std::vector<QualType>& QV);
  bool lookForTypeTraitExprs(const Expr* e, std::vector<const TypeTraitExpr*>& TTEV);
  size_t outerFunctionorMethodIdforTagDecl(TagDecl* rD);
  std::string parentFunctionOrMethodString(TagDecl* tD);
  size_t getFunctionDeclId(const FunctionDecl* FD);
  QualType lookForNonPointerType(const PointerType* tp);
  const FunctionProtoType* lookForFunctionType(QualType T);

  typedef std::set<ExprRef_t> DREMap_t;

  void lookForDeclRefWithMemberExprsInOffset(const Expr* E, std::vector<ExprRef_t>& OffsetRefs);

  void lookForDeclRefWithMemberExprsInternal(const Expr* E, DREMap_t& refs, std::vector<MemberInfo_t> *cache,
      std::vector<CStyleCastOrType> castVec, unsigned MEIdx = 0, const CallExpr* CE = 0, bool IgnoreLiteral = false);

  std::set<const MemberExpr*> DoneMEs;

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
};

typedef std::tuple<std::string,std::string,std::string> MacroDefInfo;

class DbJSONClassConsumer : public clang::ASTConsumer {
public:
  explicit DbJSONClassConsumer(ASTContext &Context, size_t fid,
		  Preprocessor &PP, bool save_exps)
    : Visitor(Context), file_id(fid), Context(Context), Macros(PP,save_exps) {
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
      COMPAT_VERSION_GE(14,
      if (isa<UsingType>(BaseType))
        break;
      )
      else if (const PointerType* PTy = BaseType->getAs<PointerType>())
        BaseType = PTy->getPointeeType();
      else if (const BlockPointerType *BPy = BaseType->getAs<BlockPointerType>())
        BaseType = BPy->getPointeeType();
      else if (const MemberPointerType *MPy = BaseType->getAs<MemberPointerType>())
        BaseType = MPy->getPointeeType();
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
      COMPAT_VERSION_GE(15,
      else if (const BTFTagAttributedType *AtrTy = BaseType->getAs<BTFTagAttributedType>())
        BaseType = AtrTy->getWrappedType();
      )
      else if (const ParenType *PTy = BaseType->getAs<ParenType>())
        BaseType = PTy->desugar();
      else
        llvm_unreachable("Unknown declarator!");
    }
    return BaseType;
  }

  static std::string translateLinkage(clang::Linkage linkage) {

	  switch(linkage) {
		case clang::compatNoLinkage:
			return "none";
		case clang::compatInternalLinkage:
			return "internal";
		// case clang::UniqueExternalLinkage:
		// 	return "";
		// case clang::VisibleNoLinkage:
		// 	return "";
		// case clang::ModuleInternalLinkage:
		// 	return "";
		// case clang::ModuleLinkage:
		// 	return "";
		case clang::compatExternalLinkage:
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
  
  void computeVarHashes();
  void computeTypeHashes();
  void computeFuncHashes();
  void processFops();
  void getFuncTemplatePars(DbJSONClassVisitor::FuncDeclData *func_data);
  void getFuncDeclHash(DbJSONClassVisitor::FuncDeclData *func_data);
  void getFuncHash(DbJSONClassVisitor::FuncData *func_data);
  void printGlobalArray(int Indentation);
  void printGlobalEntry(DbJSONClassVisitor::VarData &var_data, int Indentation);
  void printTypeArray(int Indentation);
  void printTypeEntry(DbJSONClassVisitor::TypeData &type_data, int Indentation);
  void printFuncArray(int Indentation);
  void printFuncEntry(DbJSONClassVisitor::FuncData &func_data, int Indentation);
  void printFuncDeclArray(int Indentation);
  void printFuncDeclEntry(DbJSONClassVisitor::FuncDeclData &func_data, int Indentation);
  void printFopsArray(int Indentation);
  void printFopsEntry(const DbJSONClassVisitor::FopsData &fops_data, int Indentation);
  size_t getDeclId(Decl* D, std::pair<int,unsigned long long>& extraArg);
  TypeGroup_t getTypeGroupIds(Decl** Begin, unsigned NumDecls, const PrintingPolicy &Policy);
  void get_class_references(RecordDecl* rD, TypeGroup_t& Ids, MethodGroup_t& MIds, std::vector<int>& rIds, std::vector<std::string>& rDef);
  template_default_map_t getTemplateParameters(TemplateParameterList* TPL, const std::string& Indent,
		  bool getDefaults = true);
  void printTemplateTypeDefaults(llvm::raw_string_ostream &TOut, defaut_type_map_t default_type_map,
      std::map<int,int> type_parms_idx,const std::string& Indent, bool nextJSONitem = true);
  void printTemplateArgs(llvm::raw_string_ostream &TOut, template_args_t& template_args,
      std::vector<int> type_args_idx, const std::string& Indent, bool nextJSONitem=true);
  void getTemplateArguments(const TemplateArgumentList&, const std::string& Indent, template_args_t& template_args);
  void getTemplateArguments(ArrayRef<TemplateArgument> Args, const std::string& Indent, template_args_t& template_args);

  void buildTemplateArgumentsString(const TemplateArgumentList& Args,
	  	  	  std::string& typeString, std::pair<bool,unsigned long long> extraArg = {0,0});

  void buildTemplateArgumentsString(ArrayRef<TemplateArgument> Args,
	  	  	  std::string& typeString, std::pair<bool,unsigned long long> extraArg = {0,0});

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
  
  void printDatabase();
  virtual void HandleTranslationUnit(clang::ASTContext &Context);
  
  virtual bool HandleTopLevelDecl(DeclGroupRef DR) {
	  return true;
      }
private:
  void printCallInfo(const DbJSONClassVisitor::callfunc_info_t&, std::stringstream& ss, clang::SourceManager& SM, std::vector<const CallExpr*>& CEv);
  void printCallInfo(const DbJSONClassVisitor::callfunc_info_t&, std::stringstream& ss, clang::SourceManager& SM, size_t ord, std::vector<const CallExpr*>& CEv);


  void getFuncDeclSignature(const FunctionDecl* D, std::string& fdecl_sig);

  std::map<int,std::tuple<QualType,std::string,DbJSONClassVisitor::recordInfo_t*>> revTypeMap;
  // Mapping of types to computed type strings (to speed-up type string creation)
  std::map<QualType,std::string> TypeStringMap;
  DbJSONClassVisitor Visitor;
  size_t file_id;
  ASTContext &Context;
  MacroHandler Macros;
};
void load_database(std::string filepath);
int internal_declcount(const FunctionDecl *F);
void taint_params(const clang::FunctionDecl *F, DbJSONClassVisitor::FuncData&);
bool isOwnedTagDeclType(QualType DT);
QualType resolve_Typedef_Integer_Type(QualType T);

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
  else if(T->getTypeClass()==Type::Elaborated){
    const ElaboratedType *tp = cast<ElaboratedType>(T);
    return walkTypedefType(tp->getNamedType());
  }
  else if(T->getTypeClass()==Type::TypeOf){
    const TypeOfType *tp = cast<TypeOfType>(T);
    return walkTypedefType(tp->getUnmodifiedType());
  }
  else if(T->getTypeClass()==Type::TypeOfExpr){
    const TypeOfExprType *tp = cast<TypeOfExprType>(T);
    return walkTypedefType(tp->getUnderlyingExpr()->getType());
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

namespace multi{
  extern std::string directory;
  extern std::vector<std::string> files;
  void registerVar(DbJSONClassVisitor::VarData&);
  void registerType(DbJSONClassVisitor::TypeData&);
  void registerFuncDecl(DbJSONClassVisitor::FuncDeclData&);
  void registerFuncInternal(DbJSONClassVisitor::FuncData&);
  void registerFunc(DbJSONClassVisitor::FuncData&);
  void registerFops(DbJSONClassVisitor::FopsData&);
  void handleRefs(void *rv, std::vector<int> rIds,std::vector<std::string> rDef);
  void processDatabase();
  void emitDatabase(llvm::raw_ostream&);
  void report();
}