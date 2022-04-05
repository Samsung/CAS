  // Type
  bool VisitComplexType(const ComplexType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitComplexType()\n");
    return true;
  }
  bool VisitPointerType(const PointerType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitPointerType()\n");
	  return true;
  }
  bool VisitBlockPointerType(const BlockPointerType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitBlockPointerType()\n");
    return true;
  }
  bool VisitReferenceType(const ReferenceType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitReferenceType()\n");
    return true;
  }
  bool VisitRValueReferenceType(const ReferenceType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitRValueReferenceType()\n");
    return true;
  }
  bool VisitMemberPointerType(const MemberPointerType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitMemberPointerType()\n");
    return true;
  }
  bool VisitArrayType(const ArrayType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitArrayType()\n");
    return true;
  }
  bool VisitConstantArrayType(const ConstantArrayType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitConstantArrayType()\n");
    return true;
  }
  bool VisitVariableArrayType(const VariableArrayType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitVariableArrayType()\n");
    return true;
  }
  bool VisitDependentSizedArrayType(const DependentSizedArrayType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitDependentSizedArrayType()\n");
    return true;
  }
  bool VisitDependentSizedExtVectorType(
	  const DependentSizedExtVectorType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitDependentSizedExtVectorType()\n");
    return true;
  }
  bool VisitVectorType(const VectorType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitVectorType()\n");
    return true;
  }
  bool VisitFunctionType(const FunctionType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitFunctionType()\n");
    return true;
  }
  bool VisitFunctionProtoType(const FunctionProtoType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitFunctionProtoType()\n");
    return true;
  }
  bool VisitUnresolvedUsingType(const UnresolvedUsingType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitUnresolvedUsingType()\n");
    return true;
  }
  bool VisitTypedefType(const TypedefType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitTypedefType()\n");
    return true;
  }
  bool VisitTypeOfExprType(const TypeOfExprType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitTypeOfExprType()\n");
    return true;
  }
  bool VisitDecltypeType(const DecltypeType *T) {
	  DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitDecltypeType()\n");
    return true;
  }
  bool VisitUnaryTransformType(const UnaryTransformType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitUnaryTransformType()\n");
	  return true;
  }
  bool VisitTagType(const TagType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitTagType()\n");
	  return true;
  }
  bool VisitAttributedType(const AttributedType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitAttributedType()\n");
	  return true;
  }
  bool VisitTemplateTypeParmType(const TemplateTypeParmType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitTemplateTypeParmType()\n");
	  return true;
  }
  bool VisitSubstTemplateTypeParmType(const SubstTemplateTypeParmType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitSubstTemplateTypeParmType()\n");
	  return true;
  }
  bool VisitSubstTemplateTypeParmPackType(
	  const SubstTemplateTypeParmPackType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitSubstTemplateTypeParmPackType()\n");
	  return true;
  }
  bool VisitAutoType(const AutoType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitAutoType()\n");
	  return true;
  }
  bool VisitTemplateSpecializationType(const TemplateSpecializationType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitTemplateSpecializationType()\n");
	  return true;
  }
  bool VisitInjectedClassNameType(const InjectedClassNameType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitInjectedClassNameType()\n");
	  return true;
  }
  bool VisitObjCInterfaceType(const ObjCInterfaceType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitObjCInterfaceType()\n");
	  return true;
  }
  bool VisitObjCObjectPointerType(const ObjCObjectPointerType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitObjCObjectPointerType()\n");
	  return true;
  }
  bool VisitAtomicType(const AtomicType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitAtomicType()\n");
	  return true;
  }
  bool VisitPipeType(const PipeType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitPipeType()\n");
	  return true;
  }
  bool VisitAdjustedType(const AdjustedType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitAdjustedType()\n");
	  return true;
  }
  bool VisitPackExpansionType(const PackExpansionType *T) {
    DBG(DEBUG_VISIT_TYPE, llvm::outs() << "@VisitPackExpansionType()\n");
	  return true;
  }

  // Decls
  bool VisitLabelDecl(const LabelDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitLabelDecl()\n");
	  return true;
  }
  bool VisitTypedefDecl(const TypedefDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitTypedefDecl()\n");
	  return true;
	}
  bool VisitEnumDecl(const EnumDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitEnumDecl()\n");
	  return true;
	}
  bool VisitRecordDecl(const RecordDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitRecordDecl()\n");
	  return true;
	}
  bool VisitEnumConstantDecl(const EnumConstantDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitEnumConstantDecl()\n");
	  return true;
  }
  bool VisitIndirectFieldDecl(const IndirectFieldDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitIndirectFieldDecl()\n");
	  return true;
  }
  bool VisitFunctionDecl(const FunctionDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitFunctionDecl()\n");
	  return true;
  }
  bool VisitFieldDecl(const FieldDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitFieldDecl()\n");
	  return true;
  }
  bool VisitVarDecl(const VarDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitVarDecl()\n");
	  return true;
  }
  bool VisitDecompositionDecl(const DecompositionDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitDecompositionDecl()\n");
	  return true;
  }
  bool VisitBindingDecl(const BindingDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitBindingDecl()\n");
	  return true;
  }
  bool VisitFileScopeAsmDecl(const FileScopeAsmDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitFileScopeAsmDecl()\n");
	  return true;
  }
  bool VisitImportDecl(const ImportDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitImportDecl()\n");
	  return true;
  }
  bool VisitPragmaCommentDecl(const PragmaCommentDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitPragmaCommentDecl()\n");
	  return true;
  }
  bool VisitPragmaDetectMismatchDecl(const PragmaDetectMismatchDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitPragmaDetectMismatchDecl()\n");
	  return true;
  }
  bool VisitCapturedDecl(const CapturedDecl *D) {
    DBG(DEBUG_VISIT_DECL, llvm::outs() << "@VisitCapturedDecl()\n");
	  return true;
  }

  // C++ Decls
  bool VisitNamespaceDecl(const NamespaceDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitNamespaceDecl()\n");
	  return true;
  }
  bool VisitUsingDirectiveDecl(const UsingDirectiveDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitUsingDirectiveDecl()\n");
	  return true;
  }
  bool VisitNamespaceAliasDecl(const NamespaceAliasDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitNamespaceAliasDecl()\n");
	  return true;
  }
  bool VisitTypeAliasDecl(const TypeAliasDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitTypeAliasDecl()\n");
	  return true;
  }
  bool VisitTypeAliasTemplateDecl(const TypeAliasTemplateDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitTypeAliasTemplateDecl()\n");
	  return true;
  }
  bool VisitCXXRecordDecl(const CXXRecordDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitCXXRecordDecl()\n");
	  return true;
  }
  bool VisitStaticAssertDecl(const StaticAssertDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitStaticAssertDecl()\n");
	  return true;
  }
