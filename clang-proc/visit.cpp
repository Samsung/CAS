#include "main.hpp"
#include "clang/AST/RecordLayout.h"
#include <DeclPrinter.h>
#include <StmtPrinter.h>
#include <TypePrinter.h>

// Decl visitors
bool DbJSONClassVisitor::TraverseDecl(Decl *D) {
	bool TraverseResult = RecursiveASTVisitor<DbJSONClassVisitor>::TraverseDecl(D);
	if (D) {
		switch (D->getKind()) {
			case Decl::Function:
			case Decl::CXXMethod:
			case Decl::CXXDestructor:
			case Decl::CXXConstructor:
			case Decl::CXXConversion:
			case Decl::CXXDeductionGuide:
			{
				if (!VisitFunctionDeclComplete(static_cast<FunctionDecl*>(D))) return false;
				break;
			}
			case Decl::Var:
			{
				if (!VisitVarDeclComplete(static_cast<VarDecl*>(D))) return false;
				break;
			}
			case Decl::Record:
			{
				if (!VisitRecordDeclComplete(static_cast<RecordDecl*>(D))) return false;
			}
		}
	}
	return TraverseResult;
}

// Track declaration context in some cases
bool DbJSONClassVisitor::VisitDecl(Decl *D) {
	switch (D->getKind()) {
		case Decl::Function:
		case Decl::CXXMethod:
		case Decl::CXXDestructor:
		case Decl::CXXConstructor:
		case Decl::CXXConversion:
		case Decl::CXXDeductionGuide:
		{
		FunctionDecl* FD = static_cast<FunctionDecl*>(D);
		return VisitFunctionDeclStart(FD);
		}
		case Decl::Var:
		{
		VarDecl* VD = static_cast<VarDecl*>(D);
		return VisitVarDeclStart(VD);
		}
		case Decl::Record:
		{
		RecordDecl* RD = static_cast<RecordDecl*>(D);
		return VisitRecordDeclStart(RD);
		}
	}
	return true;
}

// Variables
bool DbJSONClassVisitor::VisitVarDeclStart(const VarDecl *D) {
	if (D->isDefinedOutsideFunctionOrMethod() && !D->isStaticDataMember()
			&& D->hasGlobalStorage() && !D->isStaticLocal()) {
		// We're are taking care of global variable
		lastGlobalVarDecl = D->getNameAsString();
	}
	inVarDecl.push_back(true);
	return true;
}

bool DbJSONClassVisitor::VisitVarDeclComplete(const VarDecl *D) {
	lastGlobalVarDecl = "";
	inVarDecl.pop_back();
	return true;
}

unsigned long NumInitializedElements(const Expr* E) {

	unsigned long init_count = 0;
	if (E->getStmtClass()==Stmt::InitListExprClass) {
		const InitListExpr* ILE = static_cast<const InitListExpr*>(E);
		for (unsigned i=0; i<ILE->getNumInits(); ++i) {
			const Expr* IE = ILE->getInit(i);
			init_count+=NumInitializedElements(IE);
		}
	}
	else if (E->getStmtClass()==Stmt::ImplicitValueInitExprClass) {
	}
	else {
		init_count+=1;
	}
	return init_count;
}

bool DbJSONClassVisitor::VisitVarDecl(const VarDecl *D) {

	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitVarDecl() [" << D << ":" << D->getKind() <<"]\n"; D->dumpColor(); );

	if (_opts.debugME) {
		llvm::outs() << "@VisitVarDecl()\n";
		D->dumpColor();
	}

	if ((D->getKind() == Decl::Kind::Var)&&(D->getInit())) {
		DbJSONClassVisitor::DREMap_t DREMap;
		std::vector<CStyleCastOrType> castVec;
		const Expr* E = stripCastsEx(D->getInit(),castVec);
		bool isAddress = false;
		if (castVec.size()>0) {
			if (castVec.front().getFinalType()->getTypeClass()==Type::Pointer) {
				isAddress = true;
			}
		}
		// Check if there's implicit (or explicit) cast from the initializer
		// Handle void* as a special case and allow casting in opposite way to the initializer
		// Also ignore implicit casts for string literals
		QualType castType;
		if (getFirstCast(castVec)) {
			castType = getFirstCast(castVec)->getType();
		}
		else {
			if (D->getInit()->getStmtClass()==Stmt::ImplicitCastExprClass) {
				const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(D->getInit());
				if (ICE->getSubExpr()->getType()!=D->getType()) {
					if (isPtrToVoid(D->getType())) {
						castType = ICE->getSubExpr()->getType();
					}
					else {
						if (E->getStmtClass()!=Stmt::StringLiteralClass) {
							castType = D->getType();	
						}
					}
				}
			}
		}
		if (!castType.isNull()) {
			noticeTypeClass(castType);
		}
		// Check if init can be evaluated as a constant expression
		Expr::EvalResult Res;
		if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)) {
			int64_t i = Res.Val.getInt().extOrTrunc(64).getExtValue();
			ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS v;
			CStyleCastOrType valuecast;
			if (!castType.isNull()) {
				valuecast.setType(castType);
			}
			if (isAddress) {
				v.setAddress(i,valuecast);
			}
			else {
				v.setInteger(i,valuecast);
			}
			vMCtuple_t vMCtuple;
			v.setPrimaryFlag(false);
			DREMap_add(DREMap,v,vMCtuple);
		}
		else {
			DbJSONClassVisitor::lookup_cache_t cache;
			bool compundStmtSeen = false;
			unsigned MECnt = 0;
			lookForDeclRefWithMemberExprsInternal(D->getInit(),D->getInit(),DREMap,cache,&compundStmtSeen,
					0,&MECnt,0,true,false,false,castType);
		}

		std::string Expr;
		llvm::raw_string_ostream exprstream(Expr);
		exprstream << "[" << D->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
		DeclPrinter Printer(exprstream, Context.getPrintingPolicy(), Context, 0, false, &CTAList);
		Printer.Visit(const_cast<VarDecl*>(D));
		exprstream.flush();

		VarRef_t VR;
		VR.VDCAMUAS.setValue(D);
		std::vector<VarRef_t> vVR;
		vVR.push_back(VR);
		unsigned size = DREMap.size();
		for (DbJSONClassVisitor::DREMap_t::iterator i = DREMap.begin(); i!=DREMap.end(); ++i) {
			VarRef_t iVR;
			iVR.VDCAMUAS = (*i).first;
			vVR.push_back(iVR);
		}

		if (!D->isDefinedOutsideFunctionOrMethod()) {
			if (lastFunctionDef) {
				std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
						lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,NumInitializedElements(D->getInit()),vVR,Expr,
								getCurrentCSPtr(),DereferenceInit));
				const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
			}
		}
	}

	if (_opts.cstmt && (!D->isDefinedOutsideFunctionOrMethod())) {
		if (lastFunctionDef) {
			++lastFunctionDef->varId;
			struct DbJSONClassVisitor::VarInfo_t vi = {0,0,0,0};
			if (currentWithinCS()) {
				vi.CSPtr = getCurrentCSPtr();
				if (hasParentCS()) {
					vi.parentCSPtr = getParentCSPtr();
				}
			}
			vi.varId = lastFunctionDef->varId;

			if(D->getKind() == Decl::Kind::Var){
				vi.VD = D;
				if (_opts.debug3) {
					llvm::outs() << "VarDecl: " << D->getName().str() << " (" << lastFunctionDef->this_func->getName().str() << ")["
							<< lastFunctionDef->csIdMap[vi.CSPtr] << "] " << D->getLocation().printToString(Context.getSourceManager()) << "\n";
				}
			}
			else if(D->getKind() == Decl::Kind::ParmVar){
				vi.PVD = cast<const ParmVarDecl>(D);
				if (_opts.debug3) {
					llvm::outs() << "ParmVarDecl: " << D->getName().str() << " (" << lastFunctionDef->this_func->getName().str() << ") "
							<< D->getLocation().printToString(Context.getSourceManager()) << "\n";
				}
			}
			else if (D->getKind() == Decl::Kind::Decomposition) {
				// Do nothing (for now)
			}
			else {
				if (_opts.exit_on_error) {
					llvm::outs() << "\nERROR: Unsupported declaration kind: " << D->getDeclKindName() << "\n";
					D->dump(llvm::outs());
					llvm::outs() << D->getLocation().printToString(Context.getSourceManager()) << "\n";
					exit(EXIT_FAILURE);

				}
			}
			lastFunctionDef->varMap.insert(std::pair<const VarDecl*,VarInfo_t>(D,vi));
		}
	}

	if (lastFunctionDef) {
		const FunctionDecl* FD = lastFunctionDef->this_func;
		DBG(DEBUG_NOTICE, FD->dumpColor(); llvm::errs() << "isMember: " << int(FD->isCXXClassMember()) << "\n"; );
		const CXXRecordDecl* RD = 0;
		if (FD->isCXXClassMember()) {
			const CXXMethodDecl* MD = static_cast<const CXXMethodDecl*>(FD);
			RD = MD->getParent();
		}
		DBG(DEBUG_NOTICE, llvm::errs() << "RD: " << RD << "\n"; if (RD) RD->dumpColor(); );
		noticeTypeClass(D->getType());
		lastFunctionDef->refTypes.insert(D->getType());
	}
	if(D->getKind() == Decl::Kind::Var){
		if(D->isStaticDataMember()) return true;
		if(!D->hasGlobalStorage()) return true;
		if(D->isStaticLocal()) return true;
		std::string name = D->getNameAsString();
		static std::set<std::string> unique_name;
		if(!unique_name.insert(name).second) return true;
		int linkage = D->isExternallyVisible();
		int def_kind = D->hasDefinition();
		for( const VarDecl *RD : D->redecls()){
			if(RD->isThisDeclarationADefinition() == def_kind) {
				D=RD;
				break;
			}
		}

		switch(def_kind){
			case 2:
			{
				const VarDecl *DD = D->getDefinition();
#if CLANG_VERSION>6
				QualType ST = DD->getTypeSourceInfo() ? DD->getTypeSourceInfo()->getType() : DD->getType();
				if (isOwnedTagDeclType(ST)) {
					noticeTypeClass(ST);
				}
#endif
				noticeTypeClass(DD->getType());
				VarMap.insert({name,{DD,VarNum++}});
				revVarMap.insert({VarNum-1,DD});
				VarIndex.push_back(name);
				break;
			}
			case 1:
			{
				const VarDecl *TD = D->getActingDefinition();
#if CLANG_VERSION>6
				QualType ST = TD->getTypeSourceInfo() ? TD->getTypeSourceInfo()->getType() : TD->getType();
				if (isOwnedTagDeclType(ST)) {
					noticeTypeClass(ST);
				}
#endif
				noticeTypeClass(TD->getType());
				VarMap.insert({name,{TD,VarNum++}});
				revVarMap.insert({VarNum-1,TD});
				VarIndex.push_back(name);
				break;
			}
			case 0:
			{
				assert(linkage && "Static variable not defined");
				const VarDecl *CD = D->getCanonicalDecl();
#if CLANG_VERSION>6
				QualType ST = CD->getTypeSourceInfo() ? CD->getTypeSourceInfo()->getType() : CD->getType();
				if (isOwnedTagDeclType(ST)) {
					noticeTypeClass(ST);
				}
#endif
				noticeTypeClass(CD->getType());
				VarMap.insert({name,{CD,VarNum++}});
				revVarMap.insert({VarNum-1,CD});
				VarIndex.push_back(name);
				break;
			}
			default: {
				llvm::outs() << "WARNING: Invalid def_kind (" << (int)def_kind << ")\n";
			}
		}
	}
	return true;
}

