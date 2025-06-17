#include "main.hpp"
#include "dbjson.hpp"
#include "compat.h"
#include "clang/AST/RecordLayout.h"

void DbJSONClassVisitor::notice_class_references(RecordDecl* rD) {
	for(auto D : rD->decls()){
		if (D->isImplicit())
			continue;
		DBG(DEBUG_NOTICE, llvm::outs() << "noticeDeclClass(" << D->getDeclKindName() << ")\n");
		switch (D->getKind()) {
			case Decl::Enum:
			{
				EnumDecl* innerD = cast<EnumDecl>(D);
				QualType T = Context.getEnumType(innerD);
				DBG(DEBUG_NOTICE, llvm::outs() << innerD->isCompleteDefinition() << "\n");
				noticeTypeClass(T);
				if (innerD->isCompleteDefinition()) {
					if (innerD->getIdentifier()) {
						recordIdentifierStack.back()->first.push_back(std::string("__!enumdecl__"));
					}
					else {
						recordIdentifierStack.back()->first.push_back(std::string("__!anonenum__"));
					}
				}
				break;
			}
			case Decl::CXXRecord:
			case Decl::Record:
			{
				RecordDecl* innerD = cast<RecordDecl>(D);
				QualType T = Context.getRecordType(innerD);
				DBG(DEBUG_NOTICE, llvm::outs() << innerD->isCompleteDefinition() << "\n");
				noticeTypeClass(T);
				if (innerD->isCompleteDefinition()) {
					if (innerD->getIdentifier()) {
						recordIdentifierStack.back()->first.push_back(std::string("__!recorddecl__"));
					}
					else {
						const RecordType *tp = cast<RecordType>(T);
						if (hasNamedFields(tp->getDecl())||(emptyRecordDecl(tp->getDecl()))) {
							recordIdentifierStack.back()->first.push_back(std::string("__!anonrecord__"));
						}
						else {
							recordIdentifierStack.back()->first.push_back(std::string("__!emptyrecord__"));
						}
					}
				}
				break;
			}
			case Decl::Field:
			{
				FieldDecl* innerD = cast<FieldDecl>(D);
				if (innerD->getIdentifier()) {
					recordIdentifierStack.back()->first.push_back(innerD->getIdentifier()->getName().str());
				}
				else {
					recordIdentifierStack.back()->first.push_back(std::string("__!unnamed__"));
				}
				QualType T = innerD->getType();
				noticeTypeClass(T);
				break;
			}
			case Decl::ClassTemplate:
			{
				ClassTemplateDecl* innerD = cast<ClassTemplateDecl>(D);
				CXXRecordDecl* RD = innerD->getTemplatedDecl();
				CXXRecordDecl* pRD = static_cast<CXXRecordDecl*>(rD);
				if (recordParentDeclMap.find(RD)==recordParentDeclMap.end()) {
					recordParentDeclMap.insert(std::pair<CXXRecordDecl*,CXXRecordDecl*>(RD,pRD));
				}
				recordIdentifierStack.back()->first.push_back(std::string("__!templatedecl__"));
				break;
			}
			case Decl::ClassTemplatePartialSpecialization:
			{
				ClassTemplatePartialSpecializationDecl* innerD = cast<ClassTemplatePartialSpecializationDecl>(D);
				CXXRecordDecl* RD = static_cast<CXXRecordDecl*>(innerD);
				CXXRecordDecl* pRD = static_cast<CXXRecordDecl*>(rD);
				if (recordParentDeclMap.find(RD)==recordParentDeclMap.end()) {
					recordParentDeclMap.insert(std::pair<CXXRecordDecl*,CXXRecordDecl*>(RD,pRD));
				}
				break;
			}
			case Decl::CXXMethod:
			{
				// TODO:
			}
			break;
			case Decl::FunctionTemplate:
			{
				// TODO:
			}
			break;
			case Decl::Typedef:
			{
				TypedefDecl* innerD = cast<TypedefDecl>(D);
				VisitTypedefDeclFromClass(innerD);
				recordIdentifierStack.back()->first.push_back(std::string("__!typedefdecl__"));
			}
			break;
			case Decl::TypeAlias:
			{
				TypeAliasDecl* TAD = cast<TypeAliasDecl>(D);
				VisitTypeAliasDeclFromClass(TAD);
				recordIdentifierStack.back()->first.push_back(std::string("__!typealiasdecl__"));
			}
			break;
			case Decl::TypeAliasTemplate:
			{
				TypeAliasTemplateDecl* TATD = cast<TypeAliasTemplateDecl>(D);
				TypeAliasDecl* TAD = TATD->getTemplatedDecl();
				VisitTypeAliasDeclFromClass(TAD);
				recordIdentifierStack.back()->first.push_back(std::string("__!typealiastemplatedecl__"));
			}
			break;
			case Decl::CXXConstructor:
				break;
			default:
			{
				std::stringstream DN;
				DN << D->getDeclKindName() << "Decl";
				unsupportedDeclClass.insert(std::pair<Decl::Kind,std::string>(D->getKind(),DN.str()));
				break;
			}
		}			
	}
}

