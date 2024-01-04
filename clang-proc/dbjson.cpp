#include "main.hpp"
#include "dbjson.hpp"
#include "compat.h"

#include "json.hpp"
#include "clang/AST/RecordLayout.h"
#include <stdlib.h>
#include <random>
#include <algorithm>

int DEBUG_NOTICE;

//disabled useddef feature for now, since no one uses it
constexpr bool DISABLED = false;

thread_local size_t exprOrd;

typedef std::string name_t;
	typedef int to_index;
	typedef int from_index;
typedef std::multimap<name_t,std::pair<to_index,from_index>> db_t;
typedef std::unordered_map<name_t,std::vector<std::pair<to_index,from_index>>> ndb_t;
int load_taint_function_database(std::string filepath, ndb_t &exact, ndb_t &regex){
	std::fstream f;
	f.open(filepath, std::ios_base::in);
	std::stringstream s;
	s<<f.rdbuf();
	auto database = json::JSON::Load(s.str());
	// std::cout<<database.dump();

	for(auto d : database["exact_name"].ObjectRange()){
		name_t name = d.first;
		to_index to_index;
		from_index from_index;
		exact.insert({name,{}});
		for(int i=0;i<d.second.size();i++){
			to_index = d.second[i][0].ToInt();
			from_index = d.second[i][1].ToInt();
			exact[name].push_back({to_index,from_index});
			// std::cout<<name<<' '<<to_index<<' '<<from_index<<'\n';
		}
	}
	for(auto d : database["regex_name"].ObjectRange()){
		name_t name = d.first;
		to_index to_index;
		from_index from_index;
		regex.insert({name,{}});
		for(int i=0;i<d.second.size();i++){
			to_index = d.second[i][0].ToInt();
			from_index = d.second[i][1].ToInt();
			regex[name].push_back({to_index,from_index});
			// std::cout<<name<<' '<<to_index<<' '<<from_index<<'\n';
		}
	}
	return 0;
}

QualType resolve_Typedef_Integer_Type(QualType T) {
	if (T->getTypeClass()==Type::Typedef) {
	  	const TypedefType *tpd = cast<TypedefType>(T);
	  	TypedefNameDecl* D = tpd->getDecl();
	  	return resolve_Typedef_Integer_Type(D->getTypeSourceInfo()->getType());
	  }
	else if (T->getTypeClass()==Type::Builtin) {
		return T;
	}
	else {
		llvm::errs() << "UNSUPPORTED enum type: " << T->getTypeClassName() << "\n";
		assert(0);
	}
}

QualType resolve_Record_Type(QualType T) {
	if (T->getTypeClass()==Type::Typedef) {
	  	const TypedefType *tpd = cast<TypedefType>(T);
	  	TypedefNameDecl* D = tpd->getDecl();
	  	return resolve_Record_Type(D->getTypeSourceInfo()->getType());
	  }
	else if (T->getTypeClass()==Type::Record) {
		return T;
	}
	else if (T->getTypeClass()==Type::Elaborated) {
		const ElaboratedType *tp = cast<ElaboratedType>(T);
		QualType eT = tp->getNamedType();
		return resolve_Record_Type(eT);
	}
	else if (T->getTypeClass()==Type::ConstantArray) {
		const ConstantArrayType *tp = cast<ConstantArrayType>(T);
		QualType elT = tp->getElementType();
		return resolve_Record_Type(elT);
	}
	else if (T->getTypeClass()==Type::IncompleteArray) {
		const IncompleteArrayType *tp = cast<IncompleteArrayType>(T);
		QualType elT = tp->getElementType();
		return resolve_Record_Type(elT);
	}
	else {
		return QualType();
	}
}

static std::string getFullFunctionNamespace(const FunctionDecl *D) {
	  std::list<std::string> nsl;
	  const DeclContext* DC = D->getEnclosingNamespaceContext();
	  if ((DC) && (isa<NamespaceDecl>(DC))) {
		  const NamespaceDecl* ND = static_cast<const NamespaceDecl*>(DC);
		  nsl.push_front(ND->getNameAsString());
		  DC = ND->getParent()->getEnclosingNamespaceContext();
		  while (isa<NamespaceDecl>(DC)) {
			  const NamespaceDecl* ND = static_cast<const NamespaceDecl*>(DC);
			  nsl.push_front(ND->getNameAsString());
			  DC = ND->getParent()->getEnclosingNamespaceContext();
		  }
	  }
	  std::string fns;
	  for (auto i = nsl.begin(); i!=nsl.end(); ++i) {
		  if (i==nsl.begin()) {
			  fns+=*i;
		  }
		  else {
			  fns+="::"+*i;
		  }
	  }
	  return fns;
  }

void DbJSONClassConsumer::getFuncDeclSignature(const FunctionDecl* D, std::string& fdecl_sig) {
  if(opts.assert && Visitor.CTAList.find(D)!=Visitor.CTAList.end()) {
	  fdecl_sig += "__compiletime_assert";
  }
  else {
	  fdecl_sig += D->getName();
  }
  fdecl_sig += ' ';
  fdecl_sig += walkTypedefType(D->getType()).getAsString();
}

bool isOwnedTagDeclType(QualType DT) {
  if (DT->getTypeClass()==Type::Elaborated) {
	  TagDecl *OwnedTagDecl = cast<ElaboratedType>(DT)->getOwnedTagDecl();
	  if (OwnedTagDecl) {
		if (isa<RecordDecl>(OwnedTagDecl)) {
			const RecordDecl* rD = static_cast<RecordDecl*>(OwnedTagDecl);
			 if (rD->isCompleteDefinition()) {
				 return true;
			}
		}
		else if (isa<EnumDecl>(OwnedTagDecl)) {
			const EnumDecl* eD = static_cast<EnumDecl*>(OwnedTagDecl);
			 if (eD->isCompleteDefinition()) {
				 return true;
			}
		}
	  }
  }
  else if (DT->getTypeClass()==Type::Pointer) {
	  const PointerType *tp = cast<PointerType>(DT);
	  QualType ptrT = tp->getPointeeType();
	  return isOwnedTagDeclType(ptrT);
  }
  else if (DT->getTypeClass()==Type::IncompleteArray) {
	  const IncompleteArrayType *tp = cast<IncompleteArrayType>(DT);
	  QualType elT = tp->getElementType();
	  return isOwnedTagDeclType(elT);
  }
  else if (DT->getTypeClass()==Type::ConstantArray) {
	  const ConstantArrayType *tp = cast<ConstantArrayType>(DT);
	  QualType elT = tp->getElementType();
	  return isOwnedTagDeclType(elT);
  }
  return false;
}

  bool DbJSONClassVisitor::declGroupHasNamedFields(Decl** Begin, unsigned NumDecls) {

	    if (NumDecls == 1) {
            return isNamedField(*Begin);
	    }

	    Decl** End = Begin + NumDecls;
		TagDecl* TD = dyn_cast<TagDecl>(*Begin);
		if (TD)
		  ++Begin;

		for ( ; Begin != End; ++Begin) {
			if (isNamedField(*Begin)) return true;
		}

		return false;
  }

  bool DbJSONClassVisitor::fieldMatch(Decl* D, const FieldDecl* FD) {

	  return (D==FD);
  }

int DbJSONClassVisitor::fieldIndexInGroup(Decl** Begin, unsigned NumDecls, const FieldDecl* FD, int startIndex) {

    int idx = startIndex;

    if (NumDecls == 1) {
        if (fieldMatch(*Begin,FD)) {
            return idx;
        }
    }

    Decl** End = Begin + NumDecls;
    TagDecl* TD = dyn_cast<TagDecl>(*Begin);
    if (TD) {
        ++Begin;
        ++idx;
    }

    for ( ; Begin != End; ++Begin) {
        if (fieldMatch(*Begin,FD)) {
            return idx;
        }
    }

    return -1;
  }

	bool DbJSONClassVisitor::isNamedField(Decl* D) {

		if (D->getKind()==Decl::Field) {
			FieldDecl* innerD = cast<FieldDecl>(D);
			if (innerD->getIdentifier()) return true;
			else {
				QualType T = innerD->getType();
				if (T->getTypeClass()==Type::Record) {
					const RecordType *tp = cast<RecordType>(T);
					if (hasNamedFields(tp->getDecl())) {
						return true;
					}
				}
			}
		}
		if (D->getKind()==Decl::Record) {
			RecordDecl* innerD = cast<RecordDecl>(D);
			QualType T = Context.getRecordType(innerD);
			if (innerD->isCompleteDefinition()) {
			  if (innerD->getIdentifier()) {
				  return true;
			  }
			  else {
				  const RecordType *tp = cast<RecordType>(T);
				  return hasNamedFields(tp->getDecl());
			  }
		  }
		}

		return false;
	}

	bool DbJSONClassVisitor::emptyRecordDecl(RecordDecl* rD) {

		if (rD->isCompleteDefinition()) {
			const DeclContext *DC = cast<DeclContext>(rD);
			return DC->decls_begin()==DC->decls_end();
		}
		return false;
	}

	bool DbJSONClassVisitor::hasNamedFields(RecordDecl* rD) {

	  if (rD->isCompleteDefinition()) {
		  const DeclContext *DC = cast<DeclContext>(rD);
		  SmallVector<Decl*, 2> Decls;
		  for (DeclContext::decl_iterator D = DC->decls_begin(), DEnd = DC->decls_end();
				 D != DEnd; ++D) {
			  if (D->isImplicit())
				  continue;
			  QualType CurDeclType = getDeclType(*D);
			  if (!Decls.empty() && !CurDeclType.isNull()) {
				QualType BaseType = GetBaseType(CurDeclType);
				if (!BaseType.isNull() && isa<ElaboratedType>(BaseType))
				  BaseType = cast<ElaboratedType>(BaseType)->getNamedType();
				if (!BaseType.isNull() && isa<TagType>(BaseType) &&
					cast<TagType>(BaseType)->getDecl() == Decls[0]) {
				  Decls.push_back(*D);
				  continue;
				}
			  }
			  if (!Decls.empty()) {
				  if (declGroupHasNamedFields(Decls.data(), Decls.size())) return true;
				  Decls.clear();
			  }
			  if (isa<TagDecl>(*D) && !cast<TagDecl>(*D)->getIdentifier()) {
				Decls.push_back(*D);
				continue;
			  }
			  if (isNamedField(*D)) return true;
		  }
		  if (!Decls.empty()) {
			  if (declGroupHasNamedFields(Decls.data(), Decls.size())) return true;
			  Decls.clear();
		  }
	  }
	  return false;
	}
  
  int DbJSONClassVisitor::fieldToIndex(const FieldDecl* FD, const RecordDecl* RD) {

	int fieldIndex = 0;
	if (RD->isCompleteDefinition()) {
		  const DeclContext *DC = cast<DeclContext>(RD);
		  SmallVector<Decl*, 2> Decls;
		  for (DeclContext::decl_iterator D = DC->decls_begin(), DEnd = DC->decls_end();
				 D != DEnd; ++D) {
			  	 if ((isa<RecordDecl>(*D))&&(!cast<RecordDecl>(*D)->isCompleteDefinition())) {
			  		 continue;
			  	 }
			     if (D->isImplicit()) {
				   if (fieldMatch(*D,FD)) {
					   return fieldIndex;
				   }
				   continue;
			     }
			  QualType CurDeclType = getDeclType(*D);
			  if (!Decls.empty() && !CurDeclType.isNull()) {
				QualType BaseType = GetBaseType(CurDeclType);
				if (!BaseType.isNull() && isa<ElaboratedType>(BaseType))
				  BaseType = cast<ElaboratedType>(BaseType)->getNamedType();
				if (!BaseType.isNull() && isa<TagType>(BaseType) &&
					cast<TagType>(BaseType)->getDecl() == Decls[0]) {
				  Decls.push_back(*D);
				  continue;
				}
			  }
			  if (!Decls.empty()) {
				  int idx = fieldIndexInGroup(Decls.data(), Decls.size(), FD, fieldIndex);
				  if (idx>=0) {
					  return idx;
				  }
				  fieldIndex+=Decls.size();
				  Decls.clear();
			  }
			  if (isa<TagDecl>(*D) && !cast<TagDecl>(*D)->getIdentifier()) {
				Decls.push_back(*D);
				continue;
			  }
			  if (fieldMatch(*D,FD)) {
				  return fieldIndex;
			  }
			  ++fieldIndex;
		  }
		  if (!Decls.empty()) {
			  int idx = fieldIndexInGroup(Decls.data(), Decls.size(), FD, fieldIndex);
			  if (idx>=0) {
				  return idx;
			  }
			  Decls.clear();
		  }
	  }

	  return -1;
}

  int DbJSONClassVisitor::fieldToFieldIndex(const FieldDecl* FD, const RecordDecl* RD) {

	int fieldIndex = 0;
	if (RD->isCompleteDefinition()) {
		  const DeclContext *DC = cast<DeclContext>(RD);
		  SmallVector<Decl*, 2> Decls;
		  for (DeclContext::decl_iterator D = DC->decls_begin(), DEnd = DC->decls_end();
				 D != DEnd; ++D) {
			  	 if ((isa<RecordDecl>(*D))&&(!cast<RecordDecl>(*D)->isCompleteDefinition())) {
			  		 continue;
			  	 }
			     if (D->isImplicit()) {
				   if (fieldMatch(*D,FD)) {
					   return fieldIndex;
				   }
				   if ((*D)->getKind()==Decl::Field) {
					  ++fieldIndex;
				  }
				   continue;
			     }
			  QualType CurDeclType = getDeclType(*D);
			  if (!Decls.empty() && !CurDeclType.isNull()) {
				QualType BaseType = GetBaseType(CurDeclType);
				if (!BaseType.isNull() && isa<ElaboratedType>(BaseType))
				  BaseType = cast<ElaboratedType>(BaseType)->getNamedType();
				if (!BaseType.isNull() && isa<TagType>(BaseType) &&
					cast<TagType>(BaseType)->getDecl() == Decls[0]) {
				  Decls.push_back(*D);
				  continue;
				}
			  }
			  if (!Decls.empty()) {
				  Decls.clear();
			  }
			  if (isa<TagDecl>(*D) && !cast<TagDecl>(*D)->getIdentifier()) {
				Decls.push_back(*D);
				continue;
			  }
			  if (fieldMatch(*D,FD)) {
				  return fieldIndex;
			  }
			  if ((*D)->getKind()==Decl::Field) {
				  ++fieldIndex;
			  }
		  }
		  if (!Decls.empty()) {
			  Decls.clear();
		  }
	  }

	  return -1;
}

void DbJSONClassVisitor::setSwitchData(const Expr* caseExpr, int64_t* enumv,
		std::string* enumstr, std::string* macroValue, std::string* raw_code, int64_t* exprVal) {

	SourceManager& SM = Context.getSourceManager();
	if (caseExpr->IgnoreCasts()->getExprLoc().isMacroID()) {
		*macroValue = Lexer::getSourceText(CharSourceRange(caseExpr->IgnoreCasts()->getSourceRange(),true),
				SM,Context.getLangOpts()).str();
	}
	llvm::raw_string_ostream cstream(*raw_code);
	caseExpr->printPretty(cstream,nullptr,Context.getPrintingPolicy());
	cstream.flush();
	const DeclRefExpr* DRE = lookForBottomDeclRef(caseExpr);
	if (DRE && (DRE->getDecl()->getKind()==Decl::EnumConstant)) {
		const EnumConstantDecl* ecd = static_cast<const EnumConstantDecl*>(DRE->getDecl());
		*enumv = ecd->getInitVal().extOrTrunc(63).getExtValue();
		*enumstr = ecd->getIdentifier()->getName().str();
	}
	Expr::EvalResult Res;
	if((!caseExpr->isValueDependent()) && caseExpr->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(caseExpr,Res)){
		*exprVal = Res.Val.getInt().extOrTrunc(63).getExtValue();
	}
}

const DbJSONClassVisitor::callfunc_info_t* DbJSONClassVisitor::handleCallMemberExpr(const MemberExpr* ME,
		std::set<ValueHolder> callrefs,
		std::set<DbJSONClassVisitor::LiteralHolder> literalRefs,
		const QualType* baseType, const CallExpr* CE) {

	/* Member function call through a given object {obj.fun()}
     */

	std::stringstream protostr;
	std::stringstream className;
	std::stringstream refObj;
	const ValueDecl *VD = ME->getMemberDecl();
	if (VD->getKind()==Decl::Field) {
		const FieldDecl* FD = static_cast<const FieldDecl*>(VD);
		const FunctionProtoType* proto;
		if (baseType) {
			proto = lookForFunctionType(*baseType);
		}
		else {
			proto = lookForFunctionType(FD->getType());
		}
		if (proto) {
			protostr << " [" << proto->getNumParams() << "] ("
					<< ME->getExprLoc().printToString(Context.getSourceManager()) << ") "
					<< (const void*)proto;
			noticeTypeClass(QualType(proto,0));
		}
		else {
			return 0;
		}
		const RecordDecl* RD = FD->getParent();
		int fieldIndex = fieldToIndex(FD,RD);
		QualType RT = Context.getRecordType(RD);
		const Type *tp = cast<Type>(RT);
		if(RT->getTypeClass()==Type::Record) {
			className << "[" << RT.getAsString() << ":" << tp << "]";
		}
		const DeclRefExpr* DRE = lookForBottomDeclRef(static_cast<const Expr*>(*(ME->child_begin())));
		if (DRE) {
			refObj << "[" << DRE->getNameInfo().getAsString() << ":" << DRE << "]";
		}
		DBG(opts.debug, llvm::outs() << "  notice MemberRefCall: "
				<< refObj.str() << " " << className.str() << " " << FD->getName().str() << "()"
				<< " i(" << fieldIndex << ")" << protostr.str() << "\n" );
		callfunc_info_t nfo = {};
		nfo.FunctionDeclOrProtoType = (void*)proto;
		nfo.refObj = (void*)DRE;
		if (!DRE) {
			nfo.funcproto = true;
		}
		nfo.classType = RT;
		nfo.fieldIndex = fieldIndex;
		nfo.callrefs = callrefs;
		nfo.literalRefs = literalRefs;
		nfo.CE = CE;
		nfo.ord = exprOrd++;
		/* We might have member function call from implicit (e.g. operator()) function */
		if (lastFunctionDef) {
			nfo.csid = lastFunctionDef->csIdMap.at(getCurrentCSPtr());
			std::pair<std::set<callfunc_info_t>::iterator,bool> rv = lastFunctionDef->funcinfo.insert(nfo);
			for(unsigned long i=0; i<proto->getNumParams(); i++) {
				QualType T = proto->getParamType(i);
				lastFunctionDef->refTypes.insert(T);
			}
			/* Now save information that for this particular MemberExpr that there was a parent CallExpr involved */
			if ((MEHaveParentCE.find(ME)!=MEHaveParentCE.end())&&(MEHaveParentCE[ME]!=CE)) {
				llvm::errs() << "Multiple parent CE for MemberExpr\n";
				llvm::errs() << "MemberExpr:\n";
				ME->dumpColor();
				llvm::errs() << "CallExpr involved:\n";
				CE->dumpColor();
				llvm::errs() << "CallExpr already present:\n";
				MEHaveParentCE[ME]->dumpColor();
				assert(0);
			}
			MEHaveParentCE[ME] = CE;
			return &(*(rv.first));
		}
	}
	return 0;
}

const DbJSONClassVisitor::callfunc_info_t* DbJSONClassVisitor::handleCallVarDecl(const VarDecl* VD, const DeclRefExpr* DRE,
		std::set<ValueHolder> callrefs,
		std::set<DbJSONClassVisitor::LiteralHolder> literalRefs,
		const QualType* baseType, const CallExpr* CE) {

	/* Function call through the pointer to function {(*pfun)()}
	 */

	const FunctionProtoType* proto;
	if (baseType) {
		proto = lookForFunctionType(*baseType);
	}
	else {
		proto = lookForFunctionType(VD->getType());
	}
	/* We could have K&R function prototype without information about its arguments; ignore */
	if (!proto) return 0;
	std::stringstream protostr;
	if (proto) {
		protostr << " [" << proto->getNumParams() << "] ("
				<< DRE->getExprLoc().printToString(Context.getSourceManager()) << ") "
				<< (const void*)proto;
		noticeTypeClass(QualType(proto,0));
	}
	DBG(opts.debug, llvm::outs() << "  notice FunctionRefCall: "
			<< "(*" << DRE->getNameInfo().getAsString() << ")"
			<< protostr.str() << "\n" );
	callfunc_info_t nfo = {};
	nfo.FunctionDeclOrProtoType = (void*)proto;
	nfo.refObj = (void*)DRE;
	nfo.callrefs = callrefs;
	nfo.literalRefs = literalRefs;
	nfo.CE = CE;
	nfo.ord = exprOrd++;
	/* We might have pfunction call through pointer from implicit (e.g. operator()) function */
	if (lastFunctionDef) {
		nfo.csid = lastFunctionDef->csIdMap.at(getCurrentCSPtr());
		std::pair<std::set<callfunc_info_t>::iterator,bool> rv = lastFunctionDef->funcinfo.insert(nfo);
		for(unsigned long i=0; i<proto->getNumParams(); i++) {
			QualType T = proto->getParamType(i);
			lastFunctionDef->refTypes.insert(T);
		}
		return &(*(rv.first));
	}
	return 0;
}

bool DbJSONClassVisitor::handleCallAddress(int64_t Addr,const Expr* AddressExpr,
		std::set<ValueHolder> callrefs,
		std::set<DbJSONClassVisitor::LiteralHolder> literalRefs, const QualType* baseType,
		const CallExpr* CE, const CStyleCastExpr* CSCE) {

	/* We've come here when there was a call expression that expands into integer value
	 */

	const FunctionProtoType* proto;
	if (baseType) {
		proto = lookForFunctionType(*baseType);
	}
	else {
		return false;
	}
	/* We could have K&R function prototype without information about its arguments; ignore */
	if (!proto) return false;
	std::stringstream protostr;
	if (proto) {
		protostr << " [" << proto->getNumParams() << "] (" << ") "
				<< (const void*)proto;
		noticeTypeClass(QualType(proto,0));
	}
	DBG(opts.debug, llvm::outs() << "  notice FunctionAddressCall: "
			<< "(*" << Addr << ")"
			<< protostr.str() << "\n" );
	callfunc_info_t nfo = {};
	nfo.FunctionDeclOrProtoType = (void*)proto;
	nfo.refObj = (void*)AddressExpr;
	nfo.callrefs = callrefs;
	nfo.literalRefs = literalRefs;
	nfo.CE = CE;
	nfo.ord = exprOrd++;
	/* We might have pfunction call through pointer from implicit (e.g. operator()) function */
	if (lastFunctionDef) {
		nfo.csid = lastFunctionDef->csIdMap.at(getCurrentCSPtr());
		lastFunctionDef->funcinfo.insert(nfo);
		for(unsigned long i=0; i<proto->getNumParams(); i++) {
			QualType T = proto->getParamType(i);
			lastFunctionDef->refTypes.insert(T);
		}

		VarRef_t VR;
		std::vector<VarRef_t> vVR;
		CStyleCastOrType valuecast;
		if (CSCE) {
			valuecast.setCast(const_cast<CStyleCastExpr*>(CSCE));
		}
		VarRef_t CEVR;
		VR.VDCAMUAS.setAddress(Addr,valuecast);
		vVR.push_back(VR);
		CEVR.VDCAMUAS.setRefCall(CE,Addr,valuecast);

		std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
				lastFunctionDef->derefList.insert(DereferenceInfo_t(CEVR,0,vVR,"",getCurrentCSPtr(),DereferenceFunction));
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(nfo.ord);
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
			[CE,this](const DereferenceInfo_t *d){
				llvm::raw_string_ostream exprstream(d->Expr);
				exprstream << "[" << getAbsoluteLocation(CE->getBeginLoc()) << "]: ";
				CE->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
				exprstream.flush();
			};

		return true;
	}
	return false;
}

bool DbJSONClassVisitor::handleCallStmtExpr(const Expr* E,
		std::set<ValueHolder> callrefs,
		std::set<DbJSONClassVisitor::LiteralHolder> literalRefs, const QualType* baseType, const CallExpr* CE) {

	/* We've come here when there was a call expression that expands into StmtExpr,
	 *   really clear way of invoking a function, like:
	 *   (*({do {} while(0); pfun;}))('x',3.0);
	 *   All things considered there might be many variables that constitute the callee
	 *   (but the StmtExpr last expression value must be a function one way or the other)
	 */

	const StmtExpr* SE = static_cast<const StmtExpr*>(E);
	const CompoundStmt* CS = SE->getSubStmt();
	CompoundStmt::const_body_iterator i = CS->body_begin();
	if (i!=CS->body_end()) {
		/* We have at least one statement in the body; get the last one */
		i = CS->body_end()-1;
		const Stmt* S = *i;
		const Expr* callee = cast<Expr>(S);
		if (callee->getStmtClass()==Stmt::ImplicitCastExprClass) {
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(callee);
			const Expr* E = lookForDeclReforMemberExpr(static_cast<const Expr*>(ICE));
			return (E && (handleCallDeclRefOrMemberExpr(E,callrefs,literalRefs,0,CE)));
		}
		else if (callee->getStmtClass()==Stmt::ParenExprClass) {
			const ParenExpr* PE = static_cast<const ParenExpr*>(callee);
			QualType baseType = PE->getType();
			const Expr* SubExpr = PE->getSubExpr();
			if (SubExpr->getStmtClass()==Stmt::ConditionalOperatorClass) {
				const ConditionalOperator* CO = static_cast<const ConditionalOperator*>(SubExpr);
				return (handleCallConditionalOperator(CO,callrefs,literalRefs,&baseType,CE));
			}
			else if (SubExpr->getStmtClass()==Stmt::CStyleCastExprClass) {
				const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(SubExpr);
				const Expr* E = lookForDeclReforMemberExpr(static_cast<const Expr*>(SubExpr));
				return (E && (handleCallDeclRefOrMemberExpr(E,callrefs,literalRefs,&baseType,CE)));
			}
		}
	}

	return false;
}

bool DbJSONClassVisitor::handleCallDeclRefOrMemberExpr(const Expr* E,
		std::set<ValueHolder> callrefs,
		std::set<DbJSONClassVisitor::LiteralHolder> literalRefs, const QualType* baseType, const CallExpr* CE) {

	/* We've come here when there was a call expression that expands into DeclRef or Member expressions
	 * In the first case it can be ordinary function call {fun()} or function call through the pointer
	 *   to function {(*pfun)()}
	 * In the second case it's a member function call through a given object {obj.fun()}
	 */

	if (E->getStmtClass()==Stmt::DeclRefExprClass) {
		const DeclRefExpr* DRE = static_cast<const DeclRefExpr*>(E);
		const ValueDecl* v = DRE->getDecl();
		if (v->getKind()==Decl::Function) {
			/* We might be calling some built-in function at declaration scope and
				none of the functions were defined yet */
			if (lastFunctionDef) {
				const FunctionDecl* FD = static_cast<const FunctionDecl*>(v);
				if (FD->getIdentifier()!=0) {
					// We might be calling some special function (like operator new) in C++ which doesn't have proper identifier
					DBG(opts.debug, llvm::outs() << "  notice FunctionCall: "
							<< FD->getName().str() << "() [" << FD->getNumParams() << "] ("
							<< FD->getLocation().printToString(Context.getSourceManager()) << ") "
							<< (const void*)FD << "\n" );
					callfunc_info_t nfo = {};
					if (FD->hasBody()) {
						nfo.FunctionDeclOrProtoType = (void*)(FD->getDefinition());
					}
					else {
						nfo.FunctionDeclOrProtoType = (void*)(FD->getCanonicalDecl());
					}
					nfo.callrefs = callrefs;
					nfo.literalRefs = literalRefs;
					nfo.CE = CE;
					nfo.ord = exprOrd++;
					nfo.csid = lastFunctionDef->csIdMap.at(getCurrentCSPtr());
					lastFunctionDef->funcinfo.insert(nfo);
					/* If we used '*' operator on direct function name place it into the derefs array */
					VarRef_t VR;
					std::vector<VarRef_t> vVR;
					VarRef_t CEVR;
					const UnaryOperator* UO = lookForUnaryOperatorInCallExpr(CE);
					if (UO) {
						VR.VDCAMUAS.setUnary(UO);
						vVR.push_back(VR);
						CEVR.VDCAMUAS.setCall(CE);
						if (lastFunctionDef) {
							std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
									lastFunctionDef->derefList.insert(DereferenceInfo_t(CEVR,0,vVR,"",getCurrentCSPtr(),DereferenceFunction));
							const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(nfo.ord);
							const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
								[CE,this](const DereferenceInfo_t *d){
									llvm::raw_string_ostream exprstream(d->Expr);
									exprstream << "[" << getAbsoluteLocation(CE->getBeginLoc()) << "]: ";
									CE->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
									exprstream.flush();
								};
							return true;
						}
					}
					return true;
				}
			}
		}
		else if ((v->getKind()==Decl::Var)||(v->getKind()==Decl::ParmVar)) {
			const VarDecl* VD = static_cast<const VarDecl*>(v);
			const DbJSONClassVisitor::callfunc_info_t* nfo = handleCallVarDecl(VD,DRE,callrefs,literalRefs, baseType, CE);
			if (nfo) {
				VarRef_t VR;
				std::vector<VarRef_t> vVR;
				VarRef_t CEVR;
				const UnaryOperator* UO = lookForUnaryOperatorInCallExpr(CE);
				if (UO) {
					VR.VDCAMUAS.setUnary(UO);
					vVR.push_back(VR);
					CEVR.VDCAMUAS.setRefCall(CE,UO);
				}
				else {
					const ArraySubscriptExpr* ASE = lookForArraySubscriptExprInCallExpr(CE);
					if (ASE) {
						VR.VDCAMUAS.setAS(ASE);
						vVR.push_back(VR);
						CEVR.VDCAMUAS.setRefCall(CE,ASE);
					}
					else {
						VR.VDCAMUAS.setValue(VD);
						vVR.push_back(VR);
						CEVR.VDCAMUAS.setRefCall(CE,VD);
					}
				}

				if (lastFunctionDef) {
					std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
							lastFunctionDef->derefList.insert(DereferenceInfo_t(CEVR,0,vVR,"",getCurrentCSPtr(),DereferenceFunction));
					const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(nfo->ord);
					const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
						[CE,this](const DereferenceInfo_t *d){
							llvm::raw_string_ostream exprstream(d->Expr);
							exprstream << "[" << getAbsoluteLocation(CE->getBeginLoc()) << "]: ";
							CE->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
							exprstream.flush();
						};
					return true;
				}
			}

			return false;
		}
	}
	else if (E->getStmtClass()==Stmt::MemberExprClass) {
		const MemberExpr* ME = static_cast<const MemberExpr*>(E);
		return handleCallMemberExpr(ME,callrefs,literalRefs, baseType, CE);
	}

	return false;
}

