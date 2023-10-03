#include "main.hpp"

#define DEBUG_FOPS 0

std::string FOPSClassConsumer::getAbsoluteLocation(SourceLocation Loc){
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

std::string FOPSClassVisitor::typeMatching(QualType T) {

	switch(T->getTypeClass()) {
		case Type::Elaborated:
		{
			const ElaboratedType *tp = cast<ElaboratedType>(T);
			QualType eT = tp->getNamedType();
			return typeMatching(eT);
		}
		break;
		case Type::Record:
		{
			const RecordType *tp = cast<RecordType>(T);
			RecordDecl* rD = tp->getDecl();
			const IdentifierInfo *II = rD->getIdentifier();
			if (rD->isCompleteDefinition()) {
				if (II) {
					if (opts.fops_all) {
						return II->getName().str();
					}
					auto it = opts.fopsRecords.find(II->getName().str());
					if (it!=opts.fopsRecords.end()) {
						return *it;
					}
				}
			}
		}
		break;
		case Type::ConstantArray:
		{
			const ConstantArrayType *tp = cast<ConstantArrayType>(T);
			QualType elT = tp->getElementType();
			return typeMatching(elT);
		}
		break;
		case Type::IncompleteArray:
		{
			const IncompleteArrayType *tp = cast<IncompleteArrayType>(T);
			QualType elT = tp->getElementType();
			return typeMatching(elT);
		}
		break;
		case Type::Paren:
		{
			const ParenType *tp = cast<ParenType>(T);
			QualType iT = tp->getInnerType();
			return typeMatching(iT);
		}
		break;
		case Type::Typedef:
		{
			const TypedefType *tp = cast<TypedefType>(T);
			TypedefNameDecl* D = tp->getDecl();
			QualType tT = D->getTypeSourceInfo()->getType();
			return typeMatching(tT);
		}
		break;
		case Type::TypeOfExpr:
		{
			const TypeOfExprType *tp = cast<TypeOfExprType>(T);
			QualType toT = tp->getUnderlyingExpr()->getType();
			return typeMatching(toT);
		}
		break;
		case Type::TypeOf:
		{
			const TypeOfType *tp = cast<TypeOfType>(T);
			QualType toT = tp->getUnderlyingType();
			return typeMatching(toT);
		}
		break;
	}

	return std::string("");
}

std::string FOPSClassVisitor::typeMatchingWithPtr(QualType T) {

	switch(T->getTypeClass()) {
		case Type::Pointer:
		{
			const PointerType *tp = cast<PointerType>(T);
			QualType T = tp->getPointeeType();
			return typeMatchingWithPtr(T);
		}
		case Type::Elaborated:
		{
			const ElaboratedType *tp = cast<ElaboratedType>(T);
			QualType eT = tp->getNamedType();
			return typeMatchingWithPtr(eT);
		}
		break;
		case Type::Record:
		{
			const RecordType *tp = cast<RecordType>(T);
			RecordDecl* rD = tp->getDecl();
			const IdentifierInfo *II = rD->getIdentifier();
			if (rD->isCompleteDefinition()) {
				if (II) {
					if (opts.fops_all) {
						return II->getName().str();
					}
					auto it = opts.fopsRecords.find(II->getName().str());
					if (it!=opts.fopsRecords.end()) {
						return *it;
					}
				}
			}
		}
		break;
		case Type::ConstantArray:
		{
			const ConstantArrayType *tp = cast<ConstantArrayType>(T);
			QualType elT = tp->getElementType();
			return typeMatchingWithPtr(elT);
		}
		break;
		case Type::IncompleteArray:
		{
			const IncompleteArrayType *tp = cast<IncompleteArrayType>(T);
			QualType elT = tp->getElementType();
			return typeMatchingWithPtr(elT);
		}
		break;
		case Type::Paren:
		{
			const ParenType *tp = cast<ParenType>(T);
			QualType iT = tp->getInnerType();
			return typeMatchingWithPtr(iT);
		}
		break;
		case Type::Typedef:
		{
			const TypedefType *tp = cast<TypedefType>(T);
			TypedefNameDecl* D = tp->getDecl();
			QualType tT = D->getTypeSourceInfo()->getType();
			return typeMatchingWithPtr(tT);
		}
		break;
		case Type::TypeOfExpr:
		{
			const TypeOfExprType *tp = cast<TypeOfExprType>(T);
			QualType toT = tp->getUnderlyingExpr()->getType();
			return typeMatchingWithPtr(toT);
		}
		break;
		case Type::TypeOf:
		{
			const TypeOfType *tp = cast<TypeOfType>(T);
			QualType toT = tp->getUnderlyingType();
			return typeMatchingWithPtr(toT);
		}
		break;
	}

	return std::string("");
}

void FOPSClassVisitor::noticeImplicitCastExpr(const ImplicitCastExpr* ICE, const VarDecl * Dref, QualType initExprType,
		unsigned long index) {
	const Expr* target = ICE->getSubExpr();
	if (target->getStmtClass()==Stmt::DeclRefExprClass) {
		std::string matchedType = typeMatching(initExprType);
		if (matchedType.size()<=0) return;
		const DeclRefExpr* F = static_cast<const DeclRefExpr*>(target);
		const ValueDecl* v = F->getDecl();
		if (v->getKind()==Decl::Function) {
			const FunctionDecl* FD = static_cast<const FunctionDecl*>(v);
			DBG( opts.debugNotice, llvm::outs() << "Index: " << index << ", function: " << FD->getNameAsString() << ", Var: " <<
					(const void*)Dref << " (" << cast<NamedDecl>(Dref)->getNameAsString() << ")" <<
					" [struct " << matchedType << "]\n" );
			std::tuple<unsigned long,std::string,std::string,std::string> val(index,matchedType,FD->getNameAsString(),getFunctionDeclHash(FD));
			if (initMap.find((const void*)Dref)==initMap.end()) {
				std::set<std::tuple<unsigned long,std::string,std::string,std::string>> x;
				initMap[(const void*)Dref] = x;
			}
			initMap[(const void*)Dref].insert(val);
		}
	}
	else if (target->getStmtClass()==Stmt::ImplicitCastExprClass) {
		const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(target);
		noticeImplicitCastExpr(ICE,Dref,initExprType,index);
	}
}

void FOPSClassVisitor::noticeInitListStmt(const Stmt* initExpr, const VarDecl * Dref) {
	unsigned long i=0;
	for (const Stmt *SubStmt : initExpr->children()) {
		switch (SubStmt->getStmtClass()) {
			case Stmt::InitListExprClass:
			{
				const InitListExpr *ILE = static_cast<const InitListExpr*>(SubStmt);
				std::string matchedType = typeMatching(ILE->getType());
				if (matchedType.size()<=0) return;
				noticeInitListStmt(ILE, Dref);
			}
			break;
			case Stmt::ImplicitCastExprClass:
			{
				const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(SubStmt);
				noticeImplicitCastExpr(ICE,Dref,static_cast<const Expr*>(initExpr)->getType(), i);
			}
			break;
		}
		++i;
	}
}
void getFuncDeclSignature(const FunctionDecl* D, std::string& fdecl_sig) {
  fdecl_sig += D->getName();
  fdecl_sig += ' ';
  fdecl_sig += walkTypedefType(D->getType()).getAsString();
}
std::string FOPSClassVisitor::getFunctionDeclHash(const FunctionDecl *FD) {

	// TODO: what about C++?
	//const TemplateDecl* FT = 0;
	//const ClassTemplateSpecializationDecl* CTS = 0;
	SHA_CTX cd;
	SHA_init(&cd);
	//if (FT || CTS) SHA_update(&cd, templatePars.data(), templatePars.size());
	std::string fdecl_signature;
	getFuncDeclSignature(FD, fdecl_signature);
	SHA_update(&cd, fdecl_signature.data(), fdecl_signature.size());
	//SHA_update(&cd, classHash.data(), classHash.size());
	//SHA_update(&cd, nms.data(), nms.size());
	SHA_final(&cd);
	return base64_encode(reinterpret_cast<const unsigned char*>(cd.buf.b), 64);
}

bool FOPSClassVisitor::VisitVarDecl(const VarDecl *D) {

	DBG(opts.debugNotice, llvm::outs() << "@VisitVarDecl (" << D << ")[" << D->isThisDeclarationADefinition() << "] : "; );

	DBG(opts.debugNotice, llvm::outs() << int(D->isThisDeclarationADefinition()==VarDecl::Definition) <<
			"\n"; D->dumpColor(); );

	if (D->isThisDeclarationADefinition()==VarDecl::Definition) {
		const NamedDecl* ND = cast<NamedDecl>(D);
		if (D->hasInit()) {
			const Expr* E = D->getInit();
			if (E->getStmtClass()==Stmt::CallExprClass) return true;
			noticeInitListStmt(E,D);
		}
	}

	return true;
}

const Expr* FOPSClassVisitor::CollapseToDeclRef(const Stmt *S) {

	switch(S->getStmtClass()) {
		case Stmt::DeclRefExprClass:
		{
			return static_cast<const DeclRefExpr*>(S);
		}
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(S);
			const Expr* e = CollapseToDeclRef(ASE->getLHS());
			if (!e) {
				e = CollapseToDeclRef(ASE->getRHS());
			}
			return e;
		}
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(S);
			const Expr* target = ICE->getSubExpr();
			if ((ICE->getCastKind()==CK_ArrayToPointerDecay) || (ICE->getCastKind()==CK_LValueToRValue)) {
				return CollapseToDeclRef(target);
			}
		}
	}

	return 0;
}