void DbJSONClassVisitor::notice_template_class_references(CXXRecordDecl* TRD) {
	for(auto D : TRD->decls()){
		if (D->isImplicit())
		continue;
	DBG(DEBUG_NOTICE, llvm::outs() << "@noticeTemplateDeclClass(" << D->getDeclKindName() << ")\n");
	switch (D->getKind()) {
		case Decl::Enum:
		{
			EnumDecl* innerD = cast<EnumDecl>(D);
			QualType T = Context.getEnumType(innerD);
			DBG(DEBUG_NOTICE, llvm::outs() << innerD->isCompleteDefinition() << "\n");
			noticeTypeClass(T);
			if (innerD->isCompleteDefinition()) {
				if (innerD->getIdentifier()) {
					recordIdentifierStack.back()->first.push_back(std::string("__!enumdecl__"));
				}
				else {
					recordIdentifierStack.back()->first.push_back(std::string("__!anonenum__"));
				}
			}
			break;
		}
		case Decl::CXXRecord:
		{
			CXXRecordDecl* innerD = cast<CXXRecordDecl>(D);
			QualType T = Context.getRecordType(innerD);
			DBG(DEBUG_NOTICE, llvm::outs() << innerD->isCompleteDefinition() << "\n");
			noticeTypeClass(T);
			if (innerD->isCompleteDefinition()) {
				if (innerD->getIdentifier()) {
					recordIdentifierStack.back()->first.push_back(std::string("__!recorddecl__"));
				}
				else {
					const RecordType *tp = cast<RecordType>(T);
					if (hasNamedFields(tp->getDecl())||(emptyRecordDecl(tp->getDecl()))) {
						recordIdentifierStack.back()->first.push_back(std::string("__!anonrecord__"));
					}
					else {
						recordIdentifierStack.back()->first.push_back(std::string("__!emptyrecord__"));
					}
				}
			}
			break;
		}
		case Decl::Field:
		{
			FieldDecl* innerD = cast<FieldDecl>(D);
			if (innerD->getIdentifier()) {
				recordIdentifierStack.back()->first.push_back(innerD->getIdentifier()->getName().str());
			}
			else {
				recordIdentifierStack.back()->first.push_back(std::string("__!unnamed__"));
			}
			QualType T = innerD->getType();
			noticeTypeClass(T);
			break;
		}
		case Decl::ClassTemplate:
		{
			ClassTemplateDecl* innerD = cast<ClassTemplateDecl>(D);
			CXXRecordDecl* RD = innerD->getTemplatedDecl();
			auto RDi = recordParentDeclMap.find(RD);
			assert((RDi==recordParentDeclMap.end())||((*RDi).second==TRD));
			recordParentDeclMap.insert(std::pair<CXXRecordDecl*,CXXRecordDecl*>(RD,TRD));
			recordIdentifierStack.back()->first.push_back(std::string("__!templatedecl__"));
			break;
		}
		case Decl::ClassTemplatePartialSpecialization:
		{
			ClassTemplatePartialSpecializationDecl* innerD = cast<ClassTemplatePartialSpecializationDecl>(D);
			CXXRecordDecl* RD = static_cast<CXXRecordDecl*>(innerD);
			auto RDi = recordParentDeclMap.find(RD);
			assert((RDi==recordParentDeclMap.end())||((*RDi).second==TRD));
			recordParentDeclMap.insert(std::pair<CXXRecordDecl*,CXXRecordDecl*>(RD,TRD));
			recordIdentifierStack.back()->first.push_back(std::string("__!templatedecl__"));
			break;
		}
		case Decl::CXXMethod:
		{
			// TODO:
		}
		break;
		case Decl::FunctionTemplate:
		{
			// TODO:
		}
		break;
		case Decl::Typedef:
		{
			TypedefDecl* innerD = cast<TypedefDecl>(D);
			VisitTypedefDeclFromClass(innerD);
			recordIdentifierStack.back()->first.push_back(std::string("__!typedefdecl__"));
		}
		break;
		case Decl::TypeAlias:
		{
			TypeAliasDecl* TAD = cast<TypeAliasDecl>(D);
			VisitTypeAliasDeclFromClass(TAD);
			recordIdentifierStack.back()->first.push_back(std::string("__!typealiasdecl__"));
		}
		break;
		case Decl::TypeAliasTemplate:
		{
			TypeAliasTemplateDecl* TATD = cast<TypeAliasTemplateDecl>(D);
			TypeAliasDecl* TAD = TATD->getTemplatedDecl();
			VisitTypeAliasDeclFromClass(TAD);
			recordIdentifierStack.back()->first.push_back(std::string("__!typealiastemplatedecl__"));
		}
		break;
		case Decl::CXXConstructor:
			break;
		default:
			std::stringstream DN;
			DN << D->getDeclKindName() << "TemplateDecl";
			unsupportedDeclClass.insert(std::pair<Decl::Kind,std::string>(D->getKind(),DN.str()));
			break;
		}
	}
}