// Types
bool DbJSONClassVisitor::VisitRecordDeclStart(const RecordDecl *D) {
	recordDeclStack.push(D);
	return true;
}

bool DbJSONClassVisitor::VisitRecordDeclComplete(const RecordDecl *D) {
	recordDeclStack.pop();
	return true;
}

bool DbJSONClassVisitor::VisitRecordDecl(const RecordDecl *D) {
	QualType T = Context.getRecordType(D);
	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitRecordDecl(" << D << ")\n"; D->dump(llvm::outs()); T.dump(llvm::outs()) );
	noticeTypeClass(T);
	if (_opts.cstmt) {
		if (currentWithinCS()) {
			if (recordCSMap.find(D)==recordCSMap.end()) {
				recordCSMap.insert(std::pair<const RecordDecl*,const CompoundStmt*>(D,getCurrentCSPtr()));
			}
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitEnumDecl(const EnumDecl *D) {
	  DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitEnumDecl()\n"; D->dump(llvm::outs()) );
	  QualType T = Context.getEnumType(D);
	  noticeTypeClass(T);
	  return true;
  }

bool DbJSONClassVisitor::VisitTypedefDecl(TypedefDecl *D) {
	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitTypedefDecl()\n"; D->dump(llvm::outs()) );
	QualType T = Context.getTypeDeclType(D);
	noticeTypeClass(T);
	QualType tT = D->getTypeSourceInfo()->getType();
	if (tT->getTypeClass()==Type::Elaborated) {
		TagDecl *OwnedTagDecl = cast<ElaboratedType>(tT)->getOwnedTagDecl();
		if (OwnedTagDecl && OwnedTagDecl->isCompleteDefinition()){
			TypedefRecords.insert(OwnedTagDecl);
		}
	}
	return true;
}

// Types - c++ only
bool DbJSONClassVisitor::VisitCXXRecordDecl(const CXXRecordDecl* D) {
	return true;
}

bool DbJSONClassVisitor::VisitClassTemplateDecl(const ClassTemplateDecl *D) {
	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitClassTemplateDecl()\n"; D->dump(llvm::outs()) );

	CXXRecordDecl* RD = D->getTemplatedDecl();

	if (classTemplateMap.find(RD)==classTemplateMap.end()) {
		classTemplateMap.insert(std::pair<CXXRecordDecl*,const ClassTemplateDecl*>(RD,D));
	}

	QualType T = Context.getRecordType(static_cast<const RecordDecl*>(RD));
	noticeTypeClass(T);

	bool rv = true;
	for (auto i = D->spec_begin(); i!=D->spec_end(); ++i) {
		if (!VisitClassTemplateSpecializationDecl(*i)) rv=false;
	}

	// Notice template parameters
	const TemplateParameterList * Params = D->getTemplateParameters();

	for (unsigned i = 0, e = Params->size(); i != e; ++i) {
		const Decl *Param = Params->getParam(i);
		if (auto TTP = dyn_cast<TemplateTypeParmDecl>(Param)) {
			noticeTypeClass(Context.getTypeDeclType(TTP));
		}
		else if (auto NTTP = dyn_cast<NonTypeTemplateParmDecl>(Param)) {
			noticeTypeClass(NTTP->getType());
		}
		else if (auto TTPD = dyn_cast<TemplateTemplateParmDecl>(Param)) {
			/* Not supported */
		}
	}

	if (_opts.debug) {
		std::string _class;
		QualType RT = Context.getRecordType(RD);
		_class = RT.getAsString();
		llvm::outs() << "notice classTemplate: " << _class << RD << "\n";
	}
	return rv;
}

bool DbJSONClassVisitor::VisitClassTemplatePartialSpecializationDecl(const ClassTemplatePartialSpecializationDecl *D) {
	const CXXRecordDecl* RD = static_cast<const CXXRecordDecl*>(D);

	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitClassTemplatePartialSpecializationDecl(" << D << ")\n";
		D->dump(llvm::outs()) );

	if (classTemplatePartialSpecializationMap.find(RD)==classTemplatePartialSpecializationMap.end()) {
		classTemplatePartialSpecializationMap.insert(std::pair<const CXXRecordDecl*,
				const ClassTemplatePartialSpecializationDecl*>(RD,D));
	}

	QualType T = Context.getRecordType(static_cast<const RecordDecl*>(D));
	noticeTypeClass(T);

	// Notice template parameters
	const TemplateParameterList * Params = D->getTemplateParameters();

	for (unsigned i = 0, e = Params->size(); i != e; ++i) {
		const Decl *Param = Params->getParam(i);
		if (auto TTP = dyn_cast<TemplateTypeParmDecl>(Param)) {
			noticeTypeClass(Context.getTypeDeclType(TTP));
		}
		else if (auto NTTP = dyn_cast<NonTypeTemplateParmDecl>(Param)) {
			noticeTypeClass(NTTP->getType());
		}
		else if (auto TTPD = dyn_cast<TemplateTemplateParmDecl>(Param)) {
			/* Not supported */
		}
	}

	if (_opts.debug) {
		std::string _class;
		QualType RT = Context.getRecordType(RD);
		_class = RT.getAsString();
		std::string templatePars;
		llvm::raw_string_ostream tpstream(templatePars);
		DeclPrinter Printer(tpstream, Context.getPrintingPolicy(), Context);
		Printer.printTemplateParameters(D->getTemplateParameters());
		tpstream.flush();
		llvm::outs() << "notice classTemplatePartialSpecialization: [" << _class << "] "
				<< templatePars << " " << D << "\n";
	}
	return true;
}

bool DbJSONClassVisitor::VisitClassTemplateSpecializationDecl(const ClassTemplateSpecializationDecl *D) {

	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitClassTemplateSpecializationDecl(" << D << ")\n"; D->dump(llvm::outs()) );

	const CXXRecordDecl* RD = static_cast<const CXXRecordDecl*>(D);

	if (classTemplateSpecializationMap.find(RD)==classTemplateSpecializationMap.end()) {
		classTemplateSpecializationMap.insert(std::pair<const CXXRecordDecl*,
				const ClassTemplateSpecializationDecl*>(RD,D));
	}

	QualType RT = Context.getRecordType(RD);
	std::string _class = RT.getAsString();

	QualType T = Context.getRecordType(static_cast<const RecordDecl*>(D));
	noticeTypeClass(T);

	if (_opts.debug) {
		std::string _class;
		QualType RT = Context.getRecordType(RD);
		_class = RT.getAsString();
		std::string templatePars;
		llvm::raw_string_ostream tpstream(templatePars);
		DeclPrinter Printer(tpstream, Context.getPrintingPolicy(), Context);
#if CLANG_VERSION>9
		Printer.printTemplateArguments(D->getTemplateArgs().asArray());
#else
		Printer.printTemplateArguments(D->getTemplateArgs());
#endif
		tpstream.flush();
		llvm::outs() << "notice classTemplateSpecialization: [" << _class << "] "
				<< templatePars << " " << D << "\n";
	}
	return true;
}

bool DbJSONClassVisitor::VisitTypeAliasDecl(TypeAliasDecl *D) {
	DBG(DEBUG_NOTICE, llvm::errs() << "@notice VisitTypeAliasDecl()\n";
		if (D->getDescribedAliasTemplate()) D->getDescribedAliasTemplate()->dump(llvm::outs()); else D->dump(llvm::outs()); );

	TypeAliasTemplateDecl* TATD = D->getDescribedAliasTemplate();
	if (TATD) {
		noticeTemplateParameters(TATD->getTemplateParameters());
	}
	QualType T = Context.getTypeDeclType(D);
	noticeTypeClass(T);
	return true;
}

bool DbJSONClassVisitor::VisitTypeAliasDeclFromClass(TypeAliasDecl *D) {

	DBG(DEBUG_NOTICE, llvm::errs() << "@notice VisitTypeAliasDeclFromClass()\n";
		if (D->getDescribedAliasTemplate()) D->getDescribedAliasTemplate()->dump(llvm::outs()); else D->dump(llvm::outs()); );

	TypeAliasTemplateDecl* TATD = D->getDescribedAliasTemplate();
	if (TATD) {
		noticeTemplateParameters(TATD->getTemplateParameters());
	}
	QualType T = Context.getTypeDeclType(D);
	assert(T->getTypeClass()==Type::Typedef && "Invalid TypeClass for TypeAlias type");
	const TypedefType *ttp = cast<TypedefType>(T);
	TypedefNameDecl* TPD = ttp->getDecl();
	QualType UT = TPD->getUnderlyingType();
	const TemplateSpecializationType* tp = LookForTemplateSpecializationType(UT);
	if (TATD && tp) {
		templateSpecializationTypeAliasMap.insert(std::pair<const TemplateSpecializationType*,TypeAliasDecl*>(tp,D));
	}
	noticeTypeClass(T);

	return true;
}

bool DbJSONClassVisitor::VisitTypedefDeclFromClass(TypedefDecl *D) {
	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitTypedefDeclFromClass()\n"; D->dump(llvm::outs()) );
	QualType T = Context.getTypeDeclType(D);
	noticeTypeClass(T);
	return true;
}

// Functions
bool DbJSONClassVisitor::VisitFunctionDeclStart(const FunctionDecl *D) {
	
	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitFunctionDeclStart(" << D << ")\n"; D->dump(llvm::outs()) );
	
	if (!D->getIdentifier()) {
		std::stringstream DN;
		DN << static_cast<const Decl*>(D)->getDeclKindName() << "Decl";
		unsupportedFuncClass.insert(std::pair<Decl::Kind,std::string>(D->getDeclKind(),DN.str()));
		return true;
	}


	if (_opts.BreakFunPlaceholder!="") {
		if (D->getName().str()==_opts.BreakFunPlaceholder) {
			int __x = 0;
		}
	}

	if (D->hasBody()) {
		if (FuncMap.find(D)!=FuncMap.end()) {
			DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitFunctionDeclStart(): present\n"; );
			lastFunctionDef = &FuncMap[D];
			return true;
		}
	}
	else {
		lastFunctionDefCache = lastFunctionDef;
		lastFunctionDef = nullptr;
		if ((getFuncDeclMap().find(D)!=getFuncDeclMap().end())||(
				(_opts.assert)&&(CTAList.find(D)!=CTAList.end())
			)) {
			DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitFunctionDeclStart(): present\n"; );
			return true;
		}
	}

	QualType rT = D->getReturnType();
	std::stringstream className;
	std::stringstream funcName;
	const CXXRecordDecl* RD = 0;
	if (D->isCXXClassMember()) {
		const CXXMethodDecl* MD = static_cast<const CXXMethodDecl*>(D);
		RD = MD->getParent();
		funcName << MD->getNameAsString();
		className << RD->getNameAsString() << "::";
	}
	else {
		funcName << D->getName().str();
	}

	bool funcSaved = false;
	if (D->hasBody()) {
		const FunctionDecl * defdecl = D->getDefinition();
		if (defdecl==D) {
			FuncData x;
			x.id = FuncNum++;
			assert(FuncMap.find(D)==FuncMap.end() && "Multiple definitions of Function body");
			FuncMap.insert({D,x});
			functionStack.push_back(&FuncMap[D]);
			funcSaved = true;
			lastFunctionDef = &FuncMap[D];
			lastFunctionDef->this_func = D;
			lastFunctionDef->CSId = -1;
			lastFunctionDef->varId = -1;
			if(_opts.taint){
				FuncMap[D].declcount = internal_declcount(D);
				taint_params(D,FuncMap[D]);
			}
		}
	}
	else {
		D = D->getCanonicalDecl();
		if (getFuncDeclMap().find(D)==getFuncDeclMap().end()) {
			if (friendDeclMap.find(D)==friendDeclMap.end()) {
				// Ignored friend function declarations when arrived here
				funcSaved = true;
				if (_opts.assert) {
					if (is_compiletime_assert_decl(D,Context)) {
						/* Save only first encounter of the "void __compiletime_assert_N()" function declaration
						* Later resolve all references to __compiletime_assert_M() to the first seen declaration
						*/
						if (CTA) {
							funcSaved = false;
						}
						else {
							CTA = D;
						}
						CTAList.insert(D);
					}
				}
				if (funcSaved) {
					getFuncDeclMap().insert(std::pair<const FunctionDecl*,size_t>(D,FuncNum++));
				}
			}
		}
	}

	if ((D->hasBody() &&  (D->getDefinition()==D)))
	DBG(_opts.debug, llvm::outs() << "notice Function: " << className.str() <<
						funcName.str() << "() [" << D->getNumParams() << "] ("
						<< D->getLocation().printToString(Context.getSourceManager()) << ")"
						<< "(" << D->hasBody() << ")" << "(" << (D->hasBody() &&  (D->getDefinition()==D)) << ") "
						<< (const void*)D << "\n" );

	//D->dumpColor();

	if (funcSaved) {
		const TemplateSpecializationType* tp = LookForTemplateSpecializationType(rT);
		noticeTypeClass(rT);

		for(unsigned long i=0; i<D->getNumParams(); i++) {
			std::string TypeS;
			const ParmVarDecl* p = D->getParamDecl(i);

			QualType T = p->getTypeSourceInfo() ? p->getTypeSourceInfo()->getType() : p->getType();

			const TemplateSpecializationType* tp = LookForTemplateSpecializationType(T);
			noticeTypeClass(T);
		}

		if (D->getTemplatedKind()==FunctionDecl::TK_NonTemplate) {
		}
		else if (D->getTemplatedKind()==FunctionDecl::TK_FunctionTemplate) {
		}
		else if (D->getTemplatedKind()==FunctionDecl::TK_MemberSpecialization) {
		}
		else if (D->getTemplatedKind()==FunctionDecl::TK_FunctionTemplateSpecialization) {
		}
		else if (D->getTemplatedKind()==FunctionDecl::TK_DependentFunctionTemplateSpecialization) {
		}

		DBG(DEBUG_NOTICE, llvm::outs() << "notice Function: " << className.str() <<
				funcName.str() << "() [" << D->getNumParams() << "]: DONE\n"; );
	}
	return true;
}
  
bool DbJSONClassVisitor::VisitFunctionDeclComplete(const FunctionDecl *D) {

	if (!D->getIdentifier()) {
		return true;
	}

	if (D->hasBody()) {
		const FunctionDecl * defdecl = D->getDefinition();
		if (defdecl==D) {
			functionStack.pop_back();
			if (functionStack.size()>0) {
				lastFunctionDef = functionStack.back();
			}
			else {
				lastFunctionDef = 0;
			}
		}
	}
	else{
		lastFunctionDef = lastFunctionDefCache;
		lastFunctionDefCache = 0;
	}
	return true;
}

// Functions - c++ only
bool DbJSONClassVisitor::VisitCXXMethodDecl(const CXXMethodDecl* D) {
	//llvm::outs() << "@DbJSONClassVisitor::VisitCXXMethodDecl(" << D << ")\n";
	return true;
}

bool DbJSONClassVisitor::VisitFriendDecl(const FriendDecl *D) {
	if (!D->getFriendType()) {
		if (friendDeclMap.find(D->getFriendDecl())==friendDeclMap.end()) {
			friendDeclMap.insert(std::pair<const void*,const FriendDecl*>(D->getFriendDecl(),D));
		}
	}
	else {
		if (friendDeclMap.find(D->getFriendType())==friendDeclMap.end()) {
			friendDeclMap.insert(std::pair<const void*,const FriendDecl*>(D->getFriendType(),D));
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitFunctionTemplateDecl(const FunctionTemplateDecl *D) {

	const FunctionDecl* FD = D->getTemplatedDecl();

	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitFunctionTemplateDecl(" << D << "," << FD << ")\n"; D->dump(llvm::outs()) );

	for (auto i = D->spec_begin(); i!=D->spec_end(); ++i) {
		const FunctionDecl* sFD = static_cast<FunctionDecl*>(*i);
		if (functionTemplateMap.find(sFD)==functionTemplateMap.end()) {
			  functionTemplateMap.insert(std::pair<const FunctionDecl*,const FunctionTemplateDecl*>(sFD,D));
		  }
	}

	if (!FD) return true;

	if (_opts.debug) {
		std::string funcName = D->getName().str();
		std::string _class;
		if (D->isCXXClassMember()) {
			const CXXMethodDecl* MD = static_cast<const CXXMethodDecl*>(FD);
			const CXXRecordDecl* RD = MD->getParent();
			QualType RT = Context.getRecordType(RD);
			_class = RT.getAsString();
		}
		llvm::outs() << "notice FunctionTemplate: " << funcName << " [" << _class << "] " << FD << "\n";
	}

	if (functionTemplateMap.find(FD)==functionTemplateMap.end()) {
		functionTemplateMap.insert(std::pair<const FunctionDecl*,const FunctionTemplateDecl*>(FD,D));
	}
	return true;
}


// Stmt visitors
bool DbJSONClassVisitor::TraverseStmt(Stmt *S) {

	bool TraverseResult = RecursiveASTVisitor<DbJSONClassVisitor>::TraverseStmt(S);
	if (S && (S->getStmtClass()==Stmt::CompoundStmtClass)) {
		if (!VisitCompoundStmtComplete(static_cast<CompoundStmt*>(S))) return false;
	}
	return TraverseResult;
}

// Track compound statements
bool DbJSONClassVisitor::VisitStmt(Stmt *Node) {

	if (( 	(Node->getStmtClass()==Stmt::NullStmtClass)||
			(Node->getStmtClass()==Stmt::IfStmtClass)||
			(Node->getStmtClass()==Stmt::SwitchStmtClass)||
			(Node->getStmtClass()==Stmt::WhileStmtClass)||
			(Node->getStmtClass()==Stmt::DoStmtClass)||
			(Node->getStmtClass()==Stmt::ForStmtClass)||
			(Node->getStmtClass()==Stmt::IndirectGotoStmtClass)||
			(Node->getStmtClass()==Stmt::ReturnStmtClass)||
			(Node->getStmtClass()==Stmt::GCCAsmStmtClass)||
			(Node->getStmtClass()==Stmt::MSAsmStmtClass)||
			(Node->getStmtClass()==Stmt::GotoStmtClass)||
			(Node->getStmtClass()==Stmt::BinaryOperatorClass)||
			(Node->getStmtClass()==Stmt::BinaryConditionalOperatorClass)||
			(Node->getStmtClass()==Stmt::ConditionalOperatorClass)||
			(Node->getStmtClass()==Stmt::CallExprClass)
		) && (inVarDecl.size()==0)) {
		/* Get the location of first non-decl statement in the function body */
			if (lastFunctionDef) {
				if (lastFunctionDef->firstNonDeclStmtLoc=="") {
					SourceLocation sloc = Node->getSourceRange().getBegin();
					if (!sloc.isMacroID()) {
						lastFunctionDef->firstNonDeclStmtLoc = sloc.printToString(Context.getSourceManager());
					}
				}
			}
		}

	if (Node->getStmtClass()==Stmt::CompoundStmtClass) {
		CompoundStmt* cs = static_cast<CompoundStmt*>(Node);
		return VisitCompoundStmtStart(cs);
	}
	return true;
}

bool DbJSONClassVisitor::VisitCompoundStmtStart(const CompoundStmt *CS) {
	if (_opts.cstmt) {
		const CompoundStmt* parentCS = 0;
		if (csStack.size()>0) {
			parentCS = csStack.back();
		}
		csStack.push_back(CS);
		if (lastFunctionDef) {
			++lastFunctionDef->CSId;
			lastFunctionDef->csIdMap.insert(std::pair<const CompoundStmt*,long>(CS,lastFunctionDef->CSId));
			lastFunctionDef->csParentMap.insert(std::pair<const CompoundStmt*,const CompoundStmt*>(CS,parentCS));
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitCompoundStmtComplete(const CompoundStmt *CS) {
	if (_opts.cstmt) {
		assert(csStack.back()==CS);
		csStack.pop_back();
	}
	return true;
}

void DbJSONClassVisitor::handleConditionDeref(Expr *Cond,size_t cf_id){
	if(!Cond) return;
	QualType CT = Cond->getType();
	VarRef_t VR;
	VR.VDCAMUAS.setCond(Cond,cf_id);
	std::vector<VarRef_t> vVR;
	DbJSONClassVisitor::DREMap_t DREMap;

	std::string Expr;
	llvm::raw_string_ostream exprstream(Expr);
	exprstream << "[" << Cond->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
	Cond->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
	exprstream.flush();

	std::vector<CStyleCastOrType> castVec;
	const class Expr* E = stripCastsEx(Cond,castVec);
	bool isAddress = false;
	/* We might have address constant if we cast the value to pointer type or the return type is a pointer */
	if (castVec.size()>0) {
		if (castVec.front().getFinalType()->getTypeClass()==Type::Pointer) {
			isAddress = true;
		}
	  }
	if (!isAddress) {
		if (CT->getTypeClass()==Type::Pointer) {
			isAddress = true;
		}
	}
	// Check if there's implicit (or explicit) cast to the return value
	QualType castType;
	if (Cond->getStmtClass()==Stmt::ImplicitCastExprClass) {
		const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(Cond);
		castType = ICE->getSubExpr()->getType();
	}
	else if (Cond->getStmtClass()==Stmt::CStyleCastExprClass) {
		const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(Cond);
		castType = CSCE->getType();
	}
	if (!castType.isNull()) {
		noticeTypeClass(castType);
	  }

	 // Check if return expression can be evaluated as a constant expression
	Expr::EvalResult Res;
	if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)) {
		int64_t i = Res.Val.getInt().extOrTrunc(64).getExtValue();
		ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS v;
		CStyleCastOrType valuecast;
		if (!castType.isNull()) {
			valuecast.setType(castType);
		  }
		if (isAddress) {
			  v.setAddress(i,valuecast);
		  }
		  else {
			  v.setInteger(i,valuecast);
		  }
		vMCtuple_t vMCtuple;
		v.setPrimaryFlag(false);
		DREMap_add(DREMap,v,vMCtuple);
	}
	else {
		lookup_cache_t cache;
		bool compundStmtSeen = false;
		unsigned MECnt = 0;
		lookForDeclRefWithMemberExprsInternal(E,E,DREMap,cache,&compundStmtSeen,0,&MECnt,0,true,false,false,castType);
	}

	for (DbJSONClassVisitor::DREMap_t::iterator i = DREMap.begin(); i!=DREMap.end(); ++i) {
		VarRef_t iVR;
		iVR.VDCAMUAS = (*i).first;
		vVR.push_back(iVR);
	}

	std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
			lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,cf_id,vVR,Expr,getCurrentCSPtr(),DereferenceCond));
	const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
}

bool DbJSONClassVisitor::VisitSwitchStmt(SwitchStmt *S){
	// add compound statement if not present(should never happen)
	if(S->getBody()->getStmtClass() != Stmt::CompoundStmtClass){
		CompoundStmt *CS = CompoundStmt::CreateEmpty(Context,1);
		CS->body_begin()[0] = S->getBody();
		S->setBody(CS);
	}

	// add control flow info
	CompoundStmt *CS = static_cast<CompoundStmt*>(S->getBody());
	size_t cf_id = lastFunctionDef->cfData.size();
	lastFunctionDef->cfData.push_back({cf_switch,CS});
	lastFunctionDef->csInfoMap.insert({CS,cf_id});

	// add condition deref
	handleConditionDeref(S->getCond(),cf_id);

	if (_opts.switchopt) {
		const Expr* cond = S->getCond();
		std::vector<std::pair<DbJSONClassVisitor::caseinfo_t,DbJSONClassVisitor::caseinfo_t>> caselst;
		const SwitchCase* cse = S->getSwitchCaseList();
		while (cse) {
			if (cse->getStmtClass()==Stmt::CaseStmtClass) {
				const CaseStmt* ccse = static_cast<const CaseStmt*>(cse);
				const Expr* LHS = ccse->getLHS();
				int64_t enumvLHS = 0;
				std::string enumstrLHS;
				std::string macroValueLHS;
				std::string raw_codeLHS;
				int64_t exprValLHS = 0;
				if (LHS) {
					setSwitchData(LHS,&enumvLHS,&enumstrLHS,&macroValueLHS,&raw_codeLHS,&exprValLHS);
				}
				const Expr* RHS = ccse->getRHS();
				int64_t enumvRHS = 0;
				std::string enumstrRHS;
				std::string macroValueRHS;
				std::string raw_codeRHS;
				int64_t exprValRHS = 0;
				if (RHS) {
					setSwitchData(RHS,&enumvRHS,&enumstrRHS,&macroValueRHS,&raw_codeRHS,&exprValRHS);
				}
				caselst.push_back(
					std::pair<DbJSONClassVisitor::caseinfo_t,DbJSONClassVisitor::caseinfo_t>(
						DbJSONClassVisitor::caseinfo_t(DbJSONClassVisitor::caseenum_t(enumvLHS,enumstrLHS),macroValueLHS,raw_codeLHS,exprValLHS),
						DbJSONClassVisitor::caseinfo_t(DbJSONClassVisitor::caseenum_t(enumvRHS,enumstrRHS),macroValueRHS,raw_codeRHS,exprValRHS)
					)
				);
			}
			else if (cse->getStmtClass()==Stmt::DefaultStmtClass) {
				const DefaultStmt* dsce = static_cast<const DefaultStmt*>(cse);
			}
			cse = cse->getNextSwitchCase();
		}
		if (caselst.size()>0) {
			/* We might have switch statement in implicit (e.g. operator()) function */
			if (lastFunctionDef) {
				lastFunctionDef->switch_map.insert(std::pair<const Expr*,std::vector<std::pair<DbJSONClassVisitor::caseinfo_t,
						DbJSONClassVisitor::caseinfo_t>>>(cond,caselst));
			}
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitIfStmt(IfStmt *S){
	if (!lastFunctionDef) return true;

	// add compound statement if not present
	if(S->getThen()->getStmtClass() != Stmt::CompoundStmtClass){
		CompoundStmt *CS = CompoundStmt::CreateEmpty(Context,1);
		CS->body_begin()[0] = S->getThen();
		S->setThen(CS);
	}
	if(S->getElse() && S->getElse()->getStmtClass() != Stmt::CompoundStmtClass){
		CompoundStmt *CS = CompoundStmt::CreateEmpty(Context,1);
		CS->body_begin()[0] = S->getElse();
		S->setElse(CS);
	}
	
	// add control flow info
	CompoundStmt *CS = static_cast<CompoundStmt*>(S->getThen());
	size_t cf_id = lastFunctionDef->cfData.size();
	lastFunctionDef->cfData.push_back({cf_if,CS});
	lastFunctionDef->csInfoMap.insert({CS,cf_id});
	handleConditionDeref(S->getCond(),cf_id);

	if(S->getElse()){
		CS = static_cast<CompoundStmt*>(S->getElse());
		cf_id = lastFunctionDef->cfData.size();
		lastFunctionDef->cfData.push_back({cf_else,CS});
		lastFunctionDef->csInfoMap.insert({CS,cf_id});
		handleConditionDeref(S->getCond(),cf_id);
	}
	struct DbJSONClassVisitor::IfInfo_t ii = {0,0,0};
	if (currentWithinCS()) {
		ii.CSPtr = getCurrentCSPtr();
		if (hasParentCS()) {
			ii.parentCSPtr = getParentCSPtr();
		}
	}
	ii.ifstmt = S;
	if (_opts.debug3) {
		llvm::outs() << "IfStmt: " << "" << " (" << lastFunctionDef->this_func->getName().str() << ")["
				<< lastFunctionDef->csIdMap[ii.CSPtr] << "] " << S->getCond()->getExprLoc().printToString(Context.getSourceManager()) << "\n";
	}
	lastFunctionDef->ifMap.insert(std::pair<const IfStmt*,IfInfo_t>(S,ii));
	return true;
}

bool DbJSONClassVisitor::VisitForStmt(ForStmt *S){
	if(!lastFunctionDef) return true;

	// add compound statement if not present
	if(S->getBody()->getStmtClass() != Stmt::CompoundStmtClass){
		CompoundStmt *CS = CompoundStmt::CreateEmpty(Context,1);
		CS->body_begin()[0] = S->getBody();
		S->setBody(CS);
	}

	// add control flow info
	CompoundStmt *CS = static_cast<CompoundStmt*>(S->getBody());
	size_t cf_id = lastFunctionDef->cfData.size();
	lastFunctionDef->cfData.push_back({cf_for,CS});
	lastFunctionDef->csInfoMap.insert({CS,cf_id});

	// add condition deref
	handleConditionDeref(S->getCond(),cf_id);
	return true;
}

bool DbJSONClassVisitor::VisitWhileStmt(WhileStmt *S){
	if(!lastFunctionDef) return true;

	// add compound statement if not present
	if(S->getBody()->getStmtClass() != Stmt::CompoundStmtClass){
		CompoundStmt *CS = CompoundStmt::CreateEmpty(Context,1);
		CS->body_begin()[0] = S->getBody();
		S->setBody(CS);
	}

	// add control flow info
	CompoundStmt *CS = static_cast<CompoundStmt*>(S->getBody());
	size_t cf_id = lastFunctionDef->cfData.size();
	lastFunctionDef->cfData.push_back({cf_while,CS});
	lastFunctionDef->csInfoMap.insert({CS,cf_id});

	// add condition deref
	handleConditionDeref(S->getCond(),cf_id);
	return true;
}

bool DbJSONClassVisitor::VisitDoStmt(DoStmt *S){
	if(!lastFunctionDef) return true;

	// add compound statement if not present
	if(S->getBody()->getStmtClass() != Stmt::CompoundStmtClass){
		CompoundStmt *CS = CompoundStmt::CreateEmpty(Context,1);
		CS->body_begin()[0] = S->getBody();
		S->setBody(CS);
	}

	// add control flow info
	CompoundStmt *CS = static_cast<CompoundStmt*>(S->getBody());
	size_t cf_id = lastFunctionDef->cfData.size();
	lastFunctionDef->cfData.push_back({cf_do,CS});
	lastFunctionDef->csInfoMap.insert({CS,cf_id});

	// add condition deref
	handleConditionDeref(S->getCond(),cf_id);
	return true;
}

bool DbJSONClassVisitor::VisitGCCAsmStmt(GCCAsmStmt *S){
	if (lastFunctionDef) {
		struct DbJSONClassVisitor::GCCAsmInfo_t ai = {0,0,0};
		if (currentWithinCS()) {
			ai.CSPtr = getCurrentCSPtr();
			if (hasParentCS()) {
				ai.parentCSPtr = getParentCSPtr();
			}
		}
		ai.asmstmt = S;
		if (_opts.debug3) {
			llvm::outs() << "GCCAsmStmt: " << "" << " (" << lastFunctionDef->this_func->getName().str() << ")["
					<< lastFunctionDef->csIdMap[ai.CSPtr] << "] " << S->getAsmString()->getBytes().str() << "\n";
		}
		lastFunctionDef->asmMap.insert(std::pair<const GCCAsmStmt*,GCCAsmInfo_t>(S,ai));
	}
	return true;
}


bool DbJSONClassVisitor::VisitExpr(const Expr *Node) {
	if (_opts.debugME) {
		llvm::outs() << "@VisitExpr()\n";
		Node->dumpColor();
	}
	return true;
}

bool DbJSONClassVisitor::VisitCallExpr(CallExpr *CE){
	if (_opts.call) {
		int __callserved = 0;
		const Expr* callee = CE->getCallee();
		std::set<ValueHolder> callrefs;
		std::set<DbJSONClassVisitor::LiteralHolder> literalRefs;
		for (unsigned i=0; i!=CE->getNumArgs(); ++i) {
			lookForDeclRefExprs(CE->getArg(i),callrefs,i);
			lookForLiteral(CE->getArg(i),literalRefs,i);
		}
		if (callee->getStmtClass()==Stmt::ImplicitCastExprClass) {
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(callee);
			const Expr* E = lookForDeclReforMemberExpr(static_cast<const Expr*>(ICE));
			if (E && (handleCallDeclRefOrMemberExpr(E,callrefs,literalRefs,0,CE))) __callserved++;
			else if (!E) {
				const Expr* E = lookForStmtExpr(static_cast<const Expr*>(ICE));
				if (E && (handleCallStmtExpr(E,callrefs,literalRefs,0,CE))) __callserved++;
			}
		}
		else if (callee->getStmtClass()==Stmt::ParenExprClass) {
			const ParenExpr* PE = static_cast<const ParenExpr*>(callee);
			QualType baseType = PE->getType();
			const Expr* SE = PE->getSubExpr();
			if (SE->getStmtClass()==Stmt::ConditionalOperatorClass) {
				const ConditionalOperator* CO = static_cast<const ConditionalOperator*>(SE);
				if (handleCallConditionalOperator(CO,callrefs,literalRefs,&baseType,CE)) __callserved++;
			}
			else if (SE->getStmtClass()==Stmt::CStyleCastExprClass) {
				const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(SE);
				const Expr* E = lookForDeclReforMemberExpr(static_cast<const Expr*>(SE));
				if (E && (handleCallDeclRefOrMemberExpr(E,callrefs,literalRefs,&baseType,CE))) __callserved++;
				else if (!E) {
					const Expr* E = CSCE->getSubExpr();
					Expr::EvalResult Res;
					if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)){
						if (handleCallAddress(Res.Val.getInt().extOrTrunc(64).getExtValue(),E,callrefs,literalRefs,&baseType,CE,CSCE)) __callserved++;
					}
				}
			}
		}
		if (!__callserved) {
			DBG(_opts.debug3, llvm::outs() << "CALL not served: " << CE << "\n" );
			if (_opts.debug3) CE->dumpColor();
			MissingCallExpr.insert(CE);

		}
		else {
			const Decl* D = CE->getCalleeDecl();
			QualType rFT;
			if (!D) {
				rFT = walkToFunctionProtoType(CE->getCallee()->getType());
			}
			else {
				if (D->getKind()==Decl::Function) {
					const FunctionDecl* FD = static_cast<const FunctionDecl*>(D);
					rFT = walkToFunctionProtoType(FD->getType());
				}
				else if (D->getKind()==Decl::Var) {
					const VarDecl* VD = static_cast<const VarDecl*>(D);
					rFT = walkToFunctionProtoType(VD->getType());
				}
				else if (D->getKind()==Decl::ParmVar) {
					const ParmVarDecl* PVD = static_cast<const ParmVarDecl*>(D);
					rFT = walkToFunctionProtoType(PVD->getType());
				}
				else if (D->getKind()==Decl::Field) {
					const FieldDecl* FD = static_cast<const FieldDecl*>(D);
					rFT = walkToFunctionProtoType(FD->getType());
				}
				else {
					llvm::outs() << "WARNING: Missing implementation for DeclKind in CallExpr callback: [" << D->getDeclKindName() << ":" << D->getKind() << "]\n";
					D->dumpColor();
				}
			}
			if (lastFunctionDef) {
				/* Put function call argument expressions into derefs list */
				const FunctionProtoType *tp = 0;
				if (!rFT.isNull()) {
					tp = cast<FunctionProtoType>(rFT);
				}
				else {
					/* We might have builtin call without function prototype */
				}
				for (unsigned i=0; i!=CE->getNumArgs(); ++i) {
					QualType argT;
					const Expr* argE = CE->getArg(i);
					VarRef_t VR;
					VR.VDCAMUAS.setParm(argE);
					std::vector<VarRef_t> vVR;
					DbJSONClassVisitor::DREMap_t DREMap;
					std::string Expr;
					llvm::raw_string_ostream exprstream(Expr);
					exprstream << "[" << argE->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
					StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
					P.Visit(const_cast<class Expr*>((argE)));
					exprstream.flush();

					std::vector<CStyleCastOrType> castVec;
					const class Expr* E = stripCastsEx(argE,castVec);
					bool isAddress = false;
					/* We might have address constant if we cast the value to pointer type or the argument type is a pointer */
					if (castVec.size()>0) {
						if (castVec.front().getFinalType()->getTypeClass()==Type::Pointer) {
							isAddress = true;
						}
						}
					if (tp && (i<tp->getNumParams())) {
						argT = tp->getParamType(i);
						if (!isAddress) {
							if (argT->getTypeClass()==Type::Pointer) {
								isAddress = true;
							}
						}
					}
					// Check if there's implicit (or explicit) cast to the argument
					QualType castType;
					if (argE->getStmtClass()==Stmt::ImplicitCastExprClass) {
						const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(argE);
						castType = ICE->getSubExpr()->getType();
					}
					else if (argE->getStmtClass()==Stmt::CStyleCastExprClass) {
						const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(argE);
						castType = CSCE->getType();
					}
					if (!castType.isNull()) {
						noticeTypeClass(castType);
						}
					// Check if function argument can be evaluated as a constant expression
					Expr::EvalResult Res;
					if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)) {
						int64_t i = Res.Val.getInt().extOrTrunc(64).getExtValue();
						ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS v;
						CStyleCastOrType valuecast;
						if (!castType.isNull()) {
							valuecast.setType(castType);
							}
						if (isAddress) {
								v.setAddress(i,valuecast);
							}
							else {
								v.setInteger(i,valuecast);
							}
						vMCtuple_t vMCtuple;
						v.setPrimaryFlag(false);
						DREMap_add(DREMap,v,vMCtuple);
					}
					else {
						lookup_cache_t cache;
						bool compundStmtSeen = false;
						unsigned MECnt = 0;
						lookForDeclRefWithMemberExprsInternal(E,E,DREMap,cache,&compundStmtSeen,0,&MECnt,0,true,false,false,castType);
					}
					for (DbJSONClassVisitor::DREMap_t::iterator u = DREMap.begin(); u!=DREMap.end(); ++u) {
						VarRef_t iVR;
						iVR.VDCAMUAS = (*u).first;
						vVR.push_back(iVR);
					}
					std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
							lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,i,vVR,Expr,getCurrentCSPtr(),DereferenceParm));
					const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);					
				}
			}
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitDeclRefExpr(DeclRefExpr *DRE){
	if(lastFunctionDef || !lastGlobalVarDecl.empty() || !recordDeclStack.empty()){
		if(DRE->getDecl()->getKind() == Decl::Kind::Var){
			const VarDecl *VD = static_cast<const VarDecl*>(DRE->getDecl());
			if(VD->isDefinedOutsideFunctionOrMethod()&&!VD->isStaticDataMember()){
				if (lastFunctionDef) {
					lastFunctionDef->refVars.insert(getVarData(VD).id);
					lastFunctionDef->refVarInfo.insert(std::make_pair(getVarData(VD).id, DRE));
				}
				if ((!lastGlobalVarDecl.empty())&&(lastGlobalVarDecl!=VD->getNameAsString())) {
					getVarData(lastGlobalVarDecl).g_refVars.insert(getVarData(VD).id);
				}
				if (!recordDeclStack.empty()) {
					gtp_refVars[recordDeclStack.top()].insert(getVarData(VD).id);
				}
			}
		}
		else if(DRE->getDecl()->getKind() == Decl::Kind::EnumConstant){
			const EnumConstantDecl *ECD = static_cast<const EnumConstantDecl*>(DRE->getDecl());
			const EnumDecl *ED = static_cast<const EnumDecl*>(ECD->getDeclContext());
			QualType ET = Context.getEnumType(ED);
			noticeTypeClass(ET);
			if (lastFunctionDef) {
				lastFunctionDef->refTypes.insert(ET);
			}
			if (!lastGlobalVarDecl.empty()) {
				getVarData(lastGlobalVarDecl).g_refTypes.insert(ET);
			}
		}
		else if(DRE->getDecl()->getKind() == Decl::Kind::Function){
			const FunctionDecl *FD = static_cast<const FunctionDecl*>(DRE->getDecl());
			if (lastFunctionDef) {
				lastFunctionDef->refFuncs.insert(FD);
			}
			if (!lastGlobalVarDecl.empty()) {
				getVarData(lastGlobalVarDecl).g_refFuncs.insert(FD);
			}
			if (!recordDeclStack.empty()) {
				gtp_refFuncs[recordDeclStack.top()].insert(FD);
			}
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *UTTE){
	if(lastFunctionDef || !lastGlobalVarDecl.empty()){
		if (UTTE->isArgumentType()) {
			QualType UT = UTTE->getArgumentType();
			noticeTypeClass(UT);
			if (lastFunctionDef) {
				lastFunctionDef->refTypes.insert(UT);
			}
			if (!lastGlobalVarDecl.empty()) {
				getVarData(lastGlobalVarDecl).g_refTypes.insert(UT);
			}
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitCompoundLiteralExpr(CompoundLiteralExpr *CLE){
	if(lastFunctionDef || !lastGlobalVarDecl.empty()) {
		QualType T = CLE->getType();
		noticeTypeClass(T);
		if (lastFunctionDef) {
			lastFunctionDef->refTypes.insert(T);
		}
		if (!lastGlobalVarDecl.empty()) {
			getVarData(lastGlobalVarDecl).g_refTypes.insert(T);
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitCastExpr(const CastExpr *Node) {
	if (isa<CStyleCastExpr>(Node)) {
		if (lastFunctionDef) {
			noticeTypeClass(Node->getType());
			lastFunctionDef->refTypes.insert(Node->getType());
		}
		if (!lastGlobalVarDecl.empty()) {
			noticeTypeClass(Node->getType());
			getVarData(lastGlobalVarDecl).g_refTypes.insert(Node->getType());
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitBinaryOperator(BinaryOperator *BO) {

	// logic operators (includes bitwise operators)
	if(BO->getOpcode() >= BO_Cmp && BO->getOpcode() <= BO_LOr){
		if (_opts.debugME) {
			llvm::outs() << "@VisitBinaryOperator()\n";
			BO->dumpColor();
		}

		if (!lastFunctionDef) return true; // TODO: When derefs for globals are implemented handle this

		// Ignore if evaluates to constant value
		Expr::EvalResult Res;
		if((!BO->isValueDependent()) && BO->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(BO,Res)) {
			return true;
		}

		const Expr* LHS = BO->getLHS();
		const Expr* RHS = BO->getRHS();

		DbJSONClassVisitor::DREMap_t LDREMap;
		std::vector<CStyleCastOrType> castVec;
		const Expr* E = stripCastsEx(LHS,castVec);
		if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)) {
			int64_t i = Res.Val.getInt().extOrTrunc(64).getExtValue();
			ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS v;
			CStyleCastOrType valuecast;
			v.setInteger(i,valuecast);
			vMCtuple_t vMCtuple;
			v.setPrimaryFlag(false);
			DREMap_add(LDREMap,v,vMCtuple);
		}
		else {
			lookup_cache_t cache;
			bool compundStmtSeen = false;
			unsigned MECnt = 0;
			lookForDeclRefWithMemberExprsInternal(LHS,LHS,LDREMap,cache,&compundStmtSeen,0,&MECnt,0,true,true);
		}

		DbJSONClassVisitor::DREMap_t RDREMap;
		castVec.clear();
		E = stripCastsEx(RHS,castVec);
		if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)) {
			int64_t i = Res.Val.getInt().extOrTrunc(64).getExtValue();
			ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS v;
			CStyleCastOrType valuecast;
			v.setInteger(i,valuecast);
			vMCtuple_t vMCtuple;
			v.setPrimaryFlag(false);
			DREMap_add(RDREMap,v,vMCtuple);
		}
		else {
			lookup_cache_t cache;
			bool compundStmtSeen = false;
			unsigned MECnt = 0;
			lookForDeclRefWithMemberExprsInternal(RHS,RHS,RDREMap,cache,&compundStmtSeen,0,&MECnt,0,true,true);
		}

		std::string Expr;
		llvm::raw_string_ostream exprstream(Expr);
		exprstream << "[" << BO->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
		BO->printPretty(exprstream,nullptr,Context.getPrintingPolicy());
		exprstream.flush();

		VarRef_t VR;
		VR.VDCAMUAS.setLogic(BO);
		std::vector<VarRef_t> vVR;
		unsigned Lsize = LDREMap.size();

		for (DbJSONClassVisitor::DREMap_t::iterator i = LDREMap.begin(); i!=LDREMap.end(); ++i) {
			VarRef_t iVR;
			iVR.VDCAMUAS = (*i).first;
			vVR.push_back(iVR);
		}
		for (DbJSONClassVisitor::DREMap_t::iterator i = RDREMap.begin(); i!=RDREMap.end(); ++i) {
			VarRef_t iVR;
			iVR.VDCAMUAS = (*i).first;
			vVR.push_back(iVR);
		}

		std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
				lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,BO->getOpcode(),Lsize,vVR,Expr,getCurrentCSPtr(),DereferenceLogic));
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
	}

	// assignment operators
	if(BO->getOpcode() >= BO_Assign && BO->getOpcode() <= BO_OrAssign){

		if (_opts.debugME) {
			llvm::outs() << "@VisitBinaryOperator()\n";
			BO->dumpColor();
		}

		if (!lastFunctionDef) return true; // TODO: When derefs for globals are implemented handle this

		const Expr* LHS = BO->getLHS();
		const Expr* RHS = BO->getRHS();

		DbJSONClassVisitor::DREMap_t LDREMap;
		lookup_cache_t cache;
		bool compundStmtSeen = false;
		unsigned MECnt = 0;
		lookForDeclRefWithMemberExprsInternal(LHS,LHS,LDREMap,cache,&compundStmtSeen,0,&MECnt,0,true,true);
		
		DbJSONClassVisitor::DREMap_t RDREMap;
		std::vector<CStyleCastOrType> castVec;
		const Expr* E = stripCastsEx(RHS,castVec);
		bool isAddress = false;
		if (castVec.size()>0) {
		if (castVec.front().getFinalType()->getTypeClass()==Type::Pointer) {
			isAddress = true;
		}
		}
		// Check if there's implicit (or explicit) cast from the initializer
		// Handle void* as a special case and allow casting in opposite way to the initializer
		QualType castType;
		if (getFirstCast(castVec)) {
		castType = getFirstCast(castVec)->getType();
		}
		else {
			if (RHS->getStmtClass()==Stmt::ImplicitCastExprClass) {
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(RHS);
			if (ICE->getSubExpr()->getType()!=LHS->getType()) {
				if (isPtrToVoid(LHS->getType())) {
					castType = ICE->getSubExpr()->getType();
				}
				else {
					if (E->getStmtClass()!=Stmt::StringLiteralClass) {
						castType = LHS->getType();
					}
				}
			}
		}
		}
		if (!castType.isNull()) {
		noticeTypeClass(castType);
		}
		// Check if init can be evaluated as a constant expression
		Expr::EvalResult Res;
		if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)) {
			int64_t i = Res.Val.getInt().extOrTrunc(64).getExtValue();
			ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS v;
			CStyleCastOrType valuecast;
			if (!castType.isNull()) {
			valuecast.setType(castType);
			}
			if (isAddress) {
				v.setAddress(i,valuecast);
			}
			else {
				v.setInteger(i,valuecast);
			}
			vMCtuple_t vMCtuple;
			v.setPrimaryFlag(false);
			DREMap_add(RDREMap,v,vMCtuple);
		}
		else {
			lookup_cache_clear(cache);
			compundStmtSeen = false;
			MECnt = 0;
			lookForDeclRefWithMemberExprsInternal(RHS,RHS,RDREMap,cache,&compundStmtSeen,0,&MECnt,0,true,false,false,castType);
		}

		std::string Expr;
		llvm::raw_string_ostream exprstream(Expr);
		exprstream << "[" << BO->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
		StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
		P.Visit(const_cast<class BinaryOperator*>((BO)));
		exprstream.flush();

		VarRef_t VR;
		VR.VDCAMUAS.setCAO(BO);
		std::vector<VarRef_t> vVR;
		unsigned Lsize = LDREMap.size();
		if (Lsize>1) {
		llvm::errs() << "Multiple expressions on LHS for assignment operator: " << Lsize << "\n";
		BO->dumpColor();
		assert(0);
		}
		for (DbJSONClassVisitor::DREMap_t::iterator i = LDREMap.begin(); i!=LDREMap.end(); ++i) {
			VarRef_t iVR;
			iVR.VDCAMUAS = (*i).first;
			CStyleCastOrType valuecast;
			valuecast.setType(LHS->getType());
			noticeTypeClass(LHS->getType());
			iVR.VDCAMUAS.setCast(valuecast);
			vVR.push_back(iVR);
		}
		for (DbJSONClassVisitor::DREMap_t::iterator i = RDREMap.begin(); i!=RDREMap.end(); ++i) {
			VarRef_t iVR;
			iVR.VDCAMUAS = (*i).first;
			vVR.push_back(iVR);
		}

		std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
				lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,BO->getOpcode(),vVR,Expr,getCurrentCSPtr(),DereferenceAssign));
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
	}

	return true;
}

bool DbJSONClassVisitor::VisitUnaryOperator(UnaryOperator *E) {

	if (_opts.debugME) {
		llvm::outs() << "@VisitUnaryOperator()\n";
		E->dumpColor();
	}

	if (E->getOpcode()==UO_Deref) {
		if (_opts.debugDeref) {
			llvm::outs() << E->getBeginLoc().printToString(Context.getSourceManager()) << ": ";
			E->printPretty(llvm::outs(),0,Context.getPrintingPolicy());
			llvm::outs() << "\n";
		}
		int64_t i=0;
		std::vector<VarRef_t> vVR;
		bool ignoreErrors = false;
		bool done = lookForDerefExprs(E->getSubExpr(),&i,vVR);
		if (_opts.debugDeref) {
			llvm::outs() << done;
			if (done) {
				llvm::outs() << " [" << i << "]";
				llvm::outs() << " | " << vVR.size();
				llvm::outs() << " [";
				for (size_t i=0; i<vVR.size(); ++i) {
					VarRef_t vr = vVR[i];
					if (i!=0) llvm::outs() << ",";
					llvm::outs() << vr.MemberExprList.size();
				}
				llvm::outs() << "]";
			}
			else {
				if (!ignoreErrors) {
					E->dumpColor();
				}
			}
			llvm::outs() << "\n";
		}
		if (done) {
			VarRef_t VR;
			VR.VDCAMUAS.setUnary(E);
			if (lastFunctionDef) {
				std::string Expr;
				llvm::raw_string_ostream exprstream(Expr);
				exprstream << "[" << E->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
				StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
				P.Visit(const_cast<class UnaryOperator*>((E)));
				exprstream.flush();
				std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
						lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,i,vVR,Expr,getCurrentCSPtr(),DereferenceUnary));
				const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
			}
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitArraySubscriptExpr(ArraySubscriptExpr *Node) {

	if (_opts.debugDeref) {
		llvm::outs() << Node->getBeginLoc().printToString(Context.getSourceManager()) << ": ";
		Node->getBase()->printPretty(llvm::outs(),0,Context.getPrintingPolicy());
		llvm::outs() << "[";
		Node->getIdx()->printPretty(llvm::outs(),0,Context.getPrintingPolicy());
		llvm::outs() << "]\n";
	}

	if (_opts.debugME) {
		llvm::outs() << "@VisitArraySubscriptExpr()\n";
		Node->dumpColor();
	}

	// First handle index part of ArraySubscriptExpr
	const Expr* idxE = lookForNonTransitiveExpr(Node->getIdx());
	int64_t idx_i=0;
	std::vector<VarRef_t> idx_vVR;
	bool idx_done = true;
	// Check if expression can be evaluated as a constant expression
	Expr::EvalResult Res;
	if((!idxE->isValueDependent()) && idxE->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(idxE,Res)) {
		idx_i = Res.Val.getInt().extOrTrunc(64).getExtValue();
	}
	else {
		idx_done = lookForDerefExprs(idxE,&idx_i,idx_vVR);
	}

	// Now it's time for the base tree
	int64_t base_i=0;
	std::vector<VarRef_t> base_vVR;
	bool base_done = lookForDerefExprs(Node->getBase(),&base_i,base_vVR);
	unsigned base_size = 0;
	for (auto i=base_vVR.begin(); i!=base_vVR.end(); ++i) {
		VarRef_t& VR = *i;
		if (VR.VDCAMUAS.getKind()!=
				DbJSONClassVisitor::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS::ValueDeclOrCallExprOrAddressOrMEOrUnaryOrASKindNone) {
			base_size++;
		}
	}

	std::vector<VarRef_t> vVR(base_vVR.begin(),base_vVR.end());
	vVR.insert(vVR.end(),idx_vVR.begin(),idx_vVR.end());

	if (idx_done&&base_done) {
		VarRef_t VR;
		VR.VDCAMUAS.setAS(Node);
		if (lastFunctionDef) {
			std::string Expr;
			llvm::raw_string_ostream exprstream(Expr);
			exprstream << "[" << Node->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
			StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
			P.Visit(const_cast<class ArraySubscriptExpr*>((Node)));
			exprstream.flush();
			std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
					lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,idx_i,base_size,vVR,Expr,getCurrentCSPtr(),DereferenceArray));
			const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
		}
	}

	return true;
}

bool DbJSONClassVisitor::VisitOffsetOfExpr(OffsetOfExpr *Node) {

	if(lastFunctionDef || !lastGlobalVarDecl.empty()){
		QualType BT = Node->getTypeSourceInfo()->getType();
		noticeTypeClass(BT);
		if (lastFunctionDef) {
			lastFunctionDef->refTypes.insert(BT);
		}
		if (!lastGlobalVarDecl.empty()) {
			getVarData(lastGlobalVarDecl).g_refTypes.insert(BT);
		}
	}

	if (!lastFunctionDef) {
		return true;
	}

	VarRef_t VR;
	VR.VDCAMUAS.setOOE(Node);
	std::vector<VarRef_t> vVR;
	DbJSONClassVisitor::DREMap_t DREMap;
	bool canComputeOffset = true;
	uint64_t computedOffset = 0;

	unsigned MEIdx = 0;
	int fieldIndex = -1;
	QualType lastRecordType;
	QualType lastFieldType;
	unsigned fieldAccessCount = 0;
	for (unsigned i=0; i<Node->getNumComponents(); ++i) {
		const OffsetOfNode & C = Node->getComponent(i);
		OffsetOfNode::Kind k = C.getKind();
		if (k==OffsetOfNode::Field) {
			fieldAccessCount++;
		}
	}
	VR.VDCAMUAS.setMeCnt(fieldAccessCount-1);

	for (unsigned i=0; i<Node->getNumComponents(); ++i) {
		const OffsetOfNode & C = Node->getComponent(i);
		OffsetOfNode::Kind k = C.getKind();
		if (k==OffsetOfNode::Field) {
			FieldDecl* FD = C.getField();
			const RecordDecl* RD = FD->getParent();
            fieldIndex = fieldToIndex(FD,RD);
            int fieldFieldIndex = fieldToFieldIndex(FD,RD);
            if ((!RD->isDependentType())&&(fieldFieldIndex>=0)) {
				const ASTRecordLayout& rL = Context.getASTRecordLayout(RD);
				uint64_t off = rL.getFieldOffset(fieldFieldIndex);
				computedOffset+=off;
            }
            else {
            	canComputeOffset = false;
            }

            lastRecordType = RD->getASTContext().getRecordType(RD);
            lastFieldType = FD->getType();
            VR.MemberExprList.push_back(std::pair<size_t,MemberExprKind>(fieldIndex,static_cast<MemberExprKind>(0)));
            VR.MECastList.push_back(lastRecordType);
            noticeTypeClass(lastRecordType);
            MEIdx++;
		}
		else if (k==OffsetOfNode::Array) {
			unsigned idx = C.getArrayExprIndex();
			const Expr * E = Node->getIndexExpr(idx);
            // Check if offset expression can be evaluated as a constant expression
            Expr::EvalResult Res;
            if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)) {
                int64_t i = Res.Val.getInt().extOrTrunc(64).getExtValue();

                QualType rRT = resolve_Record_Type(lastFieldType);
                if (!rRT.isNull()) {
                	const RecordType *tp = cast<RecordType>(rRT);
					RecordDecl* rD = tp->getDecl();
					if (!rD->isDependentType()) {
						TypeInfo ti = Context.getTypeInfo(rRT);
						computedOffset+=(i)*ti.Width;
					}
					else {
						canComputeOffset = false;
					}
                }
                else {
                	canComputeOffset = false;
                }

                ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS v;
                v.setInteger(i);
                vMCtuple_t vMCtuple;
                v.setPrimaryFlag(false);
                v.setMeIdx(MEIdx-1);
                DREMap_add(DREMap,v,vMCtuple);
            }
            else {
	            lookup_cache_t cache;
	            bool compundStmtSeen = false;
	            canComputeOffset = false;
	            unsigned MECnt = 0;
	            lookForDeclRefWithMemberExprsInternal(E,E,DREMap,cache,&compundStmtSeen,MEIdx-1,&MECnt,0,true,true);            	
            }
            VR.MemberExprList.push_back(std::pair<size_t,MemberExprKind>(fieldIndex,MemberExprKindInvalid));
            VR.MECastList.push_back(lastRecordType);
		}
		else {
			/* Not supported */
		}
	}

	int64_t offsetof_offset = -1;
	if (canComputeOffset) {
		offsetof_offset = computedOffset/8;
	}

	std::string Expr;
    llvm::raw_string_ostream exprstream(Expr);
    exprstream << "[" << Node->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
    StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
    P.Visit(const_cast<class OffsetOfExpr*>((Node)));
    exprstream.flush();

    for (DbJSONClassVisitor::DREMap_t::iterator i = DREMap.begin(); i!=DREMap.end(); ++i) {
        VarRef_t iVR;
        iVR.VDCAMUAS = (*i).first;
        vVR.push_back(iVR);
    }

    std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
    		lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,offsetof_offset,vVR,Expr,getCurrentCSPtr(),DereferenceOffsetOf));
    const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);

	return true;
}

