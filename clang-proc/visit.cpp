#include "main.hpp"
#include "dbjson.hpp"
#include "compat.h"
#include "clang/AST/RecordLayout.h"

// Decl visitors
bool DbJSONClassVisitor::TraverseDecl(Decl *D) {
	if(!D) return true;
	// llvm::outs()<<std::string(2*tab++,' ')<<D->getDeclKindName()<<D<<'\n';
	bool TraverseResult = RecursiveASTVisitor<DbJSONClassVisitor>::TraverseDecl(D);
	// llvm::outs()<<std::string(2*(--tab),' ')<<D->getDeclKindName()<<" done\n";
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
				DoneMEs.clear();
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
	fopsVarDecl.push_back(D);
	if (D->isDefinedOutsideFunctionOrMethod() && !D->isStaticDataMember()
			&& D->hasGlobalStorage() && !D->isStaticLocal()) {
		// We're are taking care of global variable
		lastGlobalVarDecl = D;
		assert(D->getNameAsString().size() && "unnamed variable");
	}
	inVarDecl.push_back(true);
	return true;
}

bool DbJSONClassVisitor::VisitVarDeclComplete(const VarDecl *D) {
	fopsVarDecl.pop_back();
	lastGlobalVarDecl = nullptr;
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

	if (opts.debugME) {
		llvm::outs() << "@VisitVarDecl()\n";
		D->dumpColor();
	}

	if ((D->getKind() == Decl::Kind::Var)&&(D->getInit())) {
		if(isa<InitListExpr>(D->getInit())){
			assert(cast<InitListExpr>(D->getInit())->isSemanticForm() && "syntactic init list");
		}
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
			int64_t i = Res.Val.getInt().extOrTrunc(63).getExtValue();
			ExprRef_t v;
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
			DREMap.insert(v);
		}
		else {
			std::vector<CStyleCastOrType> castVec;
			if(!castType.isNull())
				castVec.emplace_back(castType);
			lookForDeclRefWithMemberExprsInternal(D->getInit(),DREMap,0,castVec,0,0,false);
			// if(isa<InitListExpr>(D->getInit()))
				// llvm::outs()<<"INIT: "<<D->getInit()<<' '<<cast<InitListExpr>(D->getInit())->isSemanticForm()<<' '<<cast<InitListExpr>(D->getInit())->isSyntacticForm()<<'\n';
		}

		ExprRef_t VR;
		VR.setValue(D);

		std::vector<ExprRef_t> vVR;
		vVR.push_back(VR);
		vVR.insert(vVR.end(),DREMap.begin(),DREMap.end());

		if (!D->isDefinedOutsideFunctionOrMethod()) {
			if (lastFunctionDef) {
				std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
						lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,{},NumInitializedElements(D->getInit()),vVR,"",
								getCurrentCSPtr(),DereferenceInit));
				const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
				const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
					[D,this](const DereferenceInfo_t *d){ evalExpr(D, d); };
			}
		}
	}

	if (opts.cstmt && (!D->isDefinedOutsideFunctionOrMethod())) {
		if (lastFunctionDef) {
			struct DbJSONClassVisitor::VarInfo_t vi = {0,0,0,0};
			if (currentWithinCS()) {
				vi.CSPtr = getCurrentCSPtr();
				if (hasParentCS()) {
					vi.parentCSPtr = getParentCSPtr();
				}
			}
			vi.varId = lastFunctionDef->varId++;

			if(D->getKind() == Decl::Kind::Var){
				vi.VD = D;
				if (opts.debug3) {
					llvm::outs() << "VarDecl: " << D->getName().str() << " (" << lastFunctionDef->this_func->getName().str() << ")["
							<< lastFunctionDef->csIdMap[vi.CSPtr] << "] " << D->getLocation().printToString(Context.getSourceManager()) << "\n";
				}
			}
			else if(D->getKind() == Decl::Kind::ParmVar){
				vi.PVD = cast<const ParmVarDecl>(D);
				if (opts.debug3) {
					llvm::outs() << "ParmVarDecl: " << D->getName().str() << " (" << lastFunctionDef->this_func->getName().str() << ") "
							<< D->getLocation().printToString(Context.getSourceManager()) << "\n";
				}
			}
			else if (D->getKind() == Decl::Kind::Decomposition) {
				// Do nothing (for now)
			}
			else {
				if (opts.exit_on_error) {
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
		std::string name = D->getQualifiedNameAsString();
		// D->getNameAsString();
		if(!unique_name.insert(name).second) return true;
		int linkage = D->isExternallyVisible();
		int def_kind = D->hasDefinition();
		
		for( const VarDecl *RD : D->getCanonicalDecl()->redecls()){
			if(RD->isThisDeclarationADefinition() == def_kind) {
				D=RD;
				break;
			}
		}

		switch(def_kind){
			case 2:
			{
				const VarDecl *DD = D->getDefinition();
				QualType ST = DD->getTypeSourceInfo() ? DD->getTypeSourceInfo()->getType() : DD->getType();
				if (isOwnedTagDeclType(ST)) {
					noticeTypeClass(ST);
				}
				noticeTypeClass(DD->getType());
				VarMap.insert({VarForMap(DD),{{},DD}});
				VarMap.at(VarForMap(DD)).id.setID(VarNum++);
				break;
			}
			case 1:
			{
				const VarDecl *TD = D->getActingDefinition();
				QualType ST = TD->getTypeSourceInfo() ? TD->getTypeSourceInfo()->getType() : TD->getType();
				if (isOwnedTagDeclType(ST)) {
					noticeTypeClass(ST);
				}
				noticeTypeClass(TD->getType());
				VarMap.insert({VarForMap(TD),{{},TD}});
				VarMap.at(VarForMap(TD)).id.setID(VarNum++);
				break;
			}
			case 0:
			{
				assert(linkage && "Static variable not defined");
				const VarDecl *CD = D->getCanonicalDecl();
				QualType ST = CD->getTypeSourceInfo() ? CD->getTypeSourceInfo()->getType() : CD->getType();
				if (isOwnedTagDeclType(ST)) {
					noticeTypeClass(ST);
				}
				noticeTypeClass(CD->getType());
				VarMap.insert({VarForMap(CD),{{},CD}});
				VarMap.at(VarForMap(CD)).id.setID(VarNum++);
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
	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitRecordDecl(" << D << ")\n"; D->dump(llvm::outs()); T.dump() );
	noticeTypeClass(T);
	if (opts.cstmt) {
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
	// llvm::outs()<<"VISIT: "<<D<<'\n';

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

	if (opts.debug) {
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

	if (opts.debug) {
		std::string _class;
		QualType RT = Context.getRecordType(RD);
		_class = RT.getAsString();
		std::string templatePars;
		llvm::raw_string_ostream tpstream(templatePars);
		D->getTemplateParameters()->print(tpstream,Context);
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

	if (opts.debug) {
		std::string _class;
		QualType RT = Context.getRecordType(RD);
		_class = RT.getAsString();
		std::string templatePars;
		llvm::raw_string_ostream tpstream(templatePars);
		printTemplateArgumentList(tpstream,D->getTemplateArgs().asArray(),Context.getPrintingPolicy());
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

	// add body ahead of time for initializer list
	if(isa<CXXConstructorDecl>(D)){
		auto CD = cast<CXXConstructorDecl>(D);
		if(CD->doesThisDeclarationHaveABody()){
			csStack.push_back(cast<CompoundStmt>(CD->getBody()));
		}
	}
	DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitFunctionDeclStart(" << D << ")\n"; D->dump(llvm::outs()) );

	if (D->hasBody()) {
		if (FuncMap.find(D)!=FuncMap.end()) {
			DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitFunctionDeclStart(): present\n"; );
			lastFunctionDef = &FuncMap[D];
			functionStack.push_back(&FuncMap[D]);
			return true;
		}
	}
	else {
		lastFunctionDefCache = lastFunctionDef;
		lastFunctionDef = nullptr;
		if ((getFuncDeclMap().find(D)!=getFuncDeclMap().end())||(
				(opts.assert)&&(CTAList.find(D)!=CTAList.end())
			)) {
			DBG(DEBUG_NOTICE, llvm::outs() << "@notice VisitFunctionDeclStart(): present\n"; );
			return true;
		}
	}

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
		funcName << D->getNameAsString();
	}

	bool funcSaved = false;
	if (D->hasBody()) {
		const FunctionDecl * defdecl = D->getDefinition();
		if (defdecl==D) {
			assert(FuncMap.find(D)==FuncMap.end() && "Multiple definitions of Function body");
			FuncMap.insert({D,{}});
			FuncMap.at(D).id.setID(FuncNum++);
			functionStack.push_back(&FuncMap[D]);
			funcSaved = true;
			lastFunctionDef = &FuncMap[D];
			lastFunctionDef->this_func = D;
			lastFunctionDef->CSId = 0;
			lastFunctionDef->varId = 0;
			if(opts.taint){
				FuncMap[D].declcount = internal_declcount(D);
				taint_params(D,FuncMap[D]);
			}
		}
	}
	else {
		D = D->getCanonicalDecl();
		if (getFuncDeclMap().find(D)==getFuncDeclMap().end()) {
			funcSaved = true;
			if (opts.assert) {
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
				getFuncDeclMap().insert(std::pair<const FunctionDecl*,FuncDeclData>(D,{{},D}));
				getFuncDeclMap().at(D).id.setID(FuncNum++);
			}
		}
	}

	if ((D->hasBody() &&  (D->getDefinition()==D)))
	DBG(opts.debug, llvm::outs() << "notice Function: " << className.str() <<
						funcName.str() << "() [" << D->getNumParams() << "] ("
						<< D->getLocation().printToString(Context.getSourceManager()) << ")"
						<< "(" << D->hasBody() << ")" << "(" << (D->hasBody() &&  (D->getDefinition()==D)) << ") "
						<< (const void*)D << "\n" );

	//D->dumpColor();

	if (funcSaved) {
		QualType rT = D->getReturnType();
		noticeTypeClass(rT);
		for(unsigned long i=0; i<D->getNumParams(); i++) {
			const ParmVarDecl* p = D->getParamDecl(i);
			QualType T = p->getTypeSourceInfo() ? p->getTypeSourceInfo()->getType() : p->getType();
			noticeTypeClass(T);
		}
		DBG(DEBUG_NOTICE, llvm::outs() << "notice Function: " << className.str() <<
				funcName.str() << "() [" << D->getNumParams() << "]: DONE\n"; );
	}
	return true;
}
  
bool DbJSONClassVisitor::VisitFunctionDeclComplete(const FunctionDecl *D) {

	// get deref expr strings
	if(lastFunctionDef){
		for(auto &deref : lastFunctionDef->derefList){
			deref.evalExpr();
		}
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

	if (opts.debug) {
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
	if(!S) return true;
	// llvm::outs()<<std::string(2*tab++,' ')<<S->getStmtClassName()<<S<<'\n';
	bool TraverseResult = RecursiveASTVisitor<DbJSONClassVisitor>::TraverseStmt(S);
	// llvm::outs()<<std::string(2*(--tab),' ')<<S->getStmtClassName()<<" done\n";
	if (S && (S->getStmtClass()==Stmt::CompoundStmtClass)) {
		if (!VisitCompoundStmtComplete(static_cast<CompoundStmt*>(S))) return false;
	}
	return TraverseResult;
}

// bypass the traversal of semantic form introduced by shouldVisitImplicitCode
bool DbJSONClassVisitor::TraverseInitListExpr(InitListExpr *S){
	S = S->isSemanticForm()? S : S->getSemanticForm();
	return Base::TraverseSynOrSemInitListExpr(S);
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
						lastFunctionDef->firstNonDeclStmtLoc = getAbsoluteLocation(sloc);
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
	if (opts.cstmt) {
		const CompoundStmt* parentCS = 0;
		if (csStack.size()>0) {
			parentCS = csStack.back();
		}
		// added for CXXConstructor
		if(parentCS == CS){
			csStack.pop_back();
			return VisitCompoundStmtStart(CS);
		}
		csStack.push_back(CS);
		if (lastFunctionDef) {
			lastFunctionDef->csIdMap.insert(std::pair<const CompoundStmt*,long>(CS,lastFunctionDef->CSId++));
			lastFunctionDef->csParentMap.insert(std::pair<const CompoundStmt*,const CompoundStmt*>(CS,parentCS));
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitCompoundStmtComplete(const CompoundStmt *CS) {
	if (opts.cstmt) {
		assert(csStack.back()==CS);
		csStack.pop_back();
	}
	return true;
}

void DbJSONClassVisitor::handleConditionDeref(Expr *Cond,size_t cf_id){
	if(!Cond) return;
	QualType CT = Cond->getType();
	DbJSONClassVisitor::DREMap_t DREMap;

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
		int64_t i = Res.Val.getInt().extOrTrunc(63).getExtValue();
		ExprRef_t v;
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
		DREMap.insert(v);
	}
	else {
		std::vector<CStyleCastOrType> castVec;
		if(!castType.isNull())
			castVec.emplace_back(castType);
		lookForDeclRefWithMemberExprsInternal(E,DREMap,0,castVec,0,0,false);
	}

	ExprRef_t VR;
	VR.setCond(Cond,cf_id);

	std::vector<ExprRef_t> vVR;
	vVR.insert(vVR.end(),DREMap.begin(),DREMap.end());

	std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
			lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,{},cf_id,vVR,"",getCurrentCSPtr(),DereferenceCond));
	const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
	const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
		[Cond,this](const DereferenceInfo_t *d){ evalExpr(Cond, d); };
}

bool DbJSONClassVisitor::VisitSwitchStmt(SwitchStmt *S){
	// add compound statement if not present(should never happen)
	if(S->getBody()->getStmtClass() != Stmt::CompoundStmtClass){
		CompoundStmt *CS = compatibility::createEmptyCompoundStmt(Context);
		CS->body_begin()[0] = S->getBody();
		S->setBody(CS);
	}

	// add control flow info
	CompoundStmt *CS = static_cast<CompoundStmt*>(S->getBody());
	assert(lastFunctionDef);
	size_t cf_id = lastFunctionDef->cfData.size();
	lastFunctionDef->cfData.push_back({cf_switch,CS});
	lastFunctionDef->csInfoMap.insert({CS,cf_id});

	// add condition deref
	handleConditionDeref(S->getCond(),cf_id);

	if (opts.switchopt) {
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
		CompoundStmt *CS = compatibility::createEmptyCompoundStmt(Context);
		CS->body_begin()[0] = S->getThen();
		S->setThen(CS);
	}
	if(S->getElse() && S->getElse()->getStmtClass() != Stmt::CompoundStmtClass){
		CompoundStmt *CS = compatibility::createEmptyCompoundStmt(Context);
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
	if (opts.debug3) {
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
		CompoundStmt *CS = compatibility::createEmptyCompoundStmt(Context);
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
		CompoundStmt *CS = compatibility::createEmptyCompoundStmt(Context);
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
		CompoundStmt *CS = compatibility::createEmptyCompoundStmt(Context);
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
		if (opts.debug3) {
			llvm::outs() << "GCCAsmStmt: " << "" << " (" << lastFunctionDef->this_func->getName().str() << ")["
					<< lastFunctionDef->csIdMap[ai.CSPtr] << "] " << S->getAsmString()->getBytes().str() << "\n";
		}
		lastFunctionDef->asmMap.insert(std::pair<const GCCAsmStmt*,GCCAsmInfo_t>(S,ai));
	}
	return true;
}


bool DbJSONClassVisitor::VisitExpr(const Expr *Node) {
	if (opts.debugME) {
		llvm::outs() << "@VisitExpr()\n";
		Node->dumpColor();
	}
	return true;
}

bool DbJSONClassVisitor::VisitCallExpr(CallExpr *CE){
	if (opts.call) {
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
						if (handleCallAddress(Res.Val.getInt().extOrTrunc(63).getExtValue(),E,callrefs,literalRefs,&baseType,CE,CSCE)) __callserved++;
					}
				}
			}
		}
		if (!__callserved) {
			DBG(opts.debug3, llvm::outs() << "CALL not served: " << CE << "\n" );
			if (opts.debug3) CE->dumpColor();
			MissingCallExpr.insert(CE);
			// assert(0 && "missing call");

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
				else if(D->getKind()==Decl::CXXMethod) {
					const CXXMethodDecl *MD = static_cast<const CXXMethodDecl*>(D);
					rFT = walkToFunctionProtoType(MD->getType());
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
					DbJSONClassVisitor::DREMap_t DREMap;
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
						int64_t i = Res.Val.getInt().extOrTrunc(63).getExtValue();
						ExprRef_t v;
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
						DREMap.insert(v);
					}
					else {
						std::vector<CStyleCastOrType> castVec;
						if(!castType.isNull())
							castVec.emplace_back(castType);
						lookForDeclRefWithMemberExprsInternal(E,DREMap,0,castVec,0,0,false);
					}

					ExprRef_t VR;
					VR.setParm(argE);

					std::vector<ExprRef_t> vVR;
					vVR.insert(vVR.end(),DREMap.begin(),DREMap.end());

					std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
							lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,{},i,vVR,"",getCurrentCSPtr(),DereferenceParm));
					const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
					const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
						[argE,this](const DereferenceInfo_t *d){ evalExpr(argE, d); };
				}
			}
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitDeclRefExpr(DeclRefExpr *DRE){
	if(DRE->getDecl()->getKind() == Decl::Kind::Function){
		// special case when declaration is introduced implicitly by call
		const FunctionDecl *FD = static_cast<const FunctionDecl*>(DRE->getDecl());
		if(!FD->hasBody()){
			VisitFunctionDeclStart(FD);
			VisitFunctionDeclComplete(FD);
		}
	}
	if(lastFunctionDef || lastGlobalVarDecl || !recordDeclStack.empty()){
		if(DRE->getDecl()->getKind() == Decl::Kind::Var){
			const VarDecl *VD = static_cast<const VarDecl*>(DRE->getDecl());
			if(VD->isDefinedOutsideFunctionOrMethod()&&!VD->isStaticDataMember()){
				if (lastFunctionDef) {
					lastFunctionDef->refVars[VD->getCanonicalDecl()].insert(DRE);
				}
				if ((lastGlobalVarDecl)&&(lastGlobalVarDecl!=VD)) {
					getVarData(lastGlobalVarDecl).g_refVars.insert(VD);
				}
				if (!recordDeclStack.empty()) {
					gtp_refVars[recordDeclStack.top()].insert(VD);
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
			if (lastGlobalVarDecl) {
				getVarData(lastGlobalVarDecl).g_refTypes.insert(ET);
			}
		}
		else if(DRE->getDecl()->getKind() == Decl::Kind::Function){
			const FunctionDecl *FD = static_cast<const FunctionDecl*>(DRE->getDecl());
			if (lastFunctionDef) {
				lastFunctionDef->refFuncs.insert(FD);
			}
			if (lastGlobalVarDecl) {
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
	if(lastFunctionDef || lastGlobalVarDecl || !recordDeclStack.empty()){
		if (UTTE->isArgumentType()) {
			QualType UT = UTTE->getArgumentType();
			noticeTypeClass(UT);
			if (lastFunctionDef) {
				lastFunctionDef->refTypes.insert(UT);
			}
			if (lastGlobalVarDecl) {
				getVarData(lastGlobalVarDecl).g_refTypes.insert(UT);
			}
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitCompoundLiteralExpr(CompoundLiteralExpr *CLE){
	if(lastFunctionDef || lastGlobalVarDecl) {
		QualType T = CLE->getType();
		noticeTypeClass(T);
		if (lastFunctionDef) {
			lastFunctionDef->refTypes.insert(T);
		}
		if (lastGlobalVarDecl) {
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
		if (lastGlobalVarDecl) {
			noticeTypeClass(Node->getType());
			getVarData(lastGlobalVarDecl).g_refTypes.insert(Node->getType());
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitBinaryOperator(BinaryOperator *BO) {
	
	//fops
	if(BO->getOpcode() == BinaryOperatorKind::BO_Assign &&
			BO->getLHS()->getStmtClass() == Stmt::MemberExprClass && 
			BO->getType()->isFunctionPointerType()){
		auto *ME = cast<MemberExpr>(BO->getLHS());
		const VarDecl *D = findFopsVar(ME);
		QualType T = ME->getBase()->getType()->getCanonicalTypeInternal();
		if(T->isPointerType()){
			T = T->getPointeeType();
		}
		assert(!T.hasQualifiers() && "Qualified type");
		assert(lastFunctionDef && "assignment outside function body");
		FopsObject FObj(T, D,lastFunctionDef->this_func);
		int field_index = cast<FieldDecl>(ME->getMemberDecl())->getFieldIndex();
		noticeFopsFunction(FObj, field_index, BO->getRHS());
	}

	// logic operators (includes bitwise operators)
	if(BO->getOpcode() >= BO_Cmp && BO->getOpcode() <= BO_LOr){
		if (opts.debugME) {
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
			int64_t i = Res.Val.getInt().extOrTrunc(63).getExtValue();
			ExprRef_t v;
			CStyleCastOrType valuecast;
			v.setInteger(i,valuecast);
			LDREMap.insert(v);
		}
		else {
			std::vector<CStyleCastOrType> castVec;
			lookForDeclRefWithMemberExprsInternal(LHS,LDREMap,0,castVec,0,0,true);
		}

		DbJSONClassVisitor::DREMap_t RDREMap;
		castVec.clear();
		E = stripCastsEx(RHS,castVec);
		if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)) {
			int64_t i = Res.Val.getInt().extOrTrunc(63).getExtValue();
			ExprRef_t v;
			CStyleCastOrType valuecast;
			v.setInteger(i,valuecast);
			RDREMap.insert(v);
		}
		else {
			std::vector<CStyleCastOrType> castVec;
			lookForDeclRefWithMemberExprsInternal(RHS,RDREMap,0,castVec,0,0,true);
		}

		ExprRef_t VR;
		VR.setLogic(BO);
		std::vector<ExprRef_t> vVR;
		unsigned Lsize = LDREMap.size();

		vVR.insert(vVR.end(),LDREMap.begin(),LDREMap.end());
		vVR.insert(vVR.end(),RDREMap.begin(),RDREMap.end());

		std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
				lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,{},BO->getOpcode(),Lsize,vVR,"",getCurrentCSPtr(),DereferenceLogic));
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
			[BO,this](const DereferenceInfo_t *d){ evalExpr(BO, d); };
	}

	// assignment operators
	if(BO->getOpcode() >= BO_Assign && BO->getOpcode() <= BO_OrAssign){

		if (opts.debugME) {
			llvm::outs() << "@VisitBinaryOperator()\n";
			BO->dumpColor();
		}

		if (!lastFunctionDef) return true; // TODO: When derefs for globals are implemented handle this

		const Expr* LHS = BO->getLHS();
		const Expr* RHS = BO->getRHS();

		DbJSONClassVisitor::DREMap_t LDREMap;
		std::vector<CStyleCastOrType> castVecL;
		lookForDeclRefWithMemberExprsInternal(LHS,LDREMap,0,castVecL,0,0,true);
		
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
			int64_t i = Res.Val.getInt().extOrTrunc(63).getExtValue();
			ExprRef_t v;
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
			RDREMap.insert(v);
		}
		else {
			std::vector<CStyleCastOrType> castVec;
			if(!castType.isNull())
				castVec.emplace_back(castType);
			lookForDeclRefWithMemberExprsInternal(RHS,RDREMap,0,castVec,0,0,false);
		}

		ExprRef_t VR;
		VR.setCAO(BO);
		if (LDREMap.size() < 1) {
			// TODO: Allowed multiple LHS entries, caused by conditional operator, needs better handling
			llvm::errs() << "No expressions on LHS for assignment operator: " << LDREMap.size() << "\n";
			BO->dumpColor();
			llvm::outs()<<LDREMap.size()<<'\n';
			BO->printPretty(llvm::outs(),nullptr,Context.getPrintingPolicy());
			assert(0);
		}
		ExprRef_t iVR = *LDREMap.begin();
		noticeTypeClass(LHS->getType());
		iVR.setCast(LHS->getType());

		std::vector<ExprRef_t> vVR;
		vVR.push_back(iVR);
		vVR.insert(vVR.end(),RDREMap.begin(),RDREMap.end());

		std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
				lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,{},BO->getOpcode(),vVR,"",getCurrentCSPtr(),DereferenceAssign));
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
			[BO,this](const DereferenceInfo_t *d){ evalExpr(BO, d); };
	}

	return true;
}

bool DbJSONClassVisitor::VisitUnaryOperator(UnaryOperator *E) {

	if (opts.debugME) {
		llvm::outs() << "@VisitUnaryOperator()\n";
		E->dumpColor();
	}

	if (E->getOpcode()==UO_Deref) {
		if (opts.debugDeref) {
			llvm::outs() << E->getBeginLoc().printToString(Context.getSourceManager()) << ": ";
			E->printPretty(llvm::outs(),0,Context.getPrintingPolicy());
			llvm::outs() << "\n";
		}
		int64_t i=0;
		std::vector<ExprRef_t> vVR;
		bool ignoreErrors = false;
		lookForDerefExprs(E->getSubExpr(),&i,vVR);
		if (opts.debugDeref) {
			llvm::outs() << " [" << i << "]";
			llvm::outs() << " | " << vVR.size();
			llvm::outs() << "\n";
		}

		ExprRef_t VR;
		VR.setUnary(E);
		if (lastFunctionDef) {
			std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
					lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,{},i,vVR,"",getCurrentCSPtr(),DereferenceUnary));
			const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
			const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
				[E,this](const DereferenceInfo_t *d){ evalExpr(E, d); };
		}
	}
	return true;
}

bool DbJSONClassVisitor::VisitArraySubscriptExpr(ArraySubscriptExpr *Node) {

	if (opts.debugDeref) {
		llvm::outs() << Node->getBeginLoc().printToString(Context.getSourceManager()) << ": ";
		Node->getBase()->printPretty(llvm::outs(),0,Context.getPrintingPolicy());
		llvm::outs() << "[";
		Node->getIdx()->printPretty(llvm::outs(),0,Context.getPrintingPolicy());
		llvm::outs() << "]\n";
	}

	if (opts.debugME) {
		llvm::outs() << "@VisitArraySubscriptExpr()\n";
		Node->dumpColor();
	}

	std::vector<ExprRef_t> vVR;
	// base
	int64_t base_i=0;
	lookForDerefExprs(Node->getBase(),&base_i,vVR);
	unsigned base_size = vVR.size();

	// index
	const Expr* idxE = stripCasts(Node->getIdx());
	int64_t idx_i=0;
	// Check if expression can be evaluated as a constant expression
	Expr::EvalResult Res;
	if((!idxE->isValueDependent()) && idxE->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(idxE,Res)) {
		idx_i = Res.Val.getInt().extOrTrunc(63).getExtValue();
	}
	else {
		lookForDerefExprs(idxE,&idx_i,vVR);
	}

	ExprRef_t VR;
	VR.setAS(Node);
	if (lastFunctionDef) {
		std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
				lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,{},idx_i,base_size,vVR,"",getCurrentCSPtr(),DereferenceArray));
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
			[Node,this](const DereferenceInfo_t *d){ evalExpr(Node, d); };
	}

	return true;
}

bool DbJSONClassVisitor::VisitOffsetOfExpr(OffsetOfExpr *Node) {

	if(lastFunctionDef || lastGlobalVarDecl){
		QualType BT = Node->getTypeSourceInfo()->getType();
		noticeTypeClass(BT);
		if (lastFunctionDef) {
			lastFunctionDef->refTypes.insert(BT);
		}
		if (lastGlobalVarDecl) {
			getVarData(lastGlobalVarDecl).g_refTypes.insert(BT);
		}
	}

	if (!lastFunctionDef) {
		return true;
	}

	Expr::EvalResult Res;
	int64_t offsetof_offset = -1;
	if((!Node->isValueDependent()) && Node->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(Node,Res)){
		offsetof_offset = Res.Val.getInt().extOrTrunc(63).getExtValue();
	}

	std::vector<MemberInfo_t> MInfoList;
	DbJSONClassVisitor::DREMap_t DREMap;
	bool canComputeOffset = true;

	unsigned MEIdx = 0;
	QualType lastRecordType = Node->getTypeSourceInfo()->getType();
	unsigned fieldAccessCount = 0;
	for (unsigned i=0; i<Node->getNumComponents(); ++i) {
		const OffsetOfNode & C = Node->getComponent(i);
		OffsetOfNode::Kind k = C.getKind();
		if (k==OffsetOfNode::Field || k == OffsetOfNode::Identifier) {
			fieldAccessCount++;
		}
	}

	for (unsigned i=0; i<Node->getNumComponents(); ++i) {
		const OffsetOfNode & C = Node->getComponent(i);
		OffsetOfNode::Kind k = C.getKind();
		if (k==OffsetOfNode::Identifier){
			MInfoList.push_back(MemberInfo_t{0,lastRecordType});
		}
		else if (k==OffsetOfNode::Field) {
			FieldDecl* FD = C.getField();
			const RecordDecl* RD = FD->getParent();
            lastRecordType = RD->getASTContext().getRecordType(RD);
			MInfoList.push_back(MemberInfo_t{FD,lastRecordType});
            noticeTypeClass(lastRecordType);
            MEIdx++;
		}
		else if (k==OffsetOfNode::Array) {
			unsigned idx = C.getArrayExprIndex();
			const Expr * E = Node->getIndexExpr(idx);
            // Check if offset expression can be evaluated as a constant expression
            Expr::EvalResult Res;
            if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)) {
                int64_t i = Res.Val.getInt().extOrTrunc(63).getExtValue();
                ExprRef_t v;
                v.setInteger(i);
                v.setMeIdx(MEIdx-1);
				DREMap.insert(v);
            }
            else {
				std::vector<CStyleCastOrType> castVec;
	            lookForDeclRefWithMemberExprsInternal(E,DREMap,0,castVec,MEIdx-1,0,true);
            }
			MInfoList.push_back(MemberInfo_t{0,lastRecordType});
		}
		else {
			/* Not supported */
		}
	}
	ExprRef_t VR;
	VR.setOOE(Node);
	VR.setMeCnt(fieldAccessCount-1);

	std::vector<ExprRef_t> vVR;
    vVR.insert(vVR.end(),DREMap.begin(),DREMap.end());

    std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
    		lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,MInfoList,offsetof_offset,vVR,"",getCurrentCSPtr(),DereferenceOffsetOf));
    const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
	const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
		[Node,this](const DereferenceInfo_t *d){ evalExpr(Node, d); };
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
	DbJSONClassVisitor::DREMap_t DREMap;
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
		int64_t i = Res.Val.getInt().extOrTrunc(63).getExtValue();
		ExprRef_t v;
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
		DREMap.insert(v);
	}
	else {
		std::vector<CStyleCastOrType> castVec;
		if(!castType.isNull())
			castVec.emplace_back(castType);
		lookForDeclRefWithMemberExprsInternal(E,DREMap,0,castVec,0,0,false);
	}
	ExprRef_t VR;
	VR.setRET(S);

	std::vector<ExprRef_t> vVR;
	vVR.insert(vVR.end(),DREMap.begin(),DREMap.end());

	std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
			lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,{},0,vVR,"",getCurrentCSPtr(),DereferenceReturn));
	const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
	const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
		[S,this](const DereferenceInfo_t *d){ evalExpr(S,d); };
	return true;
}