void DbJSONClassVisitor::notice_field_attributes(RecordDecl* rD, std::vector<QualType>& QV) {
	for(auto D : rD->decls()){
		if (D->isImplicit())
			continue;
		DBG(DEBUG_NOTICE, llvm::outs() << "noticeDeclAttributes(" << D->getDeclKindName() << ")\n");
		if (D->hasAttrs()) {
			const AttrVec& V = D->getAttrs();
			for (auto ai=V.begin(); ai!=V.end();) {
				const Attr* a = *ai;
				lookForAttrTypes(a,QV);
				++ai;
			}
		}
		switch (D->getKind()) {
			case Decl::Enum:
			{
				EnumDecl* innerD = cast<EnumDecl>(D);
				break;
			}
			case Decl::CXXRecord:
			case Decl::Record:
			{
				RecordDecl* innerD = cast<RecordDecl>(D);
				notice_field_attributes(innerD,QV);
				break;
			}
			case Decl::Field:
			{
				FieldDecl* innerD = cast<FieldDecl>(D);
				break;
			}
			case Decl::ClassTemplate:
			{
				ClassTemplateDecl* innerD = cast<ClassTemplateDecl>(D);
				break;
			}
			case Decl::ClassTemplatePartialSpecialization:
			{
				ClassTemplatePartialSpecializationDecl* innerD = cast<ClassTemplatePartialSpecializationDecl>(D);
				break;
			}
			case Decl::CXXMethod:
			case Decl::CXXConstructor:
				break;
			case Decl::Typedef:
			case Decl::TypeAlias:
			case Decl::TypeAliasTemplate:
				break;
			default:
			{
				std::stringstream DN;
				DN << D->getDeclKindName() << "Decl";
				unsupportedDeclClass.insert(std::pair<Decl::Kind,std::string>(D->getKind(),DN.str()));
				break;
			}
		}
	}
}

  void DbJSONClassVisitor::noticeTemplateParameters(TemplateParameterList* TPL) {
  	for (unsigned i = 0, e = TPL->size(); i != e; ++i) {
  		Decl *Param = TPL->getParam(i);
  		if (auto TTP = dyn_cast<TemplateTypeParmDecl>(Param)) {
  			noticeTypeClass(Context.getTemplateTypeParmType(TTP->getDepth(),TTP->getIndex(),TTP->isParameterPack()));
  			if (TTP->hasDefaultArgument()) {
  				noticeTypeClass(TTP->getDefaultArgument());
  			}
  		} else if (auto NTTP = dyn_cast<NonTypeTemplateParmDecl>(Param)) {
  			noticeTypeClass(NTTP->getType());
  		  }
  		else if (auto TTPD = dyn_cast<TemplateTemplateParmDecl>(Param)) {
  			// Probably will never be done
  		}
  	}
  }

  void DbJSONClassVisitor::noticeTemplateArguments(const TemplateArgumentList& Args) {
	  for (size_t I = 0, E = Args.size(); I < E; ++I) {
	      const TemplateArgument &A = Args[I];
	      if (A.getKind() == TemplateArgument::Type) {
	    	  QualType T = A.getAsType();
	    	  const TemplateTypeParmType* TTP = T->getAs<TemplateTypeParmType>();
	    	  if (TTP) {
				//   assert(!TTP->isCanonicalUnqualified() && "Only canonical template parm type2");
	    	  }
	    	  noticeTypeClass(T);
	    	  
	      }
	      else if (A.getKind() == TemplateArgument::Template) {
	    	  // Not yet
	      }
	      else if (A.getKind() == TemplateArgument::Expression) {
	    	  // No type to notice
	      }
	      else if (A.getKind() == TemplateArgument::NullPtr) {
	    	  // No type to notice
	      }
	      else if (A.getKind() == TemplateArgument::Integral) {
	    	  // No type to notice
	      }
	      else {
	    	  if (opts.exit_on_error) {
					llvm::outs() << "\nERROR: Unsupported TemplateArgument Kind: " << A.getKind() << "\n";
					A.dump(llvm::outs());
					exit(EXIT_FAILURE);
				}
	      }
	  }
  }

  void DbJSONClassVisitor::noticeTypeClass(QualType T) {
	  T = typeForMap(T);
	  if(TypeMap.find(T) != TypeMap.end())
	  	  return;

	  size_t size = 0;
	  if(!T->isIncompleteType() && !T->isDependentType() && !isa<BuiltinType>(T) && !isa<AutoType>(T)){
		size = Context.getTypeSize(T);
	  }
	//   auto optsize = Context.getTypeSizeInCharsIfKnown(T);
	//   if(optsize){
	// 	size = Context.toBits(optsize.compatGetValue());
	// 	assert(Context.getTypeSize(T) == size && "size mismatch");
	//   }
	  std::string qualifierString = getQualifierString(T);
	  switch(T->getTypeClass()) {
		  // skipped types
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
			  llvm::errs()<<"Skipped type "<<T->getTypeClassName()<<" added to TypeMap\n";
			  T.dump();
			  assert(0 && "Unreachable");
		  }
	  	  case Type::Builtin:
		  {
			  const BuiltinType *tp = cast<BuiltinType>(T);
			  if(tp->getKind() != BuiltinType::BoundMember && tp->getKind() != BuiltinType::Dependent){
				size = Context.getTypeSize(T);
			  }
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice Builtin (" <<
					  tp->getName(Context.getPrintingPolicy()).str() << ")(" << qualifierString << ")\n";
			  	  	  tp->dump() );
		  }
		  break;
		  case Type::Pointer:
		  {
			  const PointerType *tp = cast<PointerType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice Pointer (" << qualifierString << ")\n";
			  	  	  tp->dump() );
			  QualType ptrT = tp->getPointeeType();
			  noticeTypeClass(ptrT);
		  }
		  break;
		  case Type::MemberPointer:
		  {
			  const MemberPointerType *tp = cast<MemberPointerType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice MemberPointer (" << qualifierString << ")\n";
			  	  	  tp->dump() );
			  QualType pT = tp->getPointeeType();
			  noticeTypeClass(pT);
			  QualType _class = QualType(tp->getClass(),0);
			  noticeTypeClass(_class);
		  }
		  break;
		  case Type::Complex:
		  {
			  const ComplexType *tp = cast<ComplexType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice Complex (" << qualifierString << ")\n";
			  	  	  tp->dump() );
			  QualType eT = tp->getElementType();
			  noticeTypeClass(eT);
		  }
		  break;
		  case Type::Vector:
		  {
			  const VectorType *tp = cast<VectorType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice Vector (" << qualifierString << ")\n";
			  	  	  tp->dump() );
			  QualType eT = tp->getElementType();
			  noticeTypeClass(eT);
		  }
		  break;
		  case Type::ExtVector:
		  {
			  const ExtVectorType *tp = cast<ExtVectorType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice ExtVector (" << qualifierString << ")\n";
			  	  	  tp->dump() );
			  QualType eT = tp->getElementType();
			  noticeTypeClass(eT);
		  }
		  break;
		  case Type::TemplateTypeParm:
		  {
			  const TemplateTypeParmType *tp = cast<TemplateTypeParmType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice TemplateTypeParm (" << qualifierString << ")\n"; tp->dump() );
		  }
		  break;
		  case Type::RValueReference:
		  {
			  const RValueReferenceType *tp = cast<RValueReferenceType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice RValueReference (" << qualifierString << ")\n";
			  	  	  tp->dump() );
			  QualType rvrT = tp->getPointeeType();
			  noticeTypeClass(rvrT);
		  }
		  break;
		  case Type::LValueReference:
		  {
			  const LValueReferenceType *tp = cast<LValueReferenceType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice LValueReference (" << qualifierString << ")\n";
			  	  	  tp->dump() );
			  QualType lvrT = tp->getPointeeType();
			  noticeTypeClass(lvrT);
		  }
		  break;
		  case Type::DependentSizedArray:
		  {
			  const DependentSizedArrayType *tp = cast<DependentSizedArrayType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice DependentSizedArray (" << qualifierString << ")\n";
			  	  	  tp->dump() );
			  QualType elT = tp->getElementType();
			  noticeTypeClass(elT);
		  }
		  break;
		  case Type::PackExpansion:
		  {
			  const PackExpansionType *tp = cast<PackExpansionType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice PackExpansion (" << qualifierString << ")\n";
			  	  	  tp->dump() );
		  }
		  break;
		  case Type::DependentName:
		  {
			  const DependentNameType *tp = cast<DependentNameType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice DependentName (" << qualifierString << ")\n";
			  	  	  tp->dump() );
		  }
		  break;
		  case Type::UnresolvedUsing:
		  {
			  const UnresolvedUsingType *tp = cast<UnresolvedUsingType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice UnresolvedUsing (" << qualifierString << ")\n";
			  	  	  tp->dump() );
		  }
		  break;
		  case Type::TemplateSpecialization:
		  {
			  const TemplateSpecializationType *tp = cast<TemplateSpecializationType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice TemplateSpecialization (" << qualifierString << ")\n"; tp->dump();
			  llvm::outs() << "T_Kind: " << tp->getTemplateName().getKind() << "\n"; tp->getTemplateName().dump(); llvm::outs() << "\n";
			  if (tp->getTemplateName().getKind()==TemplateName::Template) {
				  tp->getTemplateName().getAsTemplateDecl()->dumpColor();
			  } );

			  assert(!tp->isSugared() && "Skipped sugar TemplateSpecialization type added to database");

			  for (const TemplateArgument &Arg : tp->template_arguments()) {
				  if (Arg.getKind() == TemplateArgument::Type) {
					  QualType TPT = Arg.getAsType();
					  noticeTypeClass(TPT);
				  }
			  }
		  }
		  break;
		  case Type::DependentTemplateSpecialization:
		  {
			  const DependentTemplateSpecializationType *tp = cast<DependentTemplateSpecializationType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice DependentTemplateSpecialization (" << qualifierString << ")\n";
				  tp->dump() );
			  assert(!tp->isSugared() && "DependentTemplateSpecialization never sugar error");

			  for (const TemplateArgument &Arg : tp->template_arguments()) {
				  if (Arg.getKind() == TemplateArgument::Type) {
					  QualType TPT = Arg.getAsType();
					//   FIXME: hash issue for other cases
					  if(TPT->getTypeClass() == Type::TemplateTypeParm)
					  	noticeTypeClass(TPT);
				  }
			  }
		  }
		  break;
		  case Type::InjectedClassName:
		  {
			  const InjectedClassNameType* IT = cast<InjectedClassNameType>(T);

			  CXXRecordDecl* TRD = IT->getDecl();
			  const ClassTemplateDecl* CTD = TRD ->getDescribedClassTemplate();
			  const ClassTemplatePartialSpecializationDecl* CTPS = 0;
			  if(isa<ClassTemplatePartialSpecializationDecl>(TRD))
				  CTPS = cast<ClassTemplatePartialSpecializationDecl>(TRD);

			//   if (classTemplateMap.find(TRD)!=classTemplateMap.end()) {
			// 	  CTD = classTemplateMap[TRD];
			//   }
			//   else {
			// 	  if (classTemplatePartialSpecializationMap.find(TRD)!=classTemplatePartialSpecializationMap.end()) {
			// 		  CTPS = classTemplatePartialSpecializationMap[TRD];
			// 	  }
			// 	  else {
			// 		  if (isa<ClassTemplatePartialSpecializationDecl>(TRD)) {
			// 			  // VisitClassTemplateSpecializationDecl might've not yet been called
			// 			  CTPS = static_cast<const ClassTemplatePartialSpecializationDecl*>(TRD);
			// 		  }
			// 	  }
			//   }

			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice InjectedClassName (" <<
					  T.getAsString() << ") " << "[" << TRD << "] " << CTD << " " << CTPS << "\n";
			  	  	  T.dump() );

			  if (CTD) {
				  noticeTemplateParameters(CTD->getTemplateParameters());
			  }
			  else if (CTPS) {
				  // Notice template type parameters and arguments
				  noticeTemplateParameters(CTPS->getTemplateParameters());

				  // TODO removed likely unnecessary code, some issues may arise
				  noticeTemplateArguments(CTPS->getTemplateArgs());
			  }
			  TypeMap.insert(std::pair<QualType,TypeData>(T, {{},T,size,0}));
			  TypeMap.at(T).id.setID(TypeNum);
			  DBG(DEBUG_NOTICE, llvm::outs() << "TypeMap[" << IT << "]RT = " << TypeNum << "\n" );
			  TypeNum++;
			  if (TRD->isCompleteDefinition()) {
				  recordInfo_t* recordInfo = new recordInfo_t;
				  recordIdentifierStack.push_back(recordInfo);
				  notice_template_class_references(TRD);
				  TypeMap[T].RInfo = recordInfo;
				  recordIdentifierStack.pop_back();
			  }
			  /* Do not save record template forward types into database; it should be resolved into proper record template
			   *  in this translation unit; if not fix would be needed here
			   */
			  return;
		  }
		  case Type::Record:
		  {
			  const RecordType *tp = cast<RecordType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice Record (" <<
					  T.getAsString() << ")";
			  	  	  if (tp->isDependentType()) llvm::outs() << " dependent";
			  	  	  if (!tp->getDecl()->isCompleteDefinition()) llvm::outs() << " not complete";
			  	  	  if (isa<CXXRecordDecl>(tp->getDecl())) llvm::outs() << " CXX";
			  	  	  if (tp->getDecl()->getIdentifier()) llvm::outs() << " I(" << tp->getDecl()->getIdentifier()->getName().str() << ")";
			  	  	  llvm::outs() << "\n";
			  	  	  T.dump() );

			  RecordDecl* rD = tp->getDecl();
			  const IdentifierInfo *II = rD->getIdentifier();

			  TypeMap.insert(std::pair<QualType,TypeData>(T, {{},T,size,0}));
			  TypeMap.at(T).id.setID(TypeNum);
			  DBG(DEBUG_NOTICE, llvm::outs() << "TypeMap[" << tp << "]R = " << TypeNum << "\n" );
			  TypeNum++;

			  if (isa<CXXRecordDecl>(rD)) {
				CXXRecordDecl* cpprD =  cast<CXXRecordDecl>(rD);
				if(isa<ClassTemplateSpecializationDecl>(cpprD)){
					ClassTemplateSpecializationDecl* CTSD = cast<ClassTemplateSpecializationDecl>(cpprD);
					noticeTemplateArguments(CTSD->getTemplateArgs());
				}
			  }

			  if (rD->isCompleteDefinition()) {
				  if (II) {
					  DBG(opts.debug2, llvm::outs() << "notice Record(" << II->getName().str() << ")(" << qualifierString << ")" <<
							  " [" << tp << "]\n" );
				  }
				  else {
					  DBG(opts.debug2, llvm::outs() << "notice Record()(" << qualifierString << ")" <<
							  " [" << tp << "]\n" );
				  }
				  if (opts.debug2) {
					  tp->dump();
				  }
				  recordInfo_t* recordInfo = new recordInfo_t;
				  recordIdentifierStack.push_back(recordInfo);
				  notice_class_references(rD);
				  if (!tp->isDependentType()) {
					  const ASTRecordLayout& rL = Context.getASTRecordLayout(rD);
					  for (size_t u=0; u<rL.getFieldCount(); ++u) {
						  uint64_t off = rL.getFieldOffset(u);
						  recordInfo->second.push_back(off);
					  }
				  }
				  TypeMap[T].RInfo = recordInfo;
				  DBG(opts.debug2, llvm::outs() << "T[" << tp << "]: " );
				  DBG(opts.debug2,
						  for (auto i=recordInfo->first.begin(); i!=recordInfo->first.end(); ++i) {
							  llvm::outs() << *i << " ";
						  } llvm::outs() << "\n" );
				  recordIdentifierStack.pop_back();
			  }
			  else {
				  assert(II && "Record forward without a declarator name");
				  DBG(DEBUG_NOTICE, llvm::outs() << "notice Record forward (" << II->getName().str() << ")(" << qualifierString << ")\n" );
			  }
		  }
		  break;
		  case Type::ConstantArray:
		  {
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice ConstantArray (" << qualifierString << ")\n";
			  	  	  T.dump() );
			  const ConstantArrayType *tp = cast<ConstantArrayType>(T);
			  QualType elT = tp->getElementType();
			  noticeTypeClass(elT);
		  }
		  break;
		  case Type::IncompleteArray:
		  {
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice IncompleteArray (" << qualifierString << ")\n";
			  	  	  T.dump() );
			  const IncompleteArrayType *tp = cast<IncompleteArrayType>(T);
			  QualType elT = tp->getElementType();
			  noticeTypeClass(elT);
		  }
		  break;
		  case Type::VariableArray:
		  {
			  DBG(DEBUG_NOTICE, llvm::outs() << "notice @VariableArray (" << qualifierString << ")\n";
			  	  	  T.dump() );
			  const VariableArrayType *tp = cast<VariableArrayType>(T);
			  QualType vaT = tp->getElementType();
			  noticeTypeClass(vaT);
		  }
		  break;
		  case Type::Typedef:
		  {
			  const TypedefType *tp = cast<TypedefType>(T);
			  TypedefNameDecl* D = tp->getDecl();
			  IdentifierInfo *II = D->getIdentifier();
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice Typedef (" << II->getName().str() << ")(" << qualifierString << ")\n";
			  	  	  T.dump() );
			  QualType tT = D->getTypeSourceInfo()->getType();
			  noticeTypeClass(tT);
		  }
		  break;
		  case Type::Enum:
		  {
			  const EnumType *tp = cast<EnumType>(T);
			  QualType eT = tp->getDecl()->getIntegerType();
			  EnumDecl* eD = tp->getDecl();
			  const IdentifierInfo *II = eD->getIdentifier();
			  if (eD->isCompleteDefinition()) {
				  if (II) {
					  DBG(DEBUG_NOTICE, llvm::outs() << "@notice Enum(" << II->getName().str() << ")(" << qualifierString << ")\n";
					  	  	  T.dump() );
				  }
				  else {
					  DBG(DEBUG_NOTICE, llvm::outs() << "@notice Enum()(" << qualifierString << ")\n";
					  	  	  T.dump() );
				  }
				  noticeTypeClass(eT);
			  }
			  else {
				  assert(II && "Enum forward without a declarator name");
				  DBG(DEBUG_NOTICE, llvm::outs() << "@notice Enum forward (" << II->getName().str() << ")(" << qualifierString << ")\n";
				  	  	  T.dump() );
			  }
		  }
		  break;
		  case Type::Auto:
		  {
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice Auto (" << qualifierString << ")\n";
			  	  	  T.dump() );
			  const AutoType *tp = cast<AutoType>(T);
			  assert(!tp->isSugared() && "Skipped sugar Auto type added to database");
		  }
		  break;
		  case Type::FunctionProto:
		  {
			  const FunctionProtoType *tp = cast<FunctionProtoType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice FunctionProto[" << tp->getNumParams() << "] (" << qualifierString << ")\n";
			  	  	  T.dump());
			  TypeMap.insert(std::pair<QualType,TypeData>(T, {{},T,size,0}));
			  TypeMap.at(T).id.setID(TypeNum);

			  DBG(DEBUG_NOTICE, llvm::outs() << "TypeMap[" << T.getAsOpaquePtr() << "]F = " << TypeNum << "\n" );
			  TypeNum++;
			  noticeTypeClass(tp->getReturnType());
			  for (unsigned i = 0; i<tp->getNumParams(); ++i) {
				  noticeTypeClass(tp->getParamType(i));
			}
		  }
		  break;
		  case Type::FunctionNoProto:
		  {
			  const FunctionNoProtoType *tp = cast<FunctionNoProtoType>(T);
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice FunctionNoProto[] (" << qualifierString << ")\n";
			  	  	  T.dump());
			  noticeTypeClass(tp->getReturnType());
		  }
		  break;
		  default:
		  {
			  DBG(DEBUG_NOTICE, llvm::outs() << "@notice unknown (" << T->getTypeClassName() << ")\n";
			  	  	  T.dump());
			  missingTypes.insert(std::pair<Type::TypeClass,std::string>(T->getTypeClass(),std::string(T->getTypeClassName())));
		  }
		  return;
	  }

	  if (TypeMap.find(T)==TypeMap.end()) {
		  TypeMap.insert(std::pair<QualType,TypeData>(T, {{},T,size,0}));
		  TypeMap.at(T).id.setID(TypeNum);
		  DBG(DEBUG_NOTICE, llvm::outs() << "TypeMap[" << T.getAsOpaquePtr() << "]E = " << TypeNum << "\n" );
		  TypeNum++;
	  }
  }