#if 0
  template<typename SpecializationDecl>
  bool VisitTemplateDeclSpecialization(const SpecializationDecl *D, bool DumpExplicitInst,
          bool DumpRefOnly) {
	  return true;
  }
#endif
#if 0
  template<typename TemplateDecl>
  bool VisitTemplateDecl(const TemplateDecl *D, bool DumpExplicitInst) {
	  return true;
  }
#endif
  bool VisitFunctionTemplateDecl(const FunctionTemplateDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitFunctionTemplateDecl()\n");
	  return true;
  }
  bool VisitClassTemplateDecl(const ClassTemplateDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitClassTemplateDecl()\n");
	  return true;
  }
  bool VisitClassTemplateSpecializationDecl(
	  const ClassTemplateSpecializationDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitClassTemplateSpecializationDecl()\n");
	  return true;
  }
  bool VisitClassTemplatePartialSpecializationDecl(
	  const ClassTemplatePartialSpecializationDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitClassTemplatePartialSpecializationDecl()\n");
	  return true;
  }
  bool VisitClassScopeFunctionSpecializationDecl(
	  const ClassScopeFunctionSpecializationDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitClassScopeFunctionSpecializationDecl()\n");
	  return true;
  }
  bool VisitBuiltinTemplateDecl(const BuiltinTemplateDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitBuiltinTemplateDecl()\n");
	  return true;
  }
  bool VisitVarTemplateDecl(const VarTemplateDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitVarTemplateDecl()\n");
	  return true;
  }
  bool VisitVarTemplateSpecializationDecl(
	  const VarTemplateSpecializationDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitVarTemplateSpecializationDecl()\n");
	  return true;
  }
  bool VisitVarTemplatePartialSpecializationDecl(
	  const VarTemplatePartialSpecializationDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitVarTemplatePartialSpecializationDecl()\n");
	  return true;
  }
  bool VisitTemplateTypeParmDecl(const TemplateTypeParmDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitTemplateTypeParmDecl()\n");
	  return true;
  }
  bool VisitNonTypeTemplateParmDecl(const NonTypeTemplateParmDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitNonTypeTemplateParmDecl()\n");
	  return true;
  }
  bool VisitTemplateTemplateParmDecl(const TemplateTemplateParmDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitTemplateTemplateParmDecl()\n");
	  return true;
  }
  bool VisitUsingDecl(const UsingDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitUsingDecl()\n");
	  return true;
  }
  bool VisitUnresolvedUsingTypenameDecl(const UnresolvedUsingTypenameDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitUnresolvedUsingTypenameDecl()\n");
	  return true;
  }
  bool VisitUnresolvedUsingValueDecl(const UnresolvedUsingValueDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitUnresolvedUsingValueDecl()\n");
	  return true;
  }
  bool VisitUsingShadowDecl(const UsingShadowDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitUsingShadowDecl()\n");
	  return true;
  }
  bool VisitConstructorUsingShadowDecl(const ConstructorUsingShadowDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitConstructorUsingShadowDecl()\n");
	  return true;
  }
  bool VisitLinkageSpecDecl(const LinkageSpecDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitLinkageSpecDecl()\n");
	  return true;
  }
  bool VisitAccessSpecDecl(const AccessSpecDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitAccessSpecDecl()\n");
	  return true;
  }
  bool VisitFriendDecl(const FriendDecl *D) {
    DBG(DEBUG_VISIT_CPPDECL, llvm::outs() << "@VisitFriendDecl()\n");
	  return true;
  }

  // Statements
  bool VisitStmt(const Stmt *Node) {
    DBG(DEBUG_VISIT_STMT, llvm::outs() << "@VisitStmt()\n");
	  return true;
  }
  bool VisitDeclStmt(const DeclStmt *Node) {
    DBG(DEBUG_VISIT_STMT, llvm::outs() << "@VisitDeclStmt()\n");
	  return true;
  }
  bool VisitAttributedStmt(const AttributedStmt *Node) {
    DBG(DEBUG_VISIT_STMT, llvm::outs() << "@VisitAttributedStmt()\n");
	  return true;
  }
  bool VisitLabelStmt(const LabelStmt *Node) {
    DBG(DEBUG_VISIT_STMT, llvm::outs() << "@VisitLabelStmt()\n");
	  return true;
  }
  bool VisitGotoStmt(const GotoStmt *Node) {
    DBG(DEBUG_VISIT_STMT, llvm::outs() << "@VisitGotoStmt()\n");
	  return true;
  }
  bool VisitCXXCatchStmt(const CXXCatchStmt *Node) {
    DBG(DEBUG_VISIT_STMT, llvm::outs() << "@VisitCXXCatchStmt()\n");
	  return true;
  }
  bool VisitCapturedStmt(const CapturedStmt *Node) {
    DBG(DEBUG_VISIT_STMT, llvm::outs() << "@VisitCapturedStmt()\n");
	  return true;
  }

  // Expressions
  bool VisitExpr(const Expr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitExpr()\n");
	  return true;
  }
  bool VisitCastExpr(const CastExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCastExpr()\n");
	  return true;
  }
  bool VisitDeclRefExpr(const DeclRefExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitDeclRefExpr()\n");
	  return true;
  }
  bool VisitPredefinedExpr(const PredefinedExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitPredefinedExpr()\n");
	  return true;
  }
  bool VisitCharacterLiteral(const CharacterLiteral *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCharacterLiteral()\n");
	  return true;
  }
  bool VisitIntegerLiteral(const IntegerLiteral *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitIntegerLiteral()\n");
	  return true;
  }
  bool VisitFloatingLiteral(const FloatingLiteral *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitFloatingLiteral()\n");
	  return true;
  }
  bool VisitStringLiteral(const StringLiteral *Str) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitStringLiteral()\n");
	  return true;
  }
  bool VisitInitListExpr(const InitListExpr *ILE) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitInitListExpr()\n");
	  return true;
  }
  bool VisitArrayInitLoopExpr(const ArrayInitLoopExpr *ILE) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitArrayInitLoopExpr()\n");
	  return true;
  }
  bool VisitArrayInitIndexExpr(const ArrayInitIndexExpr *ILE) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitArrayInitIndexExpr()\n");
	  return true;
  }
  bool VisitUnaryOperator(const UnaryOperator *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitUnaryOperator()\n");
	  return true;
  }
  bool VisitUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitUnaryExprOrTypeTraitExpr()\n");
	  return true;
  }
  bool VisitMemberExpr(const MemberExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitMemberExpr()\n");
	  return true;
  }
  bool VisitExtVectorElementExpr(const ExtVectorElementExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitExtVectorElementExpr()\n");
	  return true;
  }
  bool VisitBinaryOperator(const BinaryOperator *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitBinaryOperator()\n");
	  return true;
  }
  bool VisitCompoundAssignOperator(const CompoundAssignOperator *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCompoundAssignOperator()\n");
	  return true;
  }
  bool VisitAddrLabelExpr(const AddrLabelExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitAddrLabelExpr()\n");
	  return true;
  }
  bool VisitBlockExpr(const BlockExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitBlockExpr()\n");
	  return true;
  }
  bool VisitOpaqueValueExpr(const OpaqueValueExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitOpaqueValueExpr()\n");
	  return true;
  }

  bool VisitCXXNamedCastExpr(const CXXNamedCastExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCXXNamedCastExpr()\n");
	  return true;
  }
  bool VisitCXXBoolLiteralExpr(const CXXBoolLiteralExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCXXBoolLiteralExpr()\n");
	  return true;
  }
  bool VisitCXXThisExpr(const CXXThisExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCXXThisExpr()\n");
	  return true;
  }
  bool VisitCXXFunctionalCastExpr(const CXXFunctionalCastExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCXXFunctionalCastExpr()\n");
	  return true;
  }
  bool VisitCXXConstructExpr(const CXXConstructExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCXXConstructExpr()\n");
	  return true;
  }
  bool VisitCXXBindTemporaryExpr(const CXXBindTemporaryExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCXXBindTemporaryExpr()\n");
	  return true;
  }
  bool VisitCXXNewExpr(const CXXNewExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCXXNewExpr()\n");
	  return true;
  }
  bool VisitCXXDeleteExpr(const CXXDeleteExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCXXDeleteExpr()\n");
	  return true;
  }
  bool VisitMaterializeTemporaryExpr(const MaterializeTemporaryExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitMaterializeTemporaryExpr()\n");
	  return true;
  }
  bool VisitExprWithCleanups(const ExprWithCleanups *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitExprWithCleanups()\n");
	  return true;
  }
  bool VisitUnresolvedLookupExpr(const UnresolvedLookupExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitUnresolvedLookupExpr()\n");
	  return true;
  }
  bool dumpCXXTemporary(const CXXTemporary *Temporary) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@dumpCXXTemporary()\n");
	  return true;
  }
  bool VisitLambdaExpr(const LambdaExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitLambdaExpr()\n");
	  return true;
  }
  bool VisitSizeOfPackExpr(const SizeOfPackExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitSizeOfPackExpr()\n");
	  return true;
  }
  bool VisitCXXDependentScopeMemberExpr(const CXXDependentScopeMemberExpr *Node) {
    DBG(DEBUG_VISIT_EXPR, llvm::outs() << "@VisitCXXDependentScopeMemberExpr()\n");
	  return true;
  }