bool DbJSONClassVisitor::VisitMemberExpr(MemberExpr *Node) {

	if(lastFunctionDef || lastGlobalVarDecl){
		QualType T = Node->getBase()->getType();
		T=GetBaseType(T);
		noticeTypeClass(T);
		if (lastFunctionDef) {
			lastFunctionDef->refTypes.insert(T);
		}
		if (lastGlobalVarDecl) {
			getVarData(lastGlobalVarDecl).g_refTypes.insert(T);
		}
	}

	if (DoneMEs.find(Node)!=DoneMEs.end()) {
		return true;
	}

	if (opts.debugME) {
		llvm::outs() << "@VisitMemberExpr()\n";
		Node->dumpColor();
	}

	/* We have primary MemberExpr (i.e. MemberExpr that is not in the chain of other MemberExprs); handle it */

	DbJSONClassVisitor::DREMap_t DREMap;
	std::vector<MemberInfo_t> MInfoList;
	std::vector<CStyleCastOrType> castVec;
	const CallExpr* CE = 0;
	if (MEHaveParentCE.find(Node)!=MEHaveParentCE.end()) {
		CE = MEHaveParentCE[Node];
	}
	lookForDeclRefWithMemberExprsInternal(Node,DREMap,&MInfoList,castVec,0,CE);

	ExprRef_t VR;;
	VR.setME(Node);
	VR.setMeCnt(MInfoList.size());
	std::vector<ExprRef_t> vVR;

	/* Now save all the referenced variables */
	vVR.insert(vVR.end(),DREMap.begin(),DREMap.end());

	if (lastFunctionDef) {
		std::pair<std::set<DereferenceInfo_t>::iterator,bool> rv =
				lastFunctionDef->derefList.insert(DereferenceInfo_t(VR,MInfoList,0,vVR,"",getCurrentCSPtr(),DereferenceMember));
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->addOrd(exprOrd++);
		const_cast<DbJSONClassVisitor::DereferenceInfo_t*>(&(*rv.first))->evalExprInner =
				[CE,Node,this](const DereferenceInfo_t *d){ evalExpr((Expr*)CE?:Node, d); };
	}

	return true;
}