// type interface for TypeMap
QualType DbJSONClassVisitor::typeForMap(QualType T){
	unsigned int quals = T.getLocalFastQualifiers();
	switch(T->getTypeClass()){
		//add type as is
		case Type::Builtin:
		case Type::Pointer:
		case Type::MemberPointer:
		case Type::Complex:
		case Type::Vector:
		case Type::ExtVector:
		case Type::TemplateTypeParm:
		case Type::RValueReference:
		case Type::LValueReference:
		case Type::DependentSizedArray:
		case Type::PackExpansion:
		case Type::DependentName:
		case Type::UnresolvedUsing:
		case Type::DependentTemplateSpecialization:
		case Type::InjectedClassName: //TODO: add fringe case checks(removed cases)
		case Type::Record: //TODO: add fringe case checks(removed cases)
		case Type::ConstantArray:
		case Type::IncompleteArray:
		case Type::Typedef:
		case Type::Enum:
		case Type::FunctionProto:
		case Type::FunctionNoProto:
			return T;

		// add one per element type	
		case Type::VariableArray:{
			auto tp = cast<VariableArrayType>(T);
			QualType eT = tp->getElementType();
			if(vaTMap.find(eT) == vaTMap.end())
				vaTMap.insert({eT,T});
			return vaTMap.at(eT);
		}

		// add only if not sugar
		case Type::TemplateSpecialization:{
			auto tp = cast<TemplateSpecializationType>(T);
			if(tp->isSugared())
				return typeForMap(tp->desugar());
			else
				return T;
		}
		case Type::DeducedTemplateSpecialization:{
			auto tp = cast<DeducedTemplateSpecializationType>(T);
			if(tp->isSugared())
				return typeForMap(tp->desugar());
			else
				return T;
		}
		case Type::Auto:{
			auto tp = cast<AutoType>(T);
			if(tp->isSugared())
				return typeForMap(tp->desugar().withFastQualifiers(quals));
			else{
				return T;
			}
		}

		//never add
		case Type::MacroQualified:{
			auto tp = cast<MacroQualifiedType>(T);
			// TODO: forward qualifiers
			//assert(0 && "Found MacroQualified");
			return typeForMap(tp->getUnderlyingType());
		}
		case Type::Attributed:{
			auto tp = cast<AttributedType>(T);
			// TODO: handle type properly
			//assert(0 && "Found Attributed");
			return typeForMap(tp->getEquivalentType());
		}
		COMPAT_VERSION_GE(15,
		case Type::BTFTagAttributed:{
			auto tp = cast<BTFTagAttributedType>(T);
			return typeForMap(tp->getWrappedType());
		}
		)
		case Type::UnaryTransform:{
			auto tp = cast<UnaryTransformType>(T);
			assert(tp->getUTTKind() == UnaryTransformType::EnumUnderlyingType && "Not an enum!");
			if(tp->isSugared())
				return typeForMap(tp->getUnderlyingType().withFastQualifiers(quals));
			else
			// clang implementation doesn't match decribed behaviour, add BaseType for now
				return typeForMap(tp->getBaseType().withFastQualifiers(quals));
		}
		case Type::Atomic:{
			auto tp = cast<AtomicType>(T);
			return typeForMap(tp->getValueType().withFastQualifiers(quals));
		}
		case Type::Elaborated:{
			auto tp = cast<ElaboratedType>(T);
			return typeForMap(tp->getNamedType().withFastQualifiers(quals));
		}
		case Type::Decltype:{
			auto tp = cast<DecltypeType>(T);
			// sometimes builtin dependent
			return typeForMap(tp->getUnderlyingType().withFastQualifiers(quals));
		}
		case Type::SubstTemplateTypeParm:{
			auto tp = cast<SubstTemplateTypeParmType>(T);
			return typeForMap(tp->getReplacementType().withFastQualifiers(quals));
		}
		case Type::Paren:{
			auto tp = cast<ParenType>(T);
			// TODO: forward qualifiers (never has any?)
			assert(!quals && "qualified paren type");
			return typeForMap(tp->getInnerType());
		}
		case Type::TypeOfExpr:{
			auto tp = cast<TypeOfExprType>(T);
			// sometimes builtin dependent
			return typeForMap(tp->getUnderlyingExpr()->getType().withFastQualifiers(quals));
		}
		case Type::TypeOf:{
			auto tp = cast<TypeOfType>(T);
			return typeForMap(tp->getUnmodifiedType().withFastQualifiers(quals));
		}
		case Type::Decayed:{
			auto tp = cast<DecayedType>(T);
			return typeForMap(tp->getDecayedType());
		}
		COMPAT_VERSION_GE(14,
		case Type::Using:{
			auto tp = cast<UsingType>(T);
			return typeForMap(tp->getUnderlyingType());
		}
		)
		
		//unhandled types
		default:{
			llvm::errs()<<"Unhandled type: "<<T->getTypeClassName()<<'\n';
			T.dump();
			llvm_unreachable("Unhandled type");
		}
	}

	return T;
}