bool DbJSONClassVisitor::handleCallConditionalOperator(const ConditionalOperator* CO,
		std::set<ValueHolder> callrefs,
		std::set<DbJSONClassVisitor::LiteralHolder> literalRefs,
		const QualType* baseType, const CallExpr* CE) {

	const Expr* ELHS = lookForDeclReforMemberExpr(static_cast<const Expr*>(CO->getLHS()));
	const Expr* ERHS = lookForDeclReforMemberExpr(static_cast<const Expr*>(CO->getRHS()));
	return (ELHS && (handleCallDeclRefOrMemberExpr(ELHS,callrefs,literalRefs,baseType,CE))
			&& ERHS && (handleCallDeclRefOrMemberExpr(ERHS,callrefs,literalRefs,baseType,CE)));
}

size_t DbJSONClassVisitor::getFunctionDeclId(const FunctionDecl *FD) {

	FD = FD->hasBody() ? FD->getDefinition() : FD->getCanonicalDecl();

	if(FD->hasDefiningAttr())
		FD = FD->getCanonicalDecl();

	// in case of function template instantiations
	if(functionTemplateMap.find(FD) != functionTemplateMap.end()){
		const FunctionTemplateDecl *FTD = functionTemplateMap.at(FD);
		for(auto TD : FTD->redecls()){
			if(cast<FunctionTemplateDecl>(TD)->isThisDeclarationADefinition()){
				FTD = cast<FunctionTemplateDecl>(TD);
			}
		}
		FD = FTD->getTemplatedDecl();
	}

	if (FuncMap.find(FD) != FuncMap.end()) {
		return FuncMap.at(FD).id;
	}
	else if (FuncDeclMap.find(FD) != FuncDeclMap.end()) {
		return FuncDeclMap.at(FD).id;
	}
	else if (opts.assert&&(CTAList.find(FD) != CTAList.end())) {
		return FuncDeclMap.at(CTA).id;
	}
	else{
		FD->dump();
		assert(0 && "Called function not in function maps\n");
	}
}

  size_t DbJSONClassVisitor::outerFunctionorMethodIdforTagDecl(TagDecl* tD) {

	  size_t id = SIZE_MAX;
	  DeclContext* DC = tD->getParentFunctionOrMethod();
	  if (!DC) return id;
	  if (isa<FunctionDecl>(DC) || isa<CXXMethodDecl>(DC)) {
		const FunctionDecl* FD = static_cast<const FunctionDecl*>(DC);
		id = getFunctionDeclId(FD);
	  }
	  return id;
  }

  std::string DbJSONClassVisitor::parentFunctionOrMethodString(TagDecl* tD) {
	  std::string outerFn;
	  DeclContext* DC = tD->getParentFunctionOrMethod();
	  if (!DC) return outerFn;
	  if (isa<FunctionDecl>(DC)) {
		const FunctionDecl* FD = static_cast<const FunctionDecl*>(DC);
		if (isCXXTU(Context)) {
			std::string nms = getFullFunctionNamespace(FD);
			if (!nms.empty()) nms+="::";
			outerFn=nms+FD->getNameAsString();
		}
		else {
			outerFn = FD->getNameAsString();
		}
	  }
	  else if (isa<CXXMethodDecl>(DC)) {
	  const CXXMethodDecl* MD = static_cast<const CXXMethodDecl*>(DC);
	  const CXXRecordDecl* RD = MD->getParent();
	  QualType RT = Context.getRecordType(RD);
	  std::string _class = RT.getAsString();
	  outerFn = _class + MD->getNameAsString();
	  }
	  return outerFn;
  }