bool FOPSClassVisitor::noticeMemberExprForAssignment(const MemberExpr* ME, struct AssignInfo& AI, unsigned fieldIndex, bool nestedME) {

	static std::vector<const MemberExpr*> smallStackME;
	if (!nestedME) {
		smallStackME.clear();
	}
	if (smallStackME.size()<2) {
		smallStackME.push_back(ME);
	}
	const ValueDecl* v = ME->getMemberDecl();
	DBG(opts.debugNotice, llvm::outs() << "[" << v->getDeclKindName() << "]"; );
	if (v->getKind()==Decl::Field) {
		for (const Stmt *SubStmt : static_cast<const Expr*>(ME)->children()) {
			if (SubStmt->getStmtClass()==Stmt::MemberExprClass) {
				if (noticeMemberExprForAssignment(static_cast<const MemberExpr*>(SubStmt),AI,fieldIndex,true)) return true;
			}
			else {
				const Expr* RE = CollapseToDeclRef(SubStmt);
				if (RE) {
					assert(RE->getStmtClass()==Stmt::DeclRefExprClass && "Invalid DeclRefExpr object");
					const DeclRefExpr* D = static_cast<const DeclRefExpr*>(RE);
					std::string matchedType;
					if (smallStackME.size()>1) {
						matchedType = typeMatchingWithPtr(smallStackME.back()->getType());
					}
					else {
						matchedType = typeMatchingWithPtr(D->getDecl()->getType());
					}
					if (matchedType.size()>0) {
						AI.name = D->getDecl()->getNameAsString();
						AI.D = D->getDecl();
						AI.type = matchedType;
						AI.index = fieldIndex;
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool FOPSClassVisitor::VisitBinaryOperator(const BinaryOperator *Node) {

	if (Node->getOpcode()==BinaryOperatorKind::BO_Assign) {

		struct AssignInfo AI = {"",0,""};
		Expr* lhs = Node->getLHS();
		DBG(opts.debugNotice, llvm::outs() << "@VisitBinaryOperator::Assign (" << Node << ")[" <<
				lhs->getStmtClassName() << "]"; );

		if (lhs->getStmtClass()==Stmt::MemberExprClass) {
			const MemberExpr* ME = static_cast<const MemberExpr*>(lhs);
			const ValueDecl* v = ME->getMemberDecl();
			const FieldDecl* FD = static_cast<const FieldDecl*>(v);
			unsigned fieldIndex = FD->getFieldIndex();
			noticeMemberExprForAssignment(ME,AI,fieldIndex);
		}

		DBG(opts.debugNotice, llvm::outs() << "\n"; Node->dumpColor(); );

		Expr* rhs = Node->getRHS();
		if (rhs->getStmtClass()==Stmt::ImplicitCastExprClass) {
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(rhs);
			const Expr* target = ICE->getSubExpr();
			if (target->getStmtClass()==Stmt::DeclRefExprClass) {
				const DeclRefExpr* F = static_cast<const DeclRefExpr*>(target);
				const ValueDecl* v = F->getDecl();
				if (v->getKind()==Decl::Function) {
					const FunctionDecl* FD = static_cast<const FunctionDecl*>(v);
					if (AI.D) {
						std::tuple<unsigned long,std::string,std::string,std::string> val(AI.index,AI.type,
								FD->getNameAsString(),getFunctionDeclHash(FD));
						if (initMap.find(AI.D)==initMap.end()) {
							std::set<std::tuple<unsigned long,std::string,std::string,std::string>> x;
							initMap[AI.D] = x;
						}
						initMap[AI.D].insert(val);
					}
				}
			}
		}
	}
	return true;
}

void FOPSClassConsumer::printVarMaps(int Indentation) {

	// Convert initMap to a list of initMapFixedType_t maps
	std::map<std::string,FOPSClassVisitor::initMapFixedType_t> cvMap;
	for (auto i=Visitor.initMap.begin(); i!=Visitor.initMap.end(); ++i) {
		const void* Dref = (*i).first;
		for (auto j=(*i).second.begin(); j!=(*i).second.end(); ++j) {
			unsigned long index = std::get<0>(*j);
			std::string type = std::get<1>(*j);
			std::string name = std::get<2>(*j);
			std::string declhash = std::get<3>(*j);
			if (cvMap.find(type)==cvMap.end()) {
				cvMap.insert(std::pair<std::string,FOPSClassVisitor::initMapFixedType_t>(type,FOPSClassVisitor::initMapFixedType_t()));
			}
			FOPSClassVisitor::initMapFixedType_t& iMap = cvMap[type];
			std::tuple<unsigned long,std::string,std::string> val(index,name,declhash);
			if (iMap.find(Dref)==iMap.end()) {
				std::set<std::tuple<unsigned long,std::string,std::string>> x;
				std::pair<std::string,std::set<std::tuple<unsigned long,std::string,std::string>>> y(type,x);
				iMap[Dref] = y;
			}
			iMap[Dref].second.insert(val);
		}
	}

	std::string Indent(Indentation,'\t');
	llvm::outs() << Indent << "[\n";
	for (auto i=cvMap.begin(); i!=cvMap.end();) {
		printVarMap((*i).second,Indentation);
		++i;
		if (i!=cvMap.end()) llvm::outs() << ",";
		llvm::outs() << "\n";
	}
	llvm::outs() << Indent << "]\n";
}

void FOPSClassConsumer::printVarMap(FOPSClassVisitor::initMapFixedType_t& FTVM, int Indentation) {
	std::string Indent(Indentation,'\t');
	for (auto i = FTVM.begin(); i!=FTVM.end();) {
		// Count how many times each member index occurs
		std::map<unsigned long,unsigned long> indexCount;
		for (auto j=(*i).second.second.begin(); j!=(*i).second.second.end(); ++j) {
			if (indexCount.find(std::get<0>((*j)))==indexCount.end()) {
				indexCount.insert(std::pair<unsigned long,unsigned long>(std::get<0>((*j)),1));
			}
			else {
				indexCount[std::get<0>((*j))]++;
			}
		}
		// Take the maximum number of occurences
		unsigned long max_count = 0;
		for (auto j=indexCount.begin(); j!=indexCount.end(); ++j) {
			if ((*j).second>max_count) max_count=(*j).second;
		}
		// Assign values to each member index occurence
		std::map<unsigned long,std::vector<std::tuple<unsigned long,std::string,std::string>>> indexValues;
		for (auto j=(*i).second.second.begin(); j!=(*i).second.second.end(); ++j) {
			if (indexValues.find(std::get<0>((*j)))==indexValues.end()) {
				std::vector<std::tuple<unsigned long,std::string,std::string>> y;
				y.push_back(*j);
				indexValues.insert(std::pair<unsigned long,std::vector<std::tuple<unsigned long,std::string,std::string>>>(
						std::get<0>((*j)),y));
			}
			else {
				indexValues[std::get<0>((*j))].push_back(*j);
			}
		}
		// Now print in batches
		for (unsigned long n=0; n<max_count; ++n) {
			llvm::outs() << Indent << "\t{\n";
			const VarDecl * D = static_cast<const VarDecl*>((*i).first);
			llvm::outs() << "\t\t\t\"name\": " << "\"" << cast<NamedDecl>(D)->getNameAsString() << "\"" << ",\n";
			std::string locstart = getAbsoluteLocation(D->getSourceRange().getBegin());
			llvm::outs() << "\t\t\t\"type\": " << "\"" << (*i).second.first << "\"" << ",\n";
			llvm::outs() << "\t\t\t\"location\": " << "\"" << locstart << "\"" << ",\n";
			llvm::outs() << "\t\t\t\"members\": {\n";
			bool first = true;
			for (auto j=indexValues.begin(); j!=indexValues.end(); ++j) {
				// print n+1'st occurence of each member index (if present)
				if (n+1<=indexCount[(*j).first]) {
					if (!first) {
						llvm::outs() << ",\n";
					}
					llvm::outs() << "\t\t\t\t\"" << std::get<0>((*j).second[n]) << "\": \"" << std::get<2>((*j).second[n]) << "\"";
					first = false;
				}
			}
			llvm::outs() << "\n";
			llvm::outs() << "\t\t\t}\n";
			llvm::outs() << Indent << "\t}";
			if (n+1<=max_count-1) llvm::outs() << ",";
			llvm::outs() << "\n";
		}
		++i;
		if (i!=FTVM.end()) llvm::outs() << ",\n";
	}
}

void FOPSClassConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
	  Visitor.TraverseDecl(Context.getTranslationUnitDecl());
	  llvm::outs() << "{\n";
	  llvm::outs() << "\t\"sourcen\": " << 1 << ",\n";
	  llvm::outs() << "\t\"sources\": [\n\t\t{ \"" << multi::files.at(file_id) << "\" : " << 0 << " }\n\t],\n";
	  llvm::outs() << "\t\"varn\": " << Visitor.getVarNum() << ",\n";
	  llvm::outs() << "\t\"vars\":" << "\n";
	  printVarMaps(1);
	  llvm::outs() << "\n}\n";
  }