//fops
const VarDecl* DbJSONClassVisitor::findFopsVar(const Expr *E){
	bool ConsumeAddress = false;
	const Expr *Base = E;
	while(true){
		switch(E->getStmtClass()){
			case Stmt::CallExprClass:
			case Stmt::StmtExprClass:
				// unsupported
				return nullptr;
			case Stmt::ImplicitCastExprClass:
				E = cast<ImplicitCastExpr>(E)->getSubExpr();
				break;
			case Stmt::CStyleCastExprClass:
				E = cast<CStyleCastExpr>(E)->getSubExpr();
				break;
			case Stmt::ParenExprClass:
				E = cast<ParenExpr>(E)->getSubExpr();
				break;
			case Stmt::DeclRefExprClass:{
				auto D = cast<DeclRefExpr>(E)->getDecl();
				if(isa<VarDecl>(D)){
					return cast<VarDecl>(D);
				}
				return nullptr;
			}
			case Stmt::MemberExprClass:{
				E = cast<MemberExpr>(E)->getBase();
				break;
			}
			case Stmt::UnaryOperatorClass:{
				E = cast<UnaryOperator>(E)->getSubExpr();
				break;
			}
			case Stmt::ArraySubscriptExprClass:
				E = cast<ArraySubscriptExpr>(E)->getBase();
				break;

			default:{
				if(opts.exit_on_error){
					llvm::errs()<<"[unhandled]"<<E->getStmtClassName()<<'\n';
					E->dump();
					E->printPretty(llvm::errs(),nullptr,Context.getPrintingPolicy());
					llvm::errs()<<E->getBeginLoc().printToString(Context.getSourceManager())<<'\n';
					assert(0);
				}
				return nullptr;
			}
		}
	}
}