std::string DbJSONClassConsumer::getAbsoluteLocation(SourceLocation Loc){
	if(Loc.isInvalid())
		return "<invalid loc>";
	auto &SM = Context.getSourceManager();
	SourceLocation ELoc = SM.getExpansionLoc(Loc);

	StringRef RPath = SM.getFileEntryForID(SM.getFileID(ELoc))->tryGetRealPathName();
	if(RPath.empty()){
		//fallback to default
		llvm::errs()<<"Failed to get absolute location\n";
		llvm::errs()<<Loc.printToString(SM)<<'\n';
		return Loc.printToString(SM);
	}
	PresumedLoc PLoc = SM.getPresumedLoc(ELoc);
	if(PLoc.isInvalid())
		return "<invalid>";
	std::string locstr;
	llvm::raw_string_ostream s(locstr);
	s << RPath.str() << ':' << PLoc.getLine() << ':' << PLoc.getColumn();
	return s.str();
}
std::string DbJSONClassVisitor::getAbsoluteLocation(SourceLocation Loc){
	if(Loc.isInvalid())
		return "<invalid loc>";
	auto &SM = Context.getSourceManager();
	SourceLocation ELoc = SM.getExpansionLoc(Loc);

	StringRef RPath = SM.getFileEntryForID(SM.getFileID(ELoc))->tryGetRealPathName();
	if(RPath.empty()){
		//fallback to default
		llvm::errs()<<"Failed to get absolute location\n";
		llvm::errs()<<Loc.printToString(SM)<<'\n';
		return Loc.printToString(SM);
	}
	PresumedLoc PLoc = SM.getPresumedLoc(ELoc);
	if(PLoc.isInvalid())
		return "<invalid>";
	std::string locstr;
	llvm::raw_string_ostream s(locstr);
	s << RPath.str() << ':' << PLoc.getLine() << ':' << PLoc.getColumn();
	return s.str();
}

  std::string DbJSONClassConsumer::render_switch_json(const Expr* cond,
		  std::vector<std::pair<DbJSONClassVisitor::caseinfo_t,DbJSONClassVisitor::caseinfo_t>>& caselst,
		  std::string Indent) {

	  std::stringstream swdata;
	  std::string condbody;
	  llvm::raw_string_ostream cstream(condbody);
	  cond->printPretty(cstream,nullptr,Context.getPrintingPolicy());
	  cstream.flush();

	  swdata << Indent << "{\n";
	  swdata << Indent << "\t\t" << "\"condition\": \"" << json::json_escape(condbody) << "\",\n";
	  swdata << Indent << "\t\t" << "\"cases\": [\n";
	  for (auto u = caselst.begin(); u!=caselst.end();) {
		  swdata << Indent << "\t\t\t\t\t\t\t\t[ ";
		  DbJSONClassVisitor::caseinfo_t ciLHS = (*u).first;
		  DbJSONClassVisitor::caseinfo_t ciRHS = (*u).second;
		  /* Here we have the following entry for single case expression:
		   * [ expressionValue, enumCodeRepr, macroCodeRepr, rawCodeRepr ]
		   *  expressionValue: this is the computed value of the case constant expression (integer)
		   *  enumCodeRepr: if the case value comes directly from single enum identifier this is the string representation of this identifier
		   *  macroCodeRepr: if the case value comes directly from single macro identifier this is the string representation of this identifier
		   *  rawCodeRepr: this is the raw code representation of the case expression
		   * ]
		   * It is possible to use two expressions in the case to represent a interval, i.e. "case 8:12:",
		   *  in this case the case entry looks as follows:
		   * [ expressionValueLHS, enumCodeReprLHS, macroCodeReprLHS, rawCodeReprLHS,
		   *   expressionValueRHS, enumCodeReprRHS, macroCodeReprRHS, rawCodeReprRHS ]
		   * where first 4 elements corresponds to the left side of the interval and last 4 elements to the right side of interval
		   */
		  swdata << std::get<3>(ciLHS) << ", " << "\"" << std::get<0>(ciLHS).second << "\", \"" <<
				  json::json_escape(std::get<1>(ciLHS)) << "\", \"" << json::json_escape(std::get<2>(ciLHS)) << "\"";
		  if ((!std::get<0>(ciRHS).second.empty()) ||
				  (!std::get<1>(ciRHS).empty()) || (!std::get<2>(ciRHS).empty())) {
			  swdata << ", ";
			  	  swdata << std::get<3>(ciRHS) << ", " << "\"" << std::get<0>(ciRHS).second << "\", \"" <<
					  json::json_escape(std::get<1>(ciRHS)) << "\", \"" << json::json_escape(std::get<2>(ciRHS)) << "\"";
		  }
		  swdata << " ]";
		  ++u;
		  if (u!=caselst.end()) swdata << ",";
		  swdata << "\n";
	  }
	  swdata << Indent << "\t\t" << "]\n";
	  swdata << Indent << "}";

	  return swdata.str();
  }

  std::string DbJSONClassVisitor::refvarinfo_t::idString() {
    std::stringstream out;
    if (type==CALLVAR_FLOATING) {
        out << fp;
    }
    else if (type==CALLVAR_STRING) {
        out << "\"" << json::json_escape(s) << "\"";
    }
    else if (type==CALLVAR_INTEGER) {
    	int64_t i = (int64_t)id;
    	out << i;
    }
    else {
        /* MongoDB doesn't take 64bit unsigned values; cast it to signed value then */
        int64_t i = (int64_t)id;
        out << i;
    }
    return out.str();
  }

  std::string DbJSONClassVisitor::refvarinfo_t::LiteralString() {
	  if (lh.type==LiteralHolder::LiteralChar) {
		  std::stringstream ss;
		  ss << lh.prvLiteral.charLiteral;
		  return ss.str();
	  }
	  if (lh.type==LiteralHolder::LiteralInteger) {
		  std::stringstream ss;
		  ss << lh.prvLiteral.integerLiteral.extOrTrunc(63).getExtValue();
		  return ss.str();
	  }
	  if (lh.type==LiteralHolder::LiteralString) {
		  return "\"" + json::json_escape(lh.prvLiteral.stringLiteral) + "\"";
	  }
	  if (lh.type==LiteralHolder::LiteralFloat) {
		  std::stringstream ss;
		  ss << lh.prvLiteral.floatingLiteral;
		  return ss.str();
	  }
	  return "";
  }

  void DbJSONClassConsumer::printGlobalArray(int Indentation){
	  for(auto i = Visitor.getVarMap().begin(); i!=Visitor.getVarMap().end();i++) {
		  DbJSONClassVisitor::VarData &var_data = i->second;
      if(var_data.output == nullptr) continue;
		  printGlobalEntry(var_data,Indentation);
	  }
  }

  void DbJSONClassConsumer::printGlobalEntry(DbJSONClassVisitor::VarData &var_data, int Indentation){
	  llvm::raw_string_ostream GOut(*var_data.output);
	  std::string Indent(Indentation,'\t');
	  const VarDecl *D = var_data.Node;
	  QualType ST = D->getTypeSourceInfo() ? D->getTypeSourceInfo()->getType() : D->getType();
	  std::string name = D->getNameAsString();
	  std::set<QualType> STset;
	  if (D->getType()!=ST) {
		  if (ST->getTypeClass()==Type::Typedef) {
			  /* Clear qualifiers when adding typedef source type to references
			   *   so we could avoid qualification mismatch with global variable type
			   *   and typedef definition
			   */
			  var_data.g_refTypes.insert(ST.withoutLocalFastQualifiers());
			  STset.insert(ST);
		  }
	  }
	  std::string initstring;
	  if(D->hasInit()){
		  llvm::raw_string_ostream initstream(initstring);
		  D->getInit()->printPretty(initstream,nullptr,Context.getPrintingPolicy());
		  initstream.flush();
	  }
	  std::string def;
	  clang::PrintingPolicy policy = Context.getPrintingPolicy();
	  if (opts.adddefs) {
		  if (isOwnedTagDeclType(ST)) {
			  policy.IncludeTagDefinition = true;
			  var_data.g_refTypes.insert(ST);
			  var_data.g_refTypes.insert(D->getType());
		  }
		  llvm::raw_string_ostream defstream(def);
		  D->print(defstream,policy);
		  defstream.flush();
	  }
	  GOut << Indent << "\t{\n";
	  GOut << Indent << "\t\t\"name\": \"" << name << "\",\n";
	  GOut << Indent << "\t\t\"hash\": \"" << var_data.hash << "\",\n";
	  GOut << Indent << "\t\t\"id\": " << var_data.id << ",\n";
	  if (opts.adddefs) {
		  GOut << Indent << "\t\t\"def\": \"" << json::json_escape(def) << "\",\n";
	  }
	  std::stringstream globalrefs;
	  globalrefs << "[ ";
	  for (auto u = var_data.g_refVars.begin(); u!=var_data.g_refVars.end();) {
		  globalrefs << " " << Visitor.getVarData(*u).id;
		  ++u;
		  if (u!=var_data.g_refVars.end()) {
			  globalrefs << ",";
		  }
	  }
	  globalrefs << " ]";
	  std::vector<int> decls;
	  std::stringstream refs;
	  refs << "[ ";
	  int n=0;
	  for (auto u = var_data.g_refTypes.begin(); u!=var_data.g_refTypes.end(); ++n) {
		  QualType T = *u;
		  if (isOwnedTagDeclType(T)) decls.push_back(n);
		  /* Fix for #160
		  if (STset.find(T)!=STset.end()) decls.push_back(n);*/
		  refs << " " << Visitor.getTypeData(T).id;
		  ++u;
		  if (u!=var_data.g_refTypes.end()) {
			  refs << ",";
		  }
	  }
	  refs << " ]";
	  std::stringstream declsstream;
	  declsstream << "[ ";
	  for (auto u = decls.begin(); u!=decls.end();) {
		  declsstream << " " << *u;
		  ++u;
		  if (u!=decls.end()) {
			  declsstream << ",";
		  }
	  }
	  declsstream << " ]";
	  std::stringstream funrefs;
	  funrefs << "[ ";
	  for (auto u = var_data.g_refFuncs.begin(); u!=var_data.g_refFuncs.end();) {
			size_t rid = Visitor.getFunctionDeclId(*u);
			if (rid==SIZE_MAX) {
				(*u)->dump(llvm::errs());
				llvm::errs() << "@parent:\n";
				D->dump(llvm::errs());
				assert(0 && "Referenced function not in function maps\n");
			}
		  funrefs << " " << rid;
		  ++u;
		  if (u!=var_data.g_refFuncs.end()) {
			  funrefs << ",";
		  }
	  }
	  funrefs << " ]";
	  
	  // literals
	  std::stringstream int_literals;
	  std::stringstream char_literals;
	  std::stringstream float_literals;
	  std::stringstream string_literals;	
	  for(DbJSONClassVisitor::LiteralHolder lh : var_data.g_literals){
		  switch(lh.type){
			  case DbJSONClassVisitor::LiteralHolder::LiteralInteger: {
				  if(int_literals.str().empty()){
					  int_literals << "[ ";
				  }
				  else{
					  int_literals << ", ";
				  }
				  int_literals << lh.prvLiteral.integerLiteral.extOrTrunc(63).getExtValue();
				  break;
			  }
			  case DbJSONClassVisitor::LiteralHolder::LiteralChar:{
				  if(char_literals.str().empty()){
					  char_literals << "[ ";
				  }
				  else{
					  char_literals << ", ";
				  }
				  char_literals << lh.prvLiteral.charLiteral;
				  break;
			  }
			  case DbJSONClassVisitor::LiteralHolder::LiteralFloat:{
				  if(float_literals.str().empty()){
					  float_literals << "[ ";
				  }
				  else{
					  float_literals << ", ";
				  }
				  float_literals << lh.prvLiteral.floatingLiteral;
				  break;
			  }
			  case DbJSONClassVisitor::LiteralHolder::LiteralString:{
				  if(string_literals.str().empty()){
					  string_literals << "[ ";
				  }
				  else{
					  string_literals << ", ";
				  }
				  string_literals << "\"" + json::json_escape(lh.prvLiteral.stringLiteral) + "\"";
				  break;
			  }
		  }
	  }
	  int_literals << (int_literals.str().empty() ? "[]" : " ]");
	  char_literals << (char_literals.str().empty() ? "[]" : " ]");
	  float_literals << (float_literals.str().empty() ? "[]" : " ]");
	  string_literals << (string_literals.str().empty() ? "[]" : " ]");
	  GOut << Indent << "\t\t\"literals\":{\n";
	  GOut << Indent << "\t\t\t\"integer\": " << int_literals.str() <<",\n";
	  GOut << Indent << "\t\t\t\"character\": " << char_literals.str() <<",\n";
	  GOut << Indent << "\t\t\t\"floating\": " << float_literals.str() <<",\n";
	  GOut << Indent << "\t\t\t\"string\": " << string_literals.str() <<"\n";
	  GOut << Indent << "\t\t},\n";
	  GOut << Indent << "\t\t\"globalrefs\": " << globalrefs.str() << ",\n";
	  GOut << Indent << "\t\t\"refs\": " << refs.str() << ",\n";
	  GOut << Indent << "\t\t\"funrefs\": " << funrefs.str() << ",\n";
	  GOut << Indent << "\t\t\"decls\": " << declsstream.str() << ",\n";
	  GOut << Indent << "\t\t\"fid\": " << file_id << ",\n";
	  GOut << Indent << "\t\t\"type\": " << Visitor.getTypeData(D->getType()).id << ",\n";
	  GOut << Indent << "\t\t\"linkage\": \"" << translateLinkage(D->getLinkageInternal()) << "\",\n";
	  GOut << Indent << "\t\t\"location\": \"" << getAbsoluteLocation(D->getLocation()) << "\",\n";
	  GOut << Indent << "\t\t\"deftype\": " << D->hasDefinition() << ",\n";
	  GOut << Indent << "\t\t\"hasinit\": " << D->hasInit() << ",\n";
	  GOut << Indent << "\t\t\"init\": \"" << json::json_escape(initstring) << "\"\n";
	  GOut << Indent << "\t}";
  }

  void DbJSONClassVisitor::varInfoForRefs(FuncData &func_data,
		  const std::set<ValueHolder>& refs,
		  std::set<DbJSONClassVisitor::LiteralHolder> literals,
		  std::vector<struct refvarinfo_t>& refvarList) {

	for (auto ri = refs.begin(); ri!=refs.end(); ++ri) {
		const ValueDecl* VD = (*ri).getValue();
		unsigned pos = (*ri).getPos();
		  if (VD->getKind()==Decl::Function) {
			  const FunctionDecl* FD = static_cast<const FunctionDecl*>(VD);
			  refvarList.push_back(refvarinfo_t(getFunctionDeclId(FD),refvarinfo_t::CALLVAR_FUNCTION,pos));
		  }
		  else if (VD->getKind()==Decl::Var) {
			  const VarDecl* VrD = static_cast<const VarDecl*>(VD);
			  if (VrD->isDefinedOutsideFunctionOrMethod()) {
			  	  if (!VrD->isStaticDataMember()) {
					  if (getVarMap().find(VarForMap(VrD))!=getVarMap().end()) {
						  refvarList.push_back(refvarinfo_t(getVarData(VrD).id,
								  refvarinfo_t::CALLVAR_GLOBAL,pos));
					  }
					  else {
						  if (opts.exit_on_error) {
							  llvm::errs() << "\nERROR: Cannot find global variable in map for:\n";
							  VrD->dump(llvm::errs());
							  exit(EXIT_FAILURE);
						  }
						  else {
							  refvarList.push_back(refvarinfo_t(-1,refvarinfo_t::CALLVAR_GLOBAL,pos));
						  }
					  }
					}
			  }
			  else {
				  if (func_data.varMap.find(VrD)!=func_data.varMap.end()) {
					  refvarList.push_back(refvarinfo_t(func_data.varMap[VrD].varId,
							  refvarinfo_t::CALLVAR_LOCAL,pos));
				  }
				  else {
					  if (opts.exit_on_error) {
						  llvm::errs() << "\nERROR: Cannot find local variable in map for:\n";
						  VrD->dump(llvm::errs());
						  exit(EXIT_FAILURE);
					  }
					  else {
						  refvarList.push_back(refvarinfo_t(-1,refvarinfo_t::CALLVAR_LOCAL,pos));
					  }
				  }
			  }
		  }
		  else if (VD->getKind()==Decl::ParmVar) {
			  const ParmVarDecl* PVrD = static_cast<const ParmVarDecl*>(VD);
			  if (func_data.varMap.find(PVrD)!=func_data.varMap.end()) {
				  refvarList.push_back(refvarinfo_t(func_data.varMap[PVrD].varId,
						  refvarinfo_t::CALLVAR_LOCALPARM,pos));
			  }
			  else {
				  if (opts.exit_on_error) {
					  llvm::errs() << "\nERROR: Cannot find function parameter variable in map for:\n";
					  PVrD->dump(llvm::errs());
					  exit(EXIT_FAILURE);
				  }
				  else {
					  refvarList.push_back(refvarinfo_t(-1,refvarinfo_t::CALLVAR_LOCAL,pos));
				  }
			  }
		  }
	}
	for (auto li = literals.begin(); li!=literals.end(); ++li) {
		DbJSONClassVisitor::LiteralHolder lh = (*li);
		if (lh.type==DbJSONClassVisitor::LiteralHolder::LiteralChar) {
			refvarList.push_back(refvarinfo_t(lh,refvarinfo_t::CALLVAR_CHARLITERAL,lh.pos));
		}
		if (lh.type==DbJSONClassVisitor::LiteralHolder::LiteralFloat) {
			refvarList.push_back(refvarinfo_t(lh,refvarinfo_t::CALLVAR_FLOATLITERAL,lh.pos));
		}
		if (lh.type==DbJSONClassVisitor::LiteralHolder::LiteralString) {
			refvarList.push_back(refvarinfo_t(lh,refvarinfo_t::CALLVAR_STRINGLITERAL,lh.pos));
		}
		if (lh.type==DbJSONClassVisitor::LiteralHolder::LiteralInteger) {
			refvarList.push_back(refvarinfo_t(lh,refvarinfo_t::CALLVAR_INTEGERLITERAL,lh.pos));
		}
	}

  }

  bool DbJSONClassVisitor::VR_referenced(VarRef_t& VR, std::set<const MemberExpr*>& MERef, std::set<const UnaryOperator*>& UnaryRef,
  		std::set<const ArraySubscriptExpr*>& ASRef, std::set<const BinaryOperator*>& CAORef, std::set<const BinaryOperator*>& LogicRef,
		std::set<const OffsetOfExpr*>& OOERef) {

	  if (VR.VDCAMUAS.getKind()==
			  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindUnary) {
		  return UnaryRef.find(VR.VDCAMUAS.getUnary())!=UnaryRef.end();
	  }
	  if (VR.VDCAMUAS.getKind()==
			  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAS) {
		  return ASRef.find(VR.VDCAMUAS.getAS())!=ASRef.end();
	  }
	  if (VR.VDCAMUAS.getKind()==
			  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindMemberExpr) {
		  return MERef.find(VR.VDCAMUAS.getME().first)!=MERef.end();
	  }
	  if (VR.VDCAMUAS.getKind()==
			  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCAO) {
		  return CAORef.find(VR.VDCAMUAS.getCAO())!=CAORef.end();
	  }
	  if (VR.VDCAMUAS.getKind()==
			  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindLogic) {
		  return LogicRef.find(VR.VDCAMUAS.getLogic())!=LogicRef.end();
	  }
	  if (VR.VDCAMUAS.getKind()==
			  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindOOE) {
		  return OOERef.find(VR.VDCAMUAS.getOOE())!=OOERef.end();
	  }

	  return false;
  }

  bool DbJSONClassVisitor::varInfoForVarRef(FuncData &func_data, VarRef_t VR,  struct refvarinfo_t& refvar,
		  std::map<const CallExpr*,unsigned long>& CEIdxMap,std::map<const MemberExpr*,unsigned>& MEIdxMap,
		  std::map<const UnaryOperator*,unsigned>& UnaryIdxMap, std::map<const ArraySubscriptExpr*,unsigned>& ASIdxMap,
		  std::map<const ValueDecl*,unsigned>& VDIdxMap, std::map<const BinaryOperator*,unsigned>& CAOIdxMap,
		  std::map<const BinaryOperator*,unsigned>& LogicIdxMap, std::map<const OffsetOfExpr*,unsigned>& OOEIdxMap) {

	long valueCastId = -1;
	CStyleCastOrType valuecast = VR.VDCAMUAS.getValueCast();
	if (valuecast.isValid()) {
		valueCastId = getTypeData(valuecast.getFinalType()).id;
	}

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindNone) return false;

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindValue) {

	  if (VR.VDCAMUAS.getValue()->getKind()==Decl::Var) {
		const VarDecl* VrD = static_cast<const VarDecl*>(VR.VDCAMUAS.getValue());

		  if (VrD->isDefinedOutsideFunctionOrMethod()) {
			  if (!VrD->isStaticDataMember()) {
				  if (getVarMap().find(VarForMap(VrD))!=getVarMap().end()) {
					  refvar.set(getVarData(VrD).id,refvarinfo_t::CALLVAR_GLOBAL,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
				  }
				  else {
					  if (opts.exit_on_error) {
						  llvm::errs() << "\nERROR: Cannot find global variable in map for:\n";
						  VrD->dump(llvm::errs());
						  exit(EXIT_FAILURE);
					  }
					  else {
						  refvar.set(-1,refvarinfo_t::CALLVAR_GLOBAL,VR.VDCAMUAS.getMeIdx());
					  }
				  }
				}
		  }
		  else {
			  if (func_data.varMap.find(VrD)!=func_data.varMap.end()) {
				  refvar.set(func_data.varMap[VrD].varId,refvarinfo_t::CALLVAR_LOCAL,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
			  }
			  else {
				  if (opts.exit_on_error) {
					  llvm::errs() << "\nERROR: Cannot find local variable in map for:\n";
					  VrD->dump(llvm::errs());
					  exit(EXIT_FAILURE);
				  }
				  else {
					  refvar.set(-1,refvarinfo_t::CALLVAR_LOCAL,VR.VDCAMUAS.getMeIdx());
				  }
			  }
		  }
	  }
	  else if (VR.VDCAMUAS.getValue()->getKind()==Decl::ParmVar) {
		  const ParmVarDecl* PVrD = static_cast<const ParmVarDecl*>(VR.VDCAMUAS.getValue());
		  if (func_data.varMap.find(PVrD)!=func_data.varMap.end()) {
			  refvar.set(func_data.varMap[PVrD].varId,refvarinfo_t::CALLVAR_LOCALPARM,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
		  }
		  else {
			  if (opts.exit_on_error) {
				  llvm::errs() << "\nERROR: Cannot find function parameter variable in map for:\n";
				  PVrD->dump(llvm::errs());
				  exit(EXIT_FAILURE);
			  }
			  else {
				  refvar.set(-1,refvarinfo_t::CALLVAR_LOCAL,VR.VDCAMUAS.getMeIdx());
			  }
		  }
	  }
	  else if (VR.VDCAMUAS.getValue()->getKind()==Decl::Function) {
		  const FunctionDecl* FD = static_cast<const FunctionDecl*>(VR.VDCAMUAS.getValue());
		  refvar.set(getFunctionDeclId(FD),refvarinfo_t::CALLVAR_FUNCTION,VR.VDCAMUAS.getMeIdx());
	  }
	  else {
		  if (opts.exit_on_error) {
			  llvm::errs() << "\nERROR: Invalid VarRef parameter for dereference:\n";
			  VR.VDCAMUAS.getValue()->dump(llvm::errs());
			  exit(EXIT_FAILURE);
		  }
		  else {
			  return false;
		  }
	  }

	  return true;
	}

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAddress) {
		int64_t addr = VR.VDCAMUAS.getAddress();
		refvar.set(addr,refvarinfo_t::CALLVAR_ADDRESS,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
		return true;
	}

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindInteger) {
		int64_t addr = VR.VDCAMUAS.getInteger();
		refvar.set(addr,refvarinfo_t::CALLVAR_INTEGER,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
		return true;
	}

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindFloating) {
		double fp = VR.VDCAMUAS.getFloating();
		refvar.setFp(-1,refvarinfo_t::CALLVAR_FLOATING,VR.VDCAMUAS.getMeIdx(),0,valueCastId,fp);
		return true;
	}

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindString) {
		std::string s = VR.VDCAMUAS.getString();
		refvar.setString(-1,refvarinfo_t::CALLVAR_STRING,VR.VDCAMUAS.getMeIdx(),0,valueCastId,s);
		return true;
	}

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindMemberExpr) {
		std::pair<const MemberExpr*,QualType> ME = VR.VDCAMUAS.getME();
		if (MEIdxMap.find(ME.first)==MEIdxMap.end()) {
			llvm::errs() << "Missing MemberExpr in Index Map\n";
			ME.first->dumpColor();
			llvm::errs() << "@FunctionDecl:\n";
			func_data.this_func->dumpColor();
			assert(0);
		}
		refvar.set(MEIdxMap[ME.first],refvarinfo_t::CALLVAR_MEMBER,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
		return true;
	}

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindUnary) {
		const UnaryOperator* UO = VR.VDCAMUAS.getUnary();
		if (UnaryIdxMap.find(UO)==UnaryIdxMap.end()) {
			llvm::errs() << "Missing UnaryOperator in Index Map\n";
			UO->dumpColor();
			llvm::errs() << "@FunctionDecl:\n";
			func_data.this_func->dumpColor();
			assert(0);
		}
		refvar.set(UnaryIdxMap[UO],refvarinfo_t::CALLVAR_UNARY,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
		return true;
	}

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAS) {
		const ArraySubscriptExpr* AS = VR.VDCAMUAS.getAS();
		if (ASIdxMap.find(AS)==ASIdxMap.end()) {
			llvm::errs() << "Missing ArraySubscriptExpr in Index Map\n";
			AS->dumpColor();
			llvm::errs() << "@FunctionDecl:\n";
			func_data.this_func->dumpColor();
			assert(0);
		}
		refvar.set(ASIdxMap[AS],refvarinfo_t::CALLVAR_ARRAY,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
		return true;
	}

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCAO) {
		const BinaryOperator* CAO = VR.VDCAMUAS.getCAO();
		if (CAOIdxMap.find(CAO)==CAOIdxMap.end()) {
			llvm::errs() << "Missing BinaryOperator in Index Map\n";
			CAO->dumpColor();
			llvm::errs() << "@FunctionDecl:\n";
			func_data.this_func->dumpColor();
			assert(0);
		}
		refvar.set(CAOIdxMap[CAO],refvarinfo_t::CALLVAR_ASSIGN,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
		return true;
	}
	
	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindLogic) {
		const BinaryOperator* CAO = VR.VDCAMUAS.getLogic();
		if (LogicIdxMap.find(CAO)==LogicIdxMap.end()) {
			llvm::errs() << "Missing logic BinaryOperator in Index Map\n";
			CAO->dumpColor();
			llvm::errs() << "@FunctionDecl:\n";
			func_data.this_func->dumpColor();
			assert(0);
		}
		refvar.set(LogicIdxMap[CAO],refvarinfo_t::CALLVAR_LOGIC,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
		return true;
	}

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindOOE) {
		const OffsetOfExpr* OOE= VR.VDCAMUAS.getOOE();
		if (OOEIdxMap.find(OOE)==OOEIdxMap.end()) {
			llvm::errs() << "Missing OffsetOfExpr in Index Map\n";
			OOE->dumpColor();
			llvm::errs() << "@FunctionDecl:\n";
			func_data.this_func->dumpColor();
			assert(0);
		}
		refvar.set(OOEIdxMap[OOE],refvarinfo_t::CALLVAR_OFFSETOF,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
		return true;
	}

	/* We must have ValueDeclOrCallExprOrAddress::ValueDeclOrCallExprOrAddressKind(Ref)Call */
	const CallExpr* CE = VR.VDCAMUAS.getCall();
	if (CEIdxMap.find(CE)==CEIdxMap.end()) return false;

	if (VR.VDCAMUAS.getKind()==ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCall) {
		refvar.set(CEIdxMap[CE],refvarinfo_t::CALLVAR_CALLREF,VR.VDCAMUAS.getMeIdx(),0,valueCastId);
	}
	else {
		const UnaryOperator* UO = VR.VDCAMUAS.getUnary();
		if (UO) {
			if (UnaryIdxMap.find(UO)==UnaryIdxMap.end()) {
				llvm::errs() << "Missing UnaryOperator in Index Map\n";
				UO->dumpColor();
				llvm::errs() << "@FunctionDecl:\n";
				func_data.this_func->dumpColor();
				assert(0);
			}
			refvar.set(CEIdxMap[CE],refvarinfo_t::CALLVAR_REFCALLREF,VR.VDCAMUAS.getMeIdx(),UnaryIdxMap[UO],valueCastId);
		}
		else {
			const ArraySubscriptExpr* AS = VR.VDCAMUAS.getAS();
			if (AS) {
				if (ASIdxMap.find(AS)==ASIdxMap.end()) {
					llvm::errs() << "Missing ArraySubscriptExpr in Index Map\n";
					AS->dumpColor();
					llvm::errs() << "@FunctionDecl:\n";
					func_data.this_func->dumpColor();
					assert(0);
				}
				refvar.set(CEIdxMap[CE],refvarinfo_t::CALLVAR_REFCALLREF,VR.VDCAMUAS.getMeIdx(),ASIdxMap[AS],valueCastId);
			}
			else {
				const BinaryOperator* CAO = VR.VDCAMUAS.getCAO();
				if (CAO) {
					if (CAOIdxMap.find(CAO)!=CAOIdxMap.end()){
						refvar.set(CEIdxMap[CE],refvarinfo_t::CALLVAR_REFCALLREF,VR.VDCAMUAS.getMeIdx(),CAOIdxMap[CAO],valueCastId);
					}
					else if(LogicIdxMap.find(CAO) != LogicIdxMap.end()){
						refvar.set(CEIdxMap[CE],refvarinfo_t::CALLVAR_REFCALLREF,VR.VDCAMUAS.getMeIdx(),LogicIdxMap[CAO],valueCastId);
					}
					else{
						llvm::errs() << "Missing CompoundAssigOperator or logic BinaryOperator in Index Map\n";
						CAO->dumpColor();
						llvm::errs() << "@FunctionDecl:\n";
						func_data.this_func->dumpColor();
						assert(0);
					}
				}
				else {
					/* Formality? As we almost surely cannot get here */
					const OffsetOfExpr* OOE = VR.VDCAMUAS.getOOE();
					if (OOE) {
						if (OOEIdxMap.find(OOE)==OOEIdxMap.end()) {
							llvm::errs() << "Missing OffsetOfExpr in Index Map\n";
							OOE->dumpColor();
							llvm::errs() << "@FunctionDecl:\n";
							func_data.this_func->dumpColor();
							assert(0);
						}
						refvar.set(CEIdxMap[CE],refvarinfo_t::CALLVAR_REFCALLREF,VR.VDCAMUAS.getMeIdx(),OOEIdxMap[OOE],valueCastId);
					}
					else {
						const ValueDecl* VD = VR.VDCAMUAS.getValue();
						if (VD) {
							if (VDIdxMap.find(VD)==VDIdxMap.end()) {
								llvm::errs() << "Missing ValueDecl in Index Map\n";
								VD->dumpColor();
								llvm::errs() << "@FunctionDecl:\n";
								func_data.this_func->dumpColor();
								assert(0);
							}
							refvar.set(CEIdxMap[CE],refvarinfo_t::CALLVAR_REFCALLREF,VR.VDCAMUAS.getMeIdx(),VDIdxMap[VD],valueCastId);
						}
						else {
							/* This must be a refcall through address */
							int64_t address = VR.VDCAMUAS.getAddress();
							refvar.set(CEIdxMap[CE],refvarinfo_t::CALLVAR_ADDRCALLREF,VR.VDCAMUAS.getMeIdx(),address,valueCastId);
						}
					}
				}
			}
		}
	}

	return true;
  }

  void DbJSONClassConsumer::computeVarHashes(){
	  SourceManager& SM = Context.getSourceManager();
	  auto file = SM.getFileEntryForID(SM.getMainFileID())->tryGetRealPathName();
	  assert(!file.empty() && "No absolute path for file");

	  for (auto i = Visitor.getVarMap().begin(); i!=Visitor.getVarMap().end(); ++i) {
		  const VarDecl *D = i->first;
		  DbJSONClassVisitor::VarData &var_data = i->second;
		  var_data.hash = D->getNameAsString();
		  if(!D->isExternallyVisible()){
			var_data.hash.append(file.str());
		  }
		  multi::registerVar(var_data);
	  }
  }

  void DbJSONClassConsumer::computeTypeHashes(){
	  SourceManager& SM = Context.getSourceManager();
	  for (auto i = Visitor.getTypeMap().begin(); i!=Visitor.getTypeMap().end(); ++i) {
		  QualType T = i->first;
		  DbJSONClassVisitor::TypeData &type_data = i->second;
		  if (opts.debugbuild) {
			  llvm::outs() << "@buildTypeString for:\n";
			  T.dump();
		  }
		  buildTypeString(T,type_data.hash);
		  if (opts.debugbuild) {
			  llvm::outs() << "[" << type_data.hash << "]\n-------------------- done!\n";
		  }
		  multi::registerType(type_data);
	  }
  }

  void DbJSONClassConsumer::getFuncTemplatePars(DbJSONClassVisitor::FuncDeclData *func_data){
	const FunctionDecl *D = func_data->this_func;
	llvm::raw_string_ostream tpstream(func_data->templatePars);
	if (Visitor.functionTemplateMap.find(D)!=Visitor.functionTemplateMap.end()) {
		auto FT = Visitor.functionTemplateMap[D];
		FT->getTemplateParameters()->print(tpstream,Context);
		tpstream.flush();
	}
	else if (isa<CXXMethodDecl>(D)) {
		CXXRecordDecl* RD = const_cast<CXXRecordDecl*>(static_cast<const CXXMethodDecl*>(D)->getParent());
		if (Visitor.classTemplateMap.find(RD)!=Visitor.classTemplateMap.end()) {
			auto FT = Visitor.classTemplateMap[RD];
			FT->getTemplateParameters()->print(tpstream,Context);
		}
		else if (Visitor.classTemplateSpecializationMap.find(RD)!=Visitor.classTemplateSpecializationMap.end()) {
			auto CTS = Visitor.classTemplateSpecializationMap[RD];
			printTemplateArgumentList(tpstream,CTS->getTemplateArgs().asArray(),Context.getPrintingPolicy());
		}
		else if (Visitor.classTemplatePartialSpecializationMap.find(RD)!=Visitor.classTemplatePartialSpecializationMap.end()) {
			auto CTS = Visitor.classTemplatePartialSpecializationMap[RD];
			cast<ClassTemplatePartialSpecializationDecl>(CTS)->getTemplateParameters()->print(tpstream,Context);
		}
	}
	tpstream.flush();
  }

  void DbJSONClassConsumer::getFuncDeclHash(DbJSONClassVisitor::FuncDeclData *func_data){
	const FunctionDecl *D = func_data->this_func;
	SHA_CTX cd;
	SHA_init(&cd);
	
	getFuncTemplatePars(func_data);
	SHA_update(&cd, func_data->templatePars.data(), func_data->templatePars.size());

	getFuncDeclSignature(D,func_data->signature);
	SHA_update(&cd, func_data->signature.data(), func_data->signature.size());

	if (D->isCXXClassMember()) {
		const CXXMethodDecl* MD = static_cast<const CXXMethodDecl*>(D);
		const CXXRecordDecl* RD = MD->getParent();
		QualType RT = Context.getRecordType(RD);
		DbJSONClassVisitor::TypeData &RT_data = Visitor.getTypeData(RT);
		// build ahead of time
		if(!RT_data.hash.size()) buildTypeString(RT,RT_data.hash);
		SHA_update(&cd, RT_data.hash.data(), RT_data.hash.size());
	}

	if (isa<CXXMethodDecl>(D)) {
		CXXRecordDecl* RD = const_cast<CXXRecordDecl*>(static_cast<const CXXMethodDecl*>(D)->getParent());
		func_data->nms = Visitor.parentFunctionOrMethodString(RD);
	}
	if (func_data->nms.empty()) func_data->nms = getFullFunctionNamespace(D);
	SHA_update(&cd, func_data->nms.data(), func_data->nms.size());

	SHA_final(&cd);
	func_data->declhash = base64_encode(reinterpret_cast<const unsigned char*>(cd.buf.b),64);
  }

  void DbJSONClassConsumer::getFuncHash(DbJSONClassVisitor::FuncData *func_data){
	const FunctionDecl *D = func_data->this_func;
	getFuncDeclHash(func_data);
	SHA_CTX c;
	SHA_init(&c);

	SHA_update(&c, func_data->templatePars.data(), func_data->templatePars.size());

	llvm::raw_string_ostream bstream(func_data->body);
	D->print(bstream);
	bstream.flush();
	SHA_update(&c, func_data->body.data(), func_data->body.size());

	SourceManager& SM = Context.getSourceManager();
	std::string exp_loc = getAbsoluteLocation(SM.getExpansionLoc(D->getLocation()));
	SHA_update(&c,exp_loc.data(),exp_loc.size());

	//Adding static variable references to hash
	std::set<std::string> ordered;
	for (auto u = func_data->refVars.begin(); u!=func_data->refVars.end();u++){
		if(!D->isExternallyVisible()){
			std::string &vhash = Visitor.getVarData(u->first).hash;
			ordered.insert(vhash);
		}
	}
	for(auto &vhash : ordered){
		SHA_update(&c,vhash.data(),vhash.size());
	}

	if (D->isCXXClassMember()) {
		const CXXMethodDecl* MD = static_cast<const CXXMethodDecl*>(D);
		const CXXRecordDecl* RD = MD->getParent();
		QualType RT = Context.getRecordType(RD);
		DbJSONClassVisitor::TypeData &RT_data = Visitor.getTypeData(RT);
		// build ahead of time
		if(!RT_data.hash.size()) buildTypeString(RT,RT_data.hash);
		SHA_update(&c, RT_data.hash.data(), RT_data.hash.size());
	}

	SHA_update(&c, func_data->nms.data(), func_data->nms.size());

	SHA_final(&c);
	func_data->hash = base64_encode(reinterpret_cast<const unsigned char*>(c.buf.b), 64);
  }

  static bool isFunctionDefinitionDiscarded(const FunctionDecl *FD) {
    auto &Context = FD->getASTContext();
    if (!FD->isInlined()) return false;
    if (Context.getLangOpts().CPlusPlus && !FD->hasAttr<GNUInlineAttr>())
      return false;
    return Context.GetGVALinkageForFunction(FD) == GVA_AvailableExternally;
  }

  void DbJSONClassConsumer::computeFuncHashes() {
    // declarations
    for(auto i = Visitor.getFuncDeclMap().begin(); i!=Visitor.getFuncDeclMap().end(); ++i ){
      auto &func_data = i->second;
	  func_data.fid = file_id;
      getFuncDeclHash(&func_data);
      multi::registerFuncDecl(func_data);
    }
    // definitions
    for (auto i = Visitor.getFuncMap().begin(); i!=Visitor.getFuncMap().end(); ++i) {
      DbJSONClassVisitor::FuncData &func_data = i->second;
	  func_data.fid = file_id;
      getFuncHash(&func_data);
      if(func_data.this_func->getLinkageInternal() == Linkage::InternalLinkage){
        multi::registerFuncInternal(func_data);
      }
      else if(isFunctionDefinitionDiscarded(func_data.this_func)){
        multi::registerFuncInternal(func_data);
      }
      else if(func_data.this_func->isWeak()){
        multi::registerFunc(func_data);
      }
      else{
        multi::registerFunc(func_data);
      }
    }
  }

  void DbJSONClassConsumer::printFuncArray(int Indentation) {
	  for (auto i = Visitor.getFuncMap().begin(); i!=Visitor.getFuncMap().end();i++) {
		  DbJSONClassVisitor::FuncData &func_data = i->second;
      if(func_data.output == nullptr) continue;
		  printFuncEntry(func_data,Indentation);
	  }
  }

  void DbJSONClassConsumer::printFuncEntry(DbJSONClassVisitor::FuncData &func_data, int Indentation) {
	  llvm::raw_string_ostream FOut(*func_data.output);
	  SourceManager& SM = Context.getSourceManager();
	  std::string Indent(Indentation,'\t');
		  const FunctionDecl* D = func_data.this_func;
		  bool dependentClass = false;
		  bool hasTemplatePars = func_data.templatePars.size();
		  std::string outerFnForClass;
		  if (isa<CXXMethodDecl>(D)) {
			  CXXRecordDecl* RD = const_cast<CXXRecordDecl*>(static_cast<const CXXMethodDecl*>(D)->getParent());
			  if (!RD->isDefinedOutsideFunctionOrMethod()) {
				  outerFnForClass = Visitor.parentFunctionOrMethodString(RD);
			  }
		  }
		  
		  auto Range = SM.getExpansionRange(D->getSourceRange()).getAsRange();
		  std::string upBody = Lexer::getSourceText(CharSourceRange(Range, true),SM,Context.getLangOpts()).str();
		  std::string fbody;
		  Stmt* body = D->getBody();
		  std::string fcsbody;
		  llvm::raw_string_ostream bcsstream(fcsbody);
		  body->printPretty(bcsstream,nullptr,Context.getPrintingPolicy());
		  bcsstream.flush();
		  std::string fname = D->getName().str();
		  std::string linkage = translateLinkage(D->getLinkageInternal());
		  std::string declbody = func_data.body.substr(0,func_data.body.find("{")-1);
		  // locations
		  std::string expansions;
		  llvm::raw_string_ostream exp_os(expansions);
		  if(opts.save_expansions && !D->getLocation().isMacroID()){
			auto BRange = SM.getExpansionRange(body->getSourceRange());
			auto ExpRanges = Macros.getExpansionRanges();
			bool first_exp = true;
			for(auto it = ExpRanges.lower_bound(BRange.getBegin()), end = ExpRanges.upper_bound(BRange.getEnd()); it!= end;it++){
				const char *text = Macros.getExpansionText(it->first);
				if(!text) continue;
				unsigned int pos = SM.getFileOffset(it->first) -SM.getFileOffset(Range.getBegin());
				unsigned int len = SM.getFileOffset(it->second) - SM.getFileOffset(it->first);
				if(!first_exp) exp_os<<",\n";
				first_exp = false;
				exp_os<<Indent<<"\t\t\t{\n";
				exp_os<<Indent<<"\t\t\t\t\"pos\": "<<pos<<",\n";
				exp_os<<Indent<<"\t\t\t\t\"len\": "<<len<<",\n";
				exp_os<<Indent<<"\t\t\t\t\"text\": \""<<json::json_escape(text)<<"\"\n";
				exp_os<<Indent<<"\t\t\t}";
			}
		  }
          SHA_CTX csc;
          SHA_init(&csc);
          SHA_update(&csc, fcsbody.data(), fcsbody.size());
		  SHA_final(&csc);
		  
		  std::stringstream argss;
		  argss << "[";
		  QualType rT = D->getReturnType();
		  DbJSONClassVisitor::TypeData &r_data = Visitor.getTypeData(rT);
		  argss << " " << r_data.id;
		  for(unsigned long j=0; j<D->getNumParams(); j++) {
			  const ParmVarDecl* p = D->getParamDecl(j);
			  QualType T = p->getTypeSourceInfo() ? p->getTypeSourceInfo()->getType() :p->getType();
			  DbJSONClassVisitor::TypeData &par_data = Visitor.getTypeData(T);
			  argss << ", " << par_data.id;
		  }
		  argss << " ]";
		  std::stringstream calls;
		  std::stringstream call_info;
		  std::vector<const CallExpr*> call_info_CE_v;
		  std::vector<std::vector<struct DbJSONClassVisitor::refvarinfo_t>> callrefs_v;
		  std::vector<std::string> refcalls;
		  std::stringstream refcall_info;
		  std::vector<const CallExpr*> refcall_info_CE_v;
		  std::vector<std::vector<struct DbJSONClassVisitor::refvarinfo_t>> refcallrefs_v;
		  std::map<const CallExpr*,unsigned long> CEIdxMap;
		  if (opts.call) {
			  bool next_item = false;
			  calls << "[";
			  call_info << "[";
			  unsigned long call_array_index = 0;
			  for (auto u = func_data.funcinfo.begin(); u!=func_data.funcinfo.end();) {

				  std::vector<struct DbJSONClassVisitor::refvarinfo_t> callvarList;
				  if (((*u).refObj) || (*u).funcproto) {
					  ++u;
					  continue;
				  }
				  const FunctionDecl *CF = static_cast<const FunctionDecl*>(u->FunctionDeclOrProtoType);
				  if (!next_item) { calls << " "; call_info << " ";}
				  else { calls << ", "; call_info << ", ";}
				  calls << Visitor.getFunctionDeclId(CF);
				  Visitor.varInfoForRefs(func_data,(*u).callrefs,(*u).literalRefs,callvarList);
				  printCallInfo(*u, call_info, SM, call_info_CE_v);
				  callrefs_v.push_back(callvarList);
				  CEIdxMap.insert(std::pair<const CallExpr*,unsigned long>((*u).CE,call_array_index));
				  call_array_index++;
				  ++u;
				  next_item = true;
			  }
			  calls << " ]";
			  call_info << "\n\t\t\t\t]";
			  std::vector<std::string> refcall_info_items;
			  for (auto u = func_data.funcinfo.begin(); u!=func_data.funcinfo.end(); ++u) {
				  if ((((*u).refObj) || (*u).funcproto) && !((*u).classType.isNull())) {
					  std::vector<struct DbJSONClassVisitor::refvarinfo_t> callvarList;
					  std::stringstream refcall;
					  QualType fT = QualType::getFromOpaquePtr((*u).FunctionDeclOrProtoType);
					  int fid = Visitor.getTypeData(fT).id;
					  QualType cT = (*u).classType;
					  int cid = Visitor.getTypeData(cT).id;
					  // [ FUNCTION PROTOTYPE | CLASS TYPE | FIELD INDEX ]
					  refcall << "[ " << fid << ", " << cid << ", " << (*u).fieldIndex << " ]";
					  refcalls.push_back(refcall.str());
					  std::stringstream refcallinfo;
					  printCallInfo((*u),refcallinfo,SM,exprOrd++,refcall_info_CE_v);
					  refcall_info_items.push_back(refcallinfo.str());
					  Visitor.varInfoForRefs(func_data,(*u).callrefs,(*u).literalRefs,callvarList);
					  refcallrefs_v.push_back(callvarList);
					  CEIdxMap.insert(std::pair<const CallExpr*,unsigned long>((*u).CE,call_array_index));
					  call_array_index++;
				  }
				  else if (((*u).refObj) && (*u).FunctionDeclOrProtoType) {
					  // Mere pointer to function call through variable
					  // [ FUNCTION PROTOTYPE ]
					  std::vector<struct DbJSONClassVisitor::refvarinfo_t> callvarList;
					  std::stringstream refcall;
					  QualType fT = QualType::getFromOpaquePtr((*u).FunctionDeclOrProtoType);
					  int fid = Visitor.getTypeData(fT).id;
					  refcall << "[ " << fid << " ]";
					  refcalls.push_back(refcall.str());
					  std::stringstream refcallinfo;
					  printCallInfo((*u),refcallinfo,SM,exprOrd++,refcall_info_CE_v);
					  refcall_info_items.push_back(refcallinfo.str());
					  Visitor.varInfoForRefs(func_data,(*u).callrefs,(*u).literalRefs,callvarList);
					  refcallrefs_v.push_back(callvarList);
					  CEIdxMap.insert(std::pair<const CallExpr*,unsigned long>((*u).CE,call_array_index));
					  call_array_index++;
				  }
				  else {
					  continue;
				  }
			  }
			  refcall_info << "[";
			  for (auto u=refcall_info_items.begin(); u!=refcall_info_items.end(); ++u) {
				  if (u!=refcall_info_items.begin()) {
					  refcall_info << ",";
				  }
				  refcall_info << (*u);
			  }
			  refcall_info << "\n\t\t\t\t]";
		  }
		  std::stringstream derefs;
		  derefs << Indent << "\t\t\"derefs\": [";
		  bool deref_first = true;
		  std::map<const MemberExpr*,unsigned> MEIdxMap;
		  std::map<const UnaryOperator*,unsigned> UnaryIdxMap;
		  std::map<const ArraySubscriptExpr*,unsigned> ASIdxMap;
		  std::map<const ValueDecl*,unsigned> VDIdxMap;
		  std::map<const BinaryOperator*,unsigned> CAOIdxMap;
		  std::map<const BinaryOperator*,unsigned> LogicIdxMap;
		  std::map<const OffsetOfExpr*,unsigned> OOEIdxMap;
		  /* Keep dereference information entries in predefined order */
		  std::vector<DbJSONClassVisitor::DereferenceInfo_t> derefList(func_data.derefList.begin(),func_data.derefList.end());
		  for (auto u = derefList.begin(); u!=derefList.end(); ++u) {
			  DbJSONClassVisitor::DereferenceInfo_t DI = *u;
			  if (DI.Kind==DbJSONClassVisitor::DereferenceMember) {
				  MEIdxMap.insert(std::pair<const MemberExpr*,unsigned>(DI.VR.VDCAMUAS.getME().first,std::distance(derefList.begin(),u)));
			  }
			  else if (DI.Kind==DbJSONClassVisitor::DereferenceUnary) {
				  UnaryIdxMap.insert(std::pair<const UnaryOperator*,unsigned>(DI.VR.VDCAMUAS.getUnary(),std::distance(derefList.begin(),u)));
			  }
			  else if (DI.Kind==DbJSONClassVisitor::DereferenceArray) {
				  ASIdxMap.insert(std::pair<const ArraySubscriptExpr*,unsigned>(DI.VR.VDCAMUAS.getAS(),std::distance(derefList.begin(),u)));
			  }
			  else if (DI.Kind==DbJSONClassVisitor::DereferenceFunction) {
				  VDIdxMap.insert(std::pair<const ValueDecl*,unsigned>(DI.VR.VDCAMUAS.getValue(),std::distance(derefList.begin(),u)));
			  }
			  else if (DI.Kind==DbJSONClassVisitor::DereferenceAssign) {
				  CAOIdxMap.insert(std::pair<const BinaryOperator*,unsigned>(DI.VR.VDCAMUAS.getCAO(),std::distance(derefList.begin(),u)));
			  }
			  else if (DI.Kind==DbJSONClassVisitor::DereferenceLogic) {
				  LogicIdxMap.insert(std::pair<const BinaryOperator*,unsigned>(DI.VR.VDCAMUAS.getLogic(),std::distance(derefList.begin(),u)));
			  }
			  else if (DI.Kind==DbJSONClassVisitor::DereferenceOffsetOf) {
				  OOEIdxMap.insert(std::pair<const OffsetOfExpr*,unsigned>(DI.VR.VDCAMUAS.getOOE(),std::distance(derefList.begin(),u)));
			  }
		  }
		  /* Find out which entries are referenced by other entries */
		  std::set<const MemberExpr*> MERef;
		  std::set<const UnaryOperator*> UnaryRef;
		  std::set<const ArraySubscriptExpr*> ASRef;
		  std::set<const BinaryOperator*> CAORef;
		  std::set<const BinaryOperator*> LogicRef;
		  std::set<const OffsetOfExpr*> OOERef;
		  for (auto u = derefList.begin(); u!=derefList.end(); ++u) {
			  DbJSONClassVisitor::DereferenceInfo_t DI = *u;
			  std::vector<DbJSONClassVisitor::VarRef_t>& vVR = DI.vVR;
			  for (size_t x=0; x<vVR.size(); ++x) {
				  DbJSONClassVisitor::VarRef_t iVR = vVR[x];
				  if (iVR.VDCAMUAS.getKind()==DbJSONClassVisitor::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::
						  ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindMemberExpr) {
						std::pair<const MemberExpr*,QualType> ME = iVR.VDCAMUAS.getME();
						MERef.insert(ME.first);
					}
					if (iVR.VDCAMUAS.getKind()==DbJSONClassVisitor::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::
							ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindUnary) {
						const UnaryOperator* UO = iVR.VDCAMUAS.getUnary();
						UnaryRef.insert(UO);
					}
					if (iVR.VDCAMUAS.getKind()==DbJSONClassVisitor::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::
							ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindAS) {
						const ArraySubscriptExpr* AS = iVR.VDCAMUAS.getAS();
						ASRef.insert(AS);
					}
					if (iVR.VDCAMUAS.getKind()==DbJSONClassVisitor::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::
							ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindCAO) {
						const BinaryOperator* CAO = iVR.VDCAMUAS.getCAO();
						CAORef.insert(CAO);
					}
					if (iVR.VDCAMUAS.getKind()==DbJSONClassVisitor::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::
							ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindLogic) {
						const BinaryOperator* CAO = iVR.VDCAMUAS.getLogic();
						LogicRef.insert(CAO);
					}
					if (iVR.VDCAMUAS.getKind()==DbJSONClassVisitor::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::
							ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindOOE) {
						const OffsetOfExpr* OOE = iVR.VDCAMUAS.getOOE();
						OOERef.insert(OOE);
					}
			  }
		  }
		  /* Now print dereference entries */
		  typedef std::tuple<std::string,std::string,std::string,std::string,std::vector<unsigned>,std::vector<size_t>> deref_entry_t;
		  std::map<std::string,size_t> deref_set;
		  std::vector<deref_entry_t> deref_entries;
		  std::vector<unsigned> rmMap; /* Keeps track how many entries were deduplicated so far for a given index */
		  std::map<const Expr*,size_t> parmIndexMap;
		  std::map<std::string,const Expr*> parmReprMap;
		  unsigned rmCount = 0;
		  for (auto u = func_data.derefList.begin(); u!=func_data.derefList.end(); ++u) {
			  std::stringstream deref_intro;
			  std::stringstream deref_core;
			  std::stringstream deref_expr;
			  std::vector<unsigned> offrefids;
			  std::vector<size_t> ords;
			  DbJSONClassVisitor::DereferenceInfo_t DI = *u;
			  int64_t offset = DI.i;
			  std::vector<DbJSONClassVisitor::VarRef_t>& vVR = DI.vVR;
			  long csid = func_data.csIdMap[DI.CSPtr];
			  if ((DI.Kind==DbJSONClassVisitor::DereferenceMember)||(DI.Kind==DbJSONClassVisitor::DereferenceOffsetOf)) {
				  if (DI.Kind==DbJSONClassVisitor::DereferenceMember) {
					  DbJSONClassVisitor::VarRef_t& VR = DI.VR;
					  if (VR.MemberExprList.size()<=0) {
					  	llvm::errs() << "Invalid size for member expression list:\n";
					  	llvm::errs() << DI.Expr << "\n";
					  	VR.VDCAMUAS.getME().first->dumpColor();
					  	llvm::errs() << "Function: " << func_data.this_func->getName().str() << "\n";
					  	assert(0);
					  }
					  if (deref_first) {
						  deref_intro << "\n";
					  }
					  else deref_intro << ",\n";
					  deref_core << Indent << "\t\t\t{\n";
					  deref_core << Indent << "\t\t\t\t\"member\" : [ ";
					  size_t x=VR.MemberExprList.size()-1;
					  while (true) {
						  if (x<VR.MemberExprList.size()-1) deref_core << ",";
						  deref_core << VR.MemberExprList[x].first;
						  if (x<=0) break;
						  --x;
					  }
					  deref_core << " ],\n";

					  deref_core << Indent << "\t\t\t\t\"type\" : [ ";
					  x=VR.MECastList.size()-1;
					  while (true) {
						  if (x<VR.MECastList.size()-1) deref_core << ",";
						  QualType TC = VR.MECastList[x];
						  //TODO: temporary fix
						  // for -> dereference TC is pointer to struct type
						  // which sometimes is not added to type database
						  // TC should never be a pointer (unless decided otherwise)
						  // fix in place where type is added to deref data
						  if(TC->isPointerType()) TC = TC->getPointeeType();
						  deref_core << Visitor.getTypeData(TC).id;
						  if (x<=0) break;
						  --x;
					  }
					  deref_core << " ],\n";

					  deref_core << Indent << "\t\t\t\t\"access\" : [ ";
					  x=VR.MemberExprList.size()-1;
					  while (true) {
						  if (x<VR.MemberExprList.size()-1) deref_core << ",";
						  deref_core << VR.MemberExprList[x].second;
						  if (x<=0) break;
						  --x;
					  }
					  deref_core << " ],\n";

					  deref_core << Indent << "\t\t\t\t\"shift\" : [ ";
					  x=VR.OffsetList.size()-1;
					  while (true) {
						  if (x<VR.OffsetList.size()-1) deref_core << ",";
						  deref_core << VR.OffsetList[x];
						  if (x<=0) break;
						  --x;
					  }
					  deref_core << " ],\n";

					  bool haveMCall = false;
					  for (x=0; x<VR.MCallList.size(); ++x) {
						  if (VR.MCallList[x]!=0) {
							  haveMCall = true;
							  break;
						  }
					  }
					  if (haveMCall) {
						  deref_core << Indent << "\t\t\t\t\"mcall\" : [ ";
						  x=VR.MCallList.size()-1;
						  while (true) {
							  if (x<VR.MCallList.size()-1) deref_core << ",";
							  if (VR.MCallList[x]) {
								  if (CEIdxMap.find(VR.MCallList[x])==CEIdxMap.end()) {
									  llvm::errs() << "Missing Call Expression in Index Map\n";
									  VR.MCallList[x]->dumpColor();
									  assert(0);
								  }
								  deref_core << CEIdxMap[VR.MCallList[x]];
							  }
							  else {
								  deref_core << "-1";
							  }
							  if (x<=0) break;
							  --x;
						  }
						  deref_core << " ],\n";
					  }
				  }
				  if (DI.Kind==DbJSONClassVisitor::DereferenceOffsetOf) {
					  DbJSONClassVisitor::VarRef_t& VR = DI.VR;
					  if (VR.MemberExprList.size()<=0) {
					  	llvm::errs() << "Invalid size for field access list:\n";
					  	llvm::errs() << DI.Expr << "\n";
					  	VR.VDCAMUAS.getOOE()->dumpColor();
					  	llvm::errs() << "Function: " << func_data.this_func->getName().str() << "\n";
					  	assert(0);
					  }
					  if (deref_first) {
						  deref_intro << "\n";
					  }
					  else deref_intro << ",\n";
					  deref_core << Indent << "\t\t\t{\n";
					  deref_core << Indent << "\t\t\t\t\"member\" : [ ";

					  for (auto x=VR.MemberExprList.begin(); x!=VR.MemberExprList.end(); ++x) {
					  	  if (x!=VR.MemberExprList.begin()) deref_core << ",";
					  	  if ((*x).second==DbJSONClassVisitor::MemberExprKindInvalid) {
					  	  	deref_core << "-1";
					  	  }
					  	  else {
					  	  	deref_core << (*x).first;
					  	  }
					  }
					  deref_core << " ],\n";
					  deref_core << Indent << "\t\t\t\t\"type\" : [ ";
					  for (auto x=VR.MECastList.begin(); x!=VR.MECastList.end(); ++x) {
					  	  if (x!=VR.MECastList.begin()) deref_core << ",";
						  if (!(*x).isNull()) {
							  QualType TC = *x;
							  deref_core << Visitor.getTypeData(TC).id;
						  }
						  else {
						  	  deref_core << "-1";
						  }
					  }
					  deref_core << " ],\n";
				  }
			  }
			  else {
				  if (deref_first) {
					  deref_intro << "\n";
					  deref_intro << Indent << "\t\t\t{\n";
				  }
				  else {
					  deref_intro << ",\n";
					  deref_intro << Indent << "\t\t\t{\n";
				  }
			  }
			  deref_core << Indent << "\t\t\t\t\"kind\" : \"" << DI.KindString() << "\",\n";
			  if ((DI.Kind!=DbJSONClassVisitor::DereferenceMember)&&(DI.Kind!=DbJSONClassVisitor::DereferenceReturn)) {
				  if (DI.Kind==DbJSONClassVisitor::DereferenceFunction) {
					  DbJSONClassVisitor::VarRef_t& CEVR = DI.VR;
					  if (CEIdxMap.find(CEVR.VDCAMUAS.getCall())==CEIdxMap.end()) {
						  llvm::errs() << "Missing Call Expression in Index Map\n";
						  CEVR.VDCAMUAS.getCall()->dumpColor();
						  assert(0);
					  }
					  deref_core << Indent << "\t\t\t\t\"offset\" : " << CEIdxMap[CEVR.VDCAMUAS.getCall()] << ",\n";
				  }
				  else if(DI.Kind==DbJSONClassVisitor::DereferenceCond){
					  auto CS = func_data.cfData[offset].CS;
					  deref_core << Indent << "\t\t\t\t\"offset\" : " << func_data.csIdMap.at(CS) << ",\n";
				  }
				  else {
					  deref_core << Indent << "\t\t\t\t\"offset\" : " << offset << ",\n";
					  
				  }
			  }
			  if (DI.Kind==DbJSONClassVisitor::DereferenceArray || DI.Kind==DbJSONClassVisitor::DereferenceLogic) {
				  deref_core << Indent << "\t\t\t\t\"basecnt\" : " << DI.baseCnt << ",\n";
			  }
			  deref_core << Indent << "\t\t\t\t\"offsetrefs\" : [";
			  bool ideref_first = true;
			  std::stringstream deref_core_subst;
			  deref_core_subst << deref_core.str();
			  for (size_t x=0; x<vVR.size(); ++x) {
				  DbJSONClassVisitor::VarRef_t iVR = vVR[x];
				  DbJSONClassVisitor::refvarinfo_t irefvar;
				  if (Visitor.varInfoForVarRef(func_data,iVR,irefvar,CEIdxMap,MEIdxMap,UnaryIdxMap,ASIdxMap,VDIdxMap,CAOIdxMap,LogicIdxMap,OOEIdxMap)) {
					  if (ideref_first) {
					  	deref_core << "\n";
					  	deref_core_subst << "\n";
					  }
					  else {
					  	deref_core << ",\n";
					  	deref_core_subst << ",\n";
					  }
					  deref_core << Indent << "\t\t\t\t\t{\n";
					  deref_core_subst << Indent << "\t\t\t\t\t{\n";
					  deref_core << Indent << "\t\t\t\t\t\t\"kind\" : \"" << irefvar.typeString() << "\",\n";
					  deref_core_subst << Indent << "\t\t\t\t\t\t\"kind\" : \"" << irefvar.typeString() << "\",\n";
					  if (irefvar.getCastId()!=-1) {
						  deref_core << Indent << "\t\t\t\t\t\t\"cast\" : " << irefvar.getCastId() << ",\n";
						  deref_core_subst << Indent << "\t\t\t\t\t\t\"cast\" : " << irefvar.getCastId() << ",\n";
					  }
					  if (irefvar.isReferrent()) {
					  	deref_core << Indent << "\t\t\t\t\t\t\"id\" : " << JSONReplacementToken;
					  	offrefids.push_back(irefvar.id);
					  }
					  else {
					  	deref_core << Indent << "\t\t\t\t\t\t\"id\" : " << irefvar.idString();
					  }
					  deref_core_subst << Indent << "\t\t\t\t\t\t\"id\" : " << irefvar.idString();
					  if ((DI.Kind==DbJSONClassVisitor::DereferenceMember)||(DI.Kind==DbJSONClassVisitor::DereferenceOffsetOf)) {
						  deref_core << ",\n";
						  deref_core_subst << ",\n";
						  deref_core << Indent << "\t\t\t\t\t\t\"mi\" : " << DI.VR.VDCAMUAS.getMeCnt()-irefvar.mi;
						  deref_core_subst << Indent << "\t\t\t\t\t\t\"mi\" : " << DI.VR.VDCAMUAS.getMeCnt()-irefvar.mi;
					  }
					  if ((irefvar.getType()==DbJSONClassVisitor::refvarinfo_t::CALLVAR_REFCALLREF)||
							  (irefvar.getType()==DbJSONClassVisitor::refvarinfo_t::CALLVAR_ADDRCALLREF)){
						  deref_core << ",\n";
						  deref_core_subst << ",\n";
						  deref_core << Indent << "\t\t\t\t\t\t\"di\" : " << irefvar.di;
						  deref_core_subst << Indent << "\t\t\t\t\t\t\"di\" : " << irefvar.di;
					  }
					  deref_core << "\n";
					  deref_core_subst << "\n";
					  deref_core << Indent << "\t\t\t\t\t}";
					  deref_core_subst << Indent << "\t\t\t\t\t}";
					  ideref_first = false;
				  }
			  }
			  deref_core << Indent << "\n\t\t\t\t\t\t],\n";
			  deref_core_subst << Indent << "\n\t\t\t\t\t\t],\n";

			  deref_core << Indent << "\t\t\t\t\"ord\" : [ " << ORDReplacementToken;
			  deref_core_subst << Indent << "\t\t\t\t\"ord\" : [ " << ORDReplacementToken;
			  for (auto x=DI.ord.begin(); x!=DI.ord.end(); ++x) {
				ords.push_back(*x);
			  }
			  deref_core << " ],\n";
			  deref_core_subst << " ],\n";

			  deref_expr << Indent << "\t\t\t\t\"expr\" : \"" << json::json_escape(DI.Expr) << "\",\n";
			  deref_expr << Indent << "\t\t\t\t\"csid\" : " << csid << "\n";
			  /* Currently when some dereference entry is referenced by other it is not deduplicated,
			   *  even though it is the same as other entry encountered before
			   *  TODO: make proper deduplication with proper link resolution
			   */
			  if ((deref_set.find(deref_core_subst.str())==deref_set.end())||(Visitor.VR_referenced(DI.VR,MERef,UnaryRef,ASRef,CAORef,LogicRef,OOERef))) {
				  deref_entry_t dentry(deref_intro.str(),deref_core.str(),deref_expr.str(),Indent+"\t\t\t}",offrefids,ords);
				  deref_entries.push_back(dentry);
				  if (DI.Kind==DbJSONClassVisitor::DereferenceParm) {
					  parmIndexMap.insert(std::pair<const class Expr*,size_t>(DI.VR.VDCAMUAS.getParm(),deref_entries.size()-1));
					  parmReprMap.insert(std::pair<std::string,const class Expr*>(deref_core_subst.str(),DI.VR.VDCAMUAS.getParm()));
				  }
				  deref_set.insert(std::pair<std::string,size_t>(deref_core_subst.str(),deref_entries.size()-1));
				  deref_first = false;
			  }
			  else {
			  	/* Make deduplication */
				auto ii = deref_set.find(deref_core_subst.str());
				if (ii!=deref_set.end()) {
					size_t dentry_index = (*ii).second;
					deref_entry_t& dentry = deref_entries[dentry_index];
					std::vector<size_t>& ordvec = std::get<5>(dentry);
					ordvec.insert(ordvec.end(),ords.begin(),ords.end());
					if (DI.Kind==DbJSONClassVisitor::DereferenceParm) {
						if (parmReprMap.find(deref_core_subst.str())==parmReprMap.end()) {
							llvm::errs() << "ERROR: Missing 'parm' deref entry representation:\n";
							llvm::errs() << deref_core_subst.str() << "\n";
							llvm::errs() << "Expr: " << json::json_escape(DI.Expr) << "\n";
							llvm::errs() << "Function: " << func_data.this_func->getName().str() << "\n";
							llvm::errs() << "Body:\n" << json::json_escape(func_data.body) << "\n";
							llvm::errs() << "upBody:\n" << json::json_escape(upBody) << "\n";
							llvm::errs() << "----------------------------------------\n";
							exit(2);
						}
						const class Expr* uExpr = parmReprMap[deref_core_subst.str()];
						parmIndexMap.insert(std::pair<const class Expr*,size_t>(DI.VR.VDCAMUAS.getParm(),parmIndexMap[uExpr]));
					}
				}
			  	rmCount+=1;
			  }
			  rmMap.push_back(rmCount);
		  }
		  /* Actually print it to the stdout here */
		  for (auto u = deref_entries.begin(); u!=deref_entries.end(); ++u) {
		  	deref_entry_t& dentry = *u;
		  	derefs << std::get<0>(dentry);
		  	std::string dcore = std::get<1>(dentry);
		  	std::vector<unsigned>& ofids = std::get<4>(dentry);
		  	std::vector<size_t>& ords = std::get<5>(dentry);
		  	for (auto w = ofids.begin(); w!=ofids.end(); ++w) {
		  		size_t pos = dcore.find(JSONReplacementToken);
		  		std::stringstream idStr;
		  		if (pos==std::string::npos) {
		  			llvm::errs() << "ERROR: Couldn't find JSON replacement token for id " << *w << " (index " << std::distance(ofids.begin(),w) << ")\n";
		  			llvm::errs() << "Number of ids: " << ofids.size() << "\n";
		  			llvm::errs() << "Dereference entry index: " << std::distance(deref_entries.begin(),u) << "\n";
		  			assert(0);
		  		}
		  		unsigned shift = rmMap[*w];
				int64_t _id = (int64_t)(*w-shift);
				idStr << _id;
		  		dcore.replace(pos, JSONReplacementToken.size(), idStr.str());
		  	}
		  	std::stringstream ordStr;
		  	for (auto w = ords.begin(); w!=ords.end(); ++w) {
		  		if (w!=ords.begin()) ordStr << ",";
		  		ordStr << *w;
		  	}
		  	size_t pos = dcore.find(ORDReplacementToken);
		  	if (pos!=std::string::npos) {
		  		dcore.replace(pos, ORDReplacementToken.size(), ordStr.str());
		  	}
		  	derefs << dcore;
		  	derefs << std::get<2>(dentry);
		  	derefs << std::get<3>(dentry);
		  }
		  derefs << "\n" << Indent << "\t\t],\n";
		  std::stringstream globalrefs;
		  std::stringstream globalrefInfo;
		  globalrefs << "[ ";
		  globalrefInfo << "[ ";
		  for (auto u = func_data.refVars.begin(); u!=func_data.refVars.end();) {
			  globalrefs << " " << Visitor.getVarData(u->first).id;
			  globalrefInfo << "\n\t\t\t\t\t[";
			  auto range = u->second;
			  for (auto i = range.begin(); i!=range.end();)
			  {				
				std::string location = (*i)->getBeginLoc().printToString(SM);
				std::string startString = location.substr(location.find_last_of(':', location.find_last_of(':') - 1) + 1);
				location = (*i)->getEndLoc().printToString(SM);
				std::string endString = location.substr(location.find_last_of(':', location.find_last_of(':') - 1) + 1);
				globalrefInfo << "\n\t\t\t\t\t\t{";
				globalrefInfo << "\n\t\t\t\t\t\t\t\"start\":\"";
				globalrefInfo << startString;
				globalrefInfo << "\",\n\t\t\t\t\t\t\t\"end\":\"";
				globalrefInfo << endString;
				globalrefInfo << "\"\n\t\t\t\t\t\t}";

				++i;
				if (i!=range.end()) {
				  globalrefInfo << ",";
			    }
			  }
			  globalrefInfo << "\n\t\t\t\t\t]";
			  ++u;
			  if (u!=func_data.refVars.end()) {
				  globalrefs << ",";
				  globalrefInfo << ",";
			  }
		  }
		  globalrefs << " ]";
		  globalrefInfo << "\n\t\t\t\t]";
		  std::stringstream funrefs;
		  funrefs << "[ ";
		  for (auto u = func_data.refFuncs.begin(); u!=func_data.refFuncs.end();) {
		  		size_t rid = Visitor.getFunctionDeclId(*u);
		  		if (rid==SIZE_MAX) {
		  			(*u)->dump(llvm::errs());
		  			llvm::errs() << "@parent:\n";
		  			D->dump(llvm::errs());
		  			assert(0 && "Referenced function not in function maps\n");
		  		}
			  funrefs << " " << rid;
			  ++u;
			  if (u!=func_data.refFuncs.end()) {
				  funrefs << ",";
			  }
		  }
		  funrefs << " ]";
		  std::stringstream refs;
		  std::stringstream decls;
		  std::set<int> refsids_set;
		  std::vector<int> refsids;
		  std::vector<unsigned> declsidxs;
		  unsigned idx=0;
		  for (auto u = func_data.refTypes.begin(); u!=func_data.refTypes.end(); ++u) {
			  const TagDecl* TD = 0;
			  QualType T = *u;
			  auto x = refsids_set.insert(Visitor.getTypeData(T).id);
			  if (x.second) {
				  refsids.push_back(Visitor.getTypeData(T).id);
				  idx++;
				  if (T->getTypeClass()==Type::Elaborated) {
					  const ElaboratedType *tp = cast<ElaboratedType>(T);
					  T = tp->getNamedType();
				  }
				  if (T->getTypeClass()==Type::Record) {
					  const RecordType *rT = cast<RecordType>(T);
					  TD = rT->getDecl();
				  }
				  else if (T->getTypeClass()==Type::Enum) {
					  const EnumType *eT = cast<EnumType>(T);
					  TD = eT->getDecl();
				  }
			  }
			  if (TD) {
				  if (TD->isCompleteDefinition() && !TD->isDefinedOutsideFunctionOrMethod()) {
					  	  if (Visitor.outerFunctionorMethodIdforTagDecl(const_cast<TagDecl*>(TD))==func_data.id) {
						  declsidxs.push_back(idx-1);
					  }
				  }
			  }
		  }
		  refs << "[ ";
		  for (auto u = refsids.begin(); u!=refsids.end();) {
			  refs << " " << *u;
			  ++u;
			  if (u!=refsids.end()) {
				  refs << ",";
			  }
		  }
		  refs << " ]";
		  decls << "[ ";
		  for (auto u = declsidxs.begin(); u!=declsidxs.end();) {
			  decls << " " << *u;
			  ++u;
			  if (u!=declsidxs.end()) {
				  decls << ",";
			  }
		  }
		  decls << " ]";
		  std::stringstream attributes;
		  attributes << "[";
		  if (D->hasAttrs()) {
			  const AttrVec& V = D->getAttrs();
			  int first=1;
			  for (auto ai=V.begin(); ai!=V.end();) {
				  const Attr* a = *ai;
				  std::string attrs(a->getSpelling());
				  if (first) {
					  attributes << " \"" << attrs << "\"";
					  first = 0;
				  }
				  else {
					  attributes << ", \"" << attrs << "\"";
				  }
				  ++ai;
			  }
		  }
		  attributes << " ]";
		  
		  // handling literals
		  std::stringstream int_literals;
		  std::stringstream char_literals;
		  std::stringstream float_literals;
		  std::stringstream string_literals;
		  for(DbJSONClassVisitor::LiteralHolder lh : func_data.literals){
			  switch(lh.type){
				  case DbJSONClassVisitor::LiteralHolder::LiteralInteger: {
					  if(int_literals.str().empty()){
						  int_literals << "[ ";
					  }
					  else{
						  int_literals << ", ";
					  }
					  int_literals << lh.prvLiteral.integerLiteral.extOrTrunc(63).getExtValue();
					  break;
				  }
				  case DbJSONClassVisitor::LiteralHolder::LiteralChar:{
					  if(char_literals.str().empty()){
						  char_literals << "[ ";
					  }
					  else{
						  char_literals << ", ";
					  }
					  char_literals << lh.prvLiteral.charLiteral;
					  break;
				  }
				  case DbJSONClassVisitor::LiteralHolder::LiteralFloat:{
					  if(float_literals.str().empty()){
						  float_literals << "[ ";
					  }
					  else{
						  float_literals << ", ";
					  }
					  float_literals << lh.prvLiteral.floatingLiteral;
					  break;
				  }
				  case DbJSONClassVisitor::LiteralHolder::LiteralString:{
					  if(string_literals.str().empty()){
						  string_literals << "[ ";
					  }
					  else{
						  string_literals << ", ";
					  }
					  string_literals << "\"" + json::json_escape(lh.prvLiteral.stringLiteral) + "\"";
					  break;
				  }
			  }
		  }
		  int_literals << (int_literals.str().empty() ? "[]" : " ]");
		  char_literals << (char_literals.str().empty() ? "[]" : " ]");
		  float_literals << (float_literals.str().empty() ? "[]" : " ]");
		  string_literals << (string_literals.str().empty() ? "[]" : " ]");
		  
		  FOut << Indent << "\t{\n";
		  FOut << Indent << "\t\t\"name\": \"" << D->getName().str() << "\",\n";
		  if (isCXXTU(Context)) {
			  FOut << Indent << "\t\t\"namespace\": \"" << func_data.nms << "\",\n";
		  }
		  FOut << Indent << "\t\t\"id\": " << func_data.id << ",\n";
		  FOut << Indent << "\t\t\"fid\": " << file_id << ",\n";
		  FOut << Indent << "\t\t\"fids\": [ " << file_id << " ],\n";
		  FOut << Indent << "\t\t\"nargs\": " << D->getNumParams() << ",\n";
		  FOut << Indent << "\t\t\"variadic\": " << (D->isVariadic()?("true"):("false")) << ",\n";
		  FOut << Indent << "\t\t\"firstNonDeclStmt\": \"" << func_data.firstNonDeclStmtLoc << "\",\n";
		  if (D->isInlined()) FOut << Indent << "\t\t\"inline\": true,\n";
		  if (D->isCXXClassMember()) {
			  const CXXMethodDecl* MD = static_cast<const CXXMethodDecl*>(D);
			  const CXXRecordDecl* RD = MD->getParent();
			  if (RD->isDependentType()) dependentClass = true;
		  }
		  if (hasTemplatePars || dependentClass) FOut << Indent << "\t\t\"template\": true,\n";
		  if (!outerFnForClass.empty()) {
			  FOut << Indent << "\t\t\"classOuterFn\": \"" << outerFnForClass << "\",\n";
		  }
		  FOut << Indent << "\t\t\"linkage\": \"" << translateLinkage(D->getLinkageInternal()) << "\",\n";
		  if (D->isCXXClassMember()) {
			  FOut << Indent << "\t\t\"member\": true,\n";
			  const CXXMethodDecl* MD = static_cast<const CXXMethodDecl*>(D);
			  const CXXRecordDecl* RD = MD->getParent();
			  QualType RT = Context.getRecordType(RD);
			  std::string _class = RT.getAsString();
			  FOut << Indent << "\t\t\"class\": \"" << _class << "\",\n";
			  DbJSONClassVisitor::TypeData &RT_data = Visitor.getTypeData(RT);
			  FOut << Indent << "\t\t\"classid\": " << RT_data.id << ",\n";
		  }
		  FOut << Indent << "\t\t\"attributes\": " << attributes.str() << ",\n"; 
		  func_data.cshash = base64_encode(reinterpret_cast<const unsigned char*>(csc.buf.b), 64);
		  FOut << Indent << "\t\t\"hash\": \"" << func_data.hash << "\",\n";
		  FOut << Indent << "\t\t\"cshash\": \"" << func_data.cshash << "\",\n";
		  if (opts.addbody) {
			  if (hasTemplatePars) FOut << Indent << "\t\t\"template_parameters\": \"" << json::json_escape(func_data.templatePars) << "\",\n";
			  FOut << Indent << "\t\t\"body\": \"" << json::json_escape(func_data.body) << "\",\n";
			  FOut << Indent << "\t\t\"unpreprocessed_body\": \"" << json::json_escape(upBody) << "\",\n";
			  if(opts.save_expansions){
				FOut << Indent << "\t\t\"macro_expansions\":[\n";
				FOut << exp_os.str()<<'\n';
				FOut << Indent << "\t\t],\n";
			  }
			  FOut << Indent << "\t\t\"declbody\": \"" << json::json_escape(declbody) << "\",\n";
		  }
		  FOut << Indent << "\t\t\"signature\": \"" << json::json_escape(func_data.signature) << "\",\n";
		  FOut << Indent << "\t\t\"declhash\": \"" << func_data.declhash << "\",\n";
		  FOut << Indent << "\t\t\"location\": \"" << getAbsoluteLocation(D->getLocation()) << "\",\n";
		  std::string sloc = getAbsoluteLocation(D->getSourceRange().getBegin());
		  std::string eloc = getAbsoluteLocation(D->getSourceRange().getEnd());
		  FOut << Indent << "\t\t\"start_loc\": \"" << sloc << "\",\n";
		  FOut << Indent << "\t\t\"end_loc\": \"" << eloc << "\",\n";
		  FOut << Indent << "\t\t\"refcount\": " << 1 << ",\n";
		  // literals
		  FOut << Indent << "\t\t\"literals\":{\n";
		  FOut << Indent << "\t\t\t\"integer\": " << int_literals.str() <<",\n";
		  FOut << Indent << "\t\t\t\"character\": " << char_literals.str() <<",\n";
		  FOut << Indent << "\t\t\t\"floating\": " << float_literals.str() <<",\n";
		  FOut << Indent << "\t\t\t\"string\": " << string_literals.str() <<"\n";
		  FOut << Indent << "\t\t},\n";
		  //added taint to db.json
		  if(opts.taint){
			  FOut << Indent << "\t\t\"declcount\": "<<func_data.declcount<<",\n";
			  FOut << Indent << "\t\t\"taint\": {\n";
			  for(auto param : func_data.taintdata){
				  FOut << Indent << "\t\t\t\"" << param.first << "\":\n";
				  FOut << Indent << "\t\t\t\t[ ";
				  for(auto t : param.second){
					  if (func_data.varMap.find(t.second)==func_data.varMap.end()) {
						  Visitor.MissingVarDecl.insert(t.second);
						  if (opts.exit_on_error) {
							  llvm::errs() << "\nERROR: Cannot find local variable for:\n";
							  t.second->dump(llvm::errs());
							  exit(EXIT_FAILURE);
						  }
						  break;
					  }
					  FOut << "[ " << t.first << ", " << func_data.varMap[t.second].varId << " ]";
					  if(*param.second.rbegin() != t) FOut << ", ";
				  }
				  FOut << " ]";
				  if(*func_data.taintdata.rbegin() != param ) FOut << ",\n";
			  }
			  FOut << "\n" << Indent << "\t\t},\n";
		  }
		  //finished taint
		  if (opts.call) {
			  FOut << Indent << "\t\t\"calls\": " << calls.str() << ",\n";
			  std::string cis = call_info.str();
			  for (auto ci=call_info_CE_v.begin(); ci!=call_info_CE_v.end(); ++ci) {
				  const CallExpr* CE = *ci;
				  std::stringstream args;
				  for (unsigned ai=0; ai!=CE->getNumArgs(); ++ai) {
					  const Expr* argE = CE->getArg(ai);
					  if (parmIndexMap.find(argE)==parmIndexMap.end()) {
						  llvm::errs() << "ERROR: Missing function argument expression in 'parmIndexMap' [CE(" <<
								  std::distance(call_info_CE_v.begin(),ci) << "),ai(" << ai << ")]:\n";
						  argE->dumpColor();
						  llvm::errs() << "\n";
						  CE->dumpColor();
						  std::string CExpr;
						  llvm::raw_string_ostream exprstream(CExpr);
						  CE->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
						  exprstream.flush();
						  llvm::errs() << "Expr: " << exprstream.str() << "\n";
						  llvm::errs() << "parmIndexMap.size(): " << parmIndexMap.size() << "\n";
						  for (auto u=parmIndexMap.begin(); u!=parmIndexMap.end(); ++u) {
							  llvm::raw_string_ostream exprstream(CExpr);
							  u->first->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
							  exprstream.flush();
							  llvm::errs() << "  arg: " << exprstream.str() << "\n";
						  }
						  llvm::errs() << "Function: " << func_data.this_func->getName().str() << "\n";
						  llvm::errs() << "Body:\n" << json::json_escape(func_data.body) << "\n";
						  llvm::errs() << "upBody:\n" << json::json_escape(upBody) << "\n";
						  exit(2);
					  }
					  size_t di = parmIndexMap[argE];
					  if (ai!=0) args << ",";
					  args << di;
				  }
				  size_t pos = cis.find(ARGeplacementToken);
				  if (pos==std::string::npos) {
					  llvm::errs() << "ERROR: Couldn't find ARG replacement token for call index (" << std::distance(call_info_CE_v.begin(), ci) << ")\n";
					  CE->dumpColor();
					  exit(2);
				  }
				  cis.replace(pos, ARGeplacementToken.size(), args.str());
			  }
			  FOut << Indent << "\t\t\"call_info\": " << cis << ",\n";
			  FOut << Indent << "\t\t\"callrefs\": [\n";
			  for (auto i=callrefs_v.begin(); i!=callrefs_v.end();) {
				  auto callrefs = (*i);
				  FOut << Indent << "\t\t\t[\n";
				  for (auto j=callrefs.begin(); j!=callrefs.end();) {
					  FOut << Indent << "\t\t\t\t{\n";
					  FOut << Indent << "\t\t\t\t\t\"type\" : \"" << (*j).typeString() << "\",\n";
					  FOut << Indent << "\t\t\t\t\t\"pos\" : " << (*j).getPos() << ",\n";
					  if (!(*j).isLiteral()) {
						  FOut << Indent << "\t\t\t\t\t\"id\" : " << (*j).id << "\n";
					  }
					  else {
						  FOut << Indent << "\t\t\t\t\t\"id\" : " << (*j).LiteralString() << "\n";
					  }
					  FOut << Indent << "\t\t\t\t}";
					  ++j;
					  if (j!=callrefs.end()) FOut << ",";
					  FOut << "\n";
				  }
				  FOut << Indent << "\t\t\t]";
				  ++i;
				  if (i!=callrefs_v.end()) FOut << ",";
				  FOut << "\n";
			  }
			  FOut << Indent << "\t\t],\n";
			  FOut << Indent << "\t\t\"refcalls\": [\n";
			  for (auto i=refcalls.begin(); i!=refcalls.end();) {
				  FOut << Indent << "\t\t\t\t" << (*i);
				  ++i;
				  if (i!=refcalls.end()) FOut << ",";
				  FOut << "\n";
			  }
			  FOut << Indent << "\t\t],\n";
			  std::string ris = refcall_info.str();
			  for (auto ri=refcall_info_CE_v.begin(); ri!=refcall_info_CE_v.end(); ++ri) {
				  const CallExpr* CE = *ri;
				  std::stringstream args;
				  for (unsigned ai=0; ai!=CE->getNumArgs(); ++ai) {
					  const Expr* argE = CE->getArg(ai);
					  if (parmIndexMap.find(argE)==parmIndexMap.end()) {
						  llvm::errs() << "ERROR: Missing function argument expression in 'parmIndexMap' [rCE(" <<
								  std::distance(refcall_info_CE_v.begin(),ri) << "),ai(" << ai << ")]:\n";
						  argE->dumpColor();
						  llvm::errs() << "\n";
						  CE->dumpColor();
						  std::string CExpr;
						  llvm::raw_string_ostream exprstream(CExpr);
						  CE->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
						  exprstream.flush();
						  llvm::errs() << "Expr: " << exprstream.str() << "\n";
						  exit(2);
					  }
					  size_t di = parmIndexMap[argE];
					  if (ai!=0) args << ",";
					  args << di;
				  }
				  size_t pos = ris.find(ARGeplacementToken);
				  if (pos==std::string::npos) {
					  llvm::errs() << "ERROR: Couldn't find ARG replacement token for call index (" << std::distance(refcall_info_CE_v.begin(), ri) << ")\n";
					  CE->dumpColor();
					  exit(2);
				  }
				  ris.replace(pos, ARGeplacementToken.size(), args.str());
			  }

			  FOut << Indent << "\t\t\"refcall_info\": " << ris << ",\n";
			  FOut << Indent << "\t\t\"refcallrefs\": [\n";
			  for (auto i=refcallrefs_v.begin(); i!=refcallrefs_v.end();) {
				  auto refcallrefs = (*i);
				  FOut << Indent << "\t\t\t[\n";
				  for (auto j=refcallrefs.begin(); j!=refcallrefs.end();) {
					  FOut << Indent << "\t\t\t\t{\n";
					  FOut << Indent << "\t\t\t\t\t\"type\" : \"" << (*j).typeString() << "\",\n";
					  FOut << Indent << "\t\t\t\t\t\"pos\" : " << (*j).getPos() << ",\n";
					  if (!(*j).isLiteral()) {
						  FOut << Indent << "\t\t\t\t\t\"id\" : " << (*j).id << "\n";
					  }
					  else {
						  FOut << Indent << "\t\t\t\t\t\"id\" : " << (*j).LiteralString() << "\n";
					  }
					  FOut << Indent << "\t\t\t\t}";
					  ++j;
					  if (j!=refcallrefs.end()) FOut << ",";
					  FOut << "\n";
				  }
				  FOut << Indent << "\t\t\t]";
				  ++i;
				  if (i!=refcallrefs_v.end()) FOut << ",";
				  FOut << "\n";
			  }
			  FOut << Indent << "\t\t],\n";
		  }
		  else {
			FOut << Indent << "\t\t\"calls\": [],\n";
			FOut << Indent << "\t\t\"callrefs\": [],\n";
		  	FOut << Indent << "\t\t\"refcalls\": [],\n";
		  }
		  if (opts.switchopt) {
			  std::stringstream switches;
			  FOut << Indent << "\t\t\"switches\": [\n";
			  for (auto u = func_data.switch_map.begin(); u!=func_data.switch_map.end();) {
				  const Expr* cond = (*u).first;
				  std::vector<std::pair<DbJSONClassVisitor::caseinfo_t,DbJSONClassVisitor::caseinfo_t>> caselst = (*u).second;
				  FOut << render_switch_json(cond,caselst,Indent+"\t\t\t\t");
				  ++u;
				  if (u!=func_data.switch_map.end()) {
					  FOut << ",\n";
				  }
			  }
			  FOut << "\n" << Indent << "\t\t],\n";
		  }
		  if (opts.cstmt) {
			  FOut << Indent << "\t\t\"csmap\": [\n";
			  for (auto u = func_data.csParentMap.begin(); u!=func_data.csParentMap.end();) {
				  const CompoundStmt* CS = (*u).first;
				  assert(func_data.csIdMap.find(CS)!=func_data.csIdMap.end());
				  const CompoundStmt* parentCS = (*u).second;
				  if (parentCS) {
                                          if (func_data.csIdMap.find(parentCS)==func_data.csIdMap.end()) {
                                              parentCS=0;
                                          }
				  }
				  DbJSONClassVisitor::ControlFlowKind kind = DbJSONClassVisitor::cf_none;
				  if(func_data.csInfoMap.find(CS) != func_data.csInfoMap.end()){
					  kind = func_data.cfData[func_data.csInfoMap.at(CS)].kind;
				  }
				  FOut << Indent << "\t\t\t{\n";
				  FOut << Indent << "\t\t\t\"id\": " << func_data.csIdMap[CS] << ",\n";
				  if (parentCS) {
					  FOut << Indent << "\t\t\t\"pid\": " << func_data.csIdMap[parentCS] << ",\n";
				  }
				  else {
					  FOut << Indent << "\t\t\t\"pid\": " << -1 << ",\n";
				  }
				  FOut << Indent << "\t\t\t\"cf\": \"" << DbJSONClassVisitor::ControlFlowName(kind) <<"\"\n";
				  FOut << Indent << "\t\t\t}";
				  ++u;
				  if (u!=func_data.csParentMap.end()) {
					  FOut << ",";
				  }
				  FOut << "\n";
			  }
			  FOut << Indent << "\t\t],\n";
			  FOut << Indent << "\t\t\"locals\": [\n";
			  bool first = true;
			  for (auto u = func_data.varMap.begin(); u!=func_data.varMap.end();) {
				  const VarDecl* VD = 0;
				  DbJSONClassVisitor::VarInfo_t vi = (*u).second;
				  if ((!vi.VD) && (!vi.PVD)) {
					++u;
					continue;
				  }
				  if (vi.VD) {
					  VD = vi.VD;
				  }
				  else {
					  VD = vi.PVD;
				  }
				  if (!first) {
					  FOut << ",\n";
				  }
				  FOut << Indent << "\t\t\t{\n";
				  // id, name, type, csid, location
				  FOut << Indent << "\t\t\t\t\"id\": " << vi.varId << ",\n";
				  FOut << Indent << "\t\t\t\t\"name\": \"" << VD->getName().str() << "\",\n";
				  if (vi.VD) {
					  FOut << Indent << "\t\t\t\t\"parm\": " << "false" << ",\n";
				  }
				  else {
					  FOut << Indent << "\t\t\t\t\"parm\": " << "true" << ",\n";
				  }
				  QualType T = VD->getType();
				  FOut << Indent << "\t\t\t\t\"type\": " << Visitor.getTypeData(T).id << ",\n";
				  if (VD->isStaticLocal()) {
					  FOut << Indent << "\t\t\t\t\"static\": " << "true" << ",\n";
				  }
				  else {
					  FOut << Indent << "\t\t\t\t\"static\": " << "false" << ",\n";
				  }
				  if (VD->isUsed()) {
					  FOut << Indent << "\t\t\t\t\"used\": " << "true" << ",\n";
				  }
				  else {
					  FOut << Indent << "\t\t\t\t\"used\": " << "false" << ",\n";
				  }
				  if (vi.VD) {
					  FOut << Indent << "\t\t\t\t\"csid\": " << func_data.csIdMap[vi.CSPtr] << ",\n";
				  }
				  FOut << Indent << "\t\t\t\t\"location\": \"" << getAbsoluteLocation(VD->getLocation()) << "\"\n";
				  FOut << Indent << "\t\t\t}";
				  ++u;
				  first = false;
			  }
              FOut << "\n";
			  FOut << Indent << "\t\t],\n";
              FOut << derefs.str();
			  FOut << Indent << "\t\t\"ifs\": [\n";
			  for (auto u = func_data.ifMap.begin(); u!=func_data.ifMap.end();) {
				  DbJSONClassVisitor::IfInfo_t ii = (*u).second;
				  FOut << Indent << "\t\t\t{\n";
				  FOut << Indent << "\t\t\t\t\"csid\": " << func_data.csIdMap[ii.CSPtr] << ",\n";
				  FOut << Indent << "\t\t\t\t\"refs\":\n" << Indent << "\t\t\t\t[\n";
				  std::vector<struct DbJSONClassVisitor::refvarinfo_t> ifVarList;
				  std::set<DbJSONClassVisitor::ValueHolder> ifrefs;
				  std::set<DbJSONClassVisitor::LiteralHolder> literalifRefs;
				  Visitor.lookForDeclRefExprs(ii.ifstmt->getCond(),ifrefs);
				  Visitor.lookForLiteral(ii.ifstmt->getCond(),literalifRefs);
				  Visitor.varInfoForRefs(func_data,ifrefs,literalifRefs,ifVarList);
				  for (auto j=ifVarList.begin(); j!=ifVarList.end();) {
					  FOut << Indent << "\t\t\t\t\t{\n";
					  FOut << Indent << "\t\t\t\t\t\t\"type\" : \"" << (*j).typeString() << "\",\n";
					  if (!(*j).isLiteral()) {
						  FOut << Indent << "\t\t\t\t\t\t\"id\" : " << (*j).id << "\n";
					  }
					  else {
						  FOut << Indent << "\t\t\t\t\t\t\"id\" : " << (*j).LiteralString() << "\n";
					  }
					  FOut << Indent << "\t\t\t\t\t}";
					  ++j;
					  if (j!=ifVarList.end()) FOut << ",";
					  FOut << "\n";
				  }
				  FOut << Indent << "\t\t\t\t]\n";
				  FOut << Indent << "\t\t\t}";
				  ++u;
				  if (u!=func_data.ifMap.end()) {
					  FOut << ",";
				  }
				  FOut << "\n";
			  }
			  FOut << Indent << "\t\t],\n";
			  FOut << Indent << "\t\t\"asm\": [\n";
			  for (auto u = func_data.asmMap.begin(); u!=func_data.asmMap.end();) {
				  DbJSONClassVisitor::GCCAsmInfo_t ai = (*u).second;
				  FOut << Indent << "\t\t\t{\n";
				  FOut << Indent << "\t\t\t\t\"csid\": " << func_data.csIdMap[ai.CSPtr] << ",\n";
				  FOut << Indent << "\t\t\t\t\"str\": \"" << json::json_escape(ai.asmstmt->getAsmString()->getBytes().str()) << "\"\n";
				  FOut << Indent << "\t\t\t}";
				  ++u;
				  if (u!=func_data.asmMap.end()) {
					  FOut << ",";
				  }
				  FOut << "\n";
			  }
			  FOut << Indent << "\t\t],\n";
		  }
		  FOut << Indent << "\t\t\"globalrefs\": " << globalrefs.str() << ",\n";
		  FOut << Indent << "\t\t\"globalrefInfo\": " << globalrefInfo.str() << ",\n";
		  FOut << Indent << "\t\t\"funrefs\": " << funrefs.str() << ",\n";
		  FOut << Indent << "\t\t\"refs\": " << refs.str() << ",\n";
		  FOut << Indent << "\t\t\"decls\": " << decls.str() << ",\n";
		  FOut << Indent << "\t\t\"types\": " << argss.str() << "\n";
		  FOut << Indent << "\t}";
  }

  void DbJSONClassConsumer::printCallInfo(const DbJSONClassVisitor::callfunc_info_t& cfi, std::stringstream& ss,
		  clang::SourceManager& SM, std::vector<const CallExpr*>& CEv)
  {
	std::string location = getAbsoluteLocation(cfi.CE->getBeginLoc());
	std::string startString = location.substr(location.find_last_of(':', location.find_last_of(':') - 1) + 1);
	location = getAbsoluteLocation(cfi.CE->getEndLoc());
	std::string endString = location.substr(location.find_last_of(':', location.find_last_of(':') - 1) + 1);	

	ss << "\n\t\t\t\t\t{";
	ss << "\n\t\t\t\t\t\t\"start\":\"";
	ss << startString;
	ss << "\",\n\t\t\t\t\t\t\"end\":\"";
	ss << endString;
	ss << "\",\n\t\t\t\t\t\t\"ord\":";
	ss << cfi.ord;
	ss << ",\n\t\t\t\t\t\t\"args\": [ " << ARGeplacementToken << " ]";
	CEv.push_back(cfi.CE);
	std::string Expr;
	llvm::raw_string_ostream exprstream(Expr);
	cfi.CE->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
	exprstream.flush();
	ss << ",\n\t\t\t\t\t\t\"expr\": \"" << json::json_escape(Expr) << "\"";
	ss << ",\n\t\t\t\t\t\t\"loc\": \"" << location << "\"";
	ss << ",\n\t\t\t\t\t\t\"csid\": " << cfi.csid;
	ss << "\n\t\t\t\t\t}";
  }

  void DbJSONClassConsumer::printCallInfo(const DbJSONClassVisitor::callfunc_info_t& cfi, std::stringstream& ss, clang::SourceManager& SM, size_t ord,
		  std::vector<const CallExpr*>& CEv)
  {
	std::string location = getAbsoluteLocation(cfi.CE->getBeginLoc());
	std::string startString = location.substr(location.find_last_of(':', location.find_last_of(':') - 1) + 1);
	location = getAbsoluteLocation(cfi.CE->getEndLoc());
	std::string endString = location.substr(location.find_last_of(':', location.find_last_of(':') - 1) + 1);

	ss << "\n\t\t\t\t\t{";
	ss << "\n\t\t\t\t\t\t\"start\":\"";
	ss << startString;
	ss << "\",\n\t\t\t\t\t\t\"end\":\"";
	ss << endString;
	ss << "\",\n\t\t\t\t\t\t\"ord\":";
	ss << ord;
	ss << ",\n\t\t\t\t\t\t\"args\": [ " << ARGeplacementToken << " ]";
	CEv.push_back(cfi.CE);
	std::string Expr;
	llvm::raw_string_ostream exprstream(Expr);
	cfi.CE->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
	exprstream.flush();
	ss << ",\n\t\t\t\t\t\t\"expr\": \"" << json::json_escape(Expr) << "\"";
	ss << ",\n\t\t\t\t\t\t\"loc\": \"" << location << "\"";
	ss << ",\n\t\t\t\t\t\t\"csid\": " << cfi.csid;
	ss << "\n\t\t\t\t\t}";
  }

  void DbJSONClassConsumer::printFuncDeclArray(int Indentation) {
    for (auto i = Visitor.getFuncDeclMap().begin(); i!=Visitor.getFuncDeclMap().end();i++) {
		  DbJSONClassVisitor::FuncDeclData &func_data = i->second;
      if(func_data.output == nullptr) continue;
		  printFuncDeclEntry(func_data,Indentation);
	  }
  }

  void DbJSONClassConsumer::printFuncDeclEntry(DbJSONClassVisitor::FuncDeclData &func_data, int Indentation) {
		  llvm::raw_string_ostream FDOut(*func_data.output);
  		  std::string Indent(Indentation,'\t');
  		  const FunctionDecl* D = func_data.this_func;

  		  bool dependentClass = false;
		  bool hasTemplatePars = func_data.templatePars.size();
  		  std::string fdeclbody;
  		  llvm::raw_string_ostream bstream(fdeclbody);
		  D->print(bstream);
  		  bstream.flush();
  		  if (fdeclbody.find("extern") == 0) {
  			std::string::size_type n = 0;
  			n = fdeclbody.find_first_of( " \t", n );
  			fdeclbody.erase( 0,  fdeclbody.find_first_not_of( " \t", n ) );
  		  }
  		  std::stringstream argss;
  		  argss << "[";
  		  QualType rT = D->getReturnType();
  		  DbJSONClassVisitor::TypeData &r_data = Visitor.getTypeData(rT);
  		  argss << " " << r_data.id;
  		  for(unsigned long j=0; j<D->getNumParams(); j++) {
  			  const ParmVarDecl* p = D->getParamDecl(j);
  			  QualType T = p->getTypeSourceInfo() ? p->getTypeSourceInfo()->getType() : p->getType();
  			  DbJSONClassVisitor::TypeData &par_data = Visitor.getTypeData(T);
  			  argss << ", " << par_data.id;
   		  }
  		  argss << " ]";
  		  FDOut << Indent << "\t{\n";
  		  if (Visitor.CTAList.find(D)!=Visitor.CTAList.end()) {
  			FDOut << Indent << "\t\t\"name\": \"" << "__compiletime_assert" << "\",\n";
  		  }
  		  else {
  			FDOut << Indent << "\t\t\"name\": \"" << D->getName().str() << "\",\n";
  		  }
  		  std::string nms = getFullFunctionNamespace(D);
  		  if (isCXXTU(Context)) {
  			  FDOut << Indent << "\t\t\"namespace\": \"" << nms << "\",\n";
  		  }
  		  FDOut << Indent << "\t\t\"id\": " << func_data.id << ",\n";
  		  FDOut << Indent << "\t\t\"fid\": " << file_id << ",\n";
  		  FDOut << Indent << "\t\t\"nargs\": " << D->getNumParams() << ",\n";
  		  FDOut << Indent << "\t\t\"variadic\": " << (D->isVariadic()?("true"):("false")) << ",\n";
  		  if (D->isCXXClassMember()) {
			  const CXXMethodDecl* MD = static_cast<const CXXMethodDecl*>(D);
			  const CXXRecordDecl* RD = MD->getParent();
			  if (RD->isDependentType()) dependentClass = true;
		  }
  		  if (hasTemplatePars || dependentClass) FDOut << Indent << "\t\t\"template\": true,\n";
  		  FDOut << Indent << "\t\t\"linkage\": \"" << translateLinkage(D->getLinkageInternal()) << "\",\n";
  		  if (D->isCXXClassMember()) {
  			  FDOut << Indent << "\t\t\"member\": true,\n";
  			  const CXXMethodDecl* MD = static_cast<const CXXMethodDecl*>(D);
  			  const CXXRecordDecl* RD = MD->getParent();
  			  QualType RT = Context.getRecordType(RD);
  			  std::string _class = RT.getAsString();
  			  FDOut << Indent << "\t\t\"class\": \"" << _class << "\",\n";
			  DbJSONClassVisitor::TypeData &RT_data = Visitor.getTypeData(RT);
			  FDOut << Indent << "\t\t\"classid\": " << RT_data.id << ",\n";
  		  }
  		  if (opts.addbody) {
  			  if (hasTemplatePars) FDOut << Indent << "\t\t\"template_paremeters\": \"" << json::json_escape(func_data.templatePars) << "\",\n";
  			  FDOut << Indent << "\t\t\"decl\": \"" << json::json_escape(fdeclbody) << "\",\n";
  		  }
		  FDOut << Indent << "\t\t\"signature\": \"" << json::json_escape(func_data.signature) << "\",\n";
  		  FDOut << Indent << "\t\t\"declhash\": \"" << func_data.declhash << "\",\n";
  		  FDOut << Indent << "\t\t\"location\": \"" << getAbsoluteLocation(D->getLocation()) << "\",\n";
  		  FDOut << Indent << "\t\t\"refcount\": " << 1 << ",\n";
  		  FDOut << Indent << "\t\t\"types\": " << argss.str() << "\n";
  		  FDOut << Indent << "\t}";
  }

  size_t DbJSONClassConsumer::getDeclId(Decl* D, std::pair<int,unsigned long long>& extraArg) {
	  switch (D->getKind()) {
		  case Decl::Enum:
		  {
			  EnumDecl* innerD = cast<EnumDecl>(D);
			  QualType T = Context.getEnumType(innerD);
			  if (innerD->isCompleteDefinition()) {
				  extraArg.first = EXTRA_ENUMDECL;
				  return Visitor.getTypeData(T).id;
			  }
			  break;
		  }
		  case Decl::CXXRecord:
		  case Decl::Record:
		  {
			  RecordDecl* innerD = cast<RecordDecl>(D);
			  QualType T = Context.getRecordType(innerD);
  			  if (innerD->isCompleteDefinition()) {
				  extraArg.first = EXTRA_RECORDDECL;
				  return Visitor.getTypeData(T).id;
			  }
			  break;
		  }
		  case Decl::Field:
		  {
			  FieldDecl* innerD = cast<FieldDecl>(D);
			  QualType T = innerD->getType();
			  if (innerD->isBitField()) {
				  extraArg.first = EXTRA_BITFIELD;
				  extraArg.second = innerD->getBitWidthValue(Context);
			  }
			  return Visitor.getTypeData(T).id;
		  }
		  break;
		  case Decl::CXXMethod:
		  {
			  CXXMethodDecl* innerD = cast<CXXMethodDecl>(D);
			  extraArg.first = EXTRA_METHODDECL;
			  return Visitor.getFunctionDeclId(innerD);
		  }
		  break;
		  case Decl::FunctionTemplate:
		  {
			  FunctionTemplateDecl* innerD = cast<FunctionTemplateDecl>(D);
			  FunctionDecl* FD = innerD->getTemplatedDecl();
			  CXXMethodDecl* MD = static_cast<CXXMethodDecl*>(FD);
			  return getDeclId(MD,extraArg);
		  }
		  break;
		  case Decl::TypeAlias:
		  {
			  TypeAliasDecl* TAD = cast<TypeAliasDecl>(D);
			  QualType T = Context.getTypeDeclType(TAD);
			  extraArg.first = EXTRA_TYPEALIASDECL;
			  return Visitor.getTypeData(T).id;
		  }
	      break;
		  case Decl::TypeAliasTemplate:
		  {
			  TypeAliasTemplateDecl* TATD = cast<TypeAliasTemplateDecl>(D);
			  QualType T = Context.getTypeDeclType(TATD->getTemplatedDecl());
			  extraArg.first = EXTRA_TYPEALIASTEMPLATEDECL;
			  return Visitor.getTypeData(T).id;
		  }
		  break;
		  case Decl::Typedef:
		  {
			  TypedefDecl* innerD = cast<TypedefDecl>(D);
			  QualType T = Context.getTypeDeclType(innerD);
			  extraArg.first = EXTRA_TYPEDEFDECL;
			  return Visitor.getTypeData(T).id;
		  }
		  break;
		  case Decl::CXXConstructor:
			  break;
		  case Decl::ClassTemplate:
		  {
			  ClassTemplateDecl* innerD = cast<ClassTemplateDecl>(D);
			  Visitor.VisitClassTemplateDecl(innerD);
			  assert(Visitor.InjectedClassNameMap.find(innerD)!=Visitor.InjectedClassNameMap.end() && "InjectedClassNameMap missing entry");
			  const InjectedClassNameType* IT = Visitor.InjectedClassNameMap[innerD];
			  QualType T = QualType(IT,0);
			  extraArg.first = EXTRA_TEMPLATEDECL;
			  return Visitor.getTypeData(T).id;
		  }
		  case Decl::ClassTemplatePartialSpecialization:
		  {
			  ClassTemplatePartialSpecializationDecl* innerD = cast<ClassTemplatePartialSpecializationDecl>(D);
			  Visitor.VisitClassTemplatePartialSpecializationDecl(innerD);
			  assert(Visitor.InjectedClassNamePSMap.find(innerD)!=Visitor.InjectedClassNamePSMap.end() && "InjectedClassNamePSMap missing entry");
			  const InjectedClassNameType* IT = Visitor.InjectedClassNamePSMap[innerD];
			  QualType T = QualType(IT,0);
			  extraArg.first = EXTRA_TEMPLATEDECL;
			  return Visitor.getTypeData(T).id;
			  break;
		  }
		  case Decl::StaticAssert:{
        break;
      }
      default:
			  std::stringstream DN;
			  DN << D->getDeclKindName() << "DeclId";
        llvm::errs()<<DN.str()<<'\n';
        D->dump();
			  Visitor.unsupportedDeclClass.insert(std::pair<Decl::Kind,std::string>(D->getKind(),DN.str()));
			  assert(0 && "Unknown object");
			  break;
	  }
	  return -1;
  }

  DbJSONClassConsumer::TypeGroup_t DbJSONClassConsumer::getTypeGroupIds(Decl** Begin, unsigned NumDecls,
                        const PrintingPolicy &Policy) {
	  TypeGroup_t out;

	  if (NumDecls == 1) {
		  std::pair<int,unsigned long long> extraArg(0,0);
		  int id = getDeclId(*Begin,extraArg);
    	if (id>=0) {
    		out.push_back(std::pair<int,std::pair<int,unsigned long long>>(id,extraArg));
    	}
      return out;
    }

    Decl** End = Begin + NumDecls;
    TagDecl* TD = dyn_cast<TagDecl>(*Begin);
    if (TD) {
    	// Keep declarations of anonymous records/enums
    }

    PrintingPolicy SubPolicy(Policy);

    for ( ; Begin != End; ++Begin) {
    	std::pair<int,unsigned long long> extraArg(0,0);
    	int id = getDeclId(*Begin,extraArg);
    	if (id>=0) {
    		out.push_back(std::pair<int,std::pair<int,unsigned long long>>(id,extraArg));
    	}
    }
    return out;
  }

  void DbJSONClassConsumer::get_class_references(RecordDecl* rD, TypeGroup_t& Ids, MethodGroup_t& MIds, std::vector<int>& rIds, std::vector<std::string>& rDef) {
	  const DeclContext *DC = cast<DeclContext>(rD);
	  SmallVector<Decl*, 2> Decls;
	  std::string def;
	  llvm::raw_string_ostream defstream(def);
	  setCustomStructDefs(opts.csd);

	  for (DeclContext::decl_iterator D = DC->decls_begin(), DEnd = DC->decls_end();
			 D != DEnd; ++D) {
		  if (D->isImplicit())
			  continue;
		  QualType CurDeclType = getDeclType(*D);
		  if (!Decls.empty() && !CurDeclType.isNull()) {
			QualType BaseType = GetBaseType(CurDeclType);
			if (!BaseType.isNull() && isa<ElaboratedType>(BaseType))
			  BaseType = cast<ElaboratedType>(BaseType)->getNamedType();
			if (!BaseType.isNull() && isa<TagType>(BaseType) &&
				cast<TagType>(BaseType)->getDecl() == Decls[0]) {
			  Decls.push_back(*D);
			  continue;
			}
		  }
		  if (!Decls.empty()) {
			  processDeclGroupNoClear(Decls,defstream,Context.getPrintingPolicy());
			  bool first = true;
			  TypeGroup_t nIds = getTypeGroupIds(Decls.data(), Decls.size(), Context.getPrintingPolicy());
			  for (auto i = nIds.begin(); i!=nIds.end(); ++i) {
				  if ((*i).second.first==EXTRA_METHODDECL) {
					  MIds.push_back(*i);
				  }
				  else {
					  Ids.push_back(*i);
					  Decl *cD = Decls[i-nIds.begin()];
					  if(first){
						  if(Decls.size() == 1){
							  rIds.push_back((*i).first);
						  }
						  else{
							  int referenced = 0;
							  for(size_t i = 0; i < Decls.size(); i++){
								  if(Decls[i]->isReferenced()){
									 referenced = 1;
									 break; 
								  }
							  }
							  rIds.push_back(referenced?(*i).first:-1);
						  }

					  }
					  else{
						  rIds.push_back(cD->isReferenced()?(*i).first:-1);
					  }
					  if(first){
						  first = false;
						  defstream.flush();
						  rDef.push_back(def);
						  def.clear();
					  }
					  else{
						  rDef.push_back("");
					  }
				  }
			  }
			  Decls.clear();
		  }
		  if (isa<TagDecl>(*D) && !cast<TagDecl>(*D)->getIdentifier()) {
			Decls.push_back(*D);
			continue;
		  }
		  std::pair<int,unsigned long long> extraArg(0,0);
		  int nId = getDeclId(*D,extraArg);
		  if (nId>=0) {
			  if (extraArg.first==EXTRA_METHODDECL) {
				  MIds.push_back(std::pair<int,std::pair<int,unsigned long long>>(nId,extraArg));
			  }
			  else {
				  Ids.push_back(std::pair<int,std::pair<int,unsigned long long>>(nId,extraArg));
				  if((*D)->isReferenced() || (*D)->getKind() == Decl::Kind::Record){
					  rIds.push_back(nId);
				  }
				  else{	
					  rIds.push_back(-1);
				  }
				  D->print(defstream);
				  defstream.flush();
				  rDef.push_back(def+";\n");
				  def.clear();
			  }
		  }
	  }
	  if (!Decls.empty()) {
		  processDeclGroupNoClear(Decls,defstream,Context.getPrintingPolicy());
		  bool first = true;
		  TypeGroup_t nIds = getTypeGroupIds(Decls.data(), Decls.size(), Context.getPrintingPolicy());
		  for (auto i = nIds.begin(); i!=nIds.end(); ++i) {
			  if ((*i).second.first==EXTRA_METHODDECL) {
				  MIds.push_back(*i);
			  }
			  else {
				  Ids.push_back(*i);
				  Decl *cD = Decls[i-nIds.begin()];
				  if(first){
					  if(Decls.size() == 1){
						  rIds.push_back((*i).first);
					  }
					  else{
						  int referenced = 0;
						  for(size_t i = 0; i < Decls.size(); i++){
							  if(Decls[i]->isReferenced()){
								  referenced = 1;
								  break; 
							  }
						  }
						  rIds.push_back(referenced?(*i).first:-1);
					  }
				  }
				  else{
					  rIds.push_back(cD->isReferenced()?(*i).first:-1);
				  }
				  if(first){
					  first = false;
					  defstream.flush();
					  rDef.push_back(def);
					  def.clear();
				  }
				  else{
					  rDef.push_back("");
				  }
			  }
		  }
		  Decls.clear();
	  }
	  setCustomStructDefs(0);
  }

  void DbJSONClassConsumer::getTemplateArguments(const TemplateArgumentList& Args,
		  const std::string& Indent, template_args_t& template_args) {

	  getTemplateArguments(Args.asArray(),Indent,template_args);
  }

  void DbJSONClassConsumer::getTemplateArguments(ArrayRef<TemplateArgument> Args,
		  const std::string& Indent, template_args_t& template_args) {

	  type_args_t& type_args = template_args.first;
	  nontype_args_t& nontype_args = template_args.second;
	  for (size_t I = 0, E = Args.size(); I < E; ++I) {
	      const TemplateArgument &A = Args[I];
	      if (A.getKind() == TemplateArgument::Type) {
	    	  QualType T = A.getAsType();
	    	  const TemplateTypeParmType* TTP = T->getAs<TemplateTypeParmType>();
	    	  if (TTP) {
				  assert(!TTP->isCanonicalUnqualified() && "Only canonical template parm type");
	    	  }
	    	  type_args.push_back(std::pair<QualType,unsigned>(T,I));
	      }
	      else if (A.getKind() == TemplateArgument::Template) {
	    	  // Not yet
	      }
	      else if (A.getKind() == TemplateArgument::Expression) {
	    	  if (auto E = A.getAsExpr()) {
	    		  std::string expr;
				  llvm::raw_string_ostream exprstream(expr);
				  E->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
				  exprstream.flush();
	    		  nontype_args.push_back(std::pair<std::string,unsigned>(expr,I));
	    	  }
	      }
	      else if (A.getKind() == TemplateArgument::NullPtr) {
	    	  nontype_args.push_back(std::pair<std::string,unsigned>("\"nullptr\"",I));
	      }
	      else if (A.getKind() == TemplateArgument::Integral) {
	    	  llvm::APSInt Iv = A.getAsIntegral();
	    	  nontype_args.push_back(std::pair<std::string,unsigned>("\""+compatibility::toString(Iv)+"\"",I));
	      }
	      else {
	    	  if (opts.exit_on_error) {
					llvm::errs() << "\nERROR: Unsupported TemplateArgument Kind: " << A.getKind() << "\n";
					A.dump(llvm::errs());
					exit(EXIT_FAILURE);
				}
	      }
	  }

  }

  void DbJSONClassConsumer::printTemplateArgs(llvm::raw_string_ostream &TOut, template_args_t& template_args,
  		  std::vector<int> type_args_idx, const std::string& Indent, bool nextJSONitem) {

	  assert(template_args.first.size()==type_args_idx.size() && "Templare arguments array not aligned with correspnding index array");

	  TOut << Indent << "\t\t\"type_args\": [\n";
	  for (size_t i=0; i<template_args.first.size();++i) {
		  int idx = type_args_idx[i];
		  TOut << Indent << "\t\t\t[ " << template_args.first[i].second << "," << idx << " ]";
		  if (i<template_args.first.size()-1) {
			   TOut << ",\n";
		  }
		  else {
			  TOut << "\n";
		  }
	  }
	  TOut << Indent << "\t\t],\n";
	  TOut << Indent << "\t\t\"nontype_args\": [\n";
	  for (auto i=template_args.second.begin(); i!=template_args.second.end();) {
		  TOut << Indent << "\t\t\t[ " << (*i).second << "," << (*i).first << " ]";
		  ++i;
		  if (i!=template_args.second.end()) {
			   TOut << ",\n";
		  }
		  else {
			  TOut << "\n";
		  }
	  }
	  TOut << Indent << "\t\t]";
	  if (nextJSONitem) {
		  TOut << ",";
	  }
	  TOut << "\n";
  }


  DbJSONClassConsumer::template_default_map_t DbJSONClassConsumer::getTemplateParameters(TemplateParameterList* TPL,
  		  const std::string& Indent, bool getDefaults) {

	  defaut_type_map_t default_type_map;
	  default_nontype_map_t default_nontype_map;
	  std::vector<int> parmIds;
	  for (unsigned i = 0, e = TPL->size(); i != e; ++i) {
		Decl *Param = TPL->getParam(i);
		if (auto TTP = dyn_cast<TemplateTypeParmDecl>(Param)) {
		  if ((getDefaults)&&(TTP->hasDefaultArgument())) {
			  default_type_map.insert(std::pair<int,QualType>(i,TTP->getDefaultArgument()));
		  };
		  QualType QT = Context.getTemplateTypeParmType(TTP->getDepth(),TTP->getIndex(),TTP->isParameterPack(),TTP);
		  parmIds.push_back(Visitor.getTypeData(QT).id);
		} else if (auto NTTP = dyn_cast<NonTypeTemplateParmDecl>(Param)) {
		  StringRef Name;
		  if (IdentifierInfo *II = NTTP->getIdentifier())
			Name = II->getName();
		  parmIds.push_back(Visitor.getTypeData(NTTP->getType()).id);
		  if ((getDefaults)&&(NTTP->hasDefaultArgument())) {
			  default_nontype_map.insert(std::pair<int,Expr*>(i,NTTP->getDefaultArgument()));
		  }
		} else if (auto TTPD = dyn_cast<TemplateTemplateParmDecl>(Param)) {
			// Probably will never be done
		}
	  }
	  return template_default_map_t(default_type_map,default_nontype_map,parmIds);
  }

  void DbJSONClassConsumer::printTemplateTypeDefaults(llvm::raw_string_ostream &TOut, defaut_type_map_t default_type_map,
		  std::map<int,int> type_parms_idx, const std::string& Indent, bool nextJSONitem) {

	  assert(default_type_map.size()==type_parms_idx.size() && "Templare parameters array not aligned with correspnding index array");

	  TOut << Indent << "\t\t\"type_defaults\": {\n";
	  for (auto i=default_type_map.begin(); i!=default_type_map.end();) {
		  TOut << Indent << "\t\t\t\"" << (*i).first << "\": " << type_parms_idx[(*i).first];
		  ++i;
		  if (i!=default_type_map.end()) {
			   TOut << ",\n";
		  }
		  else {
			  TOut << "\n";
		  }
	  }
	  TOut << Indent << "\t\t}";
	  if (nextJSONitem) {
		  TOut << ",";
	  }
	  TOut << "\n";
  }

  void DbJSONClassConsumer::printTypeEntry(DbJSONClassVisitor::TypeData &type_data,int Indentation) {
    std::string Indent(Indentation,'\t');
	  llvm::raw_string_ostream TOut(*type_data.output);
	  QualType T = type_data.T;
	  DbJSONClassVisitor::recordInfo_t *RInfo = type_data.RInfo;
	  TOut << Indent << "\t{\n";
	  TOut << Indent << "\t\t\"id\": " << type_data.id << ",\n";
	  TOut << Indent << "\t\t\"fid\": " << file_id << ",\n";
	  TOut << Indent << "\t\t\"hash\": \"" << type_data.hash << "\",\n";
	  TOut << Indent << "\t\t\"refcount\": " << 1 << ",\n";
	  std::string qualifierString = getQualifierString(T);
	  TOut << Indent << "\t\t\"qualifiers\": \"" << qualifierString << "\",\n";
	  TOut << Indent << "\t\t\"size\": " << type_data.size << ",\n";

	  switch(T->getTypeClass()) {
		  case Type::MacroQualified:
		  case Type::Attributed:
		  case Type::UnaryTransform:
		  case Type::Atomic:
		  case Type::Elaborated:
		  case Type::Decltype:
		  case Type::SubstTemplateTypeParm:
		  case Type::Paren:
		  case Type::TypeOfExpr:
		  case Type::TypeOf:
		  case Type::Decayed:
		  {
			  llvm::errs()<<"Skipped type "<<T->getTypeClassName()<<" printed\n";
			  T.dump();
			  assert(0 && "Unreachable");
		  }
		  case Type::Builtin:
		  {
			  const BuiltinType *tp = cast<BuiltinType>(T);
			  TOut << Indent << "\t\t\"class\": \"" << "builtin" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << tp->getName(Context.getPrintingPolicy()) << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [],\n";
			  TOut << Indent << "\t\t\"usedrefs\": []\n";
		  }
		  break;
		  case Type::Pointer:
		  {
			  const PointerType *tp = cast<PointerType>(T);
			  QualType ptrT = tp->getPointeeType();
			  TOut << Indent << "\t\t\"class\": \"" << "pointer" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "*" << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(ptrT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(ptrT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::MemberPointer:
		  {
			  const MemberPointerType *tp = cast<MemberPointerType>(T);
			  QualType mptrT = tp->getPointeeType();
			  QualType cT = QualType(tp->getClass(),0);
			  TOut << Indent << "\t\t\"class\": \"" << "member_pointer" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "::*" << "\",\n";
			  TOut << Indent << "\t\t\"dependent\": " << tp->isDependentType() << ",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(cT).id;
			  TOut << ", " << Visitor.getTypeData(mptrT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(cT).id;
			  TOut << ", " << Visitor.getTypeData(mptrT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::Complex:
		  {
			  const ComplexType *tp = cast<ComplexType>(T);
			  QualType eT = tp->getElementType();
			  TOut << Indent << "\t\t\"class\": \"" << "complex" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "(x,jy)" << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(eT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(eT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::Vector:
		  {
			  const VectorType *tp = cast<VectorType>(T);
			  QualType eT = tp->getElementType();
			  TOut << Indent << "\t\t\"class\": \"" << "vector" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "v[N]" << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(eT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(eT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::ExtVector:
		  {
			  const ExtVectorType *tp = cast<ExtVectorType>(T);
			  QualType eT = tp->getElementType();
			  TOut << Indent << "\t\t\"class\": \"" << "extended_vector" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "ev[N]" << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(eT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(eT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::LValueReference:
		  {
			  const LValueReferenceType *tp = cast<LValueReferenceType>(T);
			  QualType ptrT = tp->getPointeeType();
			  TOut << Indent << "\t\t\"class\": \"" << "lv_reference" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "&" << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(ptrT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(ptrT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::RValueReference:
		  {
			  const RValueReferenceType *tp = cast<RValueReferenceType>(T);
			  QualType ptrT = tp->getPointeeType();
			  TOut << Indent << "\t\t\"class\": \"" << "rv_reference" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "&&" << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(ptrT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(ptrT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::DependentSizedArray:
		  {
			  const DependentSizedArrayType *tp = cast<DependentSizedArrayType>(T);
			  QualType elT = tp->getElementType();
			  TOut << Indent << "\t\t\"class\": \"" << "dependent_sized_array" << "\",\n";
			  std::string sizeExpr;
			  if (tp->getSizeExpr()) {
			      llvm::raw_string_ostream estream(sizeExpr);
				  tp->getSizeExpr()->printPretty(estream,nullptr,Context.getPrintingPolicy());
			      estream.flush();
			  }
			  TOut << Indent << "\t\t\"str\": \"" << json::json_escape(sizeExpr) << "\",\n";
			  TOut << Indent << "\t\t\"dependent\": " << tp->isDependentType() << ",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(elT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(elT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::PackExpansion:
		  {
			  const PackExpansionType *tp = cast<PackExpansionType>(T);
			  TOut << Indent << "\t\t\"class\": \"" << "pack_expansion" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "<...>" << "\",\n";
			  TOut << Indent << "\t\t\"dependent\": " << tp->isDependentType() << ",\n";
			  TOut << Indent << "\t\t\"refs\": [ ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [ ]\n";
		  }
		  break;
		  case Type::UnresolvedUsing:
		  {
			  const UnresolvedUsingType *tp = cast<UnresolvedUsingType>(T);
			  TOut << Indent << "\t\t\"class\": \"" << "unresolved_using" << "\",\n";
			  std::string usingDecl;
			  llvm::raw_string_ostream cstream(usingDecl);
			  tp->getDecl()->print(cstream,Context.getPrintingPolicy());
			  cstream.flush();
			  TOut << Indent << "\t\t\"str\": \"" << usingDecl << "\",\n";
			  TOut << Indent << "\t\t\"dependent\": " << tp->isDependentType() << ",\n";
			  TOut << Indent << "\t\t\"refs\": [ ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [ ]\n";
		  }
		  break;
		  case Type::Auto:
		  {
			  const AutoType *tp = cast<AutoType>(T);
			  assert(!tp->isSugared() && "sugar Auto type never in database");
			  QualType aT = tp->getDeducedType();
			  TOut << Indent << "\t\t\"class\": \"" << "auto" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "auto" << "\",\n";
			  TOut << Indent << "\t\t\"dependent\": " << tp->isDependentType() << ",\n";
			  TOut << Indent << "\t\t\"refs\": [ ";
			  if (!aT.isNull()) {
				  TOut << Visitor.getTypeData(aT).id;
			  }
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [ ";
			  if (!aT.isNull()) {
				  TOut << Visitor.getTypeData(aT).id;
			  }
			  TOut << " ]\n";
		  }
		  break;
		  case Type::TemplateTypeParm:
		  {
			  const TemplateTypeParmType *tp = cast<TemplateTypeParmType>(T);
			  TOut << Indent << "\t\t\"class\": \"" << "template_type_parm" << "\",\n";
			  if (tp->getIdentifier()) {
				  TOut << Indent << "\t\t\"str\": \"" << tp->getIdentifier()->getName().str() << "\",\n";
			  }
			  else {
				  TOut << Indent << "\t\t\"str\": \"" << "" << "\",\n";
			  }
			  TOut << Indent << "\t\t\"dependent\": " << tp->isDependentType() << ",\n";
			  TOut << Indent << "\t\t\"refs\": [ " << tp->getDepth() << ", " << tp->getIndex() << " ],";
			  TOut << Indent << "\t\t\"usedrefs\": [ " << tp->getDepth() << ", " << tp->getIndex() << " ]";
		  }
		  break;
		  case Type::DependentName:
		  {
			  const DependentNameType *tp = cast<DependentNameType>(T);
			  TOut << Indent << "\t\t\"class\": \"" << "dependent_name" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "T::*" << "\",\n";
			  TOut << Indent << "\t\t\"dependent\": " << tp->isDependentType() << ",\n";
			  TOut << Indent << "\t\t\"refs\": [ ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [ ]\n";
		  }
		  break;
		  case Type::TemplateSpecialization:
		  {
			  const TemplateSpecializationType *tp = cast<TemplateSpecializationType>(T);
			  assert(!tp->isSugared() && "sugar TemplateSpecialization type never in database");
			  TemplateName TN = tp->getTemplateName();
			  bool have_templateref = false;
			  int templateref = 0;
			  bool have_template_args = false;
			  template_args_t template_args;
			  if (tp->getTemplateName().getKind()==TemplateName::Template) {
				  TemplateDecl* TD = TN.getAsTemplateDecl();
				  if (isa<ClassTemplateDecl>(TD)) {
					  ClassTemplateDecl* CTD = static_cast<ClassTemplateDecl*>(TD);
					  assert(Visitor.InjectedClassNameMap.find(CTD)!=Visitor.InjectedClassNameMap.end() && "InjectedClassNameMap missing entry");
					  const InjectedClassNameType* IT = Visitor.InjectedClassNameMap[CTD];
					  QualType injT = QualType(IT,0);
					  templateref = Visitor.getTypeData(injT).id;
					  have_templateref = true;
					  TOut << Indent << "\t\t\"class\": \"" << "record_specialization" << "\",\n";
					  TOut << Indent << "\t\t\"str\": \"" << "T<X>" << "\",\n";
					  TOut << Indent << "\t\t\"dependent\": " << tp->isDependentType() << ",\n";
					  TOut << Indent << "\t\t\"cxxrecord\": " << "true" << ",\n";
					  TOut << Indent << "\t\t\"def\": \"";
					  T.print(TOut,Context.getPrintingPolicy());
					  TOut << "\",\n";
					  
					  // TODO: removed likely unnecessary code, some issues may arise
					  have_template_args = true;


					  if (have_template_args) {
						  getTemplateArguments(tp->template_arguments(),Indent,template_args);
					  }
				  }
				  else if (isa<TemplateTemplateParmDecl>(TD)) {
					  /* TODO */
				  }
				  else {
					  /* This must be TypeAliasTemplateDecl */
					  /* TODO */
				  }
			  }
			  else if (tp->getTemplateName().getKind()==TemplateName::SubstTemplateTemplateParmPack) {
				  /* TODO */
			  }
			  else {
				  /* This must be TemplateName::SubstTemplateTemplateParm */
				  /* TODO */
			  }
			  std::vector<int> refs;
			  TOut << Indent << "\t\t\"refs\": [";
			  TypeGroup_t Ids;
			  if (have_templateref) {
				  std::pair<int,std::pair<int,unsigned long long>> u;
				  u.first = templateref;
				  Ids.push_back(u);
				  templateref = Ids.size()-1;
			  }
			  std::vector<int> type_args_idx;
			  if (have_template_args) {
				  for (auto i=template_args.first.begin(); i!=template_args.first.end();++i) {
					  std::pair<int,std::pair<int,unsigned long long>> u;
					  u.first = Visitor.getTypeData((*i).first).id;
					  Ids.push_back(u);
					  type_args_idx.push_back(Ids.size()-1);
				  }
			  }
			  for (auto i=Ids.begin(); i!=Ids.end(); i++) {
				  if (i+1!=Ids.end()) {
					  TOut << " " << (*i).first << ",";
				  }
				  else {
					  TOut << " " << (*i).first;
				  }
				  refs.push_back((*i).first);
			  }
			  TOut << " ],";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  for (auto i=refs.begin(); i!=refs.end(); i++) {
				  if (i+1!=refs.end()) {
					  TOut << " " << (*i) << ",";
				  }
				  else {
					  TOut << " " << (*i);
				  }
			  }
			  TOut << " ]";
			  if (have_templateref || have_template_args) {
				  TOut << ",";
			  }
			  TOut << "\n";
			  /* End of refs */
			  if (have_templateref) {
				  TOut << Indent << "\t\t\"templateref\": " << templateref;
				  if (have_template_args) {
					  TOut << ",";
				  }
				  TOut << "\n";
			  }
			  if (have_template_args) {
				  printTemplateArgs(TOut,template_args,type_args_idx,Indent,false);
			  }
		  }
		  break;
		  case Type::DependentTemplateSpecialization:
		  {
			  const DependentTemplateSpecializationType *tp = cast<DependentTemplateSpecializationType>(T);
			  TOut << Indent << "\t\t\"class\": \"" << "record_specialization" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "DT<X>" << "\",\n";
			  TOut << Indent << "\t\t\"dependent\": " << tp->isDependentType() << ",\n";
			  TOut << Indent << "\t\t\"cxxrecord\": " << "true" << ",\n";
		  }
		  break;
		  case Type::InjectedClassName:
		  {
			  const InjectedClassNameType* IT = cast<InjectedClassNameType>(T);
			  CXXRecordDecl* TRD = IT->getDecl();
			  std::string def;
			  bool specialization = false;
			  if (opts.adddefs) {
				  llvm::raw_string_ostream defstream(def);
				  if (Visitor.classTemplateMap.find(TRD)!=Visitor.classTemplateMap.end()) {
					  const ClassTemplateDecl* CTD = Visitor.classTemplateMap[TRD];
					  CTD->getTemplateParameters()->print(defstream,Context);
					  defstream.flush();
				  }
				  else if (Visitor.classTemplatePartialSpecializationMap.find(TRD)
						  !=Visitor.classTemplatePartialSpecializationMap.end()) {
					  const ClassTemplatePartialSpecializationDecl* CTPS = Visitor.classTemplatePartialSpecializationMap[TRD];
					  specialization = true;
				  }
				  else {
					  assert(0 && "Cannot find template for InjectedClassName");
				  }
				  TRD->print(defstream);
				  defstream.flush();
			  }
			  TOut << Indent << "\t\t\"class\": \"" << "record_template" << "\",\n";
			  if (opts.recordLoc) {
				  TOut << Indent << "\t\t\"location\": \"" <<
						  getAbsoluteLocation(TRD->getSourceRange().getBegin()) << "\",\n";
			  }
			  TOut << Indent << "\t\t\"cxxrecord\": " << "true" << ",\n";
			  if (TRD->isUnion()) {
				  TOut << Indent << "\t\t\"union\": true,\n";
			  }
			  else {
				  TOut << Indent << "\t\t\"union\": false,\n";
			  }
			  TOut << Indent << "\t\t\"str\": \"" << T.getAsString() << "\",\n";
			  bool have_specializations = false;
			  std::vector<int> specializations;
			  bool have_partial_specializations = false;
			  std::vector<int> partial_specializations;
			  if (!specialization) {
				  /* Write down references to all specializations */
				  const ClassTemplateDecl* CTD = Visitor.classTemplateMap[TRD];
				  for (auto *I : CTD->specializations()) {
					  int spec_id = Visitor.getTypeData(Context.getRecordType(I)).id;
					  specializations.push_back(spec_id);
				  }
				  have_specializations = true;
				  llvm::SmallVector<ClassTemplatePartialSpecializationDecl*,10> PSv;
				  const_cast<ClassTemplateDecl*>(CTD)->getPartialSpecializations(PSv);
				  for (ClassTemplatePartialSpecializationDecl *CTPS : PSv) {
					  int spec_id = Visitor.getTypeData(Context.getRecordType(CTPS)).id;
					  partial_specializations.push_back(spec_id);
				  }
				  have_partial_specializations = true;
			  }
			  bool have_templateref = false;
			  int templateref = 0;
			  bool have_template_args = false;
			  template_args_t template_args;
			  enum {
				  PARMS_NONE,
				  PARMS_DEFAULTS,
				  PARMS_NODEFAULTS,
			  } have_template_parms = PARMS_NONE;
			  template_default_map_t template_parms;
			  if (!specialization) {
				  TOut << Indent << "\t\t\"specialization\": false,\n";
				  const ClassTemplateDecl* CTD = Visitor.classTemplateMap[TRD];
				  TemplateParameterList* TPL = CTD->getTemplateParameters();
				  template_parms = getTemplateParameters(TPL,Indent);
				  have_template_parms = PARMS_DEFAULTS;
			  }
			  else {
				  TOut << Indent << "\t\t\"specialization\": true,\n";
				  const ClassTemplatePartialSpecializationDecl* CTPS = Visitor.classTemplatePartialSpecializationMap[TRD];
				  ClassTemplateDecl* CTD = CTPS->getSpecializedTemplate();
				  assert(Visitor.InjectedClassNameMap.find(CTD)!=Visitor.InjectedClassNameMap.end() && "InjectedClassNameMap missing entry");
				  const InjectedClassNameType* IT = Visitor.InjectedClassNameMap[CTD];
				  QualType injT = QualType(IT,0);
				  templateref = Visitor.getTypeData(injT).id;
				  have_templateref = true;
				  TemplateParameterList* Params = CTPS->getTemplateParameters();
				  const TemplateArgumentList& Args = CTPS->getTemplateArgs();
				  template_parms = getTemplateParameters(Params,Indent,false);
				  have_template_parms = PARMS_NODEFAULTS;
				  // TODO: removed likely unnecessary code, some issues may arise
				  getTemplateArguments(Args,Indent,template_args);
				  have_template_args = true;
			  }
			  if (opts.adddefs) {
				  TOut << Indent << "\t\t\"def\": \"" << json::json_escape(def) << "\",\n";
			  }
			  std::vector<int> refs;
			  TOut << Indent << "\t\t\"refs\": [";
			  TypeGroup_t Ids;
			  MethodGroup_t MIds;
			  std::vector<int> rIds;
			  std::vector<std::string> rDef;
			  get_class_references(TRD,Ids,MIds,rIds,rDef);
			  // Merge attrrefs with refs for convenient database query?
			  // (...)
			  // Merge specializations, partial specializations, templateref, type_args, parms and type_defaults
			  if (have_specializations) {
				  for (auto i=specializations.begin(); i!=specializations.end(); ++i) {
					  std::pair<int,std::pair<int,unsigned long long>> u;
					  u.first = *i;
					  Ids.push_back(u);
					  *i = Ids.size()-1;
				  }
			  }
			  if (have_partial_specializations) {
				  for (auto i=partial_specializations.begin(); i!=partial_specializations.end(); ++i) {
					  std::pair<int,std::pair<int,unsigned long long>> u;
					  u.first = *i;
					  Ids.push_back(u);
					  *i = Ids.size()-1;
				  }
			  }
			  if (have_templateref) {
			    std::pair<int,std::pair<int,unsigned long long>> u;
			    u.first = templateref;
			    Ids.push_back(u);
			    templateref = Ids.size()-1;
			  }
			  std::vector<int> type_args_idx;
			  std::map<int,int> type_parms_idx;
			  if (have_template_args) {
			    for (auto i=template_args.first.begin(); i!=template_args.first.end();++i) {
			  	  std::pair<int,std::pair<int,unsigned long long>> u;
			  	  u.first = Visitor.getTypeData((*i).first).id;
			  	  Ids.push_back(u);
			  	  type_args_idx.push_back(Ids.size()-1);
			    }
			  }
			  if (have_template_parms!=PARMS_NONE) {
				  defaut_type_map_t& default_type_map = std::get<0>(template_parms);
				  std::vector<int>& parmIds = std::get<2>(template_parms);
				  for (auto i=parmIds.begin(); i!=parmIds.end(); i++) {
					  std::pair<int,std::pair<int,unsigned long long>> u;
					  u.first = *i;
					  Ids.push_back(u);
					  *i = Ids.size()-1;
				  }
				  if (have_template_parms==PARMS_DEFAULTS) {
					  for (auto i=default_type_map.begin(); i!=default_type_map.end();++i) {
						  std::pair<int,std::pair<int,unsigned long long>> u;
						  u.first = Visitor.getTypeData((*i).second).id;
						  Ids.push_back(u);
						  type_parms_idx[(*i).first] = Ids.size()-1;
					  }
				  }
			  }
			  for (auto i=Ids.begin(); i!=Ids.end(); i++) {
				  if (i+1!=Ids.end()) {
					  TOut << " " << (*i).first << ",";
				  }
				  else {
					  TOut << " " << (*i).first;
				  }
				  refs.push_back((*i).first);
			  }
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  for (auto i=refs.begin(); i!=refs.end(); i++) {
				  if (i+1!=refs.end()) {
					  TOut << " " << (*i) << ",";
				  }
				  else {
					  TOut << " " << (*i);
				  }
			  }
			  TOut << " ],";
			  /* End of refs */
			  TOut << Indent << "\t\t\"methods\": [";
			  for (auto i=MIds.begin(); i!=MIds.end(); i++) {
				  if (i+1!=MIds.end()) {
					  TOut << " " << (*i).first << ",";
				  }
				  else {
					  TOut << " " << (*i).first;
				  }
			  }
			  TOut << " ],\n";
			  if (have_specializations) {
				  TOut << Indent << "\t\t\"specializations\": [";
				  for (auto i=specializations.begin(); i!=specializations.end(); ++i) {
					  if (i==specializations.begin()) {
						  TOut << " " << *i;
					  }
					  else {
						  TOut << ", " << *i;
					  }
				  }
				  TOut << " ],\n";
			  }
			  if (have_partial_specializations) {
				  TOut << Indent << "\t\t\"partial_specializations\": [";
				  for (auto i=partial_specializations.begin(); i!=partial_specializations.end(); ++i) {
					  if (i==partial_specializations.begin()) {
						  TOut << " " << *i;
					  }
					  else {
						  TOut << ", " << *i;
					  }
				  }
				  TOut << " ],\n";
			  }
			  if (have_templateref) {
				  TOut << Indent << "\t\t\"templateref\": " << templateref << ",\n";
			  }
			  if (have_template_args) {
				  printTemplateArgs(TOut,template_args,type_args_idx,Indent);
			  }
			  defaut_type_map_t& default_type_map = std::get<0>(template_parms);
			  std::vector<int>& parmIds = std::get<2>(template_parms);
			  if (have_template_parms!=PARMS_NONE) {
				  TOut << Indent << "\t\t\"parms\": [";
				  for (auto i=parmIds.begin(); i!=parmIds.end(); i++) {
				    if (i+1!=parmIds.end()) {
				  	  TOut << " " << (*i) << ",";
				    }
				    else {
				  	  TOut << " " << (*i);
				    }
				  }
				  TOut << " ],\n";
				  if (have_template_parms==PARMS_DEFAULTS) {
					  printTemplateTypeDefaults(TOut, default_type_map, type_parms_idx, Indent);
				  }
			  }
			  TOut << Indent << "\t\t\"refnames\": [ ";
			  if (RInfo) {
				  for (size_t i=0; i<RInfo->first.size();) {
					  TOut << "\"" << (*RInfo).first[i] << "\"";
					  ++i;
					  if (i<RInfo->first.size())
						  TOut << ",";
				  }
			  }
			  TOut << " ],\n";
			  std::vector<std::pair<unsigned long,unsigned long>> bitfields;
			  std::vector<std::pair<unsigned long,unsigned long>> decls;
			  for (auto i=Ids.begin(); i!=Ids.end(); i++) {
				  if ((*i).second.first&EXTRA_BITFIELD) {
					  bitfields.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
				  }
				  if ((*i).second.first==EXTRA_RECORDDECL) {
					  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
				  }
				  if ((*i).second.first==EXTRA_ENUMDECL) {
					  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
				  }
				  if ((*i).second.first==EXTRA_TEMPLATEDECL) {
					  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
				  }
				  if ((*i).second.first==EXTRA_TYPEDEFDECL) {
					  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
				  }
				  if ((*i).second.first==EXTRA_TYPEALIASDECL) {
					  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
				  }
				  if ((*i).second.first==EXTRA_TYPEALIASTEMPLATEDECL) {
					  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
				  }
			  }
			  TOut << Indent << "\t\t\"bitfields\": {\n";
			  for (auto i=bitfields.begin(); i!=bitfields.end(); i++) {
				  if (i+1!=bitfields.end()) {
					  TOut << Indent << "\t\t\t\"" << (*i).first << "\" : " << (*i).second << ",\n";
				  }
				  else {
					  TOut << Indent << "\t\t\t\"" << (*i).first << "\" : " << (*i).second << "\n";
				  }
			  }
			  TOut << Indent << "\t\t},\n";
			  TOut << Indent << "\t\t\"decls\": [\n";
			  for (auto i=decls.begin(); i!=decls.end(); i++) {
				  if (i+1!=decls.end()) {
					  TOut << Indent << "\t\t\t" << (*i).first << ",\n";
				  }
				  else {
					  TOut << Indent << "\t\t\t" << (*i).first << "\n";
				  }
			  }
			  TOut << Indent << "\t\t]\n";
		  }
		  break;
		  case Type::Record:
		  {
			  const RecordType *tp = cast<RecordType>(T);
			  RecordDecl* rD = tp->getDecl();
			  std::vector<QualType> QV;
			  if (rD->hasAttrs()) {
				  const AttrVec& V = rD->getAttrs();
				  for (auto ai=V.begin(); ai!=V.end();) {
					  const Attr* a = *ai;
					  Visitor.lookForAttrTypes(a,QV);
					  ++ai;
				  }
			  }
			  Visitor.notice_field_attributes(rD,QV);
			  std::string def;
			  std::string head;
			  if (opts.adddefs) {
				  llvm::raw_string_ostream defstream(def);
				  printRecordHead(rD,defstream,Context.getPrintingPolicy());
				  defstream.flush();
				  head = def;
				  def.clear();
				  rD->print(defstream);
				  defstream.flush();
			  }
			  const IdentifierInfo *II = rD->getIdentifier();
			  TOut << Indent << "\t\t\"dependent\": " << rD->isDependentType() << ",\n";
			  if (rD->isCompleteDefinition()) {
				  TOut << Indent << "\t\t\"class\": \"" << "record" << "\",\n";
				  std::string outerFn;
				  if (!rD->isDefinedOutsideFunctionOrMethod()) {
					  std::string outerFn = Visitor.parentFunctionOrMethodString(rD);
					  TOut << Indent << "\t\t\"outerfn\": \"" << outerFn << "\",\n";
					  TOut << Indent << "\t\t\"outerfnid\": " <<
							  Visitor.outerFunctionorMethodIdforTagDecl(rD) << ",\n";
				  }
			  }
			  else {
				  TOut << Indent << "\t\t\"class\": \"" << "record_forward" << "\",\n";
			  }
			  bool PoI = false;
			  SourceLocation PoILoc;
			  bool have_templateref = false;
			  int templateref = 0;
			  bool have_template_args = false;
			  template_args_t template_args;
			  if (isa<CXXRecordDecl>(rD)) {
				  CXXRecordDecl* cpprD =  cast<CXXRecordDecl>(rD);
				  TOut << Indent << "\t\t\"cxxrecord\": " << "true" << ",\n";
				  if (isa<ClassTemplateSpecializationDecl>(cpprD)) {
					  TOut << Indent << "\t\t\"specialization\": " <<
					  	  	  (((cpprD->getTemplateSpecializationKind()==TSK_Undeclared)||
					  	  	  (cpprD->getTemplateSpecializationKind()==TSK_ExplicitSpecialization))?
					  	  			"true":"false") << ",\n";
					  TOut << Indent << "\t\t\"instantiation\": " <<
							  (((cpprD->getTemplateSpecializationKind()==TSK_ImplicitInstantiation)||
							  (cpprD->getTemplateSpecializationKind()>=TSK_ExplicitInstantiationDeclaration))?
									  "true":"false") << ",\n";
					  CXXRecordDecl* tmpl = nullptr;
					  const ClassTemplateSpecializationDecl* CTSD = 0;
					  if (cpprD->getTemplateSpecializationKind()==TSK_Undeclared) {
						  CTSD = cast<ClassTemplateSpecializationDecl>(cpprD);
						  PoILoc = CTSD->getSourceRange().getBegin();
						  PoI = true;
					  }
					  else {
						  if (Visitor.classTemplateSpecializationMap.find(cpprD)!=
								  Visitor.classTemplateSpecializationMap.end()) {
							  CTSD = Visitor.classTemplateSpecializationMap[cpprD];
						  }
						  else {
							  if (opts.exit_on_error) {
								  llvm::errs() << "\nERROR: Cannot find ClassTemplateSpecializationDecl for CXXRecord:\n";
								  cpprD->dump(llvm::errs());
								  exit(EXIT_FAILURE);
							  }
						  }
					  }
					  if (CTSD) {
						  llvm::PointerUnion<ClassTemplateDecl *,
							  ClassTemplatePartialSpecializationDecl *> Result = CTSD->getSpecializedTemplateOrPartial();
							if (Result.is<ClassTemplateDecl *>())
								tmpl = Result.get<ClassTemplateDecl *>()->getTemplatedDecl();
							else
								tmpl = Result.get<ClassTemplatePartialSpecializationDecl *>();
						  if ((cpprD->getTemplateSpecializationKind()==TSK_Undeclared)||
								  (cpprD->getTemplateSpecializationKind()==TSK_ExplicitSpecialization)) {
							  if (tmpl) {
								  templateref = Visitor.getTypeData(Context.getRecordType(tmpl)).id;
							  }
							  else {
								  templateref = -1;
							  }
							  have_templateref = true;
							  getTemplateArguments(CTSD->getTemplateArgs(),Indent,template_args);
						  }
							  have_template_args = true;
						  if ((cpprD->getTemplateSpecializationKind()==TSK_ImplicitInstantiation)||
								  (cpprD->getTemplateSpecializationKind()>=TSK_ExplicitInstantiationDeclaration)) {
							  TOut << Indent << "\t\t\"extern\": " <<
								((cpprD->getTemplateSpecializationKind()==TSK_ExplicitInstantiationDeclaration)?
										"true":"false") << ",\n";
							  CXXRecordDecl* tmpl = cpprD->getTemplateInstantiationPattern();
							  templateref = Visitor.getTypeData(Context.getRecordType(tmpl)).id;
							  have_templateref = true;
							  getTemplateArguments(CTSD->getTemplateArgs(),Indent,template_args);
							  have_template_args = true;
							  PoILoc = CTSD->getPointOfInstantiation();
							  PoI = true;
						  }
					  }
				  }
			  }
			  if (opts.recordLoc) {
				  if (PoI) {
					  TOut << Indent << "\t\t\"location\": \"" <<
							  getAbsoluteLocation(PoILoc) << "\",\n";
				  }
				  else {
					  TOut << Indent << "\t\t\"location\": \"" <<
							  getAbsoluteLocation(rD->getSourceRange().getBegin()) << "\",\n";
				  }
			  }

			  if (tp->isUnionType()) {
				  TOut << Indent << "\t\t\"union\": true,\n";
			  }
			  else {
				  TOut << Indent << "\t\t\"union\": false,\n";
			  }
			  if (II) {
				  std::string tpII;
				  if ((isa<CXXRecordDecl>(rD)) || (isa<ClassTemplateSpecializationDecl>(rD))) {
					  tpII = T.getAsString();
				  }
				  else {
					  tpII = II->getName().str();
				  }
				  TOut << Indent << "\t\t\"str\": \"" << tpII << "\",\n";
			  }
			  else {
				  TOut << Indent << "\t\t\"str\": \"" << "" << "\",\n";
			  }
			  if (opts.adddefs) {
				  TOut << Indent << "\t\t\"def\": \"" << json::json_escape(def) << "\",\n";
			  }
			  std::vector<int> QVIds;
			  if (QV.size()>0) {
				  for (auto i = QV.begin(); i!=QV.end(); ++i) {
					  int id = Visitor.getTypeData(*i).id;
					  if (id>=0) {
						  QVIds.push_back(id);
					  }
				  }
				  if (QVIds.size()>0) {
					  TOut << Indent << "\t\t\"attrrefs\": [";
					  for (auto i = QVIds.begin(); i!=QVIds.end(); ++i) {
						  if (i+1!=QVIds.end()) {
							  TOut << " " << (*i) << ",";
						  }
						  else {
							  TOut << " " << (*i);
						  }
					  }
					  TOut << " ],\n";
					  TOut << Indent << "\t\t\"attrnum\": " << QVIds.size() << ",\n";
				  }
			  }
			  TypeGroup_t Ids;
			  MethodGroup_t MIds;
			  std::vector<int> rIds;
			  std::vector<std::string> rDef;
			  if (rD->isCompleteDefinition()) {
				  get_class_references(rD,Ids,MIds,rIds,rDef);
          multi::handleRefs(type_data.usedrefs,rIds,rDef);
				  if(DISABLED && opts.adddefs && opts.csd && !rDef.empty()){
					  TOut<< Indent << "\t\t\"defhead\": \"" << json::json_escape(head) << "\",\n";
					  TOut << Indent << "\t\t\"useddef\": [";
            // will be updated in postprocessing
					  // for(auto i = rDef.begin();i!=rDef.end();i++){
						//   if(i != rDef.begin()) TOut<<",";
						//   TOut<<"\""<<json::json_escape(*i)<<"\"";
					  // }
					  TOut<<"],\n";
				  }
			  }
			  if(Visitor.isTypedefRecord(rD)){
				  for(size_t i = 0; i < rIds.size();i++){
					  rIds[i] = Ids[i].first;
				  }
			  }
			  TOut << Indent << "\t\t\"refs\": [";
			  // Merge attrrefs with refs for convenient database query
			  for (auto i = QVIds.begin(); i!=QVIds.end(); ++i) {
				  std::pair<int,std::pair<int,unsigned long long>> u;
				  u.first = *i;
				  Ids.push_back(u);
			  }
			  // Merge templateref and type_args with refs to simplify type merging
			  std::vector<int> type_args_idx;
			  if (have_templateref) {
				  std::pair<int,std::pair<int,unsigned long long>> u;
				  u.first = templateref;
				  Ids.push_back(u);
				  templateref = Ids.size()-1;
			  }
			  if (have_template_args) {
				  for (auto i=template_args.first.begin(); i!=template_args.first.end();++i) {
					  std::pair<int,std::pair<int,unsigned long long>> u;
					  u.first = Visitor.getTypeData((*i).first).id;
					  Ids.push_back(u);
					  type_args_idx.push_back(Ids.size()-1);
				  }
			  }
			  for (auto i=Ids.begin(); i!=Ids.end(); i++) {
				  if (i+1!=Ids.end()) {
					  TOut << " " << (*i).first << ",";
				  }
				  else {
					  TOut << " " << (*i).first;
				  }
			  }
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"methods\": [";
			  for (auto i=MIds.begin(); i!=MIds.end(); i++) {
				  if (i+1!=MIds.end()) {
					  TOut << " " << (*i).first << ",";
				  }
				  else {
					  TOut << " " << (*i).first;
				  }
			  }
                          TOut << " ]";
                          if (rD->isCompleteDefinition() || have_templateref || have_template_args) {
                              TOut << ",";
                          }
			  TOut << "\n";
			  /* End of refs */
			  if (have_templateref) {
				  TOut << Indent << "\t\t\"templateref\": " << templateref << ",\n";
			  }
			  if (have_template_args) {
				  printTemplateArgs(TOut,template_args,type_args_idx,Indent,rD->isCompleteDefinition());
			  }
			  if (rD->isCompleteDefinition()) {
				  TOut << Indent << "\t\t\"refnames\": [ ";
				  if (RInfo) {
					  // Add attribute refname to merged attrrefs
					  for (auto i = QVIds.begin(); i!=QVIds.end(); ++i) {
						  RInfo->first.push_back("__!attribute__");
					  }
					  for (size_t i=0; i<RInfo->first.size();) {
						  TOut << "\"" << (*RInfo).first[i] << "\"";
						  ++i;
						  if (i<RInfo->first.size())
							  TOut << ",";
					  }
				  }
				  TOut << " ],\n";
				  TOut << Indent << "\t\t\"memberoffsets\": [ ";
				  if (!rD->isDependentType()) {
					  if (RInfo) {
						  for (size_t i=0; i<RInfo->second.size();) {
							  TOut << "" << (*RInfo).second[i] << "";
							  ++i;
							  if (i<RInfo->second.size())
								  TOut << ",";
						  }
					  }
				  }
				  TOut << " ],\n";
				  TOut << Indent << "\t\t\"usedrefs\": [ ";
				  for(auto i=rIds.begin(); i!=rIds.end(); i++){
					  if(i != rIds.begin()) TOut<<",";
					  TOut<<*i;
				  }
				  TOut << " ],\n";
				  TOut << Indent << "\t\t\"globalrefs\": [ ";
				  if (Visitor.gtp_refVars.find(rD)!=Visitor.gtp_refVars.end()) {
					  for (auto u = Visitor.gtp_refVars[rD].begin(); u!=Visitor.gtp_refVars[rD].end();) {
						  TOut << " " << Visitor.getVarData(*u).id;
						  ++u;
						  if (u!=Visitor.gtp_refVars[rD].end()) {
							  TOut << ",";
						  }
					  }
				  }
				  TOut << " ],\n";
				  TOut << Indent << "\t\t\"funrefs\": [ ";
				  if (Visitor.gtp_refFuncs.find(rD)!=Visitor.gtp_refFuncs.end()) {
					  for (auto u = Visitor.gtp_refFuncs[rD].begin(); u!=Visitor.gtp_refFuncs[rD].end();) {
						  size_t fnid = Visitor.getFunctionDeclId(*u);
						  TOut << " " << fnid;
						  ++u;
						  if (u!=Visitor.gtp_refFuncs[rD].end()) {
							  TOut << ",";
						  }
					  }
				  }
				  TOut << " ],\n";
				  std::vector<std::pair<unsigned long,unsigned long>> bitfields;
				  std::vector<std::pair<unsigned long,unsigned long>> decls;
				  for (auto i=Ids.begin(); i!=Ids.end(); i++) {
					  if ((*i).second.first&EXTRA_BITFIELD) {
						  bitfields.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
					  }
					  if ((*i).second.first==EXTRA_RECORDDECL) {
						  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
					  }
					  if ((*i).second.first==EXTRA_ENUMDECL) {
						  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
					  }
					  if ((*i).second.first==EXTRA_TEMPLATEDECL) {
						  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
					  }
					  if ((*i).second.first==EXTRA_TYPEDEFDECL) {
						  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
					  }
					  if ((*i).second.first==EXTRA_TYPEALIASDECL) {
						  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
					  }
					  if ((*i).second.first==EXTRA_TYPEALIASTEMPLATEDECL) {
						  decls.push_back(std::pair<unsigned long,unsigned long>(i-Ids.begin(),(*i).second.second));
					  }
				  }
				  TOut << Indent << "\t\t\"bitfields\": {\n";
				  for (auto i=bitfields.begin(); i!=bitfields.end(); i++) {
					  if (i+1!=bitfields.end()) {
						  TOut << Indent << "\t\t\t\"" << (*i).first << "\" : " << (*i).second << ",\n";
					  }
					  else {
						  TOut << Indent << "\t\t\t\"" << (*i).first << "\" : " << (*i).second << "\n";
					  }
				  }
				  TOut << Indent << "\t\t},\n";
				  TOut << Indent << "\t\t\"decls\": [\n";
				  for (auto i=decls.begin(); i!=decls.end(); i++) {
					  if (i+1!=decls.end()) {
						  TOut << Indent << "\t\t\t" << (*i).first << ",\n";
					  }
					  else {
						  TOut << Indent << "\t\t\t" << (*i).first << "\n";
					  }
				  }
				  TOut << Indent << "\t\t]\n";
			  }
		  }
		  break;
		  case Type::ConstantArray:
		  {
			  const ConstantArrayType *tp = cast<ConstantArrayType>(T);
			  QualType elT = tp->getElementType();
			  TOut << Indent << "\t\t\"class\": \"" << "const_array" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "[N]" << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(elT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(elT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::IncompleteArray:
		  {
			  const IncompleteArrayType *tp = cast<IncompleteArrayType>(T);
			  QualType elT = tp->getElementType();
			  TOut << Indent << "\t\t\"class\": \"" << "incomplete_array" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "[]" << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(elT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(elT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::VariableArray:
		  {
			  const VariableArrayType *tp = cast<VariableArrayType>(T);
			  QualType vaT = tp->getElementType();
			  TOut << Indent << "\t\t\"class\": \"" << "variable_array" << "\",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "[X]" << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(vaT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(vaT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::Typedef:
		  {
			  const TypedefType *tp = cast<TypedefType>(T);
			  TypedefNameDecl* D = tp->getDecl();
			  std::string def;
			  bool owned = false;
			  if (opts.adddefs) {
				  llvm::raw_string_ostream defstream(def);
				  clang::PrintingPolicy policy = Context.getPrintingPolicy();
				  QualType tT = D->getTypeSourceInfo()->getType();
				  if (tT->getTypeClass()==Type::Elaborated) {
					  TagDecl *OwnedTagDecl = cast<ElaboratedType>(tT)->getOwnedTagDecl();
					  if (OwnedTagDecl) {
                                                  if (isa<RecordDecl>(OwnedTagDecl)) {
                                                      const RecordDecl* rD = static_cast<RecordDecl*>(OwnedTagDecl);
                                                       if (rD->isCompleteDefinition()) {
                                                          owned = true;
                                                          policy.IncludeTagDefinition = true;
                                                      }
                                                  }
                                                  else if (isa<EnumDecl>(OwnedTagDecl)) {
                                                      const EnumDecl* eD = static_cast<EnumDecl*>(OwnedTagDecl);
                                                       if (eD->isCompleteDefinition()) {
                                                          owned = true;
                                                          policy.IncludeTagDefinition = true;
                                                      }
                                                  }
					  }
				  }
				  D->print(defstream,policy);
				  defstream.flush();
			  }
			  IdentifierInfo *II = D->getIdentifier();
			  QualType tT = D->getTypeSourceInfo()->getType();
			  TOut << Indent << "\t\t\"class\": \"" << "typedef" << "\",\n";
			  TOut << Indent << "\t\t\"name\": \"" << II->getName().str() << "\",\n";
			  if (D->isImplicit()) {
				  TOut << Indent << "\t\t\"implicit\": true,\n";
			  }
			  else {
				  TOut << Indent << "\t\t\"implicit\": false,\n";
			  }
			  if (owned) {
				  TOut << Indent << "\t\t\"decls\": " << "[ 0 ]" << ",\n";
			  }
			  TOut << Indent << "\t\t\"str\": \"" << II->getName().str() << "\",\n";
			  if (opts.adddefs) {
				  TOut << Indent << "\t\t\"def\": \"" << json::json_escape(def) << "\",\n";
			  }
                          TOut << Indent << "\t\t\"location\": \"" << getAbsoluteLocation(D->getLocation()) << "\",\n";
			  TOut << Indent << "\t\t\"refs\": [";
			  TOut << " " << Visitor.getTypeData(tT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(tT).id;
			  TOut << " ]\n";
		  }
		  break;
		  case Type::Enum:
		  {
			  QualType eT;
			  const EnumType *tp = cast<EnumType>(T);
			  EnumDecl* eD = tp->getDecl();
			  std::string def;
			  if (opts.adddefs) {
				  llvm::raw_string_ostream defstream(def);
				  eD->print(defstream);
				  defstream.flush();
			  }
			  const IdentifierInfo *II = eD->getIdentifier();
			  if (eD->isCompleteDefinition()) {
				  eT = resolve_Typedef_Integer_Type(tp->getDecl()->getIntegerType());
				  const BuiltinType *btp = cast<BuiltinType>(eT);
				  TOut << Indent << "\t\t\"class\": \"" << "enum" << "\",\n";
				  if (!eD->isDefinedOutsideFunctionOrMethod()) {
					  std::string outerFn = Visitor.parentFunctionOrMethodString(eD);
					  TOut << Indent << "\t\t\"outerfn\": \"" << outerFn << "\",\n";
					  TOut << Indent << "\t\t\"outerfnid\": " <<
							  Visitor.outerFunctionorMethodIdforTagDecl(eD) << ",\n";
				  }
				  TOut << Indent << "\t\t\"dependent\": " << btp->isDependentType() << ",\n";
				  TOut << Indent << "\t\t\"values\": [\n";
				  TOut << Indent << "\t\t\t";
				  const DeclContext *DC = cast<DeclContext>(eD);
				  std::vector<int64_t> ConstantValues;
				  std::vector<std::string> DependentValues;
				  std::vector<std::string> EnumIdentifiers;
				  for (DeclContext::decl_iterator D = DC->decls_begin(), DEnd = DC->decls_end();
				                                       D != DEnd; ++D) {

					  if (D->isImplicit()) continue;
                                          if (D->getKind()!=Decl::EnumConstant) continue;
					  EnumConstantDecl* ED = static_cast<EnumConstantDecl*>(*D);
					  if (!eT.isNull()) {
						  ConstantValues.push_back(ED->getInitVal().extOrTrunc(63).getExtValue());
					  }
					  else {
						  // This is dependent EnumConstantDecl which cannot be resolved to integer value just yet
						  std::string exprStr;
						  llvm::raw_string_ostream estream(exprStr);
						  Expr* IE = ED->getInitExpr();
						  if (IE) {
							  IE->printPretty(estream,nullptr,Context.getPrintingPolicy());
							  estream.flush();
						  }
						  DependentValues.push_back(exprStr);
					  }
					  EnumIdentifiers.push_back(ED->getIdentifier()->getName().str());
				  }
				  if (!eT.isNull()) {
					  for (auto i = ConstantValues.begin(); i!=ConstantValues.end();) {
						  TOut << (*i);
						  ++i;
						  if (i!=ConstantValues.end()) {
							  TOut << ", ";
						  }
					  }
				  }
				  else {
					  for (auto i = DependentValues.begin(); i!=DependentValues.end();) {
						  TOut << "\"" << json::json_escape(*i) << "\"";
						  ++i;
						  if (i!=DependentValues.end()) {
							  TOut << ", ";
						  }
					  }
				  }
				  TOut << "\n" << Indent << "\t\t],\n";
				  TOut << Indent << "\t\t\"identifiers\": [\n";
				  TOut << Indent << "\t\t\t";
				  for (auto i = EnumIdentifiers.begin(); i!=EnumIdentifiers.end();) {
					  TOut << "\"" << (*i) << "\"";
					  ++i;
					  if (i!=EnumIdentifiers.end()) {
						  TOut << ", ";
					  }
				  }
				  TOut << "\n" << Indent << "\t\t],\n";
			  }
			  else {
				  TOut << Indent << "\t\t\"class\": \"" << "enum_forward" << "\",\n";
			  }
			  if (II) {
				  std::string tpII;
				  if (isCXXTU(Context)) {
					  tpII = T.getAsString();
				  }
				  else {
					  tpII = II->getName().str();
				  }
				  TOut << Indent << "\t\t\"str\": \"" << tpII << "\",\n";
			  }
			  else {
				  TOut << Indent << "\t\t\"str\": \"" << "" << "\",\n";
			  }
			  if (opts.adddefs) {
				  TOut << Indent << "\t\t\"def\": \"" << json::json_escape(def) << "\",\n";
			  }
			  std::vector<int> refs;
			  TOut << Indent << "\t\t\"refs\": [";
			  if (eD->isCompleteDefinition()) {
				  TOut << " " << Visitor.getTypeData(eT).id;
				  refs.push_back(Visitor.getTypeData(eT).id);
				  std::set<DbJSONClassVisitor::ValueHolder> enumrefs;
				  std::vector<QualType> castrefs;
				  std::vector<QualType> QV;
				  const DeclContext *DC = cast<DeclContext>(eD);
				  for (DeclContext::decl_iterator D = DC->decls_begin(), DEnd = DC->decls_end();
													   D != DEnd; ++D) {
					  if (D->isImplicit()) continue;
                                          if (D->getKind()!=Decl::EnumConstant) continue;
					  EnumConstantDecl* ED = static_cast<EnumConstantDecl*>(*D);
					  if (ED->getInitExpr()) {
						  Visitor.lookForDeclRefExprs(ED->getInitExpr(),enumrefs);
						  Visitor.lookForExplicitCastExprs(ED->getInitExpr(),castrefs);
						  Visitor.lookForUnaryExprOrTypeTraitExpr(ED->getInitExpr(),QV);
					  }
				  }
				  std::set<int> irefs;
				  for (auto i = enumrefs.begin(); i!=enumrefs.end(); ++i) {
					  if ((*i).getValue()->getKind()==Decl::Kind::EnumConstant) {
						  const EnumConstantDecl *ECD = static_cast<const EnumConstantDecl*>((*i).getValue());
						  const EnumDecl *ED = static_cast<const EnumDecl*>(ECD->getDeclContext());
						  QualType ET = Context.getEnumType(ED);
						  irefs.insert(Visitor.getTypeData(ET).id);
					  }
					  else if ((*i).getValue()->getKind()==Decl::Kind::Var) {
						  const VarDecl *VD = static_cast<const VarDecl*>((*i).getValue());
						  if (Visitor.VarMap.find(Visitor.VarForMap(VD))!=Visitor.VarMap.end()) {
							  if(VD->isDefinedOutsideFunctionOrMethod()&&!VD->isStaticDataMember()){
								  Visitor.etp_refVars[eD].insert(Visitor.getVarData(VD).id);
							  }
						  }
					  }
				  }
				  for (auto i = castrefs.begin(); i!=castrefs.end(); ++i) {
					  irefs.insert(Visitor.getTypeData(*i).id);
				  }
				  for (auto i = QV.begin(); i!=QV.end(); ++i) {
					  irefs.insert(Visitor.getTypeData(*i).id);
				  }
				  for (auto i = irefs.begin(); i!=irefs.end(); ++i) {
					  TOut << ", " << (*i) << " ";
					  refs.push_back((*i));
				  }
			  }
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  for (auto i=refs.begin(); i!=refs.end(); i++) {
				  if (i+1!=refs.end()) {
					  TOut << " " << (*i) << ",";
				  }
				  else {
					  TOut << " " << (*i);
				  }
			  }
			  TOut << " ],";
			  TOut << Indent << "\t\t\"globalrefs\": [ ";
			  if (Visitor.etp_refVars.find(eD)!=Visitor.etp_refVars.end()) {
				  for (auto u = Visitor.etp_refVars[eD].begin(); u!=Visitor.etp_refVars[eD].end();) {
					  TOut << " " << *u;
					  ++u;
					  if (u!=Visitor.etp_refVars[eD].end()) {
						  TOut << ",";
					  }
				  }
			  }
			  TOut << " ]\n";

		  }
		  break;
		  case Type::FunctionProto:
		  {
			  const FunctionProtoType *tp = cast<FunctionProtoType>(T);
			  TOut << Indent << "\t\t\"class\": \"" << "function" << "\",\n";
			  TOut << Indent << "\t\t\"variadic\": " << tp->isVariadic() << ",\n";
			  TOut << Indent << "\t\t\"str\": \"" << "()" << "\",\n";
			  if (opts.adddefs) {
				  TOut << Indent << "\t\t\"def\": \"" << json::json_escape(T.getAsString()) << "\",\n";
			  }
			  std::vector<int> refs;
			  TOut << Indent << "\t\t\"refs\": [";
			  QualType rT = tp->getReturnType();
			  TOut << " " << Visitor.getTypeData(rT).id;
			  refs.push_back(Visitor.getTypeData(rT).id);
			  if (tp->getNumParams()>0) {
				  TOut << ",";
			  }
			  for (unsigned i = 0; i<tp->getNumParams(); ++i) {
				  QualType pT = tp->getParamType(i);
				  if (i+1<tp->getNumParams()) {
					  TOut << " " << Visitor.getTypeData(pT).id << ",";
				  }
				  else {
					  TOut << " " << Visitor.getTypeData(pT).id;
				  }
				  refs.push_back(Visitor.getTypeData(pT).id);
			  }
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  for (auto i=refs.begin(); i!=refs.end(); i++) {
				  if (i+1!=refs.end()) {
					  TOut << " " << (*i) << ",";
				  }
				  else {
					  TOut << " " << (*i);
				  }
			  }
			  TOut << " ]";
		  }
		  break;
		  case Type::FunctionNoProto:
		  {
			  const FunctionNoProtoType *tp = cast<FunctionNoProtoType>(T);
			  TOut << Indent << "\t\t\"class\": \"" << "function" << "\",\n";
			  TOut << Indent << "\t\t\"qualifiers\": \"n\",\n";
			  TOut << Indent << "\t\t\"variadic\": false,\n";
			  TOut << Indent << "\t\t\"str\": \"" << "()" << "\",\n";
			  if (opts.adddefs) {
				  TOut << Indent << "\t\t\"def\": \"" << json::json_escape(T.getAsString()) << "\",\n";
			  }
			  TOut << Indent << "\t\t\"refs\": [";
			  QualType rT = tp->getReturnType();
			  TOut << " " << Visitor.getTypeData(rT).id;
			  TOut << " ],\n";
			  TOut << Indent << "\t\t\"usedrefs\": [";
			  TOut << " " << Visitor.getTypeData(rT).id;
			  TOut << " ]\n";
		  }
		  break;
		  default:
		    llvm::errs()<<"Unhandled type in map: "<<T->getTypeClassName()<<'\n';
			T.dump();
			assert(0);
			  if (opts.exit_on_error) {
				  llvm::errs() << "\nERROR: Cannot print type specification for type:\n";
				  T.dump();
				  exit(EXIT_FAILURE);
			  }
			  break;
	  }
	  TOut << Indent << "\t}";
  }

  void DbJSONClassConsumer::printTypeArray(int Indentation) {
	  for (auto i = Visitor.getTypeMap().begin(); i!=Visitor.getTypeMap().end();i++) {
		  DbJSONClassVisitor::TypeData &type_data = i->second;
      if(type_data.output == nullptr) continue;
		  printTypeEntry(type_data,Indentation);
	  }
  }

  void DbJSONClassConsumer::processFops(){
	for(auto i = Visitor.getFopsMap().begin(); i!=Visitor.getFopsMap().end();i++){
    auto &fops_data = i->second;
      llvm::raw_string_ostream hs(fops_data.hash);
      switch(fops_data.obj.kind){
        case DbJSONClassVisitor::FopsObject::FopsKind::FObjGlobal:{
          fops_data.type_id = Visitor.getTypeData(fops_data.obj.T).id;
          fops_data.var_id = Visitor.getVarData(fops_data.obj.V).id;
          fops_data.func_id = 0;
          fops_data.loc = getAbsoluteLocation(fops_data.obj.V->getLocation());
          hs<<'g'<<fops_data.type_id <<'|'<<fops_data.var_id;
          break;
        }
        case DbJSONClassVisitor::FopsObject::FopsKind::FObjLocal:{
          auto &func_data = Visitor.getFuncData(fops_data.obj.F);
          fops_data.type_id = Visitor.getTypeData(fops_data.obj.T).id;
          fops_data.var_id = func_data.varMap.at(fops_data.obj.V).varId;
          fops_data.func_id = func_data.id;
          fops_data.loc = getAbsoluteLocation(fops_data.obj.V->getLocation());
          hs<<'l'<<fops_data.type_id <<'|'<<fops_data.var_id<<'|'<<fops_data.func_id;
          break;
        }
        case DbJSONClassVisitor::FopsObject::FopsKind::FObjFunction:{
          fops_data.type_id = Visitor.getTypeData(fops_data.obj.T).id;
          fops_data.var_id = 0;
          fops_data.func_id = Visitor.getFunctionDeclId(fops_data.obj.F);
          fops_data.loc = getAbsoluteLocation(fops_data.obj.F->getLocation());
          hs<<'f'<<fops_data.type_id<<'|'<<fops_data.func_id;
          break;
        }
      }
	  hs.flush();
      multi::registerFops(fops_data);
    }
  }


  void DbJSONClassConsumer::printFopsArray(int Indentation) {
    for (auto i = Visitor.getFopsMap().begin(); i !=Visitor.getFopsMap().end();i++) {
      const DbJSONClassVisitor::FopsData &fops_data = i->second;
      if(fops_data.output == nullptr) continue;
      printFopsEntry(fops_data,Indentation);
    }
  }

  void DbJSONClassConsumer::printFopsEntry(const DbJSONClassVisitor::FopsData &fops_data, int Indentation){
    std::string Indent(Indentation,'\t');
    llvm::raw_string_ostream Out(*fops_data.output);
    Out << Indent << "\t{\n";
    Out << Indent << "\t\t\"kind\": \""<<fops_data.obj.FopsKindName()<<"\",\n";
    Out << Indent << "\t\t\"type\": "<<fops_data.type_id<<",\n";
    Out << Indent << "\t\t\"var\": "<<fops_data.var_id<<",\n";
    Out << Indent << "\t\t\"func\": "<<fops_data.func_id<<",\n";
    Out << Indent << "\t\t\"loc\": \""<<fops_data.loc<<"\",\n";
    Out << Indent << "\t\t\"members\": {\n";
    for(auto field_iter = fops_data.fops_info.begin();field_iter != fops_data.fops_info.end();){
      Out << Indent << "\t\t\t\""<<field_iter->first<<"\": [";
      for(auto func_iter = field_iter->second.begin();func_iter!=field_iter->second.end();){
        Out<<Visitor.getFunctionDeclId(*func_iter);
        ++func_iter;
        if(func_iter!=field_iter->second.end()){
          Out<<", ";
        }
      }
      Out<<"]";
      ++field_iter;
      if(field_iter!=fops_data.fops_info.end()){
        Out<<",";
      }
      Out<<'\n';
    }
    Out << Indent << "\t\t}\n";
    Out << Indent << "\t}";
  }

  void DbJSONClassConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
	  exprOrd = 0;
	  TranslationUnitDecl *D = Context.getTranslationUnitDecl();
	  auto &SM = Context.getSourceManager();

	  if (opts.onlysrc) {
		  D->print(llvm::outs());
		  return;
	  }
	  if ((opts.tudump)||(opts.tudumpcont)||(opts.tudumpwithsrc)) {
		  D->dumpColor();
		  if (opts.tudumpwithsrc) {
			  D->print(llvm::outs());
		  }
		  if (!opts.tudumpcont) {
			  return;
		  }
	  }
	  if(opts.assert){
		setCTAList(&Visitor.CTAList);
	  }
	  Visitor.TraverseDecl(D);
	  computeVarHashes();
	  computeFuncHashes();
	  computeTypeHashes();
    processFops();

    // process data to string
    printGlobalArray(1);
    printTypeArray(1);
    printFuncArray(1);
    printFuncDeclArray(1);
    printFopsArray(1);

	  if (opts.brk) {
		  return;
	  }
	  // printDatabase();
  }

void DbJSONClassConsumer::printDatabase(){

	llvm::outs() << "{\n";
	llvm::outs() << "\t\"sourcen\": " << 1 << ",\n";
	llvm::outs() << "\t\"sources\": [\n\t\t{ \"" << multi::files.at(file_id) << "\" : " << 0 << " }\n\t],\n";
	llvm::outs() << "\t\"directory\": \"" << multi::directory << "\",\n";
	llvm::outs() << "\t\"typen\": " << Visitor.getTypeNum() << ",\n";
	llvm::outs() << "\t\"funcn\": " << Visitor.getFuncNum() << ",\n";
	llvm::outs() << "\t\"funcdecln\": " << Visitor.getFuncDeclNum() << ",\n";
	llvm::outs() << "\t\"unresolvedfuncn\": " << Visitor.getUnresolvedFuncNum() << ",\n";
	llvm::outs() << "\t\"globals\":" << "\n";
	printGlobalArray(2);
	llvm::outs() << ",\n";
	llvm::outs() << "\t\"types\":" << "\n";
	printTypeArray(2);
	llvm::outs() << ",\n";
	llvm::outs() << "\t\"funcs\":" << "\n";
	printFuncArray(2);
	llvm::outs() << ",\n";
	llvm::outs() << "\t\"funcdecls\":" << "\n";
	printFuncDeclArray(2);
	llvm::outs() << ",\n";
	llvm::outs() << "\t\"unresolvedfuncs\": []\n";
	assert(Visitor.getUnresolvedFuncNum() == 0 && "There should be no unresolved functions\n");

	if(/*disabled until full support implemented*/ 0){
		llvm::outs() << ",\n\t\"skipped_ranges\": [";
		auto &SkippedRanges = Macros.getSkippedRanges();
		for(auto it = SkippedRanges.begin(), end = SkippedRanges.end(); it != end; it++){
			if (it==SkippedRanges.begin()) llvm::outs() << "\n"; else llvm::outs() << ",\n";
			llvm::outs() << "\t\t{\n";
			llvm::outs() << "\t\t\t\"fid\": " << it->first << ",\n";
			llvm::outs() << "\t\t\t\"ranges\": [";
			for(auto r_it = it->second.begin(),r_end = it->second.end(); r_it != r_end; r_it++){
				if (r_it==it->second.begin()) llvm::outs() << "\n"; else llvm::outs() << ",\n";
				llvm::outs() << "\t\t\t\t["<<r_it->first<<", "<<r_it->second<<"]";
			}
			llvm::outs()<<"\n\t\t\t]\n";
			llvm::outs() << "\t\t}";
		}
		llvm::outs() << "\n\t]";
	}

	llvm::outs() << ",\n\t\"missingcallexprn\": " << Visitor.MissingCallExpr.size() << ",\n";
	llvm::outs() << "\n\t\"missingvardecl\": " << Visitor.MissingVarDecl.size() << ",\n";
	llvm::outs() << "\t\"missingrefsn\": " << Visitor.missingRefsCount << ",\n";
	llvm::outs() << "\t\"missingtypesn\": " << Visitor.missingTypes.size() << ",\n";
	llvm::outs() << "\t\"missingtypes\": [\n";
	for (auto i=Visitor.missingTypes.begin(); i!=Visitor.missingTypes.end();) {
		llvm::outs() << "\t\t{ \"" << (*i).second << "\" : 0 }";
		++i;
		if (i!=Visitor.missingTypes.end()) llvm::outs() << ",";
		llvm::outs() << "\n";
	}
	llvm::outs() << "\t],\n";
	llvm::outs() << "\t\"unsupportedfuncclassn\": " << Visitor.unsupportedFuncClass.size() << ",\n";
	llvm::outs() << "\t\"unsupportedfuncclass\": [\n";
	for (auto i=Visitor.unsupportedFuncClass.begin(); i!=Visitor.unsupportedFuncClass.end();) {
		llvm::outs() << "\t\t{ \"" << (*i).second << "\" : 0 }";
		++i;
		if (i!=Visitor.unsupportedFuncClass.end()) llvm::outs() << ",";
		llvm::outs() << "\n";
	}
	llvm::outs() << "\t],\n";
	llvm::outs() << "\t\"unsupporteddeclclassn\": " << Visitor.unsupportedDeclClass.size() << ",\n";
	llvm::outs() << "\t\"unsupporteddeclclass\": [\n";
	for (auto i=Visitor.unsupportedDeclClass.begin(); i!=Visitor.unsupportedDeclClass.end();) {
		llvm::outs() << "\t\t{ \"" << (*i).second << "\" : 0 }";
		++i;
		if (i!=Visitor.unsupportedDeclClass.end()) llvm::outs() << ",";
		llvm::outs() << "\n";
	}
	llvm::outs() << "\t],\n";
	llvm::outs() << "\t\"unsupportedexprrefsn\": " << Visitor.unsupportedExprRefs.size() << ",\n";
	llvm::outs() << "\t\"unsupportedexprrefs\": [\n";
	for (auto i=Visitor.unsupportedExprRefs.begin(); i!=Visitor.unsupportedExprRefs.end();) {
		llvm::outs() << "\t\t{ \"" << (*i) << "\" : 0 }";
		++i;
		if (i!=Visitor.unsupportedExprRefs.end()) llvm::outs() << ",";
		llvm::outs() << "\n";
	}
	llvm::outs() << "\t],\n";
	llvm::outs() << "\t\"unsupportedattrrefsn\": " << Visitor.unsupportedAttrRefs.size() << ",\n";
	llvm::outs() << "\t\"unsupportedattrrefs\": [\n";
	for (auto i=Visitor.unsupportedAttrRefs.begin(); i!=Visitor.unsupportedAttrRefs.end();) {
		llvm::outs() << "\t\t{ \"" << (*i) << "\" : 0 }";
		++i;
		if (i!=Visitor.unsupportedAttrRefs.end()) llvm::outs() << ",";
		llvm::outs() << "\n";
	}
	llvm::outs() << "\t]\n";
	llvm::outs() << "}\n";
}


#include <mutex>
namespace multi{
  namespace{
    struct db_status{
      size_t id;
      int kind; //enum {decl,weak,def}
      std::shared_ptr<std::string>out;
    };

    struct usedrefs{
      std::mutex m;
      struct ref{
        int id = -1;
        std::string def;
      };
      std::vector<ref>refs;
    };

    std::mutex VarLock;
    size_t VarId = 0;
    std::unordered_map<std::string,db_status>VarMap;
    std::vector<db_status*>Vars;

    std::mutex TypeLock;
    size_t TypeId = 0;
    std::unordered_map<std::string,db_status>TypeMap;
    std::vector<db_status*>Types;
    std::unordered_map<size_t,usedrefs> TypeUsedRefs;

    std::mutex FuncLock;
    size_t FuncId = 0;
    size_t FuncDeclCnt = 0;
    std::unordered_map<std::string,db_status>FuncMap;
    std::unordered_map<std::string,std::string>WeakMap;
    std::unordered_set<std::string> KnownFuncs;
    std::unordered_set<size_t> FixIdFuncs;
    std::vector<db_status*>Funcs;
    std::vector<db_status*>FDecls;
    std::unordered_map<size_t,std::set<int>>FuncFids;

	std::mutex FopsLock;
	std::unordered_map<std::string,db_status>FopsMap;
  }

  std::string directory;
  std::vector<std::string> files;

  void registerVar(DbJSONClassVisitor::VarData &var_data){
    const VarDecl *D = var_data.Node;
    std::lock_guard<std::mutex> lock(VarLock);
    auto rv = VarMap.insert({var_data.hash,{}});
    db_status &entry = rv.first->second;
    if(rv.second){
      // new entry
      entry.id = VarId++;
      entry.out = std::make_shared<std::string>();
      var_data.id.setIDProper(entry.id);
      var_data.output = entry.out;
      entry.kind = D->hasDefinition();
    }
    else{
      var_data.id.setIDProper(entry.id);
      var_data.output = nullptr;
      // update entry
      if(entry.kind < D->hasDefinition()){
        entry.out = std::make_shared<std::string>();
        var_data.output = entry.out;
      }
    }
  }

  void registerType(DbJSONClassVisitor::TypeData &type_data){
    std::lock_guard<std::mutex> lock(TypeLock);
    auto rv = TypeMap.insert({type_data.hash,{}});
    db_status &entry = rv.first->second;
    if(rv.second){
      // new entry
      entry.id = TypeId++;
      entry.out = std::make_shared<std::string>();
      type_data.id.setIDProper(entry.id);
      type_data.output = entry.out;
    }
    else{
      type_data.id.setIDProper(entry.id);
      type_data.output = nullptr;
    }
    if(type_data.T->getTypeClass() == Type::Record){
      entry.out = std::make_shared<std::string>();
      type_data.output = entry.out;
      type_data.usedrefs = &TypeUsedRefs[entry.id];
    }
  }

  void registerFuncDecl(DbJSONClassVisitor::FuncDeclData &func_data){
    std::lock_guard<std::mutex> lock(FuncLock);
    auto rv = FuncMap.insert({func_data.declhash,{}});
    db_status &entry = rv.first->second;
    if(rv.second){
      //new entry
      entry.kind = 0; //decl
      entry.id = FuncId++;
      entry.out = std::make_shared<std::string>();
      func_data.id.setIDProper(entry.id);
      func_data.output = entry.out;
      FuncDeclCnt++;
    }
    else{
      func_data.id.setIDProper(entry.id);
      func_data.output = nullptr;
    }
  }

  void registerFuncInternal(DbJSONClassVisitor::FuncData &func_data){
    std::lock_guard<std::mutex> lock(FuncLock);
    auto rv = FuncMap.insert({func_data.hash,{}});
    db_status &entry = rv.first->second;
    if(rv.second){
      // new entry
      entry.kind = 2; //def
      entry.id = FuncId++;
      entry.out = std::make_shared<std::string>();
      func_data.id.setIDProper(entry.id);
      func_data.output = entry.out;
    }
    else{
      func_data.id.setIDProper(entry.id);
      func_data.output = nullptr;
    }
    //fids
    FuncFids[entry.id].insert(func_data.fid);
  }

  void registerFunc(DbJSONClassVisitor::FuncData &func_data){
    std::lock_guard<std::mutex> lock(FuncLock);
    if(!KnownFuncs.insert(func_data.hash).second){
      // skip known function
      func_data.id.setIDProper(FuncMap.at(func_data.declhash).id);
      func_data.output = nullptr;
      FuncFids[FuncMap.at(func_data.declhash).id].insert(func_data.fid);
      return;
    }
    int kind = func_data.this_func->isWeak() ? 1 : 2; // weak : def 
    auto rv = FuncMap.insert({func_data.declhash,{}});
    db_status &entry = rv.first->second;
    if(rv.second){
      // new entry
      entry.kind = kind;
      entry.id = FuncId++;
      entry.out = std::make_shared<std::string>();
      func_data.id.setIDProper(entry.id);
      func_data.output = entry.out;
      // track weak definition
      if(kind == 1) WeakMap[func_data.declhash] = func_data.hash;
    }
    else{
      func_data.id.setIDProper(entry.id);
      if(entry.kind < kind){
        // update entry
        if(entry.kind == 0) FuncDeclCnt--;
        if(entry.kind == 1){
          // demote weak definition
          auto weak_rv = FuncMap.insert({WeakMap.at(func_data.declhash),{}});
          assert(weak_rv.second && "Entry already in map");
          db_status &weak_entry = weak_rv.first->second;
          weak_entry.kind = 1;
          weak_entry.id = FuncId++;
          weak_entry.out = entry.out;
          FixIdFuncs.insert(weak_entry.id);
        }
        entry.kind = kind;
        entry.out = std::make_shared<std::string>();
        func_data.output = entry.out;
        // track weak definition
        if(kind == 1) WeakMap[func_data.declhash] = func_data.hash;
      }
      else{
        // add weak definition (including strong definition conflicts for now)
        auto weak_rv = FuncMap.insert({func_data.hash,{}});
        assert(weak_rv.second && "Entry already in map");
        db_status &weak_entry = weak_rv.first->second;
        weak_entry.kind = kind;
        weak_entry.id = FuncId++;
        weak_entry.out = std::make_shared<std::string>();
        func_data.output = weak_entry.out;
        FixIdFuncs.insert(weak_entry.id);
      }
    }
    // fids
    FuncFids[entry.id].insert(func_data.fid);
  }

  void registerFops(DbJSONClassVisitor::FopsData &fops_data){
    std::lock_guard<std::mutex> lock(FopsLock);
    auto rv = FopsMap.insert({fops_data.hash,{}});
    db_status &entry = rv.first->second;
    if(rv.second){
      // new entry
      entry.out = std::make_shared<std::string>();
      fops_data.output = entry.out;
    }
    else{
      fops_data.output = nullptr;
    }

  }

  void handleRefs(void *rv, std::vector<int> rIds,std::vector<std::string> rDef){
    auto R = (usedrefs*)rv;
    std::lock_guard<std::mutex> lock(R->m);
    R->refs.resize(rIds.size());
    for(int i = 0;i<rIds.size();i++){
      auto &ref = R->refs[i];
      if(rIds[i]>=0){
        ref.id = rIds[i];
        ref.def = rDef[i];
      }
      if(ref.def.empty()){
        ref.def = rDef[i];
      }
    }
  }

  void updateEntry(std::string name, char delim, std::string &data, std::string &repl){
    auto begin = data.find("\"" + name+ "\":");
    if(begin == std::string::npos){
      llvm::errs()<<data<<'\n';
      llvm::errs()<<repl<<'\n';
      assert(0);
    }
    auto end = data.find(delim,begin);
    data.replace(begin,end-begin+1,repl);
  }

  void updateId(std::string& out, std::string& id){
    auto begin = out.find("\"id\":");
    auto end = out.find(',',begin);
    out.replace(begin,end-begin+1,id);
  }

  void updateFids(std::string& out, std::string& fids){
    auto begin = out.find("\"fids\":");
    auto end = out.find(']',begin);
    out.replace(begin,end-begin+1,fids);
  }

  void updateRefs(std::string &out, std::string &refs){
    auto begin = out.find("\"usedrefs\":");
    auto end = out.find(']',begin);
    out.replace(begin,end-begin+1,refs);
  }

  void processDatabase(){
    Vars.resize(VarId);
    for(auto &v : VarMap){
      Vars[v.second.id] = &v.second;
    }
    Types.resize(TypeId);
    for(auto &t : TypeMap){
      Types[t.second.id] = &t.second;
    }
    Funcs.resize(FuncId);
    for(auto &f : FuncMap){
      Funcs[f.second.id] = &f.second;
    }

    // update id
    for(auto f : FixIdFuncs){
      std::string id = "\"id\": ";
      id+=std::to_string(Funcs[f]->id)+',';
      std::string &out = *Funcs[f]->out;
      updateId(out,id);
    }

    // update fids
    for(auto &f : FuncFids){
      if(Funcs[f.first]->kind == 0) continue;
      std::string fids = "\"fids\": [";
      for(auto fid : f.second){
        fids += " " + std::to_string(fid) + ",";
      }
      fids.pop_back();
      fids+=" ]";
      std::string &out = *Funcs[f.first]->out;
      updateFids(out,fids);
    }

    // update usedrefs
    for(auto &t : TypeUsedRefs){
      if(t.second.refs.empty()) continue;
      std::string &out = *Types[t.first]->out;
      std::string usedrefs = "\"usedrefs\": [";
      std::string useddef = "\"useddef\": [";
      for(auto &ref : t.second.refs){
        usedrefs+= " " + std::to_string(ref.id) + ",";
        if(DISABLED && opts.adddefs && opts.csd){
          useddef+= " \"" + json::json_escape(ref.def) + "\",";
        }
      }
      usedrefs.pop_back();
      usedrefs+=" ]";
      updateEntry("usedrefs",']',out,usedrefs);
      if(DISABLED && opts.adddefs && opts.csd){
        useddef.pop_back();
        useddef += " ]";
        updateEntry("useddef",']',out,useddef);
      }
    }
  }

  void emitDatabase(llvm::raw_ostream &db_file){
    bool first;
    db_file << "{\n";
    db_file << "\t\"sourcen\": " << multi::files.size() << ",\n";
    db_file << "\t\"sources\": [\n";
    first = true;
    for(int i = 0; i<multi::files.size();i++){
      if(first) first = false;
      else db_file << ",\n";
      db_file<<"\t\t{ \"" << files.at(i) << "\" : " << i << " }";
    }
    db_file << "\n\t],\n";

    db_file << "\t\"directory\": \"" << multi::directory << "\",\n";
    db_file << "\t\"typen\": " << TypeId << ",\n";
    db_file << "\t\"globaln\": " << VarId << ",\n";
    db_file << "\t\"funcdecln\": " << FuncDeclCnt << ",\n";
    db_file << "\t\"funcn\": " << FuncId - FuncDeclCnt << ",\n";
    db_file << "\t\"unresolvedfuncn\": " << 0 << ",\n";
    db_file << "\t\"fopn\": " <<FopsMap.size()<<",\n";

    db_file << "\t\"globals\": [\n";
    first = true;
    for(int i = 0; i< VarId;i++){
      if(first) first = false;
      else db_file << ",\n";
      db_file<<*(Vars.at(i)->out);
    }
    db_file << "\n\t],\n";

    db_file << "\t\"types\": [\n";
    first = true;
    for(int i = 0; i< TypeId;i++){
      if(first) first = false;
      else db_file << ",\n";
      db_file<<*(Types.at(i)->out);
    }
    db_file << "\n\t],\n";

    db_file << "\t\"funcs\": [\n";
    first = true;
    for(int i = 0; i< FuncId; i++){
      if(Funcs.at(i)->kind == 0){
        FDecls.push_back(Funcs.at(i));
        continue;
      }
      if(first) first = false;
      else db_file << ",\n";
      db_file<<*(Funcs.at(i)->out);
    }
    db_file << "\n\t],\n";

    db_file << "\t\"funcdecls\": [\n";
    first = true;
    for(int i = 0; i< FuncDeclCnt;i++){
      if(first) first = false;
      else db_file << ",\n";
      db_file<<*(FDecls.at(i)->out);
    }
    db_file << "\n\t],\n";

    db_file << "\t\"unresolvedfuncs\": [],\n";

    db_file << "\t\"fops\": [\n";
    first = true;
    for(auto i = FopsMap.begin(); i != FopsMap.end();i++){
      if(first) first = false;
      else db_file << ",\n";
      db_file<<*(i->second.out);
    }
    db_file << "\n\t]\n";

    db_file << "}\n";
  }

  void report(){
    llvm::errs()<<"vars:  "<<VarId<<'\n';
    llvm::errs()<<"types: "<<TypeId<<'\n';
    llvm::errs()<<"funcdecls: "<<FuncDeclCnt<<'\n';
    llvm::errs()<<"funcs: "<<FuncId-FuncDeclCnt<<'\n';

    unsigned long vtotal=0;
    unsigned long ttotal=0;
    unsigned long ftotal=0;
    std::vector<std::string> vmap(VarId);
    for(auto &v :VarMap){
      vmap[v.second.id] = v.first;
      vtotal+=v.second.out.get()->length();
    }
    for(int i =0;i<VarId;i++){
      llvm::outs()<<"G"<< llvm::format_decimal(i,8)<<"  "<<vmap[i]<<'\n';

    }

    std::vector<std::string> tmap(TypeId);
    for(auto &t :TypeMap){
      tmap[t.second.id] = t.first;
      ttotal+=t.second.out.get()->length();
    }
    for(int i =0;i<TypeId;i++){
      llvm::outs()<<"T"<< llvm::format_decimal(i,8)<<"  "<<tmap[i]<<'\n';
    }

    std::vector<std::string> fmap(FuncId);
    for(auto &f :FuncMap){
      fmap[f.second.id] = f.first;
      assert(f.second.out && "Somehow empty pointer...");
      ftotal+=f.second.out.get()->length();
    }
    for(int i =0;i<FuncId;i++){
      llvm::outs()<<"F"<< llvm::format_decimal(i,8)<<"  "<<fmap[i]<<'\n';
    }

    llvm::errs()<<vtotal<<'\n';
    llvm::errs()<<ttotal<<'\n';
    llvm::errs()<<ftotal<<'\n';
  }
}