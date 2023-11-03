#include "main.hpp"
#include "clang/AST/RecordLayout.h"

thread_local int short_ptr = 0;
thread_local bool autoforward = false;

  void DbJSONClassConsumer::buildTemplateArgumentsString(const TemplateArgumentList& Args,
		  std::string& typeString, std::pair<bool,unsigned long long> extraArg) {

	  buildTemplateArgumentsString(Args.asArray(),typeString,extraArg);
  }

  void DbJSONClassConsumer::buildTemplateArgumentsString(ArrayRef<TemplateArgument> Args,
  		  std::string& typeString, std::pair<bool,unsigned long long> extraArg) {

	  	  std::stringstream ss;
	  	  std::vector<std::pair<std::string,unsigned>> template_args;
		  for (size_t I = 0, E = Args.size(); I < E; ++I) {
			  const TemplateArgument &A = Args[I];
			  if (A.getKind() == TemplateArgument::Type) {
				  QualType T = A.getAsType();
				  const TemplateTypeParmType* TTP = T->getAs<TemplateTypeParmType>();
				  if (TTP) {
					  assert(!TTP->isCanonicalUnqualified() && "Only canonical template parm type");
				  }
				  std::string Tstr;
				  buildTypeString(T,Tstr);
				  template_args.push_back(std::pair<std::string,unsigned>(Tstr,I));
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
					  template_args.push_back(std::pair<std::string,unsigned>(expr,I));
				  }
			  }
			  else if (A.getKind() == TemplateArgument::NullPtr) {
				  template_args.push_back(std::pair<std::string,unsigned>("\"nullptr\"",I));
			  }
			  else if (A.getKind() == TemplateArgument::Integral) {
				  llvm::APSInt Iv = A.getAsIntegral();
				  template_args.push_back(std::pair<std::string,unsigned>("\""+compatibility::toString(Iv)+"\"",I));
			  }
			  else {
				  if (opts.exit_on_error) {
						llvm::outs() << "\nERROR: Unsupported TemplateArgument Kind: " << A.getKind() << "\n";
						A.dump(llvm::outs());
						exit(EXIT_FAILURE);
					}
			  }
		  }
		  for (auto i=template_args.begin(); i!=template_args.end();) {
			  ss << (*i).first;
			  ++i;
			  if (i!=template_args.end()) {
				  ss << ":";
			  }
		  }
		  typeString.append(ss.str());
    }


  void DbJSONClassConsumer::buildTypeGroupString(Decl** Begin, unsigned NumDecls, std::string& typeString) {

    if (NumDecls == 1) {
    	buildTypeClassString(*Begin,typeString);
      return;
    }

    Decl** End = Begin + NumDecls;
    TagDecl* TD = dyn_cast<TagDecl>(*Begin);
    if (TD)
      ++Begin;

    for ( ; Begin != End; ++Begin) {
    	buildTypeClassString(*Begin,typeString);
    }
  }

  void DbJSONClassConsumer::buildTypeClassString(Decl* D, std::string& typeString) {

	  switch (D->getKind()) {
		  case Decl::Enum:
		  {
			  EnumDecl* innerD = cast<EnumDecl>(D);
			  QualType T = innerD->getIntegerType();
			  if (innerD->isCompleteDefinition()) {
				  buildTypeString(T,typeString);
			  }
			  break;
		  }
		  case Decl::Record:
		  {
			  RecordDecl* innerD = cast<RecordDecl>(D);
			  QualType T = Context.getRecordType(innerD);
			  if (innerD->isCompleteDefinition()) {
				  buildTypeString(T,typeString);
			  }
			  break;
		  }
		  case Decl::Field:
		  {
			  FieldDecl* innerD = cast<FieldDecl>(D);
			  std::pair<int,unsigned long long> extraArg(0,0);
			  if (innerD->isBitField()) {
				  extraArg.first = EXTRA_BITFIELD;
				  extraArg.second = innerD->getBitWidthValue(Context);
			  }
			  QualType T = innerD->getType();
			  if (T->getTypeClass()==Type::Typedef) {
				  const TypedefType *tp = cast<TypedefType>(T);
				  TypedefNameDecl* D = tp->getDecl();
				  IdentifierInfo *II = D->getIdentifier();
				  if (II->getName().str()=="bool") {
					  extraArg.first |= EXTRA_TYPEDEF_BOOL;
				  }
			  }
			  buildTypeString(T,typeString,extraArg);
			  IdentifierInfo *II = innerD->getIdentifier();
			  if(II){
				  typeString.append(II->getName().str());
			  }
			  break;
		  }
		  default:
			  break;
		  }
  }

  void DbJSONClassConsumer::buildRecordTypeString(QualType T, RecordDecl* rD, std::string& qualifierString,
		  	  	  	  std::string& typeString, std::pair<int,unsigned long long> extraArg) {

		  if (TypeStringMap.find(T)!=TypeStringMap.end()) {
			  typeString.append(TypeStringMap.at(T));
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Record cached("
					<< TypeStringMap.at(T).size() << ")\n" );
			  DBG(DEBUG_HASH, llvm::outs() << "build Record cached("
					<< TypeStringMap.at(T).size() << ") [" <<
					TypeStringMap.at(T) << "]\n" );
			  return;
		  }

		  const IdentifierInfo *II = rD->getIdentifier();
		  std::string rstr;

		  //definition hash
		  short_ptr++;
		  std::string rargs;
		  const DeclContext *DC = cast<DeclContext>(rD);
		  SmallVector<Decl*, 2> Decls;
		  for (DeclContext::decl_iterator D = DC->decls_begin(), DEnd = DC->decls_end();
				  D != DEnd; ++D) {
			  if (D->isImplicit())
				  continue;
			  if(isa<RecordDecl>(*D) && !(cast<RecordDecl>(*D)->isCompleteDefinition())){
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
				  buildTypeGroupString(Decls.data(),Decls.size(),rargs);
				  Decls.clear();
			  }
			  if (isa<TagDecl>(*D) && !cast<TagDecl>(*D)->getIdentifier()) {
			  Decls.push_back(*D);
			  continue;
			  }
			  buildTypeClassString(*D,rargs);
		  }
		  if (!Decls.empty()) {
			  buildTypeGroupString(Decls.data(),Decls.size(),rargs);
			  Decls.clear();
		  }
		  short_ptr--;
		  SHA_CTX c;
		  SHA_init(&c);
		  SHA_update(&c,rargs.data(),rargs.length());
		  SHA_final(&c);
		  std::string outerFn;
		  if (!rD->isDefinedOutsideFunctionOrMethod()) {
			  std::stringstream outerStr;
			  const FunctionDecl* outerDef = static_cast<FunctionDecl*>(rD->getParentFunctionOrMethod());
			  assert(outerDef && "Cannot find function definition for outer function of type");
			  DbJSONClassVisitor::FuncData &fnfo = Visitor.getFuncMap().at(outerDef);
			  // build ahead of time
			  if(!fnfo.hash.size()) getFuncHash(&fnfo);
			  outerStr << Visitor.parentFunctionOrMethodString(rD) << ":" << fnfo.hash;
			  if (opts.cstmt) {
				  if ((Visitor.getRecordCSMap().find(rD)!=Visitor.getRecordCSMap().end()) &&
						  (Visitor.getRecordCSMap()[rD]!=0)) {
					  const CompoundStmt* CS = Visitor.getRecordCSMap()[rD];
					  if (fnfo.csIdMap.find(CS)==fnfo.csIdMap.end()) {
						  llvm::errs() << "@CS: " << CS << "\n";
						  rD->dumpColor();
						  outerDef->dumpColor();
						  assert(0 && "Missing CompoundStmt in csIdMap");
					  }
					  outerStr << "@" << fnfo.csIdMap[CS];
				  }
			  }
			  outerStr.flush();
			  outerFn = outerStr.str();
		  }

		  std::stringstream ss;
		  if (II) {
			  std::string tpII;
			  if ((isa<CXXRecordDecl>(rD)) || (isa<ClassTemplateSpecializationDecl>(rD))) {
				  tpII = T.getAsString();
			  }
			  else {
				  tpII = II->getName().str();
			  }
			  ss << "R:" << outerFn << qualifierString << ":" << tpII << ":";

			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Record(" << tpII
					  << ")(" << qualifierString << ")\n" );
			  DBG(DEBUG_HASH, llvm::outs() << "build Record(" << tpII
					  << ")(" << qualifierString << ")\n" );
		  }
		  else {
			  if (rD->getTypedefNameForAnonDecl()) {
				  ss << "R:" << outerFn << qualifierString << ":" << rD->getTypedefNameForAnonDecl()->getName().str() << ":";

				  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Record("
						  << rD->getTypedefNameForAnonDecl()->getName().str() << ")(" << qualifierString << ")\n" );
				  DBG(DEBUG_HASH, llvm::outs() << "build Record("
						  << rD->getTypedefNameForAnonDecl()->getName().str() << ")(" << qualifierString << ")\n" );
			  }
			  else {
				  ss << "R:" << outerFn << qualifierString << ":" << "!anon" << ":";
				  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Record()(" << qualifierString << ")\n" );
				  DBG(DEBUG_HASH, llvm::outs() << "build Record()(" << qualifierString << ")\n" );
			  }
		  }
		  uint64_t tpSize = 0;
		  if (!T->isDependentType()) {
			  TypeInfo ti = Context.getTypeInfo(T);
			  tpSize = ti.Width;
		  }
		  ss << base64_encode(reinterpret_cast<const unsigned char*>(c.buf.b), 64) << ":" << tpSize << ";";
		  rstr.append(ss.str());
		  TypeStringMap.insert(std::pair<QualType,std::string>(T,rstr));
		  typeString.append(rstr);
  }

  void DbJSONClassConsumer::buildTypeString(QualType T,
		  	  	  	  std::string& typeString,
					  std::pair<int,unsigned long long> extraArg) {

	  if (T.isNull()) {
		  // Special case, placeholder for empty record types
		  std::stringstream ss;
		  ss << "EMPTY:0;";
		  typeString.append(ss.str());
		  return;
	  }
	  T = Visitor.typeForMap(T);
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
			  llvm::errs()<<"Built hash for skipped type "<<T->getTypeClassName()<<"\n";
			  T.dump();
			  assert(0 && "Unreachable");
		  }
		  
		  case Type::Builtin:
		  {
			  const BuiltinType *tp = cast<BuiltinType>(T);
			  std::stringstream ss;
			  if (tp->isDependentType()) {
				  ss << "B:" << qualifierString << ":";
				  std::string bname = tp->getName(Context.getPrintingPolicy()).str();
				  ss << bname << ":" << 0 << ";";
			  }
			  else {
				  TypeInfo ti = Context.getTypeInfo(T);
				  if (extraArg.first&EXTRA_BITFIELD) {
					  qualifierString.append("b");
				  }
				  ss << "B:" << qualifierString << ":";
				  if (extraArg.first&EXTRA_BITFIELD) {
					  ss << extraArg.second << ":";
				  }
				  std::string bname = tp->getName(Context.getPrintingPolicy()).str();
				  if(bname == "_Bool") bname ="bool";
				  ss << bname << ":" << ti.Width << ";";
			  }
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Builtin (" <<
					  tp->getName(Context.getPrintingPolicy()).str() << ")(" << qualifierString << ")\n" );
			  typeString.append(ss.str());
		  }
		  break;
		  case Type::Pointer:
		  {
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Pointer (" << qualifierString << ")\n" );
			  TypeInfo ti = Context.getTypeInfo(T);
			  const PointerType *tp = cast<PointerType>(T);
			  QualType ptrT = tp->getPointeeType();
			  std::stringstream ss;
			  ss << "P:" << qualifierString << ":";
			  std::string pstr;
			  if(short_ptr){
				  autoforward = true;
			  }
			  buildTypeString(ptrT,pstr);
			  autoforward=false;
			  ss << pstr << ":" << ti.Width << ";";
			  typeString.append(ss.str());
		  }
		  break;
		  case Type::LValueReference:
		  {
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build LValueReference (" << qualifierString << ")\n" );
			  TypeInfo ti = Context.getTypeInfo(T);
			  const LValueReferenceType *tp = cast<LValueReferenceType>(T);
			  QualType ptrT = tp->getPointeeType();
			  std::stringstream ss;
			  ss << "LV:" << qualifierString << ":";
			  std::string pstr;
			  if(short_ptr){
				  autoforward = true;
			  }
			  buildTypeString(ptrT,pstr);
			  autoforward=false;
			  ss << pstr << ":" << ti.Width << ";";
			  typeString.append(ss.str());
		  }
		  break;
		  case Type::RValueReference:
		  {
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build RValueReference (" << qualifierString << ")\n" );
			  TypeInfo ti = Context.getTypeInfo(T);
			  const RValueReferenceType *tp = cast<RValueReferenceType>(T);
			  QualType ptrT = tp->getPointeeType();
			  std::stringstream ss;
			  ss << "RV:" << qualifierString << ":";
			  std::string pstr;
			  if(short_ptr){
				  autoforward = true;
			  }
			  buildTypeString(ptrT,pstr);
			  autoforward=false;
			  ss << pstr << ":" << ti.Width << ";";
			  typeString.append(ss.str());
		  }
		  break;
		  case Type::MemberPointer:
		  {
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build MemberPointer (" << qualifierString << ")\n" );
			  TypeInfo ti = Context.getTypeInfo(T);
			  const MemberPointerType *tp = cast<MemberPointerType>(T);
			  QualType mptrT = tp->getPointeeType();
			  QualType cT = QualType(tp->getClass(),0);
			  std::stringstream ss;
			  ss << "MP:" << qualifierString << ":";
			  ss << T.getAsString() << ":";
			  ss << ti.Width;
			  std::string mptrTstr;
			  std::string cTstr;
			  buildTypeString(mptrT,mptrTstr);
			  buildTypeString(cT,cTstr);
			  ss << ":" << mptrTstr << ":" << cTstr << ";";
			  typeString.append(ss.str());
		  }
		  break;
		  case Type::Complex:
		  {
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Complex (" << qualifierString << ")\n" );
			  TypeInfo ti = Context.getTypeInfo(T);
			  const ComplexType *tp = cast<ComplexType>(T);
			  QualType eT = tp->getElementType();
			  std::stringstream ss;
			  ss << "CT:" << qualifierString << ":";
			  std::string estr;
			  buildTypeString(eT,estr);
			  ss << estr << ":" << ti.Width << ";";
			  typeString.append(ss.str());
		  }
		  break;
		  case Type::Vector:
		  {
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Vector (" << qualifierString << ")\n" );
			  uint64_t Width = 0;
			  const VectorType *tp = cast<VectorType>(T);
			  QualType eT = tp->getElementType();
			  if ((eT->getTypeClass()!=Type::Record) || (cast<RecordType>(eT)->getDecl()->isCompleteDefinition())) {
				  if (!tp->isDependentType()) {
					  TypeInfo ti = Context.getTypeInfo(eT);
					  Width = ti.Width*tp->getNumElements();
				  }
			  }
			  std::stringstream ss;
			  ss << "VT:" << qualifierString << ":";
			  std::string estr;
			  buildTypeString(eT,estr);
			  ss << estr << ":" << tp->getNumElements() << ":" << Width << ";";
			  typeString.append(ss.str());
		  }
		  break;
		  case Type::ExtVector:
		  {
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build ExtVector (" << qualifierString << ")\n" );
			  uint64_t Width = 0;
			  const ExtVectorType *tp = cast<ExtVectorType>(T);
			  tp->dump();
			  QualType eT = tp->getElementType();
			  if ((eT->getTypeClass()!=Type::Record) || (cast<RecordType>(eT)->getDecl()->isCompleteDefinition())) {
				  if (!tp->isDependentType()) {
					  TypeInfo ti = Context.getTypeInfo(eT);
					  Width = ti.Width*tp->getNumElements();
				  }
			  }
			  std::stringstream ss;
			  ss << "EVT:" << qualifierString << ":";
			  std::string estr;
			  buildTypeString(eT,estr);
			  ss << estr << ":" << tp->getNumElements() << ":" << Width << ";";
			  typeString.append(ss.str());
		  }
		  break;
		  case Type::DependentSizedArray:
		  {
		  	const DependentSizedArrayType *tp = cast<DependentSizedArrayType>(T);
                        std::stringstream ss;
		  	ss << "DSA:" << qualifierString << ":";
		  	QualType Qtp = QualType(tp,0);
		  	ss << Qtp.getAsString() << ":";
		  	std::set<const TemplateTypeParmType*> TPTS;
		  	LookForTemplateTypeParameters(tp->getElementType(),TPTS);
                        if (tp->getSizeExpr()) {
		  	    LookForTemplateTypeParameters(tp->getSizeExpr()->getType(),TPTS);
                        }
		  	for (auto i=TPTS.begin(); i!=TPTS.end(); ++i) {
				const TemplateTypeParmType* tp = *i;
				std::string ttpstr;
				buildTypeString(QualType(tp,0),ttpstr);
				ss << ttpstr << ":";
			}
		    std::string seTstr;
		    if (tp->getSizeExpr()) {
		    	buildTypeString(tp->getSizeExpr()->getType(),seTstr);
		    }
		  	ss << seTstr << ";";
		  	typeString.append(ss.str());
		  }
		  break;
		  case Type::PackExpansion:
		  {
		  	const PackExpansionType *tp = cast<PackExpansionType>(T);
		  	std::stringstream ss;
		  	ss << "PE:" << qualifierString << ":";
		  	QualType Qtp = QualType(tp,0);
		  	ss << Qtp.getAsString() << ";";
		  	typeString.append(ss.str());
		  }
		  break;
		  case Type::TemplateTypeParm:
		  {
		  	const TemplateTypeParmType *tp = cast<TemplateTypeParmType>(T);
		  	std::stringstream ss;
			ss << "TTP:" << qualifierString << ":";
			QualType Qtp = QualType(tp,0);
			ss << Qtp.getAsString() << ":" << tp->getDepth() << "," << tp->getIndex() <<";";
			typeString.append(ss.str());
		  }
		  break;
		  case Type::InjectedClassName:
		  {
			  const InjectedClassNameType* IT = cast<InjectedClassNameType>(T);
			  CXXRecordDecl* TRD = IT->getDecl();
			  qualifierString.append("x");
			  if (TRD->isUnion()) {
				  qualifierString.append("u");
			  }
			  std::stringstream ss;
			  const IdentifierInfo *II = TRD->getIdentifier();
			  std::string rstr;
			  if (TRD->isCompleteDefinition() && !autoforward) {
				  buildRecordTypeString(T,TRD,qualifierString,rstr,extraArg );
			  }
			  else{
				  rstr.append("RF:"+qualifierString+":"+T.getAsString()+";");
			  }
			  ss << rstr;
			  typeString.append(ss.str());

			  //update
			  std::string parentRecordHash;
			  if (Visitor.recordParentDeclMap.find(TRD)!=Visitor.recordParentDeclMap.end()) {
				  CXXRecordDecl* parentRecord = Visitor.recordParentDeclMap[TRD];
				  QualType RT = Context.getRecordType(parentRecord);
				  DbJSONClassVisitor::TypeData &RT_data = Visitor.getTypeData(RT);
				  // build ahead of time
				  if(!RT_data.hash.size()) buildTypeString(RT,RT_data.hash);
				  parentRecordHash = RT_data.hash;
				  parentRecordHash+="::";
							  }
			  typeString.insert(3,parentRecordHash);
		  }
		  break;
		  case Type::DependentName:
		  {
		  	const DependentNameType *tp = cast<DependentNameType>(T);
		  	std::set<const TemplateTypeParmType*> TPTS;
		  	NestedNameSpecifier * NNS = tp->getQualifier();
		  	if ((NNS->getKind()==NestedNameSpecifier::TypeSpecWithTemplate) ||
		  			(NNS->getKind()==NestedNameSpecifier::TypeSpec)) {
		  		const Type *T = NNS->getAsType();
		  		if (const TemplateSpecializationType *SpecType
		  				  	          = dyn_cast<TemplateSpecializationType>(T)) {
						for (const TemplateArgument &Arg : SpecType->template_arguments()) {
						  if (Arg.getKind() == TemplateArgument::Type) {
							  QualType TPT = Arg.getAsType();
							  LookForTemplateTypeParameters(TPT,TPTS);
						  }
					  }
		  		}
		  		else {
		  			QualType QT = QualType(T,0);
		  			LookForTemplateTypeParameters(QT,TPTS);
		  		}
		  	}
		  	std::stringstream ss;
			ss << "DN:" << qualifierString << ":";
			QualType Qtp = QualType(tp,0);
			ss << Qtp.getAsString() << ":";
			for (auto i=TPTS.begin(); i!=TPTS.end(); ++i) {
				const TemplateTypeParmType* tp = *i;
				std::string ttpstr;
				buildTypeString(QualType(tp,0),ttpstr);
				ss << ttpstr << ":";
			}
			ss << ";";
			typeString.append(ss.str());
		  }
		  break;
		  case Type::UnresolvedUsing:
		  {
			  const UnresolvedUsingType *tp = cast<UnresolvedUsingType>(T);
			  std::stringstream ss;
			  ss << "UU:" << qualifierString << ":";
			  std::string usingDecl;
			  llvm::raw_string_ostream cstream(usingDecl);
			  tp->getDecl()->print(cstream,Context.getPrintingPolicy());
			  cstream.flush();
			  ss << usingDecl << ";";
			  typeString.append(ss.str());
		  }
		  break;
		  case Type::TemplateSpecialization:
		  {
		  	const TemplateSpecializationType *tp = cast<TemplateSpecializationType>(T);
		  	std::stringstream ss;
		  	ss << "RS:" << qualifierString << ":";
		  	QualType Qtp = QualType(tp,0);
		  	ss << Qtp.getAsString() << ":";
		  	TemplateName TN = tp->getTemplateName();
		  	if (tp->getTemplateName().getKind()==TemplateName::Template) {
		  		TemplateDecl* TD = TN.getAsTemplateDecl();
		  		if (isa<ClassTemplateDecl>(TD)) {
		  			ClassTemplateDecl* CTD = static_cast<ClassTemplateDecl*>(TD);
	  				/* We might have a situation when at the time of noticing a template specialization there were only
	  				 *  class template forward available without the full definition; try to get the full definition now
	  				 */
	  				CTD = CTD->getMostRecentDecl();
		  			const InjectedClassNameType* IT = static_cast<const InjectedClassNameType*>(CTD->getTemplatedDecl()->getTypeForDecl());
		  			std::string tstr;
		  			buildTypeString(QualType(IT,0),tstr);
		  			ss << tstr << ":";
		  			tstr.clear();
					
					// TODO: removed likely unnecessary code, some issues may arise
					buildTemplateArgumentsString(tp->template_arguments(),tstr);
					ss << tstr << ";";
		  		}
		  		else {
		  			ss << ";";
		  		}
		  	}
		  	else {
		  		ss << ";";
		  	}
			typeString.append(ss.str());
		  }
		  break;
		  case Type::DependentTemplateSpecialization:
		  {
		  	const DependentTemplateSpecializationType *tp = cast<DependentTemplateSpecializationType>(T);
		  	std::stringstream ss;
			ss << "DTS:" << qualifierString << ":";
			QualType Qtp = QualType(tp,0);
			ss << Qtp.getAsString() << ";";
			typeString.append(ss.str());
		  }
		  break;
		  case Type::Auto:
		  {
			  const AutoType *tp = cast<AutoType>(T);
			  QualType aT = tp->getDeducedType();
			  std::stringstream ss;
			  ss << "AUTO:" << qualifierString << ":";
			  if (!aT.isNull()) {
				  std::string tstr;
				  buildTypeString(aT,tstr);
				  ss << tstr << ";";
			  }
			  else {
				  ss << "(NULL)" << ";";
			  }
			  typeString.append(ss.str());
		  }
		  break;
		  case Type::Record:
		  {
			  const RecordType *tp = cast<RecordType>(T);
			  bool do_update = false;
			  RecordDecl* rD = tp->getDecl();
			  if (isa<CXXRecordDecl>(rD)) {
				  qualifierString.append("x");
			  }
			  if (tp->isUnionType()) {
				  qualifierString.append("u");
			  }
			  const IdentifierInfo *II = rD->getIdentifier();
			  std::string rstr;
			  if (rD->isCompleteDefinition() && !autoforward) {
				  buildRecordTypeString(T,rD,qualifierString,rstr,extraArg );
				  do_update = true;
			  }
			  else {
				  autoforward=false;
				  if(!II){
					  if(rD->getTypedefNameForAnonDecl()){
						  rstr.append("RF:" + qualifierString + ":" + rD->getTypedefNameForAnonDecl()->getName().str()+ ";");
					  }
					  else{
						  buildTypeString(T,rstr);
					  }
				  }
				  else{
					  assert(II && "Record forward without a declarator name");
					  std::stringstream ss;
					  std::string tpII;
					  if ((isa<CXXRecordDecl>(rD)) || (isa<ClassTemplateSpecializationDecl>(rD))) {
						  tpII = T.getAsString();
					  }
					  else {
						  tpII = II->getName().str();
					  }
					  ss << "RF:" << qualifierString << ":" << tpII << ";";
					  
					  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Record forward(" << tpII
					  		<< ")(" << qualifierString << ")\n" );
					  DBG(DEBUG_HASH, llvm::outs() << "build Record forward(" << tpII
					  	  	<< ")(" << qualifierString << ")\n" );
					  rstr.append(ss.str());
				  }
			  }
			  typeString.append(rstr);

			  //update
			  if(do_update)
			  if ((Visitor.gtp_refVars.find(rD)!=Visitor.gtp_refVars.end())&&(Visitor.gtp_refVars[rD].size()>0)) {
				  size_t pos = typeString.rfind(";");
				  assert(pos!=typeString.npos);
				  pos = typeString.rfind(":",pos);
				  assert(pos!=typeString.npos);
				  if(pos<88){
					llvm::errs()<<typeString<<'\n';
					T.dump();
				  	assert(pos>=88);
				  }
				  std::string odt = base64_decode(typeString.substr(pos-88,88));
				  SHA_CTX c;
				  SHA_init(&c);
				  SHA_update(&c,odt.data(),odt.length());
				  std::set<std::string> ordered;
				  for (auto u = Visitor.gtp_refVars[rD].begin(); u!=Visitor.gtp_refVars[rD].end(); ++u) {
					  std::string &vhash = Visitor.getVarData(*u).hash;
					  ordered.insert(vhash);
				  }
				  for(auto &vhash : ordered){
					  SHA_update(&c,vhash.data(),vhash.length());
				  }
				  SHA_final(&c);
				  typeString.replace(pos-88,88,base64_encode(reinterpret_cast<const unsigned char*>(c.buf.b), 64));
			  }
		  }
		  break;
		  case Type::ConstantArray:
		  {
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build ConstantArray (" << qualifierString << ")\n" );
			  uint64_t Width = 0;
			  const ConstantArrayType *tp = cast<ConstantArrayType>(T);
			  QualType elT = tp->getElementType();
			  /* It happens that we got an array of records without complete definition
			   * How would that even compile?
			   */
			  if ((elT->getTypeClass()!=Type::Record) || (cast<RecordType>(elT)->getDecl()->isCompleteDefinition())) {
				  if (!tp->isDependentType()) {
					  TypeInfo ti = Context.getTypeInfo(T);
					  Width = ti.Width;
				  }
			  }
			  std::string cas("CA:");
			  cas+=qualifierString;
			  cas+=":";
			  buildTypeString(elT,cas);
			  std::stringstream ss;
			  ss << ":" << Width << ";";
			  cas.append(ss.str());
			  typeString.append(cas);
		  }
		  break;
		  case Type::IncompleteArray:
		  {
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build IncompleteArray (" << qualifierString << ")\n" );
			  const IncompleteArrayType *tp = cast<IncompleteArrayType>(T);
			  QualType elT = tp->getElementType();
			  std::string ias("IA:");
			  ias+=qualifierString;
			  ias+=":";
			  buildTypeString(elT,ias);
			  std::stringstream ss;
			  ss << ":" << 0 << ";";
			  ias.append(ss.str());
			  typeString.append(ias);
		  }
		  break;
		  case Type::VariableArray:
		  {
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build VariableArray (" << qualifierString << ")\n" );
			  const VariableArrayType *tp = cast<VariableArrayType>(T);
			  QualType vaT = tp->getElementType();
			  std::string vas("VA:");
			  buildTypeString(vaT,vas);
			  std::stringstream ss;
			  ss << ":" << qualifierString << ":" << "X" << ";";
			  vas.append(ss.str());
			  typeString.append(vas);
		  }
		  break;
		  case Type::Typedef:
		  {
			  const TypedefType *tp = cast<TypedefType>(T);
			  TypedefNameDecl* D = tp->getDecl();
			  IdentifierInfo *II = D->getIdentifier();
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Typedef ("
					  << II->getName().str() << ")(" << qualifierString << ")\n" );
			  if (extraArg.first&EXTRA_TYPEDEF_BOOL) {
				  std::stringstream ss;
				  if (extraArg.first&EXTRA_BITFIELD) {
					  qualifierString.append("b");
				  }
				  ss << "B:" << qualifierString << ":";
				  if (extraArg.first&EXTRA_BITFIELD) {
					  ss << extraArg.second << ":";
				  }
				  TypeInfo ti = Context.getTypeInfo(T);
				  ss << "bool" << ":" << ti.Width << ";";
				  typeString.append(ss.str());
			  }
			  else {
				  QualType tT = D->getTypeSourceInfo()->getType();
				  std::string tpd("TPD:");
				  tpd+=qualifierString;
				  tpd+=":";
				  buildTypeString(tT,tpd);
				  std::stringstream ss;
				  ss << ":" << II->getName().str() << ";";
				  tpd.append(ss.str());
				  typeString.append(tpd);
			  }
		  }
		  break;
		  case Type::Enum:
		  {
			  const EnumType *tp = cast<EnumType>(T);
			  EnumDecl* eD = tp->getDecl();
			  const IdentifierInfo *II = eD->getIdentifier();
			  std::string estr;
			  if (eD->isCompleteDefinition()) {
				  QualType eT = resolve_Typedef_Integer_Type(tp->getDecl()->getIntegerType());
				  const BuiltinType *btp = cast<BuiltinType>(eT);
				  uint64_t width = 0;
				  if (!btp->isDependentType()) {
					  TypeInfo ti = Context.getTypeInfo(eT);
					  width = ti.Width;
				  }
				  std::vector<int64_t> ConstantValues;
				  std::vector<std::string> ConstantNames;
				  const DeclContext *DC = cast<DeclContext>(eD);
				  for (DeclContext::decl_iterator D = DC->decls_begin(), DEnd = DC->decls_end();
				                                       D != DEnd; ++D) {

					  if (D->isImplicit()) continue;
                                          if (D->getKind()!=Decl::EnumConstant) continue;
					  EnumConstantDecl* ED = static_cast<EnumConstantDecl*>(*D);
					  if (width) {
						  ConstantValues.push_back(ED->getInitVal().extOrTrunc(63).getExtValue());
						  ConstantNames.push_back(ED->getName().str());
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
						  ConstantValues.push_back(-1);
						  ConstantNames.push_back(ED->getName().str()+"$"+exprStr);
					  }
				  }
				  std::stringstream ss;
				  if (II) {
					  std::string tpII;
					  if (isCXXTU(Context)) {
						  tpII = T.getAsString();
					  }
					  else {
						  tpII = II->getName().str();
					  }
					  ss << "E:" << qualifierString << ":" << tpII << ":";
					  for (auto i = ConstantValues.begin(); i!=ConstantValues.end();) {
						  ss << (*i) << "@" << ConstantNames[i-ConstantValues.begin()];
						  ++i;
						  if (i!=ConstantValues.end()) {
							  ss << ",";
						  }
					  }
					  ss << ":" << width << ";";

					  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Enum(" << tpII
							  << ")(" << qualifierString << ")\n" );
				  }
				  else {
					  if (eD->getTypedefNameForAnonDecl()) {
						  ss << "E:" << qualifierString << ":" << eD->getTypedefNameForAnonDecl()->getName().str() << ":";
						  for (auto i = ConstantValues.begin(); i!=ConstantValues.end();) {
							  ss << (*i) << "@" << ConstantNames[i-ConstantValues.begin()];
							  ++i;
							  if (i!=ConstantValues.end()) {
								  ss << ",";
							  }
						  }
						  ss << ":" << width << ";";

						  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Enum("
								  << eD->getTypedefNameForAnonDecl()->getName().str() << ")(" << qualifierString << ")\n" );
					  }
					  else {
						  ss << "E:" << qualifierString << ":" << "!anon" << ":";
						  for (auto i = ConstantValues.begin(); i!=ConstantValues.end();) {
							  ss << (*i) << "@" << ConstantNames[i-ConstantValues.begin()];
							  ++i;
							  if (i!=ConstantValues.end()) {
								  ss << ",";
							  }
						  }
						  ss << ":" << width << ";";

						  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Enum()(" << qualifierString << ")\n" );
					  }
				  }
				  estr.append(ss.str());
				  buildTypeString(eT,estr);
			  }
			  else {
				  assert(II && "Enum forward without a declarator name");
				  std::stringstream ss;
				  std::string tpII;
				  if (isCXXTU(Context)) {
					  tpII = T.getAsString();
				  }
				  else {
					  tpII = II->getName().str();
				  }
				  ss << "EF:" << qualifierString << ":" << tpII << ";";

				  DBG(DEBUG_TYPESTRING, llvm::outs() << "build Enum forward(" << tpII
						  << ")(" << qualifierString << ")\n" );
				  estr.append(ss.str());
			  }
			  typeString.append(estr);
		  }
		  break;
		  case Type::FunctionProto:
		  {
			  if(autoforward){
				  autoforward = false;
				  typeString.append(T.getAsString());
				  break;
			  }
			  const FunctionProtoType *tp = cast<FunctionProtoType>(T);
			  std::string fstr;
			  std::stringstream ss;
			  ss << "F:" << qualifierString;
			  if (tp->isVariadic()) {
				  ss << "r";
			  }
			  ss << ":" << tp->getNumParams() << ":";
			  fstr.append(ss.str());
			  QualType rT = tp->getReturnType();
			  buildTypeString(rT,fstr);
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build FunctionProto[" << tp->getNumParams() << "] ("
					  << qualifierString << ")\n" );
			  for (unsigned i = 0; i<tp->getNumParams(); ++i) {
				  	  fstr.append(":");
				  QualType pT = tp->getParamType(i);
				  buildTypeString(pT,fstr);
			  }
			  typeString.append(fstr);
		  }
		  break;
		  case Type::FunctionNoProto:
		  {
			  const FunctionNoProtoType *tp = cast<FunctionNoProtoType>(T);
			  std::string fstr;
			  std::stringstream ss;
			  ss << "F:n:0:";
			  fstr.append(ss.str());
			  QualType rT = tp->getReturnType();
			  buildTypeString(rT,fstr);
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build FunctionNoProto[] (" << qualifierString << ")\n" );
			  typeString.append(fstr);
		  }
		  break;
		  default:
			  DBG(DEBUG_TYPESTRING, llvm::outs() << "build unknown (" << T->getTypeClass() << ") ("
					  << qualifierString << ")\n" );
			  break;
	  }
  }