bool DbJSONClassVisitor::VisitReturnStmt(const ReturnStmt *S) {

	if (!lastFunctionDef) {
		return true;
	}

	QualType RT = lastFunctionDef->this_func->getReturnType();

	const Expr* RVE = S->getRetValue();

	if (!RVE) {
		return true;
	}

	VarRef_t VR;
	VR.VDCAMUAS.setRET(S);
	std::vector<VarRef_t> vVR;
	DbJSONClassVisitor::DREMap_t DREMap;

	std::string Expr;
	llvm::raw_string_ostream exprstream(Expr);
	exprstream << "[" << S->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
	StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
	P.Visit(const_cast<class ReturnStmt*>((S)));
	exprstream.flush();

	std::vector<CStyleCastOrType> castVec;
	const class Expr* E = stripCastsEx(RVE,castVec);
	bool isAddress = false;
	/* We might have address constant if we cast the value to pointer type or the return type is a pointer */
	if (castVec.size()>0) {
		if (castVec.front().getFinalType()->getTypeClass()==Type::Pointer) {
			isAddress = true;
		}
	  }
	if (!isAddress) {
		if (RT->getTypeClass()==Type::Pointer) {
			isAddress = true;
		}
	}
	// Check if there's implicit (or explicit) cast to the return value
	QualType castType;
	if (RVE->getStmtClass()==Stmt::ImplicitCastExprClass) {
		const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(RVE);
		castType = ICE->getSubExpr()->getType();
	}
	else if (RVE->getStmtClass()==Stmt::CStyleCastExprClass) {
		const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(RVE);
		castType = CSCE->getType();
	}
	if (!castType.isNull()) {
		noticeTypeClass(castType);
	  }

	 // Check if return expression can be evaluated as a constant expression
	Expr::EvalResult Res;
	if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)) {
		int64_t i = Res.Val.getInt().extOrTrunc(64).getExtValue();
		ValueDeclOrCallExprOrAddressOrMEOrUnaryOrAS v;
		CStyleCastOrType valuecast;
		if (!castType.isNull()) {
			valuecast.setType(castType);
		  }
		if (isAddress) {
			  v.setAddress(i,valuecast);
		  }
		  else {
			  v.setInteger(i,valuecast);
		  }
		vMCtuple_t vMCtuple;
		v.setPrimaryFlag(false);
		DREMap_add(DREMap,v,vMCtuple);
	}
	else {
		lookup_cache_t cache;
		bool compundStmtSeen = false;
		unsigned MECnt = 0;
		lookForDeclRefWithMemberExprsInternal(E,E,DREMap,cache,&compundStmtSeen,0,&MECnt,0,true,false,false,castType);
	}

	for (DbJSONClassVisitor::DREMap_t::iterator i = DREMap.begin(); i!=DREMap.end(); ++i) {
		VarRef_t iVR;
		iVR.VDCAMUAS = (*i).first;
		vVR.push_back(iVR);
	}

	std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
			lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,0,vVR,Expr,getCurrentCSPtr(),DereferenceReturn));
	const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);

	return true;
}

