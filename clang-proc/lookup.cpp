#include "main.hpp"
#include "dbjson.hpp"
#include "compat.h"

#include "clang/AST/RecordLayout.h"

void DbJSONClassVisitor::lookForLiteral(const Expr* E, std::set<DbJSONClassVisitor::LiteralHolder>& refs,
		unsigned pos) {

	switch(E->getStmtClass()) {
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			lookForLiteral(ICE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			lookForLiteral(PE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			lookForLiteral(ASE->getLHS(),refs,pos);
			break;
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			lookForLiteral(UO->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			lookForLiteral(CSCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXConstCastExprClass:
		{
			const CXXConstCastExpr* CCE = static_cast<const CXXConstCastExpr*>(E);
			lookForLiteral(CCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXReinterpretCastExprClass:
		{
			const CXXReinterpretCastExpr* RCE = static_cast<const CXXReinterpretCastExpr*>(E);
			lookForLiteral(RCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXFunctionalCastExprClass:
		{
			const CXXFunctionalCastExpr* FCE = static_cast<const CXXFunctionalCastExpr*>(E);
			lookForLiteral(FCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXBindTemporaryExprClass:
		{
			const CXXBindTemporaryExpr* BTE = static_cast<const CXXBindTemporaryExpr*>(E);
			lookForLiteral(BTE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::BinaryOperatorClass:
		{
			const BinaryOperator* BO = static_cast<const BinaryOperator*>(E);
			lookForLiteral(BO->getLHS(),refs,pos);
			lookForLiteral(BO->getRHS(),refs,pos);
			break;
		}
		case Stmt::ConditionalOperatorClass:
		{
			const ConditionalOperator* CO = static_cast<const ConditionalOperator*>(E);
			lookForLiteral(CO->getLHS(),refs,pos);
			lookForLiteral(CO->getRHS(),refs,pos);
			break;
		}
		case Stmt::BinaryConditionalOperatorClass:
		{
			const BinaryConditionalOperator* BCO = static_cast<const BinaryConditionalOperator*>(E);
			lookForLiteral(BCO->getCommon(),refs,pos);
			lookForLiteral(BCO->getFalseExpr(),refs,pos);
			break;
		}
		case Stmt::IntegerLiteralClass:
		{
			const IntegerLiteral* IL = static_cast<const IntegerLiteral*>(E);
			DbJSONClassVisitor::LiteralHolder lh;
			lh.type = DbJSONClassVisitor::LiteralHolder::LiteralInteger;
			lh.integerLiteral = llvm::APSInt(IL->getValue(),IL->getType()->isUnsignedIntegerOrEnumerationType());
			lh.pos = pos;
			refs.insert(lh);
			break;
		}
		case Stmt::CharacterLiteralClass:
		{
			const CharacterLiteral* CL = static_cast<const CharacterLiteral*>(E);
			DbJSONClassVisitor::LiteralHolder lh;
			lh.type = DbJSONClassVisitor::LiteralHolder::LiteralChar;
			lh.charLiteral = CL->getValue();
			lh.pos = pos;
			refs.insert(lh);
			break;
		}
		case Stmt::StringLiteralClass:
		{
			const StringLiteral* SL = static_cast<const StringLiteral*>(E);
			DbJSONClassVisitor::LiteralHolder lh;
			lh.type = DbJSONClassVisitor::LiteralHolder::LiteralString;
			lh.stringLiteral = SL->getBytes().str();
			/* Remove invalid control characters which break json parsing */
			lh.stringLiteral.erase(std::remove(lh.stringLiteral.begin(),lh.stringLiteral.end(),'\x1f'),lh.stringLiteral.end());
			lh.pos = pos;
			refs.insert(lh);
			break;
		}
		case Stmt::FloatingLiteralClass:
		{
			const FloatingLiteral* FL = static_cast<const FloatingLiteral*>(E);
			DbJSONClassVisitor::LiteralHolder lh;
			lh.type = DbJSONClassVisitor::LiteralHolder::LiteralFloat;
			lh.floatingLiteral = FL->getValue();
			lh.pos = pos;
			refs.insert(lh);
			break;
		}
		case Stmt::UnaryExprOrTypeTraitExprClass:
		case Stmt::StmtExprClass:
		case Stmt::PredefinedExprClass:
		case Stmt::CompoundLiteralExprClass:
		case Stmt::PackExpansionExprClass:
		case Stmt::CXXUnresolvedConstructExprClass:
		case Stmt::CXXDependentScopeMemberExprClass:
		case Stmt::CXXThisExprClass:
		case Stmt::DeclRefExprClass:
		case Stmt::CXXTemporaryObjectExprClass:
		case Stmt::CXXNullPtrLiteralExprClass:
		case Stmt::CXXBoolLiteralExprClass:
		case Stmt::DependentScopeDeclRefExprClass:
		case Stmt::UnresolvedLookupExprClass:
		case Stmt::SizeOfPackExprClass:
		case Stmt::CXXDefaultArgExprClass:
                case Stmt::GNUNullExprClass:
		{
			break;
		}
		case Stmt::LambdaExprClass:
		{
			// TODO: implement usage of catched variables
			break;
		}
		case Stmt::MemberExprClass:
		{
			const MemberExpr* ME = static_cast<const MemberExpr*>(E);
			lookForLiteral(ME->getBase(),refs,pos);
			break;
		}
		case Stmt::CXXStaticCastExprClass:
		{
			const CXXStaticCastExpr* SCE = static_cast<const CXXStaticCastExpr*>(E);
			lookForLiteral(SCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::MaterializeTemporaryExprClass:
		{
			const MaterializeTemporaryExpr* MTE = static_cast<const MaterializeTemporaryExpr*>(E);
			lookForLiteral(MTE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::ExprWithCleanupsClass:
		{
			const ExprWithCleanups* EWC = static_cast<const ExprWithCleanups*>(E);
			lookForLiteral(EWC->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CallExprClass:
		{
			const CallExpr* CE = static_cast<const CallExpr*>(E);
			std::set<DbJSONClassVisitor::LiteralHolder> __literalrefs;
			for (unsigned i=0; i!=CE->getNumArgs(); ++i) {
				lookForLiteral(CE->getArg(i),__literalrefs,pos);
			}
			for (auto i = __literalrefs.begin(); i!=__literalrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		case Stmt::CXXConstructExprClass:
		{
			const CXXConstructExpr* CE = static_cast<const CXXConstructExpr*>(E);
			std::set<DbJSONClassVisitor::LiteralHolder> __literalrefs;
			for (unsigned i=0; i!=CE->getNumArgs(); ++i) {
				lookForLiteral(CE->getArg(i),__literalrefs,pos);
			}
			for (auto i = __literalrefs.begin(); i!=__literalrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		case Stmt::CXXOperatorCallExprClass:
		{
			const CXXOperatorCallExpr* OCE = static_cast<const CXXOperatorCallExpr*>(E);
			std::set<DbJSONClassVisitor::LiteralHolder> __literalrefs;
			for (unsigned i=0; i!=OCE->getNumArgs(); ++i) {
				lookForLiteral(OCE->getArg(i),__literalrefs,pos);
			}
			for (auto i = __literalrefs.begin(); i!=__literalrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		case Stmt::CXXMemberCallExprClass:
		{
			const CXXMemberCallExpr* MCE = static_cast<const CXXMemberCallExpr*>(E);
			lookForLiteral(MCE->getImplicitObjectArgument(),refs,pos);
			std::set<DbJSONClassVisitor::LiteralHolder> __literalrefs;
			for (unsigned i=0; i!=MCE->getNumArgs(); ++i) {
				lookForLiteral(MCE->getArg(i),__literalrefs,pos);
			}
			for (auto i = __literalrefs.begin(); i!=__literalrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		default:
		{
			if (opts.exit_on_error) {
				llvm::outs() << "\nERROR: No implementation in lookForLiteral for:\n";
				E->dump();
				exit(EXIT_FAILURE);
			}
			else {
				unsupportedExprRefs.insert(std::string(E->getStmtClassName()));
			}
		}
	}
}

void DbJSONClassVisitor::lookForDeclRefExprsWithStmtExpr(const Expr* E, std::set<ValueHolder>& refs, unsigned pos ) {

	switch(E->getStmtClass()) {
		case Stmt::StmtExprClass:
		{
			const StmtExpr* SE = static_cast<const StmtExpr*>(E);
			const CompoundStmt* CS = SE->getSubStmt();
			CompoundStmt::const_body_iterator i = CS->body_begin();
			if (i!=CS->body_end()) {
				/* We have at least one statement in the body; get the last one */
				i = CS->body_end()-1;
				const Stmt* S = *i;
				lookForDeclRefExprsWithStmtExpr(cast<Expr>(S),refs,pos);
			}
			break;
		}
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(ICE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::ConstantExprClass:
		{
			const ConstantExpr* CE = static_cast<const ConstantExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(CE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::MemberExprClass:
		{
			const MemberExpr* ME = static_cast<const MemberExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(ME->getBase(),refs,pos);
			break;
		}
		case Stmt::CXXStaticCastExprClass:
		{
			const CXXStaticCastExpr* SCE = static_cast<const CXXStaticCastExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(SCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::MaterializeTemporaryExprClass:
		{
			const MaterializeTemporaryExpr* MTE = static_cast<const MaterializeTemporaryExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(MTE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::ExprWithCleanupsClass:
		{
			const ExprWithCleanups* EWC = static_cast<const ExprWithCleanups*>(E);
			lookForDeclRefExprsWithStmtExpr(EWC->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::DeclRefExprClass:
		{
			const DeclRefExpr* DRE = static_cast<const DeclRefExpr*>(E);
			const ValueDecl* v = DRE->getDecl();
			refs.insert(ValueHolder(v,pos));
			break;
		}
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(ASE->getLHS(),refs,pos);
			break;
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(PE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			lookForDeclRefExprsWithStmtExpr(UO->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(CSCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXConstCastExprClass:
		{
			const CXXConstCastExpr* CCE = static_cast<const CXXConstCastExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(CCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXReinterpretCastExprClass:
		{
			const CXXReinterpretCastExpr* RCE = static_cast<const CXXReinterpretCastExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(RCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXFunctionalCastExprClass:
		{
			const CXXFunctionalCastExpr* FCE = static_cast<const CXXFunctionalCastExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(FCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXBindTemporaryExprClass:
		{
			const CXXBindTemporaryExpr* BTE = static_cast<const CXXBindTemporaryExpr*>(E);
			lookForDeclRefExprsWithStmtExpr(BTE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::BinaryOperatorClass:
		{
			const BinaryOperator* BO = static_cast<const BinaryOperator*>(E);
			lookForDeclRefExprsWithStmtExpr(BO->getLHS(),refs,pos);
			lookForDeclRefExprsWithStmtExpr(BO->getRHS(),refs,pos);
			break;
		}
		case Stmt::ConditionalOperatorClass:
		{
			const ConditionalOperator* CO = static_cast<const ConditionalOperator*>(E);
			lookForDeclRefExprsWithStmtExpr(CO->getLHS(),refs,pos);
			lookForDeclRefExprsWithStmtExpr(CO->getRHS(),refs,pos);
			break;
		}
		case Stmt::BinaryConditionalOperatorClass:
		{
			const BinaryConditionalOperator* BCO = static_cast<const BinaryConditionalOperator*>(E);
			lookForDeclRefExprsWithStmtExpr(BCO->getCommon(),refs,pos);
			lookForDeclRefExprsWithStmtExpr(BCO->getFalseExpr(),refs,pos);
			break;
		}
		case Stmt::IntegerLiteralClass:
		case Stmt::CharacterLiteralClass:
		case Stmt::StringLiteralClass:
		case Stmt::FloatingLiteralClass:
		case Stmt::UnaryExprOrTypeTraitExprClass:
		case Stmt::PredefinedExprClass:
		case Stmt::CompoundLiteralExprClass:
		case Stmt::PackExpansionExprClass:
		case Stmt::CXXUnresolvedConstructExprClass:
		case Stmt::CXXDependentScopeMemberExprClass:
		case Stmt::CXXThisExprClass:
		case Stmt::CXXTemporaryObjectExprClass:
		case Stmt::CXXNullPtrLiteralExprClass:
		case Stmt::CXXBoolLiteralExprClass:
		case Stmt::DependentScopeDeclRefExprClass:
		case Stmt::UnresolvedLookupExprClass:
		case Stmt::SizeOfPackExprClass:
		case Stmt::CXXDefaultArgExprClass:
                case Stmt::GNUNullExprClass:
		{
			break;
		}
		case Stmt::LambdaExprClass:
		{
			// TODO: implement usage of catched variables
			break;
		}
		case Stmt::CallExprClass:
		{
			const CallExpr* CE = static_cast<const CallExpr*>(E);
			std::set<ValueHolder> __callrefs;
			for (unsigned i=0; i!=CE->getNumArgs(); ++i) {
				lookForDeclRefExprsWithStmtExpr(CE->getArg(i),__callrefs,pos);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		case Stmt::CXXConstructExprClass:
		{
			const CXXConstructExpr* CE = static_cast<const CXXConstructExpr*>(E);
			std::set<ValueHolder> __callrefs;
			for (unsigned i=0; i!=CE->getNumArgs(); ++i) {
				lookForDeclRefExprsWithStmtExpr(CE->getArg(i),__callrefs,pos);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		case Stmt::CXXOperatorCallExprClass:
		{
			const CXXOperatorCallExpr* OCE = static_cast<const CXXOperatorCallExpr*>(E);
			std::set<ValueHolder> __callrefs;
			for (unsigned i=0; i!=OCE->getNumArgs(); ++i) {
				lookForDeclRefExprsWithStmtExpr(OCE->getArg(i),__callrefs,pos);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		case Stmt::CXXMemberCallExprClass:
		{
			const CXXMemberCallExpr* MCE = static_cast<const CXXMemberCallExpr*>(E);
			lookForDeclRefExprs(MCE->getImplicitObjectArgument(),refs);
			std::set<ValueHolder> __callrefs;
			for (unsigned i=0; i!=MCE->getNumArgs(); ++i) {
				lookForDeclRefExprsWithStmtExpr(MCE->getArg(i),__callrefs,pos);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		default:
		{
			if (opts.exit_on_error) {
				llvm::outs() << "\nERROR: No implementation in lookForDeclRefExprsWithStmtExpr for:\n";
				E->dump();
				exit(EXIT_FAILURE);
			}
			else {
				unsupportedExprWithMemberExprRefs.insert(std::string(E->getStmtClassName()));
			}
		}
	}
}

void DbJSONClassVisitor::lookForDeclRefExprs(const Expr* E, std::set<ValueHolder>& refs, unsigned pos ) {

	switch(E->getStmtClass()) {
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			lookForDeclRefExprs(ICE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::ConstantExprClass:
		{
			const ConstantExpr* CE = static_cast<const ConstantExpr*>(E);
			lookForDeclRefExprs(CE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::MemberExprClass:
		{
			const MemberExpr* ME = static_cast<const MemberExpr*>(E);
			lookForDeclRefExprs(ME->getBase(),refs,pos);
			break;
		}
		case Stmt::CXXStaticCastExprClass:
		{
			const CXXStaticCastExpr* SCE = static_cast<const CXXStaticCastExpr*>(E);
			lookForDeclRefExprs(SCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::MaterializeTemporaryExprClass:
		{
			const MaterializeTemporaryExpr* MTE = static_cast<const MaterializeTemporaryExpr*>(E);
			lookForDeclRefExprs(MTE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::ExprWithCleanupsClass:
		{
			const ExprWithCleanups* EWC = static_cast<const ExprWithCleanups*>(E);
			lookForDeclRefExprs(EWC->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::DeclRefExprClass:
		{
			const DeclRefExpr* DRE = static_cast<const DeclRefExpr*>(E);
			const ValueDecl* v = DRE->getDecl();
			refs.insert(ValueHolder(v,pos));
			break;
		}
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			lookForDeclRefExprs(ASE->getLHS(),refs,pos);
			break;
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			lookForDeclRefExprs(PE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			lookForDeclRefExprs(UO->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			lookForDeclRefExprs(CSCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXConstCastExprClass:
		{
			const CXXConstCastExpr* CCE = static_cast<const CXXConstCastExpr*>(E);
			lookForDeclRefExprs(CCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXReinterpretCastExprClass:
		{
			const CXXReinterpretCastExpr* RCE = static_cast<const CXXReinterpretCastExpr*>(E);
			lookForDeclRefExprs(RCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXFunctionalCastExprClass:
		{
			const CXXFunctionalCastExpr* FCE = static_cast<const CXXFunctionalCastExpr*>(E);
			lookForDeclRefExprs(FCE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::CXXBindTemporaryExprClass:
		{
			const CXXBindTemporaryExpr* BTE = static_cast<const CXXBindTemporaryExpr*>(E);
			lookForDeclRefExprs(BTE->getSubExpr(),refs,pos);
			break;
		}
		case Stmt::BinaryOperatorClass:
		{
			const BinaryOperator* BO = static_cast<const BinaryOperator*>(E);
			lookForDeclRefExprs(BO->getLHS(),refs,pos);
			lookForDeclRefExprs(BO->getRHS(),refs,pos);
			break;
		}
		case Stmt::ConditionalOperatorClass:
		{
			const ConditionalOperator* CO = static_cast<const ConditionalOperator*>(E);
			lookForDeclRefExprs(CO->getLHS(),refs,pos);
			lookForDeclRefExprs(CO->getRHS(),refs,pos);
			break;
		}
		case Stmt::BinaryConditionalOperatorClass:
		{
			const BinaryConditionalOperator* BCO = static_cast<const BinaryConditionalOperator*>(E);
			lookForDeclRefExprs(BCO->getCommon(),refs,pos);
			lookForDeclRefExprs(BCO->getFalseExpr(),refs,pos);
			break;
		}
		case Stmt::IntegerLiteralClass:
		case Stmt::CharacterLiteralClass:
		case Stmt::StringLiteralClass:
		case Stmt::FloatingLiteralClass:
		case Stmt::UnaryExprOrTypeTraitExprClass:
		case Stmt::StmtExprClass:
		case Stmt::PredefinedExprClass:
		case Stmt::CompoundLiteralExprClass:
		case Stmt::PackExpansionExprClass:
		case Stmt::CXXUnresolvedConstructExprClass:
		case Stmt::CXXDependentScopeMemberExprClass:
		case Stmt::CXXThisExprClass:
		case Stmt::CXXTemporaryObjectExprClass:
		case Stmt::CXXNullPtrLiteralExprClass:
		case Stmt::CXXBoolLiteralExprClass:
		case Stmt::DependentScopeDeclRefExprClass:
		case Stmt::UnresolvedLookupExprClass:
		case Stmt::SizeOfPackExprClass:
		case Stmt::CXXDefaultArgExprClass:
                case Stmt::GNUNullExprClass:
		{
			break;
		}
		case Stmt::LambdaExprClass:
		{
			// TODO: implement usage of catched variables
			break;
		}
		case Stmt::CallExprClass:
		{
			const CallExpr* CE = static_cast<const CallExpr*>(E);
			std::set<ValueHolder> __callrefs;
			for (unsigned i=0; i!=CE->getNumArgs(); ++i) {
				lookForDeclRefExprs(CE->getArg(i),__callrefs,pos);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		case Stmt::CXXConstructExprClass:
		{
			const CXXConstructExpr* CE = static_cast<const CXXConstructExpr*>(E);
			std::set<ValueHolder> __callrefs;
			for (unsigned i=0; i!=CE->getNumArgs(); ++i) {
				lookForDeclRefExprs(CE->getArg(i),__callrefs,pos);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		case Stmt::CXXOperatorCallExprClass:
		{
			const CXXOperatorCallExpr* OCE = static_cast<const CXXOperatorCallExpr*>(E);
			std::set<ValueHolder> __callrefs;
			for (unsigned i=0; i!=OCE->getNumArgs(); ++i) {
				lookForDeclRefExprs(OCE->getArg(i),__callrefs,pos);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		case Stmt::CXXMemberCallExprClass:
		{
			const CXXMemberCallExpr* MCE = static_cast<const CXXMemberCallExpr*>(E);
			lookForDeclRefExprs(MCE->getImplicitObjectArgument(),refs);
			std::set<ValueHolder> __callrefs;
			for (unsigned i=0; i!=MCE->getNumArgs(); ++i) {
				lookForDeclRefExprs(MCE->getArg(i),__callrefs,pos);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.insert(*i);
			}
			break;
		}
		default:
		{
			if (opts.exit_on_error) {
				llvm::outs() << "\nERROR: No implementation in lookForDeclRefWithMemberExprs for:\n";
				E->dump();
				exit(EXIT_FAILURE);
			}
			else {
				unsupportedExprWithMemberExprRefs.insert(std::string(E->getStmtClassName()));
			}
		}
	}
}

const ArraySubscriptExpr* DbJSONClassVisitor::lookForArraySubscriptExprInCallExpr(const Expr* E) {

	switch(E->getStmtClass()) {
		case Stmt::CallExprClass:
		{
			const CallExpr* CE = static_cast<const CallExpr*>(E);
			return lookForArraySubscriptExprInCallExpr(CE->getCallee());
		}
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			return lookForArraySubscriptExprInCallExpr(ICE->getSubExpr());
		}
		case Stmt::DeclRefExprClass:
			return 0;
		case Stmt::MemberExprClass:
			return 0;
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			return ASE;
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			return lookForArraySubscriptExprInCallExpr(PE->getSubExpr());
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			return lookForArraySubscriptExprInCallExpr(UO->getSubExpr());
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			return lookForArraySubscriptExprInCallExpr(CSCE->getSubExpr());
		}
	}

	return 0;
}

const Expr* DbJSONClassVisitor::stripCastsEx(const Expr* E, std::vector<CStyleCastOrType>& vC) {

	switch(E->getStmtClass()) {
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			vC.push_back(CStyleCastOrType(ICE->getType()));
			return stripCastsEx(ICE->getSubExpr(),vC);
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			vC.push_back(const_cast<CStyleCastExpr*>(CSCE));
			return stripCastsEx(CSCE->getSubExpr(),vC);
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			return stripCastsEx(PE->getSubExpr(),vC);
		}
	}
	return E;
}

const Expr* DbJSONClassVisitor::stripCasts(const Expr* E) {
	switch(E->getStmtClass()) {
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			return stripCasts(ICE->getSubExpr());
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			return stripCasts(CSCE->getSubExpr());
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			return stripCasts(PE->getSubExpr());
		}
	}
	return E;
}

const UnaryOperator* DbJSONClassVisitor::lookForUnaryOperatorInCallExpr(const Expr* E) {

	switch(E->getStmtClass()) {
		case Stmt::CallExprClass:
		{
			const CallExpr* CE = static_cast<const CallExpr*>(E);
			return lookForUnaryOperatorInCallExpr(CE->getCallee());
		}
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			return lookForUnaryOperatorInCallExpr(ICE->getSubExpr());
		}
		case Stmt::DeclRefExprClass:
			return 0;
		case Stmt::MemberExprClass:
			return 0;
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			return lookForUnaryOperatorInCallExpr(ASE->getLHS());
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			return lookForUnaryOperatorInCallExpr(PE->getSubExpr());
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			return UO;
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			return lookForUnaryOperatorInCallExpr(CSCE->getSubExpr());
		}
	}

	return 0;
}

/* Look for variables (DeclRefExpr) or constant address values (IntegerLiteral, CharacterLiteral) and
 *  save each occurrence in the 'refs' mapping
 *  i.e. in 'a.b->c->d' the DeclRefExpr points to 'a' and in '((struct B *)30)->a' the address value is 30
 * Keep track of implicit or explicit casts and corresponding member expressions and link each
 *  MemberExpr chain with the variable in 'refs'
 * In some cases the bottom MemberExpr might originate from a function call therefore the saved variable will be
 *  a call reference, i.e. 'getB('x',3.0)->a.i'
 * Normally there is only one MemberExpr chain (therefore we have only one variable at the bottom of the chain
 *  which is the root variable of the MemberExpr)
 * In some cases we might have multiple variables in the MemberExpr chain. This can happen in two cases:
 *  1. CompoundLiteral initializer is used at the bottom of MemberExpr chain (root variable of the MemberExpr), i.e.
 *    ((struct A){.i=3,.pB=gi}).pB->a
 *  2. StmtExpr is used at the bottom of MemberExpr chain (root variable of the MemberExpr), i.e.
 *    ({do {} while(0); (struct A*)0+gi;})->i
 * It can also happen that the primary MemberExpr chain diverges into multiple secondary chains, like in the example below:
 *    (&((pA->pB+4)->a)+gi+pA->i)->i
 *  Here each MemberExpr along the chain has multiple variables at its base (and some of them can be another MemberExpr)
 *  TODO: (...)
 */

// [TODO] remove MEIdx, maybe not possible

void DbJSONClassVisitor::lookForDeclRefWithMemberExprsInternal(const Expr* E, DREMap_t& refs, std::vector<MemberInfo_t> *cache,
		std::vector<CStyleCastOrType> castVec, unsigned MEIdx, const CallExpr* CE, bool IgnoreLiteral) {
	// static int depth = 0;
	// llvm::errs()<<depth++<<":\t"<<E->getStmtClassName()<< !!cache <<'\n';
	
	switch(E->getStmtClass()) {
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(ICE->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::ConstantExprClass:
		{
			const ConstantExpr* ConstExpr = static_cast<const ConstantExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(ConstExpr->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::MemberExprClass:
		{
			const MemberExpr* ME = static_cast<const MemberExpr*>(E);
			if(cache){
				noticeTypeClass(ME->getBase()->getType());
				cache->push_back({ME,ME->getBase()->getType(),0,CE});
				DoneMEs.insert(ME);
				CE=0;
				castVec.clear();
				lookForDeclRefWithMemberExprsInternal(ME->getBase(),refs,cache,castVec,MEIdx+1,CE,IgnoreLiteral);
			}
			else {
				ExprRef_t v;
				// enxxure valuecast type
				noticeTypeClass(ME->getType());
				castVec.push_back(ME->getType());
				CStyleCastOrType valuecast = getMatchingCast(castVec);
				v.setME(ME,valuecast);
				v.setMeIdx(MEIdx);
				refs.insert(v);
			}
			break;
		}
		case Stmt::CXXStaticCastExprClass:
		{
			const CXXStaticCastExpr* SCE = static_cast<const CXXStaticCastExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(SCE->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::MaterializeTemporaryExprClass:
		{
			const MaterializeTemporaryExpr* MTE = static_cast<const MaterializeTemporaryExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(MTE->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::ExprWithCleanupsClass:
		{
			const ExprWithCleanups* EWC = static_cast<const ExprWithCleanups*>(E);
			lookForDeclRefWithMemberExprsInternal(EWC->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::IntegerLiteralClass:
		case Stmt::CharacterLiteralClass:
		{
			if (IgnoreLiteral && !cache) {
				break;
			}

			int64_t i;
			if (E->getStmtClass()==Stmt::IntegerLiteralClass) {
				const IntegerLiteral* IL = static_cast<const IntegerLiteral*>(E);
				i = llvm::APSInt(IL->getValue(),IL->getType()->isUnsignedIntegerOrEnumerationType()).extOrTrunc(63).getExtValue();
			}
			else {
				const CharacterLiteral* CL = static_cast<const CharacterLiteral*>(E);
				i = CL->getValue();
			}

			ExprRef_t v;
			CStyleCastOrType valuecast = getMatchingCast(castVec);
			v.setAddress(i,valuecast);
			v.setMeIdx(MEIdx);
			refs.insert(v);
			break;
		}

		case Stmt::StringLiteralClass:
		{
			if (IgnoreLiteral && !cache) {

				break;
			}

			const StringLiteral* SL = static_cast<const StringLiteral*>(E);
			std::string s = SL->getBytes().str();

			ExprRef_t v;
			CStyleCastOrType valuecast = getMatchingCast(castVec);
			v.setString(s,valuecast);
			v.setMeIdx(MEIdx);
			refs.insert(v);
			break;
		}
		case Stmt::FloatingLiteralClass:
		{
			if (IgnoreLiteral && !cache) {
				break;
			}

			double fp;
			const FloatingLiteral* FL = static_cast<const FloatingLiteral*>(E);
			llvm::APFloat FV = FL->getValue();
			if (&FV.getSemantics()==&llvm::APFloat::IEEEsingle()) {
			    fp = FL->getValue().convertToFloat();
			}
			else {
			    fp = FL->getValue().convertToDouble();
			}

			ExprRef_t v;
			CStyleCastOrType valuecast = getMatchingCast(castVec);
			v.setFloating(fp,valuecast);
			v.setMeIdx(MEIdx);
			refs.insert(v);
			break;
		}

		case Stmt::DeclRefExprClass:
		{
			const DeclRefExpr* DRE = static_cast<const DeclRefExpr*>(E);
			ExprRef_t v;
			CStyleCastOrType valuecast = getMatchingCast(castVec);
			if (CE) {
				const ValueDecl* VD = DRE->getDecl();
				if ((VD->getKind()==Decl::Var)||(VD->getKind()==Decl::ParmVar)) {
					v.setRefCall(CE,VD,valuecast);
				}
				else {
					v.setCall(CE,valuecast);
				}
			}
			else {
				v.setValue(DRE->getDecl(),valuecast);
			}
			v.setMeIdx(MEIdx);
			refs.insert(v);
			break;
		}
		case Stmt::CompoundLiteralExprClass:
		{
			const CompoundLiteralExpr* CLE = static_cast<const CompoundLiteralExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(CLE->getInitializer(),refs,0,castVec,MEIdx,CE,IgnoreLiteral);

			// /* Save dummy variable only to hold member expression information as there is no real primary variables */
			// [DEPRECATED]
			break;
		}
		case Stmt::InitListExprClass:	/* Can only happen at the CompoundLiteral or StmtExpression */
		{
			const InitListExpr* ILE = static_cast<const InitListExpr*>(E);
			for (unsigned i=0; i<ILE->getNumInits(); ++i) {
				std::vector<CStyleCastOrType> castVec;
				const Expr* IE = ILE->getInit(i);
				lookForDeclRefWithMemberExprsInternal(IE,refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			}
			break;
		}
		case Stmt::CompoundAssignOperatorClass:
		{
			const CompoundAssignOperator* CAO = static_cast<const CompoundAssignOperator*>(E);
			ExprRef_t v;
			CStyleCastOrType valuecast = getMatchingCast(castVec);
			if (CE) {
				v.setRefCall(CE,CAO,valuecast);
			}
			else {
				v.setCAO(CAO,valuecast);
			}
			v.setMeIdx(MEIdx);
			refs.insert(v);
			break;
		}
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			ExprRef_t v;
			CStyleCastOrType valuecast = getMatchingCast(castVec);
			if (CE) {
				v.setRefCall(CE,ASE,valuecast);
			}
			else {
				// ensure valuecast type
				noticeTypeClass(ASE->getType());
				castVec.push_back(ASE->getType());
				valuecast = getMatchingCast(castVec);
				v.setAS(ASE,valuecast);
			}
			v.setMeIdx(MEIdx);
			refs.insert(v);
			break;
		}
		case Stmt::OffsetOfExprClass:
		{
			const OffsetOfExpr* OOE = static_cast<const OffsetOfExpr*>(E);
            ExprRef_t v;
            if (CE) {
                /* I don't think we can get here; would need: 'offsetof(...)(...)' */
                v.setRefCall(CE,OOE);
            }
            else {
                v.setOOE(OOE);
            }
            v.setMeIdx(MEIdx);
			refs.insert(v);
			break;
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(PE->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			if (UO->getOpcode()==UO_Deref) {
				ExprRef_t v;
				CStyleCastOrType valuecast = getMatchingCast(castVec);
				std::set<ValueHolder> callrefs;
				lookForDeclRefExprsWithStmtExpr(UO->getSubExpr(),callrefs);
				if (CE) {
					if ((callrefs.size()==1)&&((*callrefs.begin()).getValue()->getKind()==Decl::Function)) {
						v.setCall(CE,valuecast);
					}
					else {
						v.setRefCall(CE,UO,valuecast);
					}
				}
				else {
					// ensure valuecast type
					noticeTypeClass(UO->getType());
					castVec.push_back(UO->getType());
					valuecast = getMatchingCast(castVec);
					v.setUnary(UO,valuecast);
				}
				v.setMeIdx(MEIdx);
				refs.insert(v);
			}
			else {
				lookForDeclRefWithMemberExprsInternal(UO->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			}
			break;
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			castVec.push_back(CStyleCastOrType(const_cast<CStyleCastExpr*>(CSCE)));
			lookForDeclRefWithMemberExprsInternal(CSCE->getSubExpr(),refs,cache,castVec,MEIdx,CE,false);
			break;
		}
		case Stmt::CXXConstCastExprClass:
		{
			const CXXConstCastExpr* CCE = static_cast<const CXXConstCastExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(CCE->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::CXXReinterpretCastExprClass:
		{
			const CXXReinterpretCastExpr* RCE = static_cast<const CXXReinterpretCastExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(RCE->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::CXXFunctionalCastExprClass:
		{
			const CXXFunctionalCastExpr* FCE = static_cast<const CXXFunctionalCastExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(FCE->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::CXXBindTemporaryExprClass:
		{
			const CXXBindTemporaryExpr* BTE = static_cast<const CXXBindTemporaryExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(BTE->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::BinaryOperatorClass:
		{
			const BinaryOperator* BO = static_cast<const BinaryOperator*>(E);
			if ((BO->getOpcode()==BO_Add)||(BO->getOpcode()==BO_Sub)) {
				bool Ldone = false;
				bool Rdone = false;
				int64_t shift = 0;
				Ldone = tryComputeOffsetExpr(BO->getLHS(),&shift,BO->getOpcode());
				Rdone = tryComputeOffsetExpr(BO->getRHS(),&shift,BO->getOpcode());
				if (cache) {
					cache->back().offset = shift;
				}
				if (Ldone&&Rdone) {
					/* No referenced variable; computed offset is actually an indirection address */
					ExprRef_t v;
					CStyleCastOrType valuecast = getMatchingCast(castVec);
					if (CE) {
						v.setRefCall(CE,shift,valuecast);
					}
					else {
						v.setAddress(shift,valuecast);
					}
					if (cache) {
						cache->back().offset = 0;
					}
					v.setMeIdx(MEIdx);
					refs.insert(v);
				}
				else if ((!Ldone)&&(!Rdone)) {	
					/* Both "+/-" arguments are referencing variables */
					std::vector<CStyleCastOrType> castVec;
					lookForDeclRefWithMemberExprsInternal(BO->getLHS(),refs,cache,castVec,MEIdx,CE,true);
					lookForDeclRefWithMemberExprsInternal(BO->getRHS(),refs,0,castVec,MEIdx,0,true);
				} else {
					/* Variables are referenced either on the left side or the right side of the "+/-" */
					if (!Ldone) {
						std::vector<CStyleCastOrType> castVec;
						lookForDeclRefWithMemberExprsInternal(BO->getLHS(),refs,cache,castVec,MEIdx,CE,true);
					}
					if (!Rdone) {
						std::vector<CStyleCastOrType> castVec;
						lookForDeclRefWithMemberExprsInternal(BO->getRHS(),refs,cache,castVec,MEIdx,CE,true);
					}
				}
			}
			else if(BO->getOpcode() >= BO_Cmp && BO->getOpcode() <= BO_LOr){
				Expr::EvalResult Res;
				ExprRef_t v;
				CStyleCastOrType valuecast = getMatchingCast(castVec);
				if (CE) {
					v.setRefCall(CE,BO,valuecast);
				}
				else if((!BO->isValueDependent()) && BO->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(BO,Res)) {
					v.setInteger(Res.Val.getInt().extOrTrunc(63).getExtValue(),valuecast);
				}
				else {
					v.setLogic(BO,valuecast);
				}
				v.setMeIdx(MEIdx);
				refs.insert(v);
				break;
				// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!	
			}
			else {
				std::vector<CStyleCastOrType> castVec;
				lookForDeclRefWithMemberExprsInternal(BO->getLHS(),refs,0,castVec,MEIdx,CE,true);
				lookForDeclRefWithMemberExprsInternal(BO->getRHS(),refs,0,castVec,MEIdx,0,true);
			}
			break;
		}
		case Stmt::OpaqueValueExprClass:
		{
			const OpaqueValueExpr* OVE = static_cast<const OpaqueValueExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(OVE->getSourceExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::ChooseExprClass:
		{
			const ChooseExpr* ChE = static_cast<const ChooseExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(ChE->getChosenSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::GenericSelectionExprClass:{
			const GenericSelectionExpr* GSE = static_cast<const GenericSelectionExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(GSE->getResultExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::ConditionalOperatorClass:
		{
			const ConditionalOperator* CO = static_cast<const ConditionalOperator*>(E);
			int64_t condVal = 0;
			bool condEvaluated = tryComputeOffsetExpr(CO->getCond(),&condVal,BO_Add,true);
			if (condEvaluated) {
				/* The condition was successfully computed; take the viable path only */
				std::vector<CStyleCastOrType> castVec;
				if (condVal!=0) {
					lookForDeclRefWithMemberExprsInternal(CO->getLHS(),refs,0,castVec,MEIdx,CE,IgnoreLiteral);
				}
				else {
					lookForDeclRefWithMemberExprsInternal(CO->getRHS(),refs,0,castVec,MEIdx,CE,IgnoreLiteral);
				}
			}
			else {
				/* Condition cannot be computed at compile time; we need to grab variables from both paths */
				std::vector<CStyleCastOrType> castVec;
				lookForDeclRefWithMemberExprsInternal(CO->getLHS(),refs,0,castVec,MEIdx,CE,IgnoreLiteral);
				lookForDeclRefWithMemberExprsInternal(CO->getRHS(),refs,0,castVec,MEIdx,0,IgnoreLiteral);
			}
			// /* Save dummy variable only to hold member expression information as there is no real primary variables
			//     (and possibly multiple secondary variables) */
			// [DEPRECATED]
			break;
		}
		case Stmt::BinaryConditionalOperatorClass:
		{
			const BinaryConditionalOperator* BCO = static_cast<const BinaryConditionalOperator*>(E);
			int64_t condVal = 0;
			const Expr* cond = BCO->getCond();
			if (cond->getStmtClass()==Stmt::OpaqueValueExprClass) {
				const OpaqueValueExpr* opaque_cond = static_cast<const OpaqueValueExpr*>(cond);
				cond = opaque_cond->getSourceExpr();
			}
			bool condEvaluated = tryComputeOffsetExpr(cond,&condVal,BO_Add,true);
			if (condEvaluated) {
				/* The condition was successfully computed; take the viable path only */
				std::vector<CStyleCastOrType> castVec;
				if (condVal!=0) {
					lookForDeclRefWithMemberExprsInternal(BCO->getTrueExpr(),refs,0,castVec,MEIdx,CE,IgnoreLiteral);
				}
				else {
					lookForDeclRefWithMemberExprsInternal(BCO->getFalseExpr(),refs,0,castVec,MEIdx,CE,IgnoreLiteral);
				}
			}
			else {
				/* Condition cannot be computed at compile time; we need to grab variables from both paths */
				std::vector<CStyleCastOrType> castVec;
				lookForDeclRefWithMemberExprsInternal(BCO->getTrueExpr(),refs,0,castVec,MEIdx,CE,IgnoreLiteral);
				lookForDeclRefWithMemberExprsInternal(BCO->getFalseExpr(),refs,0,castVec,MEIdx,0,IgnoreLiteral);
			}
			// /* Save dummy variable only to hold member expression information as there is no real primary variables
			//     (and possibly multiple secondary variables) */
			// [DEPRECATED]
			break;
		}
		case Stmt::UnaryExprOrTypeTraitExprClass:
		case Stmt::PredefinedExprClass:
		case Stmt::PackExpansionExprClass:
		case Stmt::CXXTemporaryObjectExprClass:
		case Stmt::CXXNullPtrLiteralExprClass:
		case Stmt::CXXBoolLiteralExprClass:
		case Stmt::DependentScopeDeclRefExprClass:
		case Stmt::SizeOfPackExprClass:
		case Stmt::CXXDefaultArgExprClass:
		case Stmt::GNUNullExprClass:
		case Stmt::CXXConstructExprClass:
		{
			break;
		}
		case Stmt::LambdaExprClass:
		{
			// TODO: implement usage of caught variables
			break;
		}
		case Stmt::StmtExprClass:
		{
			const StmtExpr* SE = static_cast<const StmtExpr*>(E);
			const CompoundStmt* CS = SE->getSubStmt();
			CompoundStmt::const_body_iterator i = CS->body_begin();
			if (i!=CS->body_end()) {
				/* We have at least one statement in the body; get the last one which is actually an expression */
				CompoundStmt::const_body_iterator e = CS->body_end()-1;
				while((e>=i)&&(!isa<Expr>(*e))) {
					e=e-1;
				}
				if (e<i) {
					break;
				}
				const Stmt* S = *e;
				lookForDeclRefWithMemberExprsInternal(cast<Expr>(S),refs,0,castVec,MEIdx,0,IgnoreLiteral);
			}
			// /* Save dummy variable only to hold member expression information as there is no real primary variables */
			// [DEPRECATED]
			break;
		}
		case Stmt::CallExprClass:
		{
			const CallExpr* _CE = static_cast<const CallExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(_CE->getCallee(),refs,cache,castVec,MEIdx,_CE,IgnoreLiteral);
			break;
		}
		case Stmt::CXXOperatorCallExprClass:
		{
			/* TODO */
			const CXXOperatorCallExpr* OCE = static_cast<const CXXOperatorCallExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(OCE->getCallee(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::CXXMemberCallExprClass:
		{
			/* TODO */
			const CXXMemberCallExpr* MCE = static_cast<const CXXMemberCallExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(MCE->getCallee(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		case Stmt::VAArgExprClass:
		{
			const VAArgExpr* VAAE = static_cast<const VAArgExpr*>(E);
			lookForDeclRefWithMemberExprsInternal(VAAE->getSubExpr(),refs,cache,castVec,MEIdx,CE,IgnoreLiteral);
			break;
		}
		// unhandled cpp nodes
		case Stmt::CXXDependentScopeMemberExprClass:
		case Stmt::UnresolvedMemberExprClass:
		case Stmt::UnresolvedLookupExprClass:
		case Stmt::CXXUnresolvedConstructExprClass:
		case Stmt::CXXThisExprClass:
		{
			ExprRef_t v;
			CStyleCastOrType valuecast = getMatchingCast(castVec);
			v.setUnhandled(E,CE,valuecast);
			v.setMeIdx(MEIdx);
			refs.insert(v);
			break;
		}

		default:
		{
			if (opts.exit_on_error) {
				llvm::outs() << "\nERROR: No implementation in lookForDeclRefExprs for:\n";
				E->dump();
				assert(0);
				exit(EXIT_FAILURE);
			}
			else {
				unsupportedExprRefs.insert(std::string(E->getStmtClassName()));
			}
		}
	}
	// llvm::errs()<<--depth<<":\t"<<E->getStmtClassName()<<" done\n";
}



void DbJSONClassVisitor::lookForDeclRefWithMemberExprsInOffset(const Expr* E, std::vector<ExprRef_t>& OffsetRefs) {

	DbJSONClassVisitor::DREMap_t DREMap;
	std::vector<CStyleCastOrType> castVec;
	lookForDeclRefWithMemberExprsInternal(E,DREMap,0,castVec,0,0);
	OffsetRefs.insert(OffsetRefs.end(),DREMap.begin(),DREMap.end());
}

/* Tries to evaluate a given expression as an integer constant expression
    (leading casts can be excluded from the computation using the `stripCastFlag` flag set to true)
    and adds the value into the `LiteralOffset` variable (taking into account the operator kind, i.e. (+/-)))
   If it is impossible to precompute the value all variable expressions are saved in the `OffsetRefs` variable
   Returns true if it was possible to precompute the value, otherwise return false */
void DbJSONClassVisitor::computeOffsetExpr(const Expr* E, int64_t* LiteralOffset,
		std::vector<ExprRef_t>& OffsetRefs, BinaryOperatorKind kind, bool stripCastFlag) {
	
	Expr::EvalResult Res;
	const Expr* nE = E;
	if (stripCastFlag) {
		nE = stripCasts(E);
	}
	if((!nE->isValueDependent()) && nE->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(nE,Res)){
		if (kind==BO_Add) {
			*LiteralOffset += Res.Val.getInt().extOrTrunc(63).getExtValue();
		}
		else {
			*LiteralOffset -= Res.Val.getInt().extOrTrunc(63).getExtValue();
		}
	}
	else {
		lookForDeclRefWithMemberExprsInOffset(E,OffsetRefs);
	}
}

/* Tries to evaluate a given expression as an integer constant expression
    (leading casts can be excluded from the computation using the `stripCastFlag` flag set to true)
    If it is possible to precompute the value adds the value into the `LiteralOffset` variable
     (taking into account the operator kind, i.e. (+/-)) and returns true
    Otherwise returns false */
bool DbJSONClassVisitor::tryComputeOffsetExpr(const Expr* E, int64_t* LiteralOffset, BinaryOperatorKind kind,
		bool stripCastFlag) {

	Expr::EvalResult Res;
	if (stripCastFlag) {
		E = stripCasts(E);
	}
	if((!E->isValueDependent()) && E->isEvaluatable(Context) && tryEvaluateIntegerConstantExpr(E,Res)){
		if (kind==BO_Add) {
			*LiteralOffset += Res.Val.getInt().extOrTrunc(63).getExtValue();
		}
		else {
			*LiteralOffset -= Res.Val.getInt().extOrTrunc(63).getExtValue();
		}
	}
	else {
		return false;
	}
	return true;
}

/*  Traverse all consecutive BinaryOperator (+/-) nodes (descend into ImplicitCastExpr, CStyleCastExpr or ParenExpr)
 *   to collect multiple potential expressions for a dereference offset and store all DeclRefExpr encountered there
 *   (including MemberExpr list) in 'OffsetRefs' (also store the sum of evaluated constant expressions in 'LiteralOffset')
 *  Returns true if the value of literal offset was computed, otherwise returns false
 *  It is assumed that the 'B0' parameter is already (+/-) BinaryOperator
 */
void DbJSONClassVisitor::mergeBinaryOperators(const BinaryOperator* BO, int64_t* LiteralOffset,
		std::vector<ExprRef_t>& OffsetRefs, BinaryOperatorKind kind) {

	// Process offset expression for BinaryOperator (in the context of pointer dereference)
	const Expr* RHS = lookForNonParenExpr(BO->getRHS());
	const Expr* LHS = lookForNonParenExpr(BO->getLHS());
	int valueComputed = 0;

	if (LHS->getStmtClass()==Stmt::BinaryOperatorClass) {
		const BinaryOperator* nBO = static_cast<const BinaryOperator*>(LHS);
		if ((nBO->getOpcode()==BO_Add)||(nBO->getOpcode()==BO_Sub)) {
			mergeBinaryOperators(nBO,LiteralOffset,OffsetRefs,nBO->getOpcode());
			computeOffsetExpr(BO->getRHS(),LiteralOffset,OffsetRefs,kind);
			return;
		}
	}

	computeOffsetExpr(BO->getLHS(),LiteralOffset,OffsetRefs,kind);

	if (RHS->getStmtClass()==Stmt::BinaryOperatorClass) {
		const BinaryOperator* nBO = static_cast<const BinaryOperator*>(RHS);
		if ((nBO->getOpcode()==BO_Add)||(nBO->getOpcode()==BO_Sub)) {
			mergeBinaryOperators(nBO,LiteralOffset,OffsetRefs,nBO->getOpcode());
			return;
		}
	}
	computeOffsetExpr(BO->getRHS(),LiteralOffset,OffsetRefs,kind);
	return;
}

/* Walk the AST hierarchy and descend into ParenExpr and return first expression other than that */
const Expr* DbJSONClassVisitor::lookForNonParenExpr(const Expr* E) {
	switch(E->getStmtClass()) {
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			return lookForNonParenExpr(PE->getSubExpr());
			break;
		}
	}
	return E;
}

bool DbJSONClassVisitor::tryEvaluateIntegerConstantExpr(const Expr* E, Expr::EvalResult& Res) {
	// E->EvaluateAsConstantExpr(Res,Context);
	compatibility::EvaluateAsConstantExpr(E,Res,Context);
	if (Res.Val.getKind()==APValue::Int) {
		return true;
	}
	return false;
}

/*
 *  Walk the AST hierarchy starting from the given expression (which should be a direct descendant of UnaryOperator *),
 *   descend into ImplicitCastExpr, CStyleCastExpr or ParenExpr and store all encountered DeclRefExpr
 *   (including MemberExpr list) or integer literals in 'OffsetRefs'.
 *  When BinaryOperator (+/-) is encountered, traverse all consecutive BinaryOperator (+/-) nodes to collect
 *   multiple potential expressions for a dereference offset
 *  All encountered DeclRefExpr (including MemberExpr list) are stored in 'OffsetRefs', sum of all expressions that
 *   can be evaluated as integer constant expression is stored in 'LiteralOffset').
 *  Returns true if at least one DeclRefExpr or integer literal  is found along the way
 *   (which must be the case for the UnaryOperator*)
 */
void DbJSONClassVisitor::lookForDerefExprs(const Expr* E, int64_t* LiteralOffset, std::vector<ExprRef_t>& OffsetRefs) {

	const Expr* nE = stripCasts(E);
	if (nE->getStmtClass()==Stmt::BinaryOperatorClass) {
		const BinaryOperator* BO = static_cast<const BinaryOperator*>(nE);
		if ((BO->getOpcode()==BO_Add)||(BO->getOpcode()==BO_Sub)) {
			*LiteralOffset = 0;
			mergeBinaryOperators(BO,LiteralOffset,OffsetRefs,BO->getOpcode());
			return;
		}
	}
	lookForDeclRefWithMemberExprsInOffset(E,OffsetRefs);
}

void DbJSONClassVisitor::lookForExplicitCastExprs(const Expr* E, std::vector<QualType>& refs ) {

	switch(E->getStmtClass()) {
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			lookForExplicitCastExprs(ICE->getSubExpr(),refs);
			break;
		}
		case Stmt::ConstantExprClass:
		{
			const ConstantExpr* CE = static_cast<const ConstantExpr*>(E);
			lookForExplicitCastExprs(CE->getSubExpr(),refs);
			break;
		}
		case Stmt::MemberExprClass:
		{
			const MemberExpr* ME = static_cast<const MemberExpr*>(E);
			lookForExplicitCastExprs(ME->getBase(),refs);
			break;
		}
		case Stmt::CXXStaticCastExprClass:
		{
			const CXXStaticCastExpr* SCE = static_cast<const CXXStaticCastExpr*>(E);
			refs.push_back(SCE->getType());
			break;
		}
		case Stmt::MaterializeTemporaryExprClass:
		{
			const MaterializeTemporaryExpr* MTE = static_cast<const MaterializeTemporaryExpr*>(E);
			lookForExplicitCastExprs(MTE->getSubExpr(),refs);
			break;
		}
		case Stmt::ExprWithCleanupsClass:
		{
			const ExprWithCleanups* EWC = static_cast<const ExprWithCleanups*>(E);
			lookForExplicitCastExprs(EWC->getSubExpr(),refs);
			break;
		}
		case Stmt::DeclRefExprClass:
		{
			const DeclRefExpr* DRE = static_cast<const DeclRefExpr*>(E);
			break;
		}
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			lookForExplicitCastExprs(ASE->getLHS(),refs);
			break;
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			lookForExplicitCastExprs(PE->getSubExpr(),refs);
			break;
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			lookForExplicitCastExprs(UO->getSubExpr(),refs);
			break;
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			refs.push_back(CSCE->getType());
			break;
		}
		case Stmt::CXXConstCastExprClass:
		{
			const CXXConstCastExpr* CCE = static_cast<const CXXConstCastExpr*>(E);
			refs.push_back(CCE->getType());
			break;
		}
		case Stmt::CXXReinterpretCastExprClass:
		{
			const CXXReinterpretCastExpr* RCE = static_cast<const CXXReinterpretCastExpr*>(E);
			refs.push_back(RCE->getType());
			break;
		}
		case Stmt::CXXFunctionalCastExprClass:
		{
			const CXXFunctionalCastExpr* FCE = static_cast<const CXXFunctionalCastExpr*>(E);
			refs.push_back(FCE->getType());
			break;
		}
		case Stmt::CXXBindTemporaryExprClass:
		{
			const CXXBindTemporaryExpr* BTE = static_cast<const CXXBindTemporaryExpr*>(E);
			lookForExplicitCastExprs(BTE->getSubExpr(),refs);
			break;
		}
		case Stmt::BinaryOperatorClass:
		{
			const BinaryOperator* BO = static_cast<const BinaryOperator*>(E);
			lookForExplicitCastExprs(BO->getLHS(),refs);
			lookForExplicitCastExprs(BO->getRHS(),refs);
			break;
		}
		case Stmt::ConditionalOperatorClass:
		{
			const ConditionalOperator* CO = static_cast<const ConditionalOperator*>(E);
			lookForExplicitCastExprs(CO->getLHS(),refs);
			lookForExplicitCastExprs(CO->getRHS(),refs);
			break;
		}
		case Stmt::BinaryConditionalOperatorClass:
		{
			const BinaryConditionalOperator* BCO = static_cast<const BinaryConditionalOperator*>(E);
			lookForExplicitCastExprs(BCO->getCommon(),refs);
			lookForExplicitCastExprs(BCO->getFalseExpr(),refs);
			break;
		}
		case Stmt::IntegerLiteralClass:
		case Stmt::CharacterLiteralClass:
		case Stmt::StringLiteralClass:
		case Stmt::FloatingLiteralClass:
		case Stmt::UnaryExprOrTypeTraitExprClass:
		case Stmt::StmtExprClass:
		case Stmt::PredefinedExprClass:
		case Stmt::CompoundLiteralExprClass:
		case Stmt::PackExpansionExprClass:
		case Stmt::CXXUnresolvedConstructExprClass:
		case Stmt::CXXDependentScopeMemberExprClass:
		case Stmt::CXXThisExprClass:
		case Stmt::CXXTemporaryObjectExprClass:
		case Stmt::CXXNullPtrLiteralExprClass:
		case Stmt::CXXBoolLiteralExprClass:
		case Stmt::DependentScopeDeclRefExprClass:
		case Stmt::UnresolvedLookupExprClass:
		case Stmt::SizeOfPackExprClass:
		case Stmt::CXXDefaultArgExprClass:
                case Stmt::GNUNullExprClass:
		{
			break;
		}
		case Stmt::LambdaExprClass:
		{
			// TODO: implement usage of catched variables
			break;
		}
		case Stmt::CallExprClass:
		{
			const CallExpr* CE = static_cast<const CallExpr*>(E);
			std::vector<QualType> __callrefs;
			for (unsigned i=0; i!=CE->getNumArgs(); ++i) {
				lookForExplicitCastExprs(CE->getArg(i),__callrefs);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.push_back(*i);
			}
			break;
		}
		case Stmt::CXXConstructExprClass:
		{
			const CXXConstructExpr* CE = static_cast<const CXXConstructExpr*>(E);
			std::vector<QualType> __callrefs;
			for (unsigned i=0; i!=CE->getNumArgs(); ++i) {
				lookForExplicitCastExprs(CE->getArg(i),__callrefs);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.push_back(*i);
			}
			break;
		}
		case Stmt::CXXOperatorCallExprClass:
		{
			const CXXOperatorCallExpr* OCE = static_cast<const CXXOperatorCallExpr*>(E);
			std::vector<QualType> __callrefs;
			for (unsigned i=0; i!=OCE->getNumArgs(); ++i) {
				lookForExplicitCastExprs(OCE->getArg(i),__callrefs);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.push_back(*i);
			}
			break;
		}
		case Stmt::CXXMemberCallExprClass:
		{
			const CXXMemberCallExpr* MCE = static_cast<const CXXMemberCallExpr*>(E);
			lookForExplicitCastExprs(MCE->getImplicitObjectArgument(),refs);
			std::vector<QualType> __callrefs;
			for (unsigned i=0; i!=MCE->getNumArgs(); ++i) {
				lookForExplicitCastExprs(MCE->getArg(i),__callrefs);
			}
			for (auto i = __callrefs.begin(); i!=__callrefs.end(); ++i) {
				refs.push_back(*i);
			}
			break;
		}
		default:
		{
			if (opts.exit_on_error) {
				llvm::outs() << "\nERROR: No implementation in lookForDeclRefExprs for:\n";
				E->dump();
				assert(0);
				exit(EXIT_FAILURE);
			}
			else {
				unsupportedExprRefs.insert(std::string(E->getStmtClassName()));
			}
		}
	}
}

const Expr* DbJSONClassVisitor::lookForDeclReforMemberExpr(const Expr* E) {

	switch(E->getStmtClass()) {
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			return lookForDeclReforMemberExpr(ICE->getSubExpr());
		}
		case Stmt::DeclRefExprClass:
		case Stmt::MemberExprClass:
			return static_cast<const Expr*>(E);
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			return lookForDeclReforMemberExpr(ASE->getLHS());
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			return lookForDeclReforMemberExpr(PE->getSubExpr());
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			return lookForDeclReforMemberExpr(UO->getSubExpr());
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			return lookForDeclReforMemberExpr(CSCE->getSubExpr());
		}
	}
	return 0;
}

const Expr* DbJSONClassVisitor::lookForStmtExpr(const Expr* E) {

	switch(E->getStmtClass()) {
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			return lookForStmtExpr(ICE->getSubExpr());
		}
		case Stmt::DeclRefExprClass:
		case Stmt::MemberExprClass:
			return 0;
		case Stmt::StmtExprClass:
			return static_cast<const Expr*>(E);
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			return lookForStmtExpr(ASE->getLHS());
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			return lookForStmtExpr(PE->getSubExpr());
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			return lookForStmtExpr(UO->getSubExpr());
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			return lookForStmtExpr(CSCE->getSubExpr());
		}
	}
	return 0;
}

const DeclRefExpr* DbJSONClassVisitor::lookForBottomDeclRef(const Expr* E) {

	switch(E->getStmtClass()) {
		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			return lookForBottomDeclRef(ICE->getSubExpr());
		}
		case Stmt::DeclRefExprClass:
		{
			return static_cast<const DeclRefExpr*>(E);
		}
		case Stmt::MemberExprClass:
		{
			const MemberExpr* ME = static_cast<const MemberExpr*>(E);
			return lookForBottomDeclRef(static_cast<const Expr*>(*(ME->child_begin())));
		}
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			return lookForBottomDeclRef(ASE->getLHS());
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			return lookForBottomDeclRef(PE->getSubExpr());
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			return lookForBottomDeclRef(UO->getSubExpr());
		}
		case Stmt::ConstantExprClass:
		{
			const ConstantExpr* CE = static_cast<const ConstantExpr*>(E);
			return lookForBottomDeclRef(CE->getSubExpr());
			break;
		}
	}

	return 0;
}

bool DbJSONClassVisitor::lookForUnaryExprOrTypeTraitExpr(const Expr* E, std::vector<QualType>& QV) {

	  switch(E->getStmtClass()) {

		case Stmt::ImplicitCastExprClass:
		{
			const ImplicitCastExpr* ICE = static_cast<const ImplicitCastExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(ICE->getSubExpr(),QV);
			break;
		}
		case Stmt::ConstantExprClass:
		{
			const ConstantExpr* CE = static_cast<const ConstantExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(CE->getSubExpr(),QV);
			break;
		}
		case Stmt::MemberExprClass:
		{
			const MemberExpr* ME = static_cast<const MemberExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(ME->getBase(),QV);
			break;
		}
		case Stmt::CXXStaticCastExprClass:
		{
			const CXXStaticCastExpr* SCE = static_cast<const CXXStaticCastExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(SCE->getSubExpr(),QV);
			break;
		}
		case Stmt::MaterializeTemporaryExprClass:
		{
			const MaterializeTemporaryExpr* MTE = static_cast<const MaterializeTemporaryExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(MTE->getSubExpr(),QV);
			break;
		}
		case Stmt::ExprWithCleanupsClass:
		{
			const ExprWithCleanups* EWC = static_cast<const ExprWithCleanups*>(E);
			lookForUnaryExprOrTypeTraitExpr(EWC->getSubExpr(),QV);
			break;
		}
		case Stmt::ArraySubscriptExprClass:
		{
			const ArraySubscriptExpr* ASE = static_cast<const ArraySubscriptExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(ASE->getLHS(),QV);
			break;
		}
		case Stmt::ParenExprClass:
		{
			const ParenExpr* PE = static_cast<const ParenExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(PE->getSubExpr(),QV);
			break;
		}
		case Stmt::UnaryOperatorClass:
		{
			const UnaryOperator* UO = static_cast<const UnaryOperator*>(E);
			lookForUnaryExprOrTypeTraitExpr(UO->getSubExpr(),QV);
			break;
		}
		case Stmt::CStyleCastExprClass:
		{
			const CStyleCastExpr* CSCE = static_cast<const CStyleCastExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(CSCE->getSubExpr(),QV);
			break;
		}
		case Stmt::CXXConstCastExprClass:
		{
			const CXXConstCastExpr* CCE = static_cast<const CXXConstCastExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(CCE->getSubExpr(),QV);
			break;
		}
		case Stmt::CXXReinterpretCastExprClass:
		{
			const CXXReinterpretCastExpr* RCE = static_cast<const CXXReinterpretCastExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(RCE->getSubExpr(),QV);
			break;
		}
		case Stmt::CXXFunctionalCastExprClass:
		{
			const CXXFunctionalCastExpr* FCE = static_cast<const CXXFunctionalCastExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(FCE->getSubExpr(),QV);
			break;
		}
		case Stmt::CXXBindTemporaryExprClass:
		{
			const CXXBindTemporaryExpr* BTE = static_cast<const CXXBindTemporaryExpr*>(E);
			lookForUnaryExprOrTypeTraitExpr(BTE->getSubExpr(),QV);
			break;
		}
		case Stmt::BinaryOperatorClass:
		{
			const BinaryOperator* BO = static_cast<const BinaryOperator*>(E);
			lookForUnaryExprOrTypeTraitExpr(BO->getLHS(),QV);
			lookForUnaryExprOrTypeTraitExpr(BO->getRHS(),QV);
			break;
		}
		case Stmt::ConditionalOperatorClass:
		{
			const ConditionalOperator* CO = static_cast<const ConditionalOperator*>(E);
			lookForUnaryExprOrTypeTraitExpr(CO->getLHS(),QV);
			lookForUnaryExprOrTypeTraitExpr(CO->getRHS(),QV);
			break;
		}
		case Stmt::BinaryConditionalOperatorClass:
		{
			const BinaryConditionalOperator* BCO = static_cast<const BinaryConditionalOperator*>(E);
			lookForUnaryExprOrTypeTraitExpr(BCO->getCommon(),QV);
			lookForUnaryExprOrTypeTraitExpr(BCO->getFalseExpr(),QV);
			break;
		}
		case Stmt::IntegerLiteralClass:
		case Stmt::CharacterLiteralClass:
		case Stmt::StringLiteralClass:
		case Stmt::FloatingLiteralClass:
		case Stmt::StmtExprClass:
		case Stmt::PredefinedExprClass:
		case Stmt::CompoundLiteralExprClass:
		case Stmt::PackExpansionExprClass:
		case Stmt::CXXUnresolvedConstructExprClass:
		case Stmt::CXXDependentScopeMemberExprClass:
		case Stmt::CXXThisExprClass:
		case Stmt::CXXTemporaryObjectExprClass:
		case Stmt::CXXNullPtrLiteralExprClass:
		case Stmt::CXXBoolLiteralExprClass:
		case Stmt::DependentScopeDeclRefExprClass:
		case Stmt::UnresolvedLookupExprClass:
		case Stmt::SizeOfPackExprClass:
		case Stmt::CXXDefaultArgExprClass:
        case Stmt::GNUNullExprClass:
        case Stmt::DeclRefExprClass:
        case Stmt::LambdaExprClass:
        case Stmt::CallExprClass:
        case Stmt::CXXConstructExprClass:
        case Stmt::CXXOperatorCallExprClass:
        case Stmt::CXXMemberCallExprClass:
		{
			break;
		}
        case Stmt::UnaryExprOrTypeTraitExprClass:
		  {
			const UnaryExprOrTypeTraitExpr *UTTE = cast<UnaryExprOrTypeTraitExpr>(E);
			if (!UTTE->isArgumentType()) {
				// TODO: handle further attributes with expressions
			}
			else {
				QV.push_back(UTTE->getArgumentType());
			}
			return true;
		  }
		default:
			{
				if (opts.exit_on_error) {
					llvm::outs() << "\nERROR: No implementation in lookForUnaryExprOrTypeTraitExpr for:\n";
					E->dump();
					exit(EXIT_FAILURE);
				}
				else {
					unsupportedUnaryOrTypeExprRefs.insert(std::string(E->getStmtClassName()));
				}
			}
  	  }
  	  return false;
  }

  bool DbJSONClassVisitor::lookForTypeTraitExprs(const Expr* e, std::vector<const TypeTraitExpr*>& TTEV) {

	  switch(e->getStmtClass()) {
		  case Stmt::ConstantExprClass:
		  {
			const ConstantExpr *CE = cast<ConstantExpr>(e);
			return lookForTypeTraitExprs(CE->getSubExpr(),TTEV);
		  }
		  case Stmt::TypeTraitExprClass:
		  {
			const TypeTraitExpr *TTE = cast<TypeTraitExpr>(e);
			TTEV.push_back(TTE);
			return true;
		  }
		  case Stmt::ParenExprClass:
		  {
			const ParenExpr *PE = cast<ParenExpr>(e);
			return lookForTypeTraitExprs(PE->getSubExpr(),TTEV);
			return true;
		  }
		  case Stmt::UnaryOperatorClass:
		  {
			const UnaryOperator *UO = cast<UnaryOperator>(e);
			return lookForTypeTraitExprs(UO->getSubExpr(),TTEV);
			return true;
		  }
  	  }
  	  return false;
  }

  void DbJSONClassVisitor::lookForAttrTypes(const Attr* attr, std::vector<QualType>& QV) {

	  switch (attr->getKind()) {
	  	  case clang::attr::Aligned:
	  	  {
	  		  const AlignedAttr *a = cast<AlignedAttr>(attr);
	  		  const Expr* e = a->getAlignmentExpr();
                          if (e) {
	  		      if (lookForUnaryExprOrTypeTraitExpr(e,QV)) break;
	  		      else {
	  		    	std::set<ValueHolder> refs;
	  			    lookForDeclRefExprs(e,refs);
	  			    if (refs.size()>0) {
	  				    for (auto i=refs.begin(); i!=refs.end(); ++i) {
	  					    if ((*i).getValue()->getKind()==Decl::EnumConstant) {
	  						    const EnumConstantDecl *ECD = cast<EnumConstantDecl>((*i).getValue());
							    const EnumDecl *ED = static_cast<const EnumDecl*>(ECD->getDeclContext());
						 	    QualType ET = Context.getEnumType(ED);
							    QV.push_back(ET);
	  					    }
	  				    }
	  			    }
	  		      }
                          }
	  		  break;
	  	  }
	  	  default:
	  	  {
				if (opts.exit_on_error) {
					llvm::outs() << "\nWARNING: No implementation in lookForAttrTypes for Kind " << attr->getKind() << "\n";
				}
				else {
					unsupportedAttrRefs.insert(attr->getKind());
				}
	  	  }
	  }
  }

void DbJSONClassConsumer::LookForTemplateTypeParameters(QualType T, std::set<const TemplateTypeParmType*>& s) {

  	switch (T->getTypeClass()) {
  		  case Type::TemplateTypeParm:
  		  {
  			  const TemplateTypeParmType* ttp = cast<TemplateTypeParmType>(T);
  			  s.insert(ttp);
  		  }
  		  break;
  		  case Type::Builtin:
  		  case Type::Enum:
  			  break;
  		  case Type::LValueReference:
  		  {
  			const LValueReferenceType* lvrT = cast<LValueReferenceType>(T);
  			LookForTemplateTypeParameters(lvrT->getPointeeType(),s);
  		  }
  		  break;
		  case Type::RValueReference:
		  {
			  const RValueReferenceType* rvrT = cast<RValueReferenceType>(T);
			  LookForTemplateTypeParameters(rvrT->getPointeeType(),s);
		  }
		  break;
		  case Type::Typedef:
		  {
			  const TypedefType* tp = cast<TypedefType>(T);
			  TypedefNameDecl* D = tp->getDecl();
			  QualType tT = D->getTypeSourceInfo()->getType();
			  LookForTemplateTypeParameters(tT,s);
		  }
		  break;
		  COMPAT_VERSION_GE(14,
		  case Type::Using:
		  {
			  const UsingType* tp = cast<UsingType>(T);
			  QualType uT = tp->getUnderlyingType();
			  LookForTemplateTypeParameters(uT,s);
		  }
		  )
		  break;
		  case Type::Decltype:
		  {
			  const DecltypeType* tp = cast<DecltypeType>(T);
			  QualType uT = tp->getUnderlyingType();
			  LookForTemplateTypeParameters(uT,s);
		  }
		  break;
		  case Type::Pointer:
		  {
			  const PointerType* tp = cast<PointerType>(T);
			  QualType pT = tp->getPointeeType();
			  LookForTemplateTypeParameters(pT,s);
		  }
		  break;
		  case Type::IncompleteArray:
		  {
			  const IncompleteArrayType* tp = cast<IncompleteArrayType>(T);
			  QualType eT = tp->getElementType();
			  LookForTemplateTypeParameters(eT,s);
		  }
		  break;
		  case Type::ConstantArray:
		  {
			  const ConstantArrayType* tp = cast<ConstantArrayType>(T);
			  QualType eT = tp->getElementType();
			  LookForTemplateTypeParameters(eT,s);
		  }
		  break;
		  case Type::DependentSizedArray:
		  {
			  const DependentSizedArrayType* tp = cast<DependentSizedArrayType>(T);
			  LookForTemplateTypeParameters(tp->getElementType(),s);
			  LookForTemplateTypeParameters(tp->getSizeExpr()->getType(),s);
		  }
		  break;
		  case Type::MemberPointer:
		  {
			  const MemberPointerType* tp = cast<MemberPointerType>(T);
			  QualType pT = tp->getPointeeType();
			  LookForTemplateTypeParameters(pT,s);
			  QualType _class = QualType(tp->getClass(),0);
			  LookForTemplateTypeParameters(_class,s);
		  }
		  break;
		  case Type::TemplateSpecialization:
		  {
			  const TemplateSpecializationType* tp = cast<TemplateSpecializationType>(T);
			  if (!tp->isSugared()) {
				  for (const TemplateArgument &Arg : tp->template_arguments()) {
					  if (Arg.getKind() == TemplateArgument::Type) {
						  QualType TPT = Arg.getAsType();
						  LookForTemplateTypeParameters(TPT,s);
					  }
				  }
			  }
		  }
		  break;
		  case Type::DependentTemplateSpecialization:
		  {
			  const DependentTemplateSpecializationType* tp = cast<DependentTemplateSpecializationType>(T);
			  if (!tp->isSugared()) {
				  for (const TemplateArgument &Arg : tp->template_arguments()) {
					  if (Arg.getKind() == TemplateArgument::Type) {
						  QualType TPT = Arg.getAsType();
						  LookForTemplateTypeParameters(TPT,s);
					  }
				  }
			  }
		  }
		  break;
		  case Type::DependentName:
		  {
			const DependentNameType* tp = cast<DependentNameType>(T);
			NestedNameSpecifier * NNS = tp->getQualifier();
			if ((NNS->getKind()==NestedNameSpecifier::TypeSpecWithTemplate) ||
					(NNS->getKind()==NestedNameSpecifier::TypeSpec)) {
				const Type *T = NNS->getAsType();
				if (const TemplateSpecializationType *SpecType
									  = dyn_cast<TemplateSpecializationType>(T)) {
						for (const TemplateArgument &Arg : SpecType->template_arguments()) {
						  if (Arg.getKind() == TemplateArgument::Type) {
							  QualType TPT = Arg.getAsType();
							  LookForTemplateTypeParameters(TPT,s);
						  }
					  }
				}
				else {
					QualType QT = QualType(T,0);
					LookForTemplateTypeParameters(QT,s);
				}
			}
		  }
		  break;
		  case Type::PackExpansion:
		  {
			  const PackExpansionType* tp = cast<PackExpansionType>(T);
		  }
		  break;
		  case Type::Record:
		  {
			  const RecordType* tp = cast<RecordType>(T);
		  }
		  break;
		  case Type::Elaborated:
		  {
			  const ElaboratedType* tp = cast<ElaboratedType>(T);
			  QualType eT = tp->getNamedType();
			  LookForTemplateTypeParameters(eT,s);
		  }
		  break;
		  case Type::FunctionProto:
		  {
			  const FunctionProtoType* tp = cast<FunctionProtoType>(T);
			  LookForTemplateTypeParameters(tp->getReturnType(),s);
			  for (unsigned i = 0; i<tp->getNumParams(); ++i) {
				  LookForTemplateTypeParameters(tp->getParamType(i),s);
			  }
		  }
		  break;
                  case Type::FunctionNoProto:
                  {
                          const FunctionNoProtoType *tp = cast<FunctionNoProtoType>(T);
                          LookForTemplateTypeParameters(tp->getReturnType(),s);
                  }
                  break;
		  case Type::Paren:
		  {
			  const ParenType* tp = cast<ParenType>(T);
			  QualType iT = tp->getInnerType();
			  LookForTemplateTypeParameters(iT,s);
		  }
		  break;
		  case Type::InjectedClassName:
		  {
			  const InjectedClassNameType* tp = cast<InjectedClassNameType>(T);
			  QualType IST = tp->getInjectedSpecializationType();
			  LookForTemplateTypeParameters(IST,s);
		  }
		  break;
		  case Type::UnresolvedUsing:
		  {
			  const UnresolvedUsingType* tp = cast<UnresolvedUsingType>(T);
		  }
		  break;
		  case Type::SubstTemplateTypeParm:
		  {
		 	const SubstTemplateTypeParmType* tp = cast<SubstTemplateTypeParmType>(T);
		 	const TemplateTypeParmType* ttp = compatibility::getReplacedParmType(tp);
		 	s.insert(ttp);
		  }
		  break;
                  case Type::Attributed:
                  {
                      const AttributedType *tp = cast<AttributedType>(T);
                      QualType mT = tp->getModifiedType();
                      (void)mT;
                      QualType eT = tp->getEquivalentType();
                      LookForTemplateTypeParameters(eT,s);
                  }
                  break;
				  COMPAT_VERSION_GE(15,
                  case Type::BTFTagAttributed:
                  {
                      const BTFTagAttributedType *tp = cast<BTFTagAttributedType>(T);
                      QualType eT = tp->getWrappedType();
                      LookForTemplateTypeParameters(eT,s);
                  }
                  break;
				  )
				  case Type::VariableArray:
                  {
                      const VariableArrayType *tp = cast<VariableArrayType>(T);
                      QualType vaT = tp->getElementType();
                      LookForTemplateTypeParameters(vaT,s);
                  }
                  break;
                  case Type::Decayed:
                  {
                      const DecayedType *tp = cast<DecayedType>(T);
                      QualType dT = tp->getPointeeType();
                      LookForTemplateTypeParameters(dT,s);
                  }
                  break;
                  case Type::TypeOfExpr:
                  {
                      const TypeOfExprType *tp = cast<TypeOfExprType>(T);
                      QualType toT = tp->getUnderlyingExpr()->getType();
                      LookForTemplateTypeParameters(toT,s);
                  }
                  break;
                  case Type::ExtVector:
                  {
                      const ExtVectorType *tp = cast<ExtVectorType>(T);
                      QualType eT = tp->getElementType();
                      LookForTemplateTypeParameters(eT,s);
                  }
                  case Type::Vector:
                  {
                      const VectorType *tp = cast<VectorType>(T);
                      QualType eT = tp->getElementType();
                      LookForTemplateTypeParameters(eT,s);
                  }
                  break;
                  case Type::Complex:
                  {
                      const ComplexType *tp = cast<ComplexType>(T);
                      QualType eT = tp->getElementType();
                      LookForTemplateTypeParameters(eT,s);
                  }
                  break;
                  case Type::TypeOf:
                  {
                      const TypeOfType *tp = cast<TypeOfType>(T);
                      QualType toT = tp->getUnmodifiedType();
                      LookForTemplateTypeParameters(toT,s);
                  }
                  break;
                  case Type::Atomic:
                  {
                      const AtomicType* tp = cast<AtomicType>(T);
                      QualType aT = tp->getValueType();
                      LookForTemplateTypeParameters(aT,s);
                  }
                  break;
                  case Type::MacroQualified:
                   {
                       const MacroQualifiedType* tp = cast<MacroQualifiedType>(T);
                       QualType uT = tp->getUnderlyingType();
                       LookForTemplateTypeParameters(uT,s);
                   }
                   break;
  		  default:
  		  {
  			  llvm::outs() << "UNSUPPORTED: " << T->getTypeClassName() << "\n";
  		  }
  	}
  }

QualType DbJSONClassVisitor::lookForNonPointerType(const PointerType* tp) {

	QualType T = tp->getPointeeType();
	if (T->getTypeClass()==Type::Pointer) {
		const PointerType *ntp = cast<PointerType>(T);
		return lookForNonPointerType(ntp);
	}
	return T;
}

const FunctionProtoType* DbJSONClassVisitor::lookForFunctionType(QualType T) {

	  switch(T->getTypeClass()) {
	  	  case Type::Pointer:
		{
			const PointerType *tp = cast<PointerType>(T);
			QualType ptrT = tp->getPointeeType();
			return lookForFunctionType(ptrT);
		}
  		  case Type::Decayed:
		  {
			  const DecayedType *tp = cast<DecayedType>(T);
			  QualType dT = tp->getPointeeType();
			  return lookForFunctionType(dT);
		  }
	  	  case Type::Elaborated:
		  {
			  const ElaboratedType *tp = cast<ElaboratedType>(T);
			  QualType eT = tp->getNamedType();
			  return lookForFunctionType(eT);
		  }
	  	  case Type::Paren:
		  {
			  const ParenType *tp = cast<ParenType>(T);
			  QualType iT = tp->getInnerType();
			  return lookForFunctionType(iT);
		  }
		  case Type::Typedef:
		  {
			  const TypedefType *tp = cast<TypedefType>(T);
			  TypedefNameDecl* D = tp->getDecl();
			  QualType tT = D->getTypeSourceInfo()->getType();
			  return lookForFunctionType(tT);
		  }
		  case Type::ConstantArray:
		  {
			  const ConstantArrayType *tp = cast<ConstantArrayType>(T);
			  QualType elT = tp->getElementType();
			  return lookForFunctionType(elT);
		  }
		  case Type::IncompleteArray:
		  {
			  const IncompleteArrayType *tp = cast<IncompleteArrayType>(T);
			  QualType elT = tp->getElementType();
			  return lookForFunctionType(elT);
		  }
		  case Type::TypeOfExpr:
		  {
			  const TypeOfExprType *tp = cast<TypeOfExprType>(T);
			  QualType toT = tp->getUnderlyingExpr()->getType();
			  return lookForFunctionType(toT);
		  }
		  case Type::TypeOf:
		  {
			  const TypeOfType *tp = cast<TypeOfType>(T);
			  QualType toT = tp->getUnmodifiedType();
			  return lookForFunctionType(toT);
		  }
		  case Type::FunctionProto:
		  {
			  const FunctionProtoType *tp = cast<FunctionProtoType>(T);
			  return tp;
		  }
	  }
	  return 0;
}