void DbJSONClassVisitor::noticeFopsFunction(FopsObject &FObj, int field_index, const Expr *E){
	E = E->IgnoreParenCasts();
	switch(E->getStmtClass()){
	case Stmt::ImplicitValueInitExprClass:
	case Stmt::IntegerLiteralClass:
	case Stmt::StringLiteralClass:
	case Stmt::MemberExprClass:
	case Stmt::CallExprClass:
	case Stmt::ArraySubscriptExprClass:
	case Stmt::StmtExprClass:
	case Stmt::CompoundLiteralExprClass:
	case Stmt::OffsetOfExprClass:
		//cases that don't directly resolve to functions
		break;
	case Stmt::OpaqueValueExprClass:
		noticeFopsFunction(FObj,field_index,cast<OpaqueValueExpr>(E)->getSourceExpr());
		break;
	case Stmt::DeclRefExprClass:{
		auto *Decl = cast<DeclRefExpr>(E)->getDecl();
		if(Decl->getKind() == Decl::Function){
			auto fops = FopsMap.emplace(FObj,FObj);
			fops.first->second.fops_info[field_index].insert(cast<FunctionDecl>(Decl));
		}
		break;
	}
	case Stmt::UnaryOperatorClass:{
		E = cast<UnaryOperator>(E)->getSubExpr();
		noticeFopsFunction(FObj, field_index, E);
		break;
	}
	case Stmt::BinaryOperatorClass:{
		E = cast<BinaryOperator>(E)->getRHS();
		noticeFopsFunction(FObj, field_index, E);
		break;
	}
	case Stmt::ConditionalOperatorClass:
	case Stmt::BinaryConditionalOperatorClass:{
		noticeFopsFunction(FObj, field_index, cast<AbstractConditionalOperator>(E)->getTrueExpr());
		noticeFopsFunction(FObj, field_index, cast<AbstractConditionalOperator>(E)->getFalseExpr());
		break;
	}
	default:
		if(opts.exit_on_error){
			llvm::errs()<<"[unhandled]"<<E->getStmtClassName()<<'\n';
			E->dump();
			E->printPretty(llvm::errs(),nullptr,Context.getPrintingPolicy());
			llvm::errs()<<E->getBeginLoc().printToString(Context.getSourceManager())<<'\n';
			assert(0);
		}
	}
	return;
}