bool DbJSONClassVisitor::VisitMemberExpr(MemberExpr *Node) {

	if(lastFunctionDef || !lastGlobalVarDecl.empty()){
		QualType T = Node->getBase()->getType();
		T=GetBaseType(T);
		noticeTypeClass(T);
		if (lastFunctionDef) {
			lastFunctionDef->refTypes.insert(T);
		}
		if (!lastGlobalVarDecl.empty()) {
			getVarData(lastGlobalVarDecl).g_refTypes.insert(T);
		}
	}

	if (DoneMEs.find(Node)!=DoneMEs.end()) {
		return true;
	}

	if (_opts.debugME) {
		llvm::outs() << "@VisitMemberExpr()\n";
		Node->dumpColor();
	}

	/* We have primary MemberExpr (i.e. MemberExpr that is not in the chain of other MemberExprs); handle it */

	DbJSONClassVisitor::DREMap_t DREMap;
	lookup_cache_t cache;
	bool CSseen = false;
	unsigned MECnt = 0;
	const CallExpr* CE = 0;
	if (MEHaveParentCE.find(Node)!=MEHaveParentCE.end()) {
		CE = MEHaveParentCE[Node];
	}
	lookForDeclRefWithMemberExprsInternal(Node,Node,DREMap,cache,&CSseen,0,&MECnt,CE);
	if (DREMap.size()==0) {
		llvm::outs() << "No DeclRefExpr found at the bottom of primary MemberExpr:\n";
		Node->dumpColor();
		std::string Expr;
		llvm::raw_string_ostream exprstream(Expr);
		StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
		P.Visit(const_cast<class MemberExpr*>((Node)));
		exprstream.flush();
		llvm::outs() << "Expr: \"" << Expr << "\"\n";
		if (lastFunctionDef) {
			llvm::outs() << "Function: " << lastFunctionDef->this_func->getName().str() << "\n";
		}
		assert(0);
	}

	/* Count number of entries in DREMap with maximum MEIdx referenced in primary chain */
	unsigned pcount = 0;
	unsigned maxMeIdx = 0;
	DbJSONClassVisitor::DREMap_t::iterator vi;
	for (DbJSONClassVisitor::DREMap_t::iterator i = DREMap.begin(); i!=DREMap.end(); ++i) {
		VarRef_t VR;
		VR.VDCAMUAS = (*i).first;
		if ((VR.VDCAMUAS.getMeIdx()>maxMeIdx)&&(VR.VDCAMUAS.isPrimary())) {
			maxMeIdx = VR.VDCAMUAS.getMeIdx();
			pcount = 1;
			vi = i;
		}
		else if ((VR.VDCAMUAS.getMeIdx()==maxMeIdx)&&(VR.VDCAMUAS.isPrimary())) {
			pcount += 1;
		}
	}
	/* Walking from Primary MemberExpr we expect to have only one variable in the primary chain
	 *  (unless there is a StmtExpr along the way) */

	if ((pcount>1)&&(!CSseen)) {
		llvm::outs() << "Multiple MemberExpr chains in primary MemberExpr:\n";
		Node->dumpColor();
		llvm::outs() << "Num chains: " << pcount << "\n";
		llvm::outs() << "MemberExpr index: " << maxMeIdx << "\n";
		std::string Expr;
		llvm::raw_string_ostream exprstream(Expr);
		StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
		P.Visit(const_cast<class MemberExpr*>((Node)));
		exprstream.flush();
		llvm::outs() << "Expr: \"" << Expr << "\"\n";
		if (lastFunctionDef) {
			llvm::outs() << "Function: " << lastFunctionDef->this_func->getName().str() << "\n";
		}
		llvm::outs() << "# Referenced variables:\n";
		for (DbJSONClassVisitor::DREMap_t::iterator i = DREMap.begin(); i!=DREMap.end(); ++i) {
			VarRef_t VR;
			VR.VDCAMUAS = (*i).first;
			if ((VR.VDCAMUAS.getMeIdx()==maxMeIdx)&&(VR.VDCAMUAS.isPrimary())) {
				llvm::outs() << "  Kind :" << VR.VDCAMUAS.getKindString() << "\n";
			}
		}
		assert(0);
	}

	VarRef_t VR;
	QualType METype = Node->getMemberDecl()->getType();
	VR.VDCAMUAS.setME(Node,METype);
	VR.VDCAMUAS.setMeCnt(MECnt);
	std::vector<VarRef_t> vVR;
	bool hasPtrME = false;

	/* Copy member expression information from the base variable of MemberExpr */
	for (std::vector<lookup_cache_tuple_t>::iterator j = vi->second.begin();
			j!=vi->second.end(); ++j) {
		DoneMEs.insert(std::get<0>(*j));
		const MemberExpr* ME = static_cast<const MemberExpr*>(std::get<0>(*j));
		const ValueDecl *VD = ME->getMemberDecl();
		if (VD->getKind()==Decl::Field) {
			const FieldDecl* FD = static_cast<const FieldDecl*>(VD);
			const RecordDecl* RD = FD->getParent();
			int fieldIndex = fieldToIndex(FD,RD);
			VR.MemberExprList.push_back(std::pair<size_t,MemberExprKind>(fieldIndex,static_cast<MemberExprKind>(ME->isArrow())));
		}
		VR.MECastList.push_back(std::get<1>(*j).getFinalType());
		VR.OffsetList.push_back(std::get<2>(*j));
		VR.MCallList.push_back(std::get<3>(*j));
		if (ME->isArrow()) hasPtrME=true;
	}

	/* Now save all the referenced variables */
	for (DbJSONClassVisitor::DREMap_t::iterator i = DREMap.begin(); i!=DREMap.end(); ++i) {
		VarRef_t iVR;
		iVR.VDCAMUAS = (*i).first;
		vVR.push_back(iVR);
	}

	if (lastFunctionDef&&(hasPtrME||!(_opts.ptrMEonly))) {
		std::string Expr;
		llvm::raw_string_ostream exprstream(Expr);
		if (CE) {
			exprstream << "[" << CE->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
			StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
			P.Visit(const_cast<class CallExpr*>((CE)));
		}
		else {
			exprstream << "[" << Node->getBeginLoc().printToString(Context.getSourceManager()) << "]: ";
			StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
			P.Visit(const_cast<class MemberExpr*>((Node)));
		}
		exprstream.flush();
		std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
				lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,0,vVR,Expr,getCurrentCSPtr(),DereferenceMember));
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
	}

	return true;
}