bool DbJSONClassVisitor::VisitInitListExpr(InitListExpr* ILE) {

	// llvm::outs()<<"ILE: "<<ILE<<' '<<ILE->isSemanticForm()<<ILE->isSyntacticForm()<<'\n';
	// probably a part of compoundliteralexpr, we handle it in assign if relevant
	if(fopsVarDecl.empty()) return true;

	QualType T = ILE->getType()->getCanonicalTypeInternal();
	if(!T->isRecordType())
		return true;

	const FunctionDecl *F = lastFunctionDef ?  lastFunctionDef->this_func : nullptr;
	T.removeLocalFastQualifiers();
	assert(!T.hasQualifiers() && "Qualified type");
	FopsObject FObj(T, fopsVarDecl.back(), F);
	ILE = ILE->isSemanticForm()? ILE : ILE->getSemanticForm();
	// ILE = ILE->getSemanticForm();
	int field_index = 0;
	for(const auto &init : ILE->children()){
		auto *expr = llvm::cast<Expr>(init);
		if(expr->getType()->isFunctionPointerType() || expr->getType()->isVoidPointerType()){
			noticeFopsFunction(FObj, field_index, expr);
		}
		field_index++;
	}
	return true;
}

bool DbJSONClassVisitor::VisitIntegerLiteral(IntegerLiteral *L){
	LiteralHolder lh;
	lh.type = LiteralHolder::LiteralInteger;
	lh.integerLiteral = llvm::APSInt(L->getValue(),L->getType()->isUnsignedIntegerOrEnumerationType());
	if(lastFunctionDef){
		lastFunctionDef->literals.insert(lh);
	}
	else if(lastGlobalVarDecl){
		getVarData(lastGlobalVarDecl).g_literals.insert(lh);
	}
	else if(!recordDeclStack.empty()){
	}
	return true;
}
bool DbJSONClassVisitor::VisitFloatingLiteral(FloatingLiteral *L){
	LiteralHolder lh;
	lh.type = LiteralHolder::LiteralFloat;
	lh.floatingLiteral = L->getValue();
	if(lastFunctionDef){
		lastFunctionDef->literals.insert(lh);
	}
	else if(lastGlobalVarDecl){
		getVarData(lastGlobalVarDecl).g_literals.insert(lh);
	}
	else if(!recordDeclStack.empty()){
	}
	return true;
}
bool DbJSONClassVisitor::VisitCharacterLiteral(CharacterLiteral *L){
	LiteralHolder lh;
	lh.type = LiteralHolder::LiteralChar;
	lh.charLiteral = L->getValue();
	if(lastFunctionDef){
		lastFunctionDef->literals.insert(lh);
	}
	else if(lastGlobalVarDecl){
		getVarData(lastGlobalVarDecl).g_literals.insert(lh);
	}
	else if(!recordDeclStack.empty()){
	}
	return true;
}
bool DbJSONClassVisitor::VisitStringLiteral(StringLiteral *L){
	LiteralHolder lh;
	lh.type = LiteralHolder::LiteralString;
	lh.stringLiteral = L->getBytes().str();
	lh.stringLiteral.erase(std::remove(lh.stringLiteral.begin(),lh.stringLiteral.end(),'\x1f'),lh.stringLiteral.end());
	if(lastFunctionDef){
		lastFunctionDef->literals.insert(lh);
	}
	else if(lastGlobalVarDecl){
		getVarData(lastGlobalVarDecl).g_literals.insert(lh);
	}
	else if(!recordDeclStack.empty()){
	}
	return true;
}