/*
bool DbJSONClassVisitor::VisitInitListExpr(InitListExpr* ILE) {

	return true;

	if (DoneILEs.find(ILE)!=DoneILEs.end()) {
		return true;
	}

	DbJSONClassVisitor::DREMap_t DREMap;
	DbJSONClassVisitor::lookup_cache_t cache;
	bool compundStmtSeen = false;
	unsigned MECnt = 0;
	lookForDeclRefWithMemberExprsInternal(ILE,ILE,DREMap,cache,&compundStmtSeen,0,&MECnt,0,true,true,true);

	std::string Expr;
	llvm::raw_string_ostream exprstream(Expr);
	StmtPrinter P(exprstream, nullptr, Context.getPrintingPolicy());
	P.Visit(const_cast<Expr*>((ILE)));
	exprstream.flush();

	return true;
}
*/
bool DbJSONClassVisitor::VisitIntegerLiteral(IntegerLiteral *L){
	LiteralHolder lh;
	lh.type = LiteralHolder::LiteralInteger;
	lh.prvLiteral.integerLiteral = llvm::APSInt(L->getValue(),L->getType()->isUnsignedIntegerOrEnumerationType());
	if(lastFunctionDef){
		lastFunctionDef->literals.insert(lh);
	}
	else if(!lastGlobalVarDecl.empty()){
		getVarData(lastGlobalVarDecl).g_literals.insert(lh);
	}
	else if(!recordDeclStack.empty()){
	}
	return true;
}
bool DbJSONClassVisitor::VisitFloatingLiteral(FloatingLiteral *L){
	LiteralHolder lh;
	lh.type = LiteralHolder::LiteralFloat;
	llvm::APFloat FV = L->getValue();
	if (&FV.getSemantics()==&llvm::APFloat::IEEEsingle()) {
		lh.prvLiteral.floatingLiteral = FV.convertToFloat();
	}
	else {
		lh.prvLiteral.floatingLiteral = FV.convertToDouble();
	}
	if(lastFunctionDef){
		lastFunctionDef->literals.insert(lh);
	}
	else if(!lastGlobalVarDecl.empty()){
		getVarData(lastGlobalVarDecl).g_literals.insert(lh);
	}
	else if(!recordDeclStack.empty()){
	}
	return true;
}
bool DbJSONClassVisitor::VisitCharacterLiteral(CharacterLiteral *L){
	LiteralHolder lh;
	lh.type = LiteralHolder::LiteralChar;
	lh.prvLiteral.charLiteral = L->getValue();
	if(lastFunctionDef){
		lastFunctionDef->literals.insert(lh);
	}
	else if(!lastGlobalVarDecl.empty()){
		getVarData(lastGlobalVarDecl).g_literals.insert(lh);
	}
	else if(!recordDeclStack.empty()){
	}
	return true;
}
bool DbJSONClassVisitor::VisitStringLiteral(StringLiteral *L){
	LiteralHolder lh;
	lh.type = LiteralHolder::LiteralString;
	lh.prvLiteral.stringLiteral = L->getBytes().str();
	lh.prvLiteral.stringLiteral.erase(std::remove(lh.prvLiteral.stringLiteral.begin(),lh.prvLiteral.stringLiteral.end(),'\x1f'),lh.prvLiteral.stringLiteral.end());
	if(lastFunctionDef){
		lastFunctionDef->literals.insert(lh);
	}
	else if(!lastGlobalVarDecl.empty()){
		getVarData(lastGlobalVarDecl).g_literals.insert(lh);
	}
	else if(!recordDeclStack.empty()){
	}
	return true;
}
