//===--- DeclPrinter.cpp - Printing implementation for Decl ASTs ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the Decl::print method, which pretty prints the
// AST back out to C/Objective-C/C++/Objective-C++ code.
//
// Modified by [2022] Samsung Electronics, co. Ltd.
//===----------------------------------------------------------------------===//
#include "DeclPrinter.h"
#include "StmtPrinter.h"
#include "TypePrinter.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/DeclVisitor.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/Basic/Module.h"
#include "llvm/Support/raw_ostream.h"
using namespace clang;

static int in_decl_group;
static int group_unused;
int csd = 0;

void Decl::print(raw_ostream &Out, unsigned Indentation,
                 bool PrintInstantiation) const {
  print(Out, getASTContext().getPrintingPolicy(), Indentation, PrintInstantiation);
}

void Decl::print(raw_ostream &Out, const PrintingPolicy &Policy,
                 unsigned Indentation, bool PrintInstantiation) const {
  DeclPrinter Printer(Out, Policy, getASTContext(), Indentation,
                      PrintInstantiation);
  Printer.Visit(const_cast<Decl*>(this));
}

void TemplateParameterList::print(raw_ostream &Out, const ASTContext &Context,
                                  bool OmitTemplateKW) const {
  print(Out, Context, Context.getPrintingPolicy(), OmitTemplateKW);
}

void TemplateParameterList::print(raw_ostream &Out, const ASTContext &Context,
                                  const PrintingPolicy &Policy,
                                  bool OmitTemplateKW) const {
  DeclPrinter Printer(Out, Policy, Context);
  Printer.printTemplateParameters(this, OmitTemplateKW);
}

static QualType GetBaseType(QualType T) {
  // FIXME: This should be on the Type class!
  QualType BaseType = T;
  while (!BaseType->isSpecifierType()) {
    if (const PointerType *PTy = BaseType->getAs<PointerType>())
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
    else if (const ParenType *PTy = BaseType->getAs<ParenType>())
      BaseType = PTy->desugar();
    else
      // This must be a syntax error.
      break;
  }
  return BaseType;
}

static QualType getDeclType(Decl* D) {
  if (TypedefNameDecl* TDD = dyn_cast<TypedefNameDecl>(D))
    return TDD->getUnderlyingType();
  if (ValueDecl* VD = dyn_cast<ValueDecl>(D))
    return VD->getType();
  return QualType();
}

void DeclPrinter::printDeclGroupCTA(Decl** Begin, unsigned NumDecls,
                      raw_ostream &Out, const PrintingPolicy &Policy,
                      unsigned Indentation, const ASTContext* Context, std::set<const FunctionDecl*>* CTAList) {
  if (NumDecls == 1) {
	DeclPrinter Printer(Out, Policy, *Context, Indentation, false, CTAList);
	Printer.Visit(*Begin);
    return;
  }

  Decl** End = Begin + NumDecls;
  TagDecl* TD = dyn_cast<TagDecl>(*Begin);
  if (TD)
    ++Begin;

  PrintingPolicy SubPolicy(Policy);

  bool isFirst = true;
  for ( ; Begin != End; ++Begin) {
    if (isFirst) {
      if(TD)
        SubPolicy.IncludeTagDefinition = true;
      SubPolicy.SuppressSpecifiers = false;
      isFirst = false;
    } else {
      if (!isFirst) Out << ", ";
      SubPolicy.IncludeTagDefinition = false;
      SubPolicy.SuppressSpecifiers = true;
    }

    DeclPrinter Printer(Out, SubPolicy, *Context, Indentation, false, CTAList);
    Printer.Visit(*Begin);
  }
}

void Decl::printGroup(Decl** Begin, unsigned NumDecls,
                      raw_ostream &Out, const PrintingPolicy &Policy,
                      unsigned Indentation) {
  if (NumDecls == 1) {
    // ignore not Freestanding
    // (*Begin)->print(Out, Policy, Indentation);
    return;
  }

  Decl** End = Begin + NumDecls;
  TagDecl* TD = dyn_cast<TagDecl>(*Begin);
  if (TD)
    ++Begin;

  PrintingPolicy SubPolicy(Policy);
  
  int group_unused_local = 1;
  for ( auto Bgn = Begin; Bgn != End; ++Bgn) {
    if((*Bgn)->isReferenced()){
      group_unused_local = 0;
      break;
    }
  }

  bool isFirst = true;
  for ( ; Begin != End; ++Begin) {
    in_decl_group = 1;
    group_unused = group_unused_local;
    if (isFirst) {
      if(TD)
        SubPolicy.IncludeTagDefinition = true;
      SubPolicy.SuppressSpecifiers = false;
      isFirst = false;
    } else {
      if (!isFirst) Out << ", ";
      SubPolicy.IncludeTagDefinition = false;
      SubPolicy.SuppressSpecifiers = true;
    }

    (*Begin)->print(Out, SubPolicy, Indentation);
  }
  group_unused = 0;
}

LLVM_DUMP_METHOD void DeclContext::dumpDeclContext() const {
  // Get the translation unit
  const DeclContext *DC = this;
  while (!DC->isTranslationUnit())
    DC = DC->getParent();

  ASTContext &Ctx = cast<TranslationUnitDecl>(DC)->getASTContext();
  DeclPrinter Printer(llvm::errs(), Ctx.getPrintingPolicy(), Ctx, 0);
  Printer.VisitDeclContext(const_cast<DeclContext *>(this), /*Indent=*/false);
}

raw_ostream& DeclPrinter::Indent(unsigned Indentation) {
  for (unsigned i = 0; i != Indentation; ++i)
    Out << "  ";
  return Out;
}

static void printPrettyAlignedAttr(AlignedAttr* A, raw_ostream &OS, const PrintingPolicy &Policy)  {

	switch(A->getSemanticSpelling()) {
		default:
			llvm_unreachable("Unknown attribute spelling!");
			break;
		case AlignedAttr::GNU_aligned: {
			OS << " __attribute__((aligned";
			unsigned TrailingOmittedArgs = 0;
			if (!A->isAlignmentExpr() || !A->getAlignmentExpr())
			  ++TrailingOmittedArgs;
			OS << "";
			if (TrailingOmittedArgs < 1)
			   OS << "(";
			OS << "";
			if (!(!A->isAlignmentExpr() || !A->getAlignmentExpr())) {
			  OS << "";
			StmtPrinter P(OS, nullptr, Policy);
			P.Visit(A->getAlignmentExpr());
			OS << "";
			}
			OS << "";
			if (TrailingOmittedArgs < 1)
			   OS << ")";
			OS << "))";
			break;
		}
		case AlignedAttr::CXX11_gnu_aligned: {
		    OS << " [[gnu::aligned";
		    unsigned TrailingOmittedArgs = 0;
		    if (!A->isAlignmentExpr() || !A->getAlignmentExpr())
		      ++TrailingOmittedArgs;
		    OS << "";
		    if (TrailingOmittedArgs < 1)
		       OS << "(";
		    OS << "";
		    if (!(!A->isAlignmentExpr() || !A->getAlignmentExpr())) {
		      OS << "";
		    StmtPrinter P(OS, nullptr, Policy);
		    P.Visit(A->getAlignmentExpr());
		    OS << "";
		    }
		    OS << "";
		    if (TrailingOmittedArgs < 1)
		       OS << ")";
		    OS << "]]";
		    break;
		}
		case AlignedAttr::Declspec_align: {
		    OS << " __declspec(align";
		    unsigned TrailingOmittedArgs = 0;
		    if (!A->isAlignmentExpr() || !A->getAlignmentExpr())
		      ++TrailingOmittedArgs;
		    OS << "";
		    if (TrailingOmittedArgs < 1)
		       OS << "(";
		    OS << "";
		    if (!(!A->isAlignmentExpr() || !A->getAlignmentExpr())) {
		      OS << "";
			StmtPrinter P(OS, nullptr, Policy);
			P.Visit(A->getAlignmentExpr());
		    OS << "";
		    }
		    OS << "";
		    if (TrailingOmittedArgs < 1)
		       OS << ")";
		    OS << ")";
		    break;
		}
		case AlignedAttr::Keyword_alignas: {
		    OS << " alignas";
		    unsigned TrailingOmittedArgs = 0;
		    if (!A->isAlignmentExpr() || !A->getAlignmentExpr())
		      ++TrailingOmittedArgs;
		    OS << "";
		    if (TrailingOmittedArgs < 1)
		       OS << "(";
		    OS << "";
		    if (!(!A->isAlignmentExpr() || !A->getAlignmentExpr())) {
		      OS << "";
			StmtPrinter P(OS, nullptr, Policy);
			P.Visit(A->getAlignmentExpr());
		    OS << "";
		    }
		    OS << "";
		    if (TrailingOmittedArgs < 1)
		       OS << ")";
		    OS << "";
		    break;
		}
		case AlignedAttr::Keyword_Alignas: {
		    OS << " _Alignas";
		    unsigned TrailingOmittedArgs = 0;
		    if (!A->isAlignmentExpr() || !A->getAlignmentExpr())
		      ++TrailingOmittedArgs;
		    OS << "";
		    if (TrailingOmittedArgs < 1)
		       OS << "(";
		    OS << "";
		    if (!(!A->isAlignmentExpr() || !A->getAlignmentExpr())) {
		      OS << "";
			StmtPrinter P(OS, nullptr, Policy);
			P.Visit(A->getAlignmentExpr());
		    OS << "";
		    }
		    OS << "";
		    if (TrailingOmittedArgs < 1)
		       OS << ")";
		    OS << "";
		    break;
		}
	}
}

static std::string quote_escape( const std::string &str ) {
        std::string output;
    for( unsigned i = 0; i < str.length(); ++i ) {
        if (str[i]=='\"') {
                output += "\\\"";
        }
        else {
                output += str[i];
        }
    }
    return output;
}


void printPrettySectionAttr(SectionAttr* A, raw_ostream &OS, const PrintingPolicy &Policy) {
  switch (A->getSemanticSpelling()) {
  default:
    llvm_unreachable("Unknown attribute spelling!");
    break;
  case 0 : {
    OS << " __attribute__((section(\"" << quote_escape(A->getName().str()) << "\")))";
    break;
  }
  case 1 : {
    OS << " [[gnu::section(\"" << quote_escape(A->getName().str()) << "\")]]";
    break;
  }
  case 2 : {
    OS << " __declspec(allocate(\"" << quote_escape(A->getName().str()) << "\"))";
    break;
  }
}
}

static void printPrettyAttr(Attr* A, raw_ostream &OS, const PrintingPolicy &Policy) {
  switch (A->getKind()) {
  case attr::AArch64VectorPcs:
    return cast<AArch64VectorPcsAttr>(A)->printPretty(OS, Policy);
  case attr::AMDGPUFlatWorkGroupSize:
    return cast<AMDGPUFlatWorkGroupSizeAttr>(A)->printPretty(OS, Policy);
  case attr::AMDGPUNumSGPR:
    return cast<AMDGPUNumSGPRAttr>(A)->printPretty(OS, Policy);
  case attr::AMDGPUNumVGPR:
    return cast<AMDGPUNumVGPRAttr>(A)->printPretty(OS, Policy);
  case attr::AMDGPUWavesPerEU:
    return cast<AMDGPUWavesPerEUAttr>(A)->printPretty(OS, Policy);
  case attr::ARMInterrupt:
    return cast<ARMInterruptAttr>(A)->printPretty(OS, Policy);
  case attr::AVRInterrupt:
    return cast<AVRInterruptAttr>(A)->printPretty(OS, Policy);
  case attr::AVRSignal:
    return cast<AVRSignalAttr>(A)->printPretty(OS, Policy);
  case attr::AbiTag:
    return cast<AbiTagAttr>(A)->printPretty(OS, Policy);
  case attr::AcquireCapability:
    return cast<AcquireCapabilityAttr>(A)->printPretty(OS, Policy);
  case attr::AcquireHandle:
    return cast<AcquireHandleAttr>(A)->printPretty(OS, Policy);
  case attr::AcquiredAfter:
    return cast<AcquiredAfterAttr>(A)->printPretty(OS, Policy);
  case attr::AcquiredBefore:
    return cast<AcquiredBeforeAttr>(A)->printPretty(OS, Policy);
  case attr::AddressSpace:
    return cast<AddressSpaceAttr>(A)->printPretty(OS, Policy);
  case attr::Alias:
    return cast<AliasAttr>(A)->printPretty(OS, Policy);
  case attr::AlignMac68k:
    return cast<AlignMac68kAttr>(A)->printPretty(OS, Policy);
  case attr::AlignValue:
    return cast<AlignValueAttr>(A)->printPretty(OS, Policy);
  case attr::Aligned:
	return printPrettyAlignedAttr(cast<AlignedAttr>(A),OS,Policy);
  case attr::AllocAlign:
    return cast<AllocAlignAttr>(A)->printPretty(OS, Policy);
  case attr::AllocSize:
    return cast<AllocSizeAttr>(A)->printPretty(OS, Policy);
  case attr::AlwaysDestroy:
    return cast<AlwaysDestroyAttr>(A)->printPretty(OS, Policy);
  case attr::AlwaysInline:
    return cast<AlwaysInlineAttr>(A)->printPretty(OS, Policy);
  case attr::AnalyzerNoReturn:
    return cast<AnalyzerNoReturnAttr>(A)->printPretty(OS, Policy);
  case attr::Annotate:
    return cast<AnnotateAttr>(A)->printPretty(OS, Policy);
  case attr::AnyX86Interrupt:
    return cast<AnyX86InterruptAttr>(A)->printPretty(OS, Policy);
  case attr::AnyX86NoCallerSavedRegisters:
    return cast<AnyX86NoCallerSavedRegistersAttr>(A)->printPretty(OS, Policy);
  case attr::AnyX86NoCfCheck:
    return cast<AnyX86NoCfCheckAttr>(A)->printPretty(OS, Policy);
  case attr::ArcWeakrefUnavailable:
    return cast<ArcWeakrefUnavailableAttr>(A)->printPretty(OS, Policy);
  case attr::ArgumentWithTypeTag:
    return cast<ArgumentWithTypeTagAttr>(A)->printPretty(OS, Policy);
  case attr::ArmMveAlias:
    return cast<ArmMveAliasAttr>(A)->printPretty(OS, Policy);
  case attr::Artificial:
    return cast<ArtificialAttr>(A)->printPretty(OS, Policy);
  case attr::AsmLabel:
    return cast<AsmLabelAttr>(A)->printPretty(OS, Policy);
  case attr::AssertCapability:
    return cast<AssertCapabilityAttr>(A)->printPretty(OS, Policy);
  case attr::AssertExclusiveLock:
    return cast<AssertExclusiveLockAttr>(A)->printPretty(OS, Policy);
  case attr::AssertSharedLock:
    return cast<AssertSharedLockAttr>(A)->printPretty(OS, Policy);
  case attr::AssumeAligned:
    return cast<AssumeAlignedAttr>(A)->printPretty(OS, Policy);
  case attr::Availability:
    return cast<AvailabilityAttr>(A)->printPretty(OS, Policy);
  case attr::BPFPreserveAccessIndex:
    return cast<BPFPreserveAccessIndexAttr>(A)->printPretty(OS, Policy);
  case attr::Blocks:
    return cast<BlocksAttr>(A)->printPretty(OS, Policy);
  case attr::C11NoReturn:
    return cast<C11NoReturnAttr>(A)->printPretty(OS, Policy);
  case attr::CDecl:
    return cast<CDeclAttr>(A)->printPretty(OS, Policy);
  case attr::CFAuditedTransfer:
    return cast<CFAuditedTransferAttr>(A)->printPretty(OS, Policy);
  case attr::CFConsumed:
    return cast<CFConsumedAttr>(A)->printPretty(OS, Policy);
  case attr::CFGuard:
    return cast<CFGuardAttr>(A)->printPretty(OS, Policy);
  case attr::CFICanonicalJumpTable:
    return cast<CFICanonicalJumpTableAttr>(A)->printPretty(OS, Policy);
  case attr::CFReturnsNotRetained:
    return cast<CFReturnsNotRetainedAttr>(A)->printPretty(OS, Policy);
  case attr::CFReturnsRetained:
    return cast<CFReturnsRetainedAttr>(A)->printPretty(OS, Policy);
  case attr::CFUnknownTransfer:
    return cast<CFUnknownTransferAttr>(A)->printPretty(OS, Policy);
  case attr::CPUDispatch:
    return cast<CPUDispatchAttr>(A)->printPretty(OS, Policy);
  case attr::CPUSpecific:
    return cast<CPUSpecificAttr>(A)->printPretty(OS, Policy);
  case attr::CUDAConstant:
    return cast<CUDAConstantAttr>(A)->printPretty(OS, Policy);
  case attr::CUDADevice:
    return cast<CUDADeviceAttr>(A)->printPretty(OS, Policy);
  case attr::CUDAGlobal:
    return cast<CUDAGlobalAttr>(A)->printPretty(OS, Policy);
  case attr::CUDAHost:
    return cast<CUDAHostAttr>(A)->printPretty(OS, Policy);
  case attr::CUDAInvalidTarget:
    return cast<CUDAInvalidTargetAttr>(A)->printPretty(OS, Policy);
  case attr::CUDALaunchBounds:
    return cast<CUDALaunchBoundsAttr>(A)->printPretty(OS, Policy);
  case attr::CUDAShared:
    return cast<CUDASharedAttr>(A)->printPretty(OS, Policy);
  case attr::CXX11NoReturn:
    return cast<CXX11NoReturnAttr>(A)->printPretty(OS, Policy);
  case attr::CallableWhen:
    return cast<CallableWhenAttr>(A)->printPretty(OS, Policy);
  case attr::Callback:
    return cast<CallbackAttr>(A)->printPretty(OS, Policy);
  case attr::Capability:
    return cast<CapabilityAttr>(A)->printPretty(OS, Policy);
  case attr::CapturedRecord:
    return cast<CapturedRecordAttr>(A)->printPretty(OS, Policy);
  case attr::CarriesDependency:
    return cast<CarriesDependencyAttr>(A)->printPretty(OS, Policy);
  case attr::Cleanup:
    return cast<CleanupAttr>(A)->printPretty(OS, Policy);
  case attr::CodeSeg:
    return cast<CodeSegAttr>(A)->printPretty(OS, Policy);
  case attr::Cold:
    return cast<ColdAttr>(A)->printPretty(OS, Policy);
  case attr::Common:
    return cast<CommonAttr>(A)->printPretty(OS, Policy);
  case attr::Const:
    return cast<ConstAttr>(A)->printPretty(OS, Policy);
  case attr::ConstInit:
    return cast<ConstInitAttr>(A)->printPretty(OS, Policy);
  case attr::Constructor:
    return cast<ConstructorAttr>(A)->printPretty(OS, Policy);
  case attr::Consumable:
    return cast<ConsumableAttr>(A)->printPretty(OS, Policy);
  case attr::ConsumableAutoCast:
    return cast<ConsumableAutoCastAttr>(A)->printPretty(OS, Policy);
  case attr::ConsumableSetOnRead:
    return cast<ConsumableSetOnReadAttr>(A)->printPretty(OS, Policy);
  case attr::Convergent:
    return cast<ConvergentAttr>(A)->printPretty(OS, Policy);
  case attr::DLLExport:
    return cast<DLLExportAttr>(A)->printPretty(OS, Policy);
  case attr::DLLExportStaticLocal:
    return cast<DLLExportStaticLocalAttr>(A)->printPretty(OS, Policy);
  case attr::DLLImport:
    return cast<DLLImportAttr>(A)->printPretty(OS, Policy);
  case attr::DLLImportStaticLocal:
    return cast<DLLImportStaticLocalAttr>(A)->printPretty(OS, Policy);
  case attr::Deprecated:
    return cast<DeprecatedAttr>(A)->printPretty(OS, Policy);
  case attr::Destructor:
    return cast<DestructorAttr>(A)->printPretty(OS, Policy);
  case attr::DiagnoseIf:
    return cast<DiagnoseIfAttr>(A)->printPretty(OS, Policy);
  case attr::DisableTailCalls:
    return cast<DisableTailCallsAttr>(A)->printPretty(OS, Policy);
  case attr::EmptyBases:
    return cast<EmptyBasesAttr>(A)->printPretty(OS, Policy);
  case attr::EnableIf:
    return cast<EnableIfAttr>(A)->printPretty(OS, Policy);
  case attr::EnumExtensibility:
    return cast<EnumExtensibilityAttr>(A)->printPretty(OS, Policy);
  case attr::ExcludeFromExplicitInstantiation:
    return cast<ExcludeFromExplicitInstantiationAttr>(A)->printPretty(OS, Policy);
  case attr::ExclusiveTrylockFunction:
    return cast<ExclusiveTrylockFunctionAttr>(A)->printPretty(OS, Policy);
  case attr::ExternalSourceSymbol:
    return cast<ExternalSourceSymbolAttr>(A)->printPretty(OS, Policy);
  case attr::FallThrough:
    return cast<FallThroughAttr>(A)->printPretty(OS, Policy);
  case attr::FastCall:
    return cast<FastCallAttr>(A)->printPretty(OS, Policy);
  case attr::Final:
    return cast<FinalAttr>(A)->printPretty(OS, Policy);
  case attr::FlagEnum:
    return cast<FlagEnumAttr>(A)->printPretty(OS, Policy);
  case attr::Flatten:
    return cast<FlattenAttr>(A)->printPretty(OS, Policy);
  case attr::Format:
    return cast<FormatAttr>(A)->printPretty(OS, Policy);
  case attr::FormatArg:
    return cast<FormatArgAttr>(A)->printPretty(OS, Policy);
  case attr::GNUInline:
    return cast<GNUInlineAttr>(A)->printPretty(OS, Policy);
  case attr::GuardedBy:
    return cast<GuardedByAttr>(A)->printPretty(OS, Policy);
  case attr::GuardedVar:
    return cast<GuardedVarAttr>(A)->printPretty(OS, Policy);
  case attr::HIPPinnedShadow:
    return cast<HIPPinnedShadowAttr>(A)->printPretty(OS, Policy);
  case attr::Hot:
    return cast<HotAttr>(A)->printPretty(OS, Policy);
  case attr::IBAction:
    return cast<IBActionAttr>(A)->printPretty(OS, Policy);
  case attr::IBOutlet:
    return cast<IBOutletAttr>(A)->printPretty(OS, Policy);
  case attr::IBOutletCollection:
    return cast<IBOutletCollectionAttr>(A)->printPretty(OS, Policy);
  case attr::IFunc:
    return cast<IFuncAttr>(A)->printPretty(OS, Policy);
  case attr::InitPriority:
    return cast<InitPriorityAttr>(A)->printPretty(OS, Policy);
  case attr::InitSeg:
    return cast<InitSegAttr>(A)->printPretty(OS, Policy);
  case attr::IntelOclBicc:
    return cast<IntelOclBiccAttr>(A)->printPretty(OS, Policy);
  case attr::InternalLinkage:
    return cast<InternalLinkageAttr>(A)->printPretty(OS, Policy);
  case attr::LTOVisibilityPublic:
    return cast<LTOVisibilityPublicAttr>(A)->printPretty(OS, Policy);
  case attr::LayoutVersion:
    return cast<LayoutVersionAttr>(A)->printPretty(OS, Policy);
  case attr::LifetimeBound:
    return cast<LifetimeBoundAttr>(A)->printPretty(OS, Policy);
  case attr::LockReturned:
    return cast<LockReturnedAttr>(A)->printPretty(OS, Policy);
  case attr::LocksExcluded:
    return cast<LocksExcludedAttr>(A)->printPretty(OS, Policy);
  case attr::LoopHint:
    return cast<LoopHintAttr>(A)->printPretty(OS, Policy);
  case attr::MIGServerRoutine:
    return cast<MIGServerRoutineAttr>(A)->printPretty(OS, Policy);
  case attr::MSABI:
    return cast<MSABIAttr>(A)->printPretty(OS, Policy);
  case attr::MSAllocator:
    return cast<MSAllocatorAttr>(A)->printPretty(OS, Policy);
  case attr::MSInheritance:
    return cast<MSInheritanceAttr>(A)->printPretty(OS, Policy);
  case attr::MSNoVTable:
    return cast<MSNoVTableAttr>(A)->printPretty(OS, Policy);
  case attr::MSP430Interrupt:
    return cast<MSP430InterruptAttr>(A)->printPretty(OS, Policy);
  case attr::MSStruct:
    return cast<MSStructAttr>(A)->printPretty(OS, Policy);
  case attr::MSVtorDisp:
    return cast<MSVtorDispAttr>(A)->printPretty(OS, Policy);
  case attr::MaxFieldAlignment:
    return cast<MaxFieldAlignmentAttr>(A)->printPretty(OS, Policy);
  case attr::MayAlias:
    return cast<MayAliasAttr>(A)->printPretty(OS, Policy);
  case attr::MicroMips:
    return cast<MicroMipsAttr>(A)->printPretty(OS, Policy);
  case attr::MinSize:
    return cast<MinSizeAttr>(A)->printPretty(OS, Policy);
  case attr::MinVectorWidth:
    return cast<MinVectorWidthAttr>(A)->printPretty(OS, Policy);
  case attr::Mips16:
    return cast<Mips16Attr>(A)->printPretty(OS, Policy);
  case attr::MipsInterrupt:
    return cast<MipsInterruptAttr>(A)->printPretty(OS, Policy);
  case attr::MipsLongCall:
    return cast<MipsLongCallAttr>(A)->printPretty(OS, Policy);
  case attr::MipsShortCall:
    return cast<MipsShortCallAttr>(A)->printPretty(OS, Policy);
  case attr::Mode:
    return cast<ModeAttr>(A)->printPretty(OS, Policy);
  case attr::NSConsumed:
    return cast<NSConsumedAttr>(A)->printPretty(OS, Policy);
  case attr::NSConsumesSelf:
    return cast<NSConsumesSelfAttr>(A)->printPretty(OS, Policy);
  case attr::NSReturnsAutoreleased:
    return cast<NSReturnsAutoreleasedAttr>(A)->printPretty(OS, Policy);
  case attr::NSReturnsNotRetained:
    return cast<NSReturnsNotRetainedAttr>(A)->printPretty(OS, Policy);
  case attr::NSReturnsRetained:
    return cast<NSReturnsRetainedAttr>(A)->printPretty(OS, Policy);
  case attr::Naked:
    return cast<NakedAttr>(A)->printPretty(OS, Policy);
  case attr::NoAlias:
    return cast<NoAliasAttr>(A)->printPretty(OS, Policy);
  case attr::NoBuiltin:
    return cast<NoBuiltinAttr>(A)->printPretty(OS, Policy);
  case attr::NoCommon:
    return cast<NoCommonAttr>(A)->printPretty(OS, Policy);
  case attr::NoDebug:
    return cast<NoDebugAttr>(A)->printPretty(OS, Policy);
  case attr::NoDeref:
    return cast<NoDerefAttr>(A)->printPretty(OS, Policy);
  case attr::NoDestroy:
    return cast<NoDestroyAttr>(A)->printPretty(OS, Policy);
  case attr::NoDuplicate:
    return cast<NoDuplicateAttr>(A)->printPretty(OS, Policy);
  case attr::NoEscape:
    return cast<NoEscapeAttr>(A)->printPretty(OS, Policy);
  case attr::NoInline:
    return cast<NoInlineAttr>(A)->printPretty(OS, Policy);
  case attr::NoInstrumentFunction:
    return cast<NoInstrumentFunctionAttr>(A)->printPretty(OS, Policy);
  case attr::NoMicroMips:
    return cast<NoMicroMipsAttr>(A)->printPretty(OS, Policy);
  case attr::NoMips16:
    return cast<NoMips16Attr>(A)->printPretty(OS, Policy);
  case attr::NoReturn:
    return cast<NoReturnAttr>(A)->printPretty(OS, Policy);
  case attr::NoSanitize:
    return cast<NoSanitizeAttr>(A)->printPretty(OS, Policy);
  case attr::NoSpeculativeLoadHardening:
    return cast<NoSpeculativeLoadHardeningAttr>(A)->printPretty(OS, Policy);
  case attr::NoSplitStack:
    return cast<NoSplitStackAttr>(A)->printPretty(OS, Policy);
  case attr::NoStackProtector:
    return cast<NoStackProtectorAttr>(A)->printPretty(OS, Policy);
  case attr::NoThreadSafetyAnalysis:
    return cast<NoThreadSafetyAnalysisAttr>(A)->printPretty(OS, Policy);
  case attr::NoThrow:
    return cast<NoThrowAttr>(A)->printPretty(OS, Policy);
  case attr::NoUniqueAddress:
    return cast<NoUniqueAddressAttr>(A)->printPretty(OS, Policy);
  case attr::NonNull:
    return cast<NonNullAttr>(A)->printPretty(OS, Policy);
  case attr::NotTailCalled:
    return cast<NotTailCalledAttr>(A)->printPretty(OS, Policy);
  case attr::OMPAllocateDecl:
    return cast<OMPAllocateDeclAttr>(A)->printPretty(OS, Policy);
  case attr::OMPCaptureKind:
    return cast<OMPCaptureKindAttr>(A)->printPretty(OS, Policy);
  case attr::OMPCaptureNoInit:
    return cast<OMPCaptureNoInitAttr>(A)->printPretty(OS, Policy);
  case attr::OMPDeclareSimdDecl:
    return cast<OMPDeclareSimdDeclAttr>(A)->printPretty(OS, Policy);
  case attr::OMPDeclareTargetDecl:
    return cast<OMPDeclareTargetDeclAttr>(A)->printPretty(OS, Policy);
  case attr::OMPDeclareVariant:
    return cast<OMPDeclareVariantAttr>(A)->printPretty(OS, Policy);
  case attr::OMPReferencedVar:
    return cast<OMPReferencedVarAttr>(A)->printPretty(OS, Policy);
  case attr::OMPThreadPrivateDecl:
    return cast<OMPThreadPrivateDeclAttr>(A)->printPretty(OS, Policy);
  case attr::OSConsumed:
    return cast<OSConsumedAttr>(A)->printPretty(OS, Policy);
  case attr::OSConsumesThis:
    return cast<OSConsumesThisAttr>(A)->printPretty(OS, Policy);
  case attr::OSReturnsNotRetained:
    return cast<OSReturnsNotRetainedAttr>(A)->printPretty(OS, Policy);
  case attr::OSReturnsRetained:
    return cast<OSReturnsRetainedAttr>(A)->printPretty(OS, Policy);
  case attr::OSReturnsRetainedOnNonZero:
    return cast<OSReturnsRetainedOnNonZeroAttr>(A)->printPretty(OS, Policy);
  case attr::OSReturnsRetainedOnZero:
    return cast<OSReturnsRetainedOnZeroAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCBoxable:
    return cast<ObjCBoxableAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCBridge:
    return cast<ObjCBridgeAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCBridgeMutable:
    return cast<ObjCBridgeMutableAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCBridgeRelated:
    return cast<ObjCBridgeRelatedAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCClassStub:
    return cast<ObjCClassStubAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCDesignatedInitializer:
    return cast<ObjCDesignatedInitializerAttr>(A)->printPretty(OS, Policy);
case attr::ObjCDirect:
    return cast<ObjCDirectAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCDirectMembers:
    return cast<ObjCDirectMembersAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCException:
    return cast<ObjCExceptionAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCExplicitProtocolImpl:
    return cast<ObjCExplicitProtocolImplAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCExternallyRetained:
    return cast<ObjCExternallyRetainedAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCGC:
    return cast<ObjCGCAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCIndependentClass:
    return cast<ObjCIndependentClassAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCInertUnsafeUnretained:
    return cast<ObjCInertUnsafeUnretainedAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCKindOf:
    return cast<ObjCKindOfAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCMethodFamily:
    return cast<ObjCMethodFamilyAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCNSObject:
    return cast<ObjCNSObjectAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCNonLazyClass:
    return cast<ObjCNonLazyClassAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCOwnership:
    return cast<ObjCOwnershipAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCPreciseLifetime:
    return cast<ObjCPreciseLifetimeAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCRequiresPropertyDefs:
    return cast<ObjCRequiresPropertyDefsAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCRequiresSuper:
    return cast<ObjCRequiresSuperAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCReturnsInnerPointer:
    return cast<ObjCReturnsInnerPointerAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCRootClass:
    return cast<ObjCRootClassAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCRuntimeName:
    return cast<ObjCRuntimeNameAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCRuntimeVisible:
    return cast<ObjCRuntimeVisibleAttr>(A)->printPretty(OS, Policy);
  case attr::ObjCSubclassingRestricted:
    return cast<ObjCSubclassingRestrictedAttr>(A)->printPretty(OS, Policy);
  case attr::OpenCLAccess:
    return cast<OpenCLAccessAttr>(A)->printPretty(OS, Policy);
  case attr::OpenCLConstantAddressSpace:
    return cast<OpenCLConstantAddressSpaceAttr>(A)->printPretty(OS, Policy);
  case attr::OpenCLGenericAddressSpace:
    return cast<OpenCLGenericAddressSpaceAttr>(A)->printPretty(OS, Policy);
  case attr::OpenCLGlobalAddressSpace:
    return cast<OpenCLGlobalAddressSpaceAttr>(A)->printPretty(OS, Policy);
  case attr::OpenCLIntelReqdSubGroupSize:
    return cast<OpenCLIntelReqdSubGroupSizeAttr>(A)->printPretty(OS, Policy);
  case attr::OpenCLKernel:
    return cast<OpenCLKernelAttr>(A)->printPretty(OS, Policy);
  case attr::OpenCLLocalAddressSpace:
    return cast<OpenCLLocalAddressSpaceAttr>(A)->printPretty(OS, Policy);
  case attr::OpenCLPrivateAddressSpace:
    return cast<OpenCLPrivateAddressSpaceAttr>(A)->printPretty(OS, Policy);
  case attr::OpenCLUnrollHint:
    return cast<OpenCLUnrollHintAttr>(A)->printPretty(OS, Policy);
  case attr::OptimizeNone:
    return cast<OptimizeNoneAttr>(A)->printPretty(OS, Policy);
  case attr::Overloadable:
    return cast<OverloadableAttr>(A)->printPretty(OS, Policy);
  case attr::Override:
    return cast<OverrideAttr>(A)->printPretty(OS, Policy);
 case attr::Owner:
    return cast<OwnerAttr>(A)->printPretty(OS, Policy);
  case attr::Ownership:
    return cast<OwnershipAttr>(A)->printPretty(OS, Policy);
  case attr::Packed:
    return cast<PackedAttr>(A)->printPretty(OS, Policy);
  case attr::ParamTypestate:
    return cast<ParamTypestateAttr>(A)->printPretty(OS, Policy);
  case attr::Pascal:
    return cast<PascalAttr>(A)->printPretty(OS, Policy);
  case attr::PassObjectSize:
    return cast<PassObjectSizeAttr>(A)->printPretty(OS, Policy);
  case attr::PatchableFunctionEntry:
    return cast<PatchableFunctionEntryAttr>(A)->printPretty(OS, Policy);
  case attr::Pcs:
    return cast<PcsAttr>(A)->printPretty(OS, Policy);
 case attr::Pointer:
    return cast<PointerAttr>(A)->printPretty(OS, Policy);
  case attr::PragmaClangBSSSection:
    return cast<PragmaClangBSSSectionAttr>(A)->printPretty(OS, Policy);
  case attr::PragmaClangDataSection:
    return cast<PragmaClangDataSectionAttr>(A)->printPretty(OS, Policy);
 case attr::PragmaClangRelroSection:
    return cast<PragmaClangRelroSectionAttr>(A)->printPretty(OS, Policy);
  case attr::PragmaClangRodataSection:
    return cast<PragmaClangRodataSectionAttr>(A)->printPretty(OS, Policy);
  case attr::PragmaClangTextSection:
    return cast<PragmaClangTextSectionAttr>(A)->printPretty(OS, Policy);
  case attr::PreserveAll:
    return cast<PreserveAllAttr>(A)->printPretty(OS, Policy);
  case attr::PreserveMost:
    return cast<PreserveMostAttr>(A)->printPretty(OS, Policy);
  case attr::PtGuardedBy:
    return cast<PtGuardedByAttr>(A)->printPretty(OS, Policy);
  case attr::PtGuardedVar:
    return cast<PtGuardedVarAttr>(A)->printPretty(OS, Policy);
  case attr::Ptr32:
    return cast<Ptr32Attr>(A)->printPretty(OS, Policy);
  case attr::Ptr64:
    return cast<Ptr64Attr>(A)->printPretty(OS, Policy);
  case attr::Pure:
    return cast<PureAttr>(A)->printPretty(OS, Policy);
  case attr::RISCVInterrupt:
    return cast<RISCVInterruptAttr>(A)->printPretty(OS, Policy);
  case attr::RegCall:
    return cast<RegCallAttr>(A)->printPretty(OS, Policy);
  case attr::Reinitializes:
    return cast<ReinitializesAttr>(A)->printPretty(OS, Policy);
  case attr::ReleaseCapability:
    return cast<ReleaseCapabilityAttr>(A)->printPretty(OS, Policy);
  case attr::ReleaseHandle:
    return cast<ReleaseHandleAttr>(A)->printPretty(OS, Policy);
  case attr::RenderScriptKernel:
    return cast<RenderScriptKernelAttr>(A)->printPretty(OS, Policy);
  case attr::ReqdWorkGroupSize:
    return cast<ReqdWorkGroupSizeAttr>(A)->printPretty(OS, Policy);
  case attr::RequiresCapability:
    return cast<RequiresCapabilityAttr>(A)->printPretty(OS, Policy);
  case attr::Restrict:
    return cast<RestrictAttr>(A)->printPretty(OS, Policy);
  case attr::ReturnTypestate:
    return cast<ReturnTypestateAttr>(A)->printPretty(OS, Policy);
  case attr::ReturnsNonNull:
    return cast<ReturnsNonNullAttr>(A)->printPretty(OS, Policy);
  case attr::ReturnsTwice:
    return cast<ReturnsTwiceAttr>(A)->printPretty(OS, Policy);
  case attr::SPtr:
    return cast<SPtrAttr>(A)->printPretty(OS, Policy);
  case attr::SYCLKernel:
    return cast<SYCLKernelAttr>(A)->printPretty(OS, Policy);
  case attr::ScopedLockable:
    return cast<ScopedLockableAttr>(A)->printPretty(OS, Policy);
  case attr::Section:
	return printPrettySectionAttr(cast<SectionAttr>(A),OS,Policy);
  case attr::SelectAny:
    return cast<SelectAnyAttr>(A)->printPretty(OS, Policy);
  case attr::Sentinel:
    return cast<SentinelAttr>(A)->printPretty(OS, Policy);
  case attr::SetTypestate:
    return cast<SetTypestateAttr>(A)->printPretty(OS, Policy);
  case attr::SharedTrylockFunction:
    return cast<SharedTrylockFunctionAttr>(A)->printPretty(OS, Policy);
  case attr::SpeculativeLoadHardening:
    return cast<SpeculativeLoadHardeningAttr>(A)->printPretty(OS, Policy);
  case attr::StdCall:
    return cast<StdCallAttr>(A)->printPretty(OS, Policy);
  case attr::Suppress:
    return cast<SuppressAttr>(A)->printPretty(OS, Policy);
  case attr::SwiftCall:
    return cast<SwiftCallAttr>(A)->printPretty(OS, Policy);
  case attr::SwiftContext:
    return cast<SwiftContextAttr>(A)->printPretty(OS, Policy);
  case attr::SwiftErrorResult:
    return cast<SwiftErrorResultAttr>(A)->printPretty(OS, Policy);
  case attr::SwiftIndirectResult:
    return cast<SwiftIndirectResultAttr>(A)->printPretty(OS, Policy);
  case attr::SysVABI:
    return cast<SysVABIAttr>(A)->printPretty(OS, Policy);
  case attr::TLSModel:
    return cast<TLSModelAttr>(A)->printPretty(OS, Policy);
  case attr::Target:
    return cast<TargetAttr>(A)->printPretty(OS, Policy);
  case attr::TestTypestate:
    return cast<TestTypestateAttr>(A)->printPretty(OS, Policy);
  case attr::ThisCall:
    return cast<ThisCallAttr>(A)->printPretty(OS, Policy);
  case attr::Thread:
    return cast<ThreadAttr>(A)->printPretty(OS, Policy);
  case attr::TransparentUnion:
    return cast<TransparentUnionAttr>(A)->printPretty(OS, Policy);
  case attr::TrivialABI:
    return cast<TrivialABIAttr>(A)->printPretty(OS, Policy);
  case attr::TryAcquireCapability:
    return cast<TryAcquireCapabilityAttr>(A)->printPretty(OS, Policy);
  case attr::TypeNonNull:
    return cast<TypeNonNullAttr>(A)->printPretty(OS, Policy);
  case attr::TypeNullUnspecified:
    return cast<TypeNullUnspecifiedAttr>(A)->printPretty(OS, Policy);
  case attr::TypeNullable:
    return cast<TypeNullableAttr>(A)->printPretty(OS, Policy);
  case attr::TypeTagForDatatype:
    return cast<TypeTagForDatatypeAttr>(A)->printPretty(OS, Policy);
  case attr::TypeVisibility:
    return cast<TypeVisibilityAttr>(A)->printPretty(OS, Policy);
  case attr::UPtr:
    return cast<UPtrAttr>(A)->printPretty(OS, Policy);
  case attr::Unavailable:
    return cast<UnavailableAttr>(A)->printPretty(OS, Policy);
  case attr::Uninitialized:
    return cast<UninitializedAttr>(A)->printPretty(OS, Policy);
  case attr::Unused:
    return cast<UnusedAttr>(A)->printPretty(OS, Policy);
  case attr::UseHandle:
    return cast<UseHandleAttr>(A)->printPretty(OS, Policy);
  case attr::Used:
    return cast<UsedAttr>(A)->printPretty(OS, Policy);
  case attr::Uuid:
    return cast<UuidAttr>(A)->printPretty(OS, Policy);
  case attr::VecReturn:
    return cast<VecReturnAttr>(A)->printPretty(OS, Policy);
  case attr::VecTypeHint:
    return cast<VecTypeHintAttr>(A)->printPretty(OS, Policy);
  case attr::VectorCall:
    return cast<VectorCallAttr>(A)->printPretty(OS, Policy);
  case attr::Visibility:
    return cast<VisibilityAttr>(A)->printPretty(OS, Policy);
  case attr::WarnUnused:
    return cast<WarnUnusedAttr>(A)->printPretty(OS, Policy);
  case attr::WarnUnusedResult:
    return cast<WarnUnusedResultAttr>(A)->printPretty(OS, Policy);
  case attr::Weak:
    return cast<WeakAttr>(A)->printPretty(OS, Policy);
  case attr::WeakImport:
    return cast<WeakImportAttr>(A)->printPretty(OS, Policy);
  case attr::WeakRef:
    return cast<WeakRefAttr>(A)->printPretty(OS, Policy);
  case attr::WebAssemblyExportName:
    return cast<WebAssemblyExportNameAttr>(A)->printPretty(OS, Policy);
  case attr::WebAssemblyImportModule:
    return cast<WebAssemblyImportModuleAttr>(A)->printPretty(OS, Policy);
  case attr::WebAssemblyImportName:
    return cast<WebAssemblyImportNameAttr>(A)->printPretty(OS, Policy);
  case attr::WorkGroupSizeHint:
    return cast<WorkGroupSizeHintAttr>(A)->printPretty(OS, Policy);
  case attr::X86ForceAlignArgPointer:
    return cast<X86ForceAlignArgPointerAttr>(A)->printPretty(OS, Policy);
  case attr::XRayInstrument:
    return cast<XRayInstrumentAttr>(A)->printPretty(OS, Policy);
  case attr::XRayLogArgs:
    return cast<XRayLogArgsAttr>(A)->printPretty(OS, Policy);
  }
  llvm_unreachable("Unexpected attribute kind!");
}


void DeclPrinter::prettyPrintAttributes(Decl *D) {
  if (Policy.PolishForDeclaration)
    return;

  if (D->hasAttrs()) {
    AttrVec &Attrs = D->getAttrs();
    for (auto *A : Attrs) {
      if (A->isInherited() || A->isImplicit())
        continue;
      switch (A->getKind()) {
#define ATTR(X)
#define PRAGMA_SPELLING_ATTR(X) case attr::X:
#include "clang/Basic/AttrList.inc"
        break;
      default:
        {
        	printPrettyAttr(A,Out,Policy);
        }
        break;
      }
    }
  }
}

void DeclPrinter::prettyPrintPragmas(Decl *D) {
  if (Policy.PolishForDeclaration)
    return;

  if (D->hasAttrs()) {
    AttrVec &Attrs = D->getAttrs();
    for (auto *A : Attrs) {
      switch (A->getKind()) {
#define ATTR(X)
#define PRAGMA_SPELLING_ATTR(X) case attr::X:
#include "clang/Basic/AttrList.inc"
        {
        	A->printPretty(Out,Policy);
        	Indent();
        }
        break;
      default:
        break;
      }
    }
  }
}

void DeclPrinter::printDeclType(QualType T, StringRef DeclName, bool Pack) {
  // Normally, a PackExpansionType is written as T[3]... (for instance, as a
  // template argument), but if it is the type of a declaration, the ellipsis
  // is placed before the name being declared.
  if (auto *PET = T->getAs<PackExpansionType>()) {
    Pack = true;
    T = PET->getPattern();
  }
  TypePrint(T,Out, Policy, (Pack ? "..." : "") + DeclName, Indentation,CTAList);
}

void DeclPrinter::ProcessDeclGroup(SmallVectorImpl<Decl*>& Decls) {
  in_decl_group = 1;
  this->Indent();
  Decl::printGroup(Decls.data(), Decls.size(), Out, Policy, Indentation);
  Out << ";\n";
  Decls.clear();
  in_decl_group = 0;
}

void DeclPrinter::ProcessDeclGroupNoClear(SmallVectorImpl<Decl*>& Decls) {
  in_decl_group = 1;
  this->Indent();
  Decl::printGroup(Decls.data(), Decls.size(), Out, Policy, Indentation);
  Out << ";\n";
  in_decl_group = 0;
}

void DeclPrinter::Print(AccessSpecifier AS) {
  switch(AS) {
  case AS_none:      llvm_unreachable("No access specifier!");
  case AS_public:    Out << "public"; break;
  case AS_protected: Out << "protected"; break;
  case AS_private:   Out << "private"; break;
  }
}

void DeclPrinter::PrintConstructorInitializers(CXXConstructorDecl *CDecl,
                                               std::string &Proto) {
  bool HasInitializerList = false;
  for (const auto *BMInitializer : CDecl->inits()) {
    if (BMInitializer->isInClassMemberInitializer())
      continue;

    if (!HasInitializerList) {
      Proto += " : ";
      Out << Proto;
      Proto.clear();
      HasInitializerList = true;
    } else
      Out << ", ";

    if (BMInitializer->isAnyMemberInitializer()) {
      FieldDecl *FD = BMInitializer->getAnyMember();
      Out << *FD;
    } else {
      Out << QualType(BMInitializer->getBaseClass(), 0).getAsString(Policy);
    }

    Out << "(";
    if (!BMInitializer->getInit()) {
      // Nothing to print
    } else {
      Expr *Init = BMInitializer->getInit();
      if (ExprWithCleanups *Tmp = dyn_cast<ExprWithCleanups>(Init))
        Init = Tmp->getSubExpr();

      Init = Init->IgnoreParens();

      Expr *SimpleInit = nullptr;
      Expr **Args = nullptr;
      unsigned NumArgs = 0;
      if (ParenListExpr *ParenList = dyn_cast<ParenListExpr>(Init)) {
        Args = ParenList->getExprs();
        NumArgs = ParenList->getNumExprs();
      } else if (CXXConstructExpr *Construct =
                     dyn_cast<CXXConstructExpr>(Init)) {
        Args = Construct->getArgs();
        NumArgs = Construct->getNumArgs();
      } else
        SimpleInit = Init;

      if (SimpleInit) {
    	  StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
    	  P.Visit(SimpleInit);
      }
      else {
        for (unsigned I = 0; I != NumArgs; ++I) {
          assert(Args[I] != nullptr && "Expected non-null Expr");
          if (isa<CXXDefaultArgExpr>(Args[I]))
            break;

          if (I)
            Out << ", ";
          StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
          P.Visit(Args[I]);
        }
      }
    }
    Out << ")";
    if (BMInitializer->isPackExpansion())
      Out << "...";
  }
}

//----------------------------------------------------------------------------
// Common C declarations
//----------------------------------------------------------------------------

void DeclPrinter::VisitDeclContext(DeclContext *DC, bool Indent) {
  if (Policy.TerseOutput)
    return;
  int csd_cache = csd;
  csd = 0;
  if (Indent)
    Indentation += Policy.Indentation;

  SmallVector<Decl*, 2> Decls;
  for (DeclContext::decl_iterator D = DC->decls_begin(), DEnd = DC->decls_end();
       D != DEnd; ++D) {

    // Don't print ObjCIvarDecls, as they are printed when visiting the
    // containing ObjCInterfaceDecl.
    if (isa<ObjCIvarDecl>(*D))
      continue;

    // Skip over implicit declarations in pretty-printing mode.
    if (D->isImplicit())
      continue;

    // Don't print implicit specializations, as they are printed when visiting
    // corresponding templates.
    if (auto FD = dyn_cast<FunctionDecl>(*D))
      if (FD->getTemplateSpecializationKind() == TSK_ImplicitInstantiation &&
          !isa<ClassTemplateSpecializationDecl>(DC))
        continue;

    // The next bits of code handle stuff like "struct {int x;} a,b"; we're
    // forced to merge the declarations because there's no other way to
    // refer to the struct in question.  When that struct is named instead, we
    // also need to merge to avoid splitting off a stand-alone struct
    // declaration that produces the warning ext_no_declarators in some
    // contexts.
    //
    // This limited merging is safe without a bunch of other checks because it
    // only merges declarations directly referring to the tag, not typedefs.
    //
    // Check whether the current declaration should be grouped with a previous
    // non-free-standing tag declaration.
    QualType CurDeclType = getDeclType(*D);
    if (!Decls.empty() && !CurDeclType.isNull()) {
      QualType BaseType = GetBaseType(CurDeclType);
      if (!BaseType.isNull() && isa<ElaboratedType>(BaseType) &&
          cast<ElaboratedType>(BaseType)->getOwnedTagDecl() == Decls[0]) {
        Decls.push_back(*D);
        continue;
      }
    }

    // If we have a merged group waiting to be handled, handle it now.
    if (!Decls.empty())
      ProcessDeclGroup(Decls);

    // If the current declaration is not a free standing declaration, save it
    // so we can merge it with the subsequent declaration(s) using it.
    if (isa<TagDecl>(*D) && !cast<TagDecl>(*D)->isFreeStanding() && cast<TagDecl>(*D)->isCompleteDefinition()) {
      Decls.push_back(*D);
      continue;
    }

    if (isa<AccessSpecDecl>(*D)) {
      Indentation -= Policy.Indentation;
      this->Indent();
      Print(D->getAccess());
      Out << ":\n";
      Indentation += Policy.Indentation;
      continue;
    }

    this->Indent();
    Visit(*D);

    // FIXME: Need to be able to tell the DeclPrinter when
    const char *Terminator = nullptr;
    if (isa<OMPThreadPrivateDecl>(*D) || isa<OMPDeclareReductionDecl>(*D) ||
        isa<OMPDeclareMapperDecl>(*D) || isa<OMPRequiresDecl>(*D) ||
        isa<OMPAllocateDecl>(*D))
      Terminator = nullptr;
    else if (isa<ObjCMethodDecl>(*D) && cast<ObjCMethodDecl>(*D)->hasBody())
      Terminator = nullptr;
    else if (auto FD = dyn_cast<FunctionDecl>(*D)) {
      if (FD->isThisDeclarationADefinition())
        Terminator = nullptr;
      else
        Terminator = ";";
    } else if (auto TD = dyn_cast<FunctionTemplateDecl>(*D)) {
      if (TD->getTemplatedDecl()->isThisDeclarationADefinition())
        Terminator = nullptr;
      else
        Terminator = ";";
    } else if (isa<NamespaceDecl>(*D) || isa<LinkageSpecDecl>(*D) ||
             isa<ObjCImplementationDecl>(*D) ||
             isa<ObjCInterfaceDecl>(*D) ||
             isa<ObjCProtocolDecl>(*D) ||
             isa<ObjCCategoryImplDecl>(*D) ||
             isa<ObjCCategoryDecl>(*D))
      Terminator = nullptr;
    else if (isa<EnumConstantDecl>(*D)) {
      DeclContext::decl_iterator Next = D;
      ++Next;
      if (Next != DEnd)
        Terminator = ",";
    } else
      Terminator = ";";

    if (Terminator)
      Out << Terminator;
    if (!Policy.TerseOutput &&
        ((isa<FunctionDecl>(*D) &&
          cast<FunctionDecl>(*D)->doesThisDeclarationHaveABody()) ||
         (isa<FunctionTemplateDecl>(*D) &&
          cast<FunctionTemplateDecl>(*D)->getTemplatedDecl()->doesThisDeclarationHaveABody())))
      ; // StmtPrinter already added '\n' after CompoundStmt.
    else
      Out << "\n";

    // Declare target attribute is special one, natural spelling for the pragma
    // assumes "ending" construct so print it here.
    if (D->hasAttr<OMPDeclareTargetDeclAttr>())
      Out << "#pragma omp end declare target\n";
  }

  if (!Decls.empty())
    ProcessDeclGroup(Decls);

  if (Indent)
    Indentation -= Policy.Indentation;
  csd = csd_cache;
}

void DeclPrinter::VisitTranslationUnitDecl(TranslationUnitDecl *D) {
  VisitDeclContext(D, false);
}

void DeclPrinter::VisitTypedefDecl(TypedefDecl *D) {
  if (!Policy.SuppressSpecifiers) {
    Out << "typedef ";

    if (D->isModulePrivate())
      Out << "__module_private__ ";
  }

  QualType Ty = D->getTypeSourceInfo()->getType();
  TypePrint(Ty,Out, Policy, D->getName(), Indentation,CTAList);
  prettyPrintAttributes(D);
}

void DeclPrinter::VisitTypeAliasDecl(TypeAliasDecl *D) {
  Out << "using " << *D;
  prettyPrintAttributes(D);
  Out << " = " << D->getTypeSourceInfo()->getType().getAsString(Policy);
}

void DeclPrinter::VisitEnumDecl(EnumDecl *D) {
  if (!Policy.SuppressSpecifiers && D->isModulePrivate())
    Out << "__module_private__ ";
  Out << "enum";
  if (D->isScoped()) {
    if (D->isScopedUsingClassTag())
      Out << " class";
    else
      Out << " struct";
  }

  prettyPrintAttributes(D);

  Out << ' ' << *D;

  if (D->isFixed() && D->getASTContext().getLangOpts().CPlusPlus11) {
    Out << " : ";
    TypePrint(D->getIntegerType(),Out, Policy,Twine(),0,CTAList);
  }

  if (D->isCompleteDefinition()) {
    Out << " {\n";
    VisitDeclContext(D);
    Indent() << "}";
  }
}

void DeclPrinter::VisitNakedTagDecl(TagDecl *D) {

  prettyPrintAttributes(D);

  if (D->isCompleteDefinition()) {
    Out << " {\n";
    VisitDeclContext(D);
    Indent() << "}";
  }
}

void DeclPrinter::PrintRecordHead(RecordDecl *D) {
  if (!Policy.SuppressSpecifiers && D->isModulePrivate())
    Out << "__module_private__ ";
  Out << D->getKindName();

  prettyPrintAttributes(D);

  if (D->getIdentifier()) {
    Out << ' ' << *D;
  }
}

void DeclPrinter::VisitRecordDecl(RecordDecl *D) {
  if (!Policy.SuppressSpecifiers && D->isModulePrivate())
    Out << "__module_private__ ";
  Out << D->getKindName();

  prettyPrintAttributes(D);

  if (D->getIdentifier()) {
    Out << ' ' << *D;
  }

  if (D->isCompleteDefinition()) {
    Out << " {\n";
    VisitDeclContext(D);
    Indent() << "}";
  }
}

void DeclPrinter::VisitEnumConstantDecl(EnumConstantDecl *D) {
  Out << *D;
  prettyPrintAttributes(D);
  if (Expr *Init = D->getInitExpr()) {
    Out << " = ";
    StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", &Context, CTAList);
    P.Visit(Init);
  }
}

static void printExplicitSpecifier(ExplicitSpecifier ES, llvm::raw_ostream &Out,
                                   PrintingPolicy &Policy,
                                   unsigned Indentation) {
  std::string Proto = "explicit";
  llvm::raw_string_ostream EOut(Proto);
  if (ES.getExpr()) {
    EOut << "(";
    StmtPrinter P(EOut, nullptr, Policy, Indentation);
    P.Visit(ES.getExpr());
    EOut << ")";
  }
  EOut << " ";
  EOut.flush();
  Out << EOut.str();
}

void DeclPrinter::VisitFunctionDecl(FunctionDecl *D) {
  if (!D->getDescribedFunctionTemplate() &&
      !D->isFunctionTemplateSpecialization())
    prettyPrintPragmas(D);

  if (D->isFunctionTemplateSpecialization())
    Out << "template<> ";
  else if (!D->getDescribedFunctionTemplate()) {
    for (unsigned I = 0, NumTemplateParams = D->getNumTemplateParameterLists();
         I < NumTemplateParams; ++I)
      printTemplateParameters(D->getTemplateParameterList(I));
  }

  CXXConstructorDecl *CDecl = dyn_cast<CXXConstructorDecl>(D);
  CXXConversionDecl *ConversionDecl = dyn_cast<CXXConversionDecl>(D);
  CXXDeductionGuideDecl *GuideDecl = dyn_cast<CXXDeductionGuideDecl>(D);
  if (!Policy.SuppressSpecifiers) {
    switch (D->getStorageClass()) {
    case SC_None: break;
    case SC_Extern: Out << "extern "; break;
    case SC_Static: Out << "static "; break;
    case SC_PrivateExtern: Out << "__private_extern__ "; break;
    case SC_Auto: case SC_Register:
      llvm_unreachable("invalid for functions");
    }

    if (D->isInlineSpecified())  Out << "inline ";
    if (D->isVirtualAsWritten()) Out << "virtual ";
    if (D->isModulePrivate())    Out << "__module_private__ ";
    if (D->isConstexprSpecified() && !D->isExplicitlyDefaulted())
      Out << "constexpr ";
    if (D->isConsteval())        Out << "consteval ";
    ExplicitSpecifier ExplicitSpec = ExplicitSpecifier::getFromDecl(D);
    if (ExplicitSpec.isSpecified())
      printExplicitSpecifier(ExplicitSpec, Out, Policy, Indentation);
  }

  PrintingPolicy SubPolicy(Policy);
  SubPolicy.SuppressSpecifiers = false;
  std::string Proto;

  if (Policy.FullyQualifiedName) {
    Proto += D->getQualifiedNameAsString();
  } else {
    llvm::raw_string_ostream OS(Proto);
    if (!Policy.SuppressScope) {
      if (const NestedNameSpecifier *NS = D->getQualifier()) {
        NS->print(OS, Policy);
      }
    }
    if (!CTAList) {
    	D->getNameInfo().printName(OS, Policy);
    }
    else {
        const FunctionDecl* CD = D->getCanonicalDecl();
        if (CTAList->find(CD)!=CTAList->end()) {
          OS << "__compiletime_assert";
        }
        else {
        	D->getNameInfo().printName(OS, Policy);
        }
    }
  }

  if (GuideDecl)
    Proto = GuideDecl->getDeducedTemplate()->getDeclName().getAsString();
  if (D->isFunctionTemplateSpecialization()) {
    llvm::raw_string_ostream POut(Proto);
    DeclPrinter TArgPrinter(POut, SubPolicy, Context, Indentation);
    const auto *TArgAsWritten = D->getTemplateSpecializationArgsAsWritten();
    if (TArgAsWritten && !Policy.PrintCanonicalTypes)
      TArgPrinter.printTemplateArguments(TArgAsWritten->arguments());
    else if (const TemplateArgumentList *TArgs =
                 D->getTemplateSpecializationArgs())
      TArgPrinter.printTemplateArguments(TArgs->asArray());
  }

  QualType Ty = D->getType();
  while (const ParenType *PT = dyn_cast<ParenType>(Ty)) {
    Proto = '(' + Proto + ')';
    Ty = PT->getInnerType();
  }

  if (const FunctionType *AFT = Ty->getAs<FunctionType>()) {
    const FunctionProtoType *FT = nullptr;
    if (D->hasWrittenPrototype())
      FT = dyn_cast<FunctionProtoType>(AFT);

    Proto += "(";
    if (FT) {
      llvm::raw_string_ostream POut(Proto);
      DeclPrinter ParamPrinter(POut, SubPolicy, Context, Indentation);
      for (unsigned i = 0, e = D->getNumParams(); i != e; ++i) {
        if (i) POut << ", ";
        ParamPrinter.VisitParmVarDecl(D->getParamDecl(i));
      }

      if (FT->isVariadic()) {
        if (D->getNumParams()) POut << ", ";
        POut << "...";
      }
    } else if (D->doesThisDeclarationHaveABody() && !D->hasPrototype()) {
      for (unsigned i = 0, e = D->getNumParams(); i != e; ++i) {
        if (i)
          Proto += ", ";
        Proto += D->getParamDecl(i)->getNameAsString();
      }
    }

    Proto += ")";

    if (FT) {
      if (FT->isConst())
        Proto += " const";
      if (FT->isVolatile())
        Proto += " volatile";
      if (FT->isRestrict())
        Proto += " restrict";

      switch (FT->getRefQualifier()) {
      case RQ_None:
        break;
      case RQ_LValue:
        Proto += " &";
        break;
      case RQ_RValue:
        Proto += " &&";
        break;
      }
    }

    if (FT && FT->hasDynamicExceptionSpec()) {
      Proto += " throw(";
      if (FT->getExceptionSpecType() == EST_MSAny)
        Proto += "...";
      else
        for (unsigned I = 0, N = FT->getNumExceptions(); I != N; ++I) {
          if (I)
            Proto += ", ";

          Proto += FT->getExceptionType(I).getAsString(SubPolicy);
        }
      Proto += ")";
    } else if (FT && isNoexceptExceptionSpec(FT->getExceptionSpecType())) {
      Proto += " noexcept";
      if (isComputedNoexcept(FT->getExceptionSpecType())) {
        Proto += "(";
        llvm::raw_string_ostream EOut(Proto);
        StmtPrinter P(EOut, nullptr, SubPolicy, Indentation, "\n", nullptr, CTAList);
        P.Visit(FT->getNoexceptExpr());
        EOut.flush();
        Proto += EOut.str();
        Proto += ")";
      }
    }

    if (CDecl) {
      if (!Policy.TerseOutput)
        PrintConstructorInitializers(CDecl, Proto);
    } else if (!ConversionDecl && !isa<CXXDestructorDecl>(D)) {
      if (FT && FT->hasTrailingReturn()) {
        if (!GuideDecl)
          Out << "auto ";
        Out << Proto << " -> ";
        Proto.clear();
      }
      TypePrint(AFT->getReturnType(),Out, Policy, Proto, 0,CTAList);
      Proto.clear();
    }
    Out << Proto;

    if (Expr *TrailingRequiresClause = D->getTrailingRequiresClause()) {
      Out << " requires ";
      StmtPrinter P(Out, nullptr, SubPolicy, Indentation, "\n", nullptr, CTAList);
      P.Visit(TrailingRequiresClause);
    }
  } else {
	  TypePrint(Ty, Out, Policy, Proto, 0,CTAList);
  }

  prettyPrintAttributes(D);

  if (D->isPure())
    Out << " = 0";
  else if (D->isDeletedAsWritten())
    Out << " = delete";
  else if (D->isExplicitlyDefaulted())
    Out << " = default";
  else if (D->doesThisDeclarationHaveABody()) {
    if (!Policy.TerseOutput) {
      if (!D->hasPrototype() && D->getNumParams()) {
        // This is a K&R function definition, so we need to print the
        // parameters.
        Out << '\n';
        DeclPrinter ParamPrinter(Out, SubPolicy, Context, Indentation);
        Indentation += Policy.Indentation;
        for (unsigned i = 0, e = D->getNumParams(); i != e; ++i) {
          Indent();
          ParamPrinter.VisitParmVarDecl(D->getParamDecl(i));
          Out << ";\n";
        }
        Indentation -= Policy.Indentation;
      } else
        Out << ' ';

      if (D->getBody()) {
        StmtPrinter P(Out, nullptr, SubPolicy, Indentation, "\n", nullptr, CTAList);
        P.Visit(const_cast<Stmt*>(D->getBody()));
      }
    } else {
      if (!Policy.TerseOutput && isa<CXXConstructorDecl>(*D))
        Out << " {}";
    }
  }
}

void DeclPrinter::VisitFriendDecl(FriendDecl *D) {
  if (TypeSourceInfo *TSI = D->getFriendType()) {
    unsigned NumTPLists = D->getFriendTypeNumTemplateParameterLists();
    for (unsigned i = 0; i < NumTPLists; ++i)
      printTemplateParameters(D->getFriendTypeTemplateParameterList(i));
    Out << "friend ";
    Out << " " << TSI->getType().getAsString(Policy);
  }
  else if (FunctionDecl *FD =
      dyn_cast<FunctionDecl>(D->getFriendDecl())) {
    Out << "friend ";
    VisitFunctionDecl(FD);
  }
  else if (FunctionTemplateDecl *FTD =
           dyn_cast<FunctionTemplateDecl>(D->getFriendDecl())) {
    Out << "friend ";
    VisitFunctionTemplateDecl(FTD);
  }
  else if (ClassTemplateDecl *CTD =
           dyn_cast<ClassTemplateDecl>(D->getFriendDecl())) {
    Out << "friend ";
    VisitRedeclarableTemplateDecl(CTD);
  }
}

void DeclPrinter::VisitFieldDecl(FieldDecl *D) {
  // FIXME: add printing of pragma attributes if required.
  if (!Policy.SuppressSpecifiers && D->isMutable())
    Out << "mutable ";
  if (!Policy.SuppressSpecifiers && D->isModulePrivate())
    Out << "__module_private__ ";

  if(csd){
    auto &Ctx = D->getASTContext();
    QualType T = D->getType().getCanonicalType();
    if(in_decl_group){
      if(group_unused){
        if(Policy.IncludeTagDefinition) Out<<"char ";
        Out << D->getName() << "[" << Ctx.getTypeSizeInChars(T).getQuantity() << "]";
        return;
      }
    } 
    else if(!D->isReferenced()){
      if(T->isFunctionPointerType()){
        //replace with void (*)() type
        Out << "void (*" << D->getName() << ")()";
        return;
      }
      else if(T->isPointerType()){
        if(GetBaseType(T)->isBuiltinType()){
          //use canonical type
          TypePrint(D->getASTContext().getUnqualifiedObjCPointerType(T),Out, Policy,D->getName(),Indentation,CTAList);
        }
        else{
          //replace with void * type
          Out << "void *" << D->getName();
        }
        return;
      }
      else if(T->isArrayType()){
        if(GetBaseType(T)->isBuiltinType()){
          //use canonical type
          TypePrint(D->getASTContext().getUnqualifiedObjCPointerType(T),Out, Policy,D->getName(),Indentation,CTAList);
        }
        else{
          //replace with char[] of proper size
          Out << "char " << D->getName() << "[" << Ctx.getTypeSizeInChars(T).getQuantity() << "]";
        }
        return;
      }
      else if(T->isRecordType()){
        //replace with char[] of proper size
        Out << "char " << D->getName() << "[" << Ctx.getTypeSizeInChars(T).getQuantity() << "]";
        return;
      }
      else if(T->isEnumeralType()){
        //get corresponding integer type
        T = cast<EnumType>(T)->getDecl()->getIntegerType();
        TypePrint(D->getASTContext().getUnqualifiedObjCPointerType(T),Out, Policy,D->getName(),Indentation,CTAList);
        if (D->isBitField()) {
          Out << " : ";
          StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
          P.Visit(D->getBitWidth());
        }
        return;
      }
      else if(T->isBuiltinType()){
        //use canonical type
        TypePrint(D->getASTContext().getUnqualifiedObjCPointerType(T),Out, Policy,D->getName(),Indentation,CTAList);
        if (D->isBitField()) {
          Out << " : ";
          StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
          P.Visit(D->getBitWidth());
        }
        return;
      }
      else if(T->isVectorType()){
    	  Out << "";
      }
      else{
        llvm::errs() << "Unhandled type: [" << T->getTypeClassName() << "] " << T.getAsString()<<'\n';
        D->dumpColor();
        llvm_unreachable("Encountered unhandled type. Exiting.");
      }
    }
  }

  TypePrint(D->getASTContext().getUnqualifiedObjCPointerType(D->getType()),Out, Policy,D->getName(),Indentation,CTAList);

  if (D->isBitField()) {
    Out << " : ";
    StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
    P.Visit(D->getBitWidth());
  }

  Expr *Init = D->getInClassInitializer();
  if (!Policy.SuppressInitializers && Init) {
    if (D->getInClassInitStyle() == ICIS_ListInit)
      Out << " ";
    else
      Out << " = ";
    StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
    P.Visit(Init);
  }
  prettyPrintAttributes(D);
}

void DeclPrinter::VisitLabelDecl(LabelDecl *D) {
  if (D->isGnuLocal()) {
	  Out << "__label__ " << D->getName().str();
  }
  else {
	  Out << *D << ":";
  }
}

void DeclPrinter::VisitVarDecl(VarDecl *D) {
  prettyPrintPragmas(D);

  QualType T = D->getTypeSourceInfo()
    ? D->getTypeSourceInfo()->getType()
    : D->getASTContext().getUnqualifiedObjCPointerType(D->getType());

  if (!Policy.SuppressSpecifiers) {
    StorageClass SC = D->getStorageClass();
    if (SC != SC_None)
      Out << VarDecl::getStorageClassSpecifierString(SC) << " ";

    switch (D->getTSCSpec()) {
    case TSCS_unspecified:
      break;
    case TSCS___thread:
      Out << "__thread ";
      break;
    case TSCS__Thread_local:
      Out << "_Thread_local ";
      break;
    case TSCS_thread_local:
      Out << "thread_local ";
      break;
    }

    if (D->isModulePrivate())
      Out << "__module_private__ ";

    if (D->isConstexpr()) {
      Out << "constexpr ";
      T.removeLocalConst();
    }
  }

  printDeclType(T, D->getName());

  prettyPrintAttributes(D);

  Expr *Init = D->getInit();
  if (!Policy.SuppressInitializers && Init) {
    bool ImplicitInit = false;
    if (CXXConstructExpr *Construct =
            dyn_cast<CXXConstructExpr>(Init->IgnoreImplicit())) {
      if (D->getInitStyle() == VarDecl::CallInit &&
          !Construct->isListInitialization()) {
        ImplicitInit = Construct->getNumArgs() == 0 ||
          Construct->getArg(0)->isDefaultArgument();
      }
    }
    if (!ImplicitInit) {
      if ((D->getInitStyle() == VarDecl::CallInit) && !isa<ParenListExpr>(Init))
        Out << "(";
      else if (D->getInitStyle() == VarDecl::CInit) {
        Out << " = ";
      }
      PrintingPolicy SubPolicy(Policy);
      SubPolicy.SuppressSpecifiers = false;
      SubPolicy.IncludeTagDefinition = false;
      StmtPrinter P(Out, nullptr, SubPolicy, Indentation, "\n", nullptr, CTAList);
      P.Visit(Init);
      if ((D->getInitStyle() == VarDecl::CallInit) && !isa<ParenListExpr>(Init))
        Out << ")";
    }
  }

}

void DeclPrinter::VisitParmVarDecl(ParmVarDecl *D) {
  VisitVarDecl(D);
}

void DeclPrinter::VisitFileScopeAsmDecl(FileScopeAsmDecl *D) {
  Out << "__asm (";
  StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
  P.Visit(D->getAsmString());
  Out << ")";
}

void DeclPrinter::VisitImportDecl(ImportDecl *D) {
  Out << "@import " << D->getImportedModule()->getFullModuleName()
      << ";\n";
}

void DeclPrinter::VisitStaticAssertDecl(StaticAssertDecl *D) {
  extern bool enable_sa;
  if(!enable_sa) return;
  Out << "static_assert(";
  StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
  P.Visit(D->getAssertExpr());
  if (StringLiteral *SL = D->getMessage()) {
    Out << ", ";
    StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
    P.Visit(SL);
  }
  Out << ")";
}

//----------------------------------------------------------------------------
// C++ declarations
//----------------------------------------------------------------------------
void DeclPrinter::VisitNamespaceDecl(NamespaceDecl *D) {
  if (D->isInline())
    Out << "inline ";
  Out << "namespace " << *D << " {\n";
  VisitDeclContext(D);
  Indent() << "}";
}

void DeclPrinter::VisitUsingDirectiveDecl(UsingDirectiveDecl *D) {
  Out << "using namespace ";
  if (D->getQualifier())
    D->getQualifier()->print(Out, Policy);
  Out << *D->getNominatedNamespaceAsWritten();
}

void DeclPrinter::VisitNamespaceAliasDecl(NamespaceAliasDecl *D) {
  Out << "namespace " << *D << " = ";
  if (D->getQualifier())
    D->getQualifier()->print(Out, Policy);
  Out << *D->getAliasedNamespace();
}

void DeclPrinter::VisitEmptyDecl(EmptyDecl *D) {
  prettyPrintAttributes(D);
}

void DeclPrinter::VisitCXXRecordDecl(CXXRecordDecl *D) {
  // FIXME: add printing of pragma attributes if required.
  if (!Policy.SuppressSpecifiers && D->isModulePrivate())
    Out << "__module_private__ ";
  Out << D->getKindName();

  prettyPrintAttributes(D);

  if (D->getIdentifier()) {
    Out << ' ' << *D;

    if (auto S = dyn_cast<ClassTemplateSpecializationDecl>(D)) {
      ArrayRef<TemplateArgument> Args = S->getTemplateArgs().asArray();
      if (!Policy.PrintCanonicalTypes)
        if (const auto* TSI = S->getTypeAsWritten())
          if (const auto *TST =
                  dyn_cast<TemplateSpecializationType>(TSI->getType()))
            Args = TST->template_arguments();
      printTemplateArguments(Args);
    }
  }

  if (D->isCompleteDefinition()) {
    // Print the base classes
    if (D->getNumBases()) {
      Out << " : ";
      for (CXXRecordDecl::base_class_iterator Base = D->bases_begin(),
             BaseEnd = D->bases_end(); Base != BaseEnd; ++Base) {
        if (Base != D->bases_begin())
          Out << ", ";

        if (Base->isVirtual())
          Out << "virtual ";

        AccessSpecifier AS = Base->getAccessSpecifierAsWritten();
        if (AS != AS_none) {
          Print(AS);
          Out << " ";
        }
        Out << Base->getType().getAsString(Policy);

        if (Base->isPackExpansion())
          Out << "...";
      }
    }

    // Print the class definition
    // FIXME: Doesn't print access specifiers, e.g., "public:"
    if (Policy.TerseOutput) {
      Out << " {}";
    } else {
      Out << " {\n";
      VisitDeclContext(D);
      Indent() << "}";
    }
  }
}

void DeclPrinter::VisitLinkageSpecDecl(LinkageSpecDecl *D) {
  const char *l;
  if (D->getLanguage() == LinkageSpecDecl::lang_c)
    l = "C";
  else {
    assert(D->getLanguage() == LinkageSpecDecl::lang_cxx &&
           "unknown language in linkage specification");
    l = "C++";
  }

  Out << "extern \"" << l << "\" ";
  if (D->hasBraces()) {
    Out << "{\n";
    VisitDeclContext(D);
    Indent() << "}";
  } else
    Visit(*D->decls_begin());
}

void DeclPrinter::printTemplateParameters(const TemplateParameterList *Params,
                                          bool OmitTemplateKW) {
  assert(Params);

  if (!OmitTemplateKW)
    Out << "template ";
  Out << '<';

  bool NeedComma = false;
  for (const Decl *Param : *Params) {
    if (Param->isImplicit())
      continue;

    if (NeedComma)
      Out << ", ";
    else
      NeedComma = true;

    if (auto TTP = dyn_cast<TemplateTypeParmDecl>(Param)) {

      if (const TypeConstraint *TC = TTP->getTypeConstraint())
        TC->print(Out, Policy);
      else if (TTP->wasDeclaredWithTypename())
        Out << "typename";
      else
        Out << "class";

      if (TTP->isParameterPack())
        Out << " ...";
      else if (!TTP->getName().empty())
        Out << ' ';

      Out << *TTP;

      if (TTP->hasDefaultArgument()) {
        Out << " = ";
        Out << TTP->getDefaultArgument().getAsString(Policy);
      };
    } else if (auto NTTP = dyn_cast<NonTypeTemplateParmDecl>(Param)) {
      StringRef Name;
      if (IdentifierInfo *II = NTTP->getIdentifier())
        Name = II->getName();
      printDeclType(NTTP->getType(), Name, NTTP->isParameterPack());

      if (NTTP->hasDefaultArgument()) {
        Out << " = ";
        StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
        P.Visit(NTTP->getDefaultArgument());
      }
    } else if (auto TTPD = dyn_cast<TemplateTemplateParmDecl>(Param)) {
      VisitTemplateDecl(TTPD);
      // FIXME: print the default argument, if present.
    }
  }

  Out << '>';
  if (!OmitTemplateKW)
    Out << ' ';
}

void DeclPrinter::printTemplateArguments(ArrayRef<TemplateArgument> Args) {
  Out << "<";
  for (size_t I = 0, E = Args.size(); I < E; ++I) {
    if (I)
      Out << ", ";
    Args[I].print(Policy, Out);
  }
  Out << ">";
}

void DeclPrinter::printTemplateArguments(ArrayRef<TemplateArgumentLoc> Args) {
  Out << "<";
  for (size_t I = 0, E = Args.size(); I < E; ++I) {
    if (I)
      Out << ", ";
    Args[I].getArgument().print(Policy, Out);
  }
  Out << ">";
}

void DeclPrinter::VisitTemplateDecl(const TemplateDecl *D) {
  printTemplateParameters(D->getTemplateParameters());

  if (const TemplateTemplateParmDecl *TTP =
        dyn_cast<TemplateTemplateParmDecl>(D)) {
    Out << "class ";
    if (TTP->isParameterPack())
      Out << "...";
    Out << D->getName();
  } else if (auto *TD = D->getTemplatedDecl())
    Visit(TD);
  else if (const auto *Concept = dyn_cast<ConceptDecl>(D)) {
    Out << "concept " << Concept->getName() << " = " ;
    StmtPrinter P(Out, nullptr, Policy, Indentation, "\n", nullptr, CTAList);
    P.Visit(Concept->getConstraintExpr());
    Out << ";";
  }
}

void DeclPrinter::VisitFunctionTemplateDecl(FunctionTemplateDecl *D) {
  prettyPrintPragmas(D->getTemplatedDecl());
  // Print any leading template parameter lists.
  if (const FunctionDecl *FD = D->getTemplatedDecl()) {
    for (unsigned I = 0, NumTemplateParams = FD->getNumTemplateParameterLists();
         I < NumTemplateParams; ++I)
      printTemplateParameters(FD->getTemplateParameterList(I));
  }
  VisitRedeclarableTemplateDecl(D);
  // Declare target attribute is special one, natural spelling for the pragma
  // assumes "ending" construct so print it here.
  if (D->getTemplatedDecl()->hasAttr<OMPDeclareTargetDeclAttr>())
    Out << "#pragma omp end declare target\n";

  // Never print "instantiations" for deduction guides (they don't really
  // have them).
  if (PrintInstantiation &&
      !isa<CXXDeductionGuideDecl>(D->getTemplatedDecl())) {
    FunctionDecl *PrevDecl = D->getTemplatedDecl();
    const FunctionDecl *Def;
    if (PrevDecl->isDefined(Def) && Def != PrevDecl)
      return;
    for (auto *I : D->specializations())
      if (I->getTemplateSpecializationKind() == TSK_ImplicitInstantiation) {
        if (!PrevDecl->isThisDeclarationADefinition())
          Out << ";\n";
        Indent();
        prettyPrintPragmas(I);
        Visit(I);
      }
  }
}

void DeclPrinter::VisitClassTemplateDecl(ClassTemplateDecl *D) {
  VisitRedeclarableTemplateDecl(D);

  if (PrintInstantiation) {
    for (auto *I : D->specializations())
      if (I->getSpecializationKind() == TSK_ImplicitInstantiation) {
        if (D->isThisDeclarationADefinition())
          Out << ";";
        Out << "\n";
        Visit(I);
      }
  }
}

void DeclPrinter::VisitClassTemplateSpecializationDecl(
                                           ClassTemplateSpecializationDecl *D) {
  Out << "template<> ";
  VisitCXXRecordDecl(D);
}

void DeclPrinter::VisitClassTemplatePartialSpecializationDecl(
                                    ClassTemplatePartialSpecializationDecl *D) {
  printTemplateParameters(D->getTemplateParameters());
  VisitCXXRecordDecl(D);
}

//----------------------------------------------------------------------------
// Objective-C declarations
//----------------------------------------------------------------------------

void DeclPrinter::PrintObjCMethodType(ASTContext &Ctx,
                                      Decl::ObjCDeclQualifier Quals,
                                      QualType T) {
  Out << '(';
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_In)
    Out << "in ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_Inout)
    Out << "inout ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_Out)
    Out << "out ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_Bycopy)
    Out << "bycopy ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_Byref)
    Out << "byref ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_Oneway)
    Out << "oneway ";
  if (Quals & Decl::ObjCDeclQualifier::OBJC_TQ_CSNullability) {
    if (auto nullability = AttributedType::stripOuterNullability(T))
      Out << getNullabilitySpelling(*nullability, true) << ' ';
  }

  Out << Ctx.getUnqualifiedObjCPointerType(T).getAsString(Policy);
  Out << ')';
}

void DeclPrinter::PrintObjCTypeParams(ObjCTypeParamList *Params) {
  Out << "<";
  unsigned First = true;
  for (auto *Param : *Params) {
    if (First) {
      First = false;
    } else {
      Out << ", ";
    }

    switch (Param->getVariance()) {
    case ObjCTypeParamVariance::Invariant:
      break;

    case ObjCTypeParamVariance::Covariant:
      Out << "__covariant ";
      break;

    case ObjCTypeParamVariance::Contravariant:
      Out << "__contravariant ";
      break;
    }

    Out << Param->getDeclName().getAsString();

    if (Param->hasExplicitBound()) {
      Out << " : " << Param->getUnderlyingType().getAsString(Policy);
    }
  }
  Out << ">";
}

void DeclPrinter::VisitObjCMethodDecl(ObjCMethodDecl *OMD) {
  if (OMD->isInstanceMethod())
    Out << "- ";
  else
    Out << "+ ";
  if (!OMD->getReturnType().isNull()) {
    PrintObjCMethodType(OMD->getASTContext(), OMD->getObjCDeclQualifier(),
                        OMD->getReturnType());
  }

  std::string name = OMD->getSelector().getAsString();
  std::string::size_type pos, lastPos = 0;
  for (const auto *PI : OMD->parameters()) {
    // FIXME: selector is missing here!
    pos = name.find_first_of(':', lastPos);
    if (lastPos != 0)
      Out << " ";
    Out << name.substr(lastPos, pos - lastPos) << ':';
    PrintObjCMethodType(OMD->getASTContext(),
                        PI->getObjCDeclQualifier(),
                        PI->getType());
    Out << *PI;
    lastPos = pos + 1;
  }

  if (OMD->param_begin() == OMD->param_end())
    Out << name;

  if (OMD->isVariadic())
      Out << ", ...";

  prettyPrintAttributes(OMD);

  if (OMD->getBody() && !Policy.TerseOutput) {
    Out << ' ';
    StmtPrinter P(Out, nullptr, Policy, 0, "\n", nullptr, CTAList);
    P.Visit(const_cast<Stmt*>(OMD->getBody()));
  }
  else if (Policy.PolishForDeclaration)
    Out << ';';
}

void DeclPrinter::VisitObjCImplementationDecl(ObjCImplementationDecl *OID) {
  std::string I = OID->getNameAsString();
  ObjCInterfaceDecl *SID = OID->getSuperClass();

  bool eolnOut = false;
  if (SID)
    Out << "@implementation " << I << " : " << *SID;
  else
    Out << "@implementation " << I;

  if (OID->ivar_size() > 0) {
    Out << "{\n";
    eolnOut = true;
    Indentation += Policy.Indentation;
    for (const auto *I : OID->ivars()) {
      Indent() << I->getASTContext().getUnqualifiedObjCPointerType(I->getType()).
                    getAsString(Policy) << ' ' << *I << ";\n";
    }
    Indentation -= Policy.Indentation;
    Out << "}\n";
  }
  else if (SID || (OID->decls_begin() != OID->decls_end())) {
    Out << "\n";
    eolnOut = true;
  }
  VisitDeclContext(OID, false);
  if (!eolnOut)
    Out << "\n";
  Out << "@end";
}

void DeclPrinter::VisitObjCInterfaceDecl(ObjCInterfaceDecl *OID) {
  std::string I = OID->getNameAsString();
  ObjCInterfaceDecl *SID = OID->getSuperClass();

  if (!OID->isThisDeclarationADefinition()) {
    Out << "@class " << I;

    if (auto TypeParams = OID->getTypeParamListAsWritten()) {
      PrintObjCTypeParams(TypeParams);
    }

    Out << ";";
    return;
  }
  bool eolnOut = false;
  Out << "@interface " << I;

  if (auto TypeParams = OID->getTypeParamListAsWritten()) {
    PrintObjCTypeParams(TypeParams);
  }

  if (SID)
    Out << " : " << QualType(OID->getSuperClassType(), 0).getAsString(Policy);

  // Protocols?
  const ObjCList<ObjCProtocolDecl> &Protocols = OID->getReferencedProtocols();
  if (!Protocols.empty()) {
    for (ObjCList<ObjCProtocolDecl>::iterator I = Protocols.begin(),
         E = Protocols.end(); I != E; ++I)
      Out << (I == Protocols.begin() ? '<' : ',') << **I;
    Out << "> ";
  }

  if (OID->ivar_size() > 0) {
    Out << "{\n";
    eolnOut = true;
    Indentation += Policy.Indentation;
    for (const auto *I : OID->ivars()) {
      Indent() << I->getASTContext()
                      .getUnqualifiedObjCPointerType(I->getType())
                      .getAsString(Policy) << ' ' << *I << ";\n";
    }
    Indentation -= Policy.Indentation;
    Out << "}\n";
  }
  else if (SID || (OID->decls_begin() != OID->decls_end())) {
    Out << "\n";
    eolnOut = true;
  }

  VisitDeclContext(OID, false);
  if (!eolnOut)
    Out << "\n";
  Out << "@end";
  // FIXME: implement the rest...
}

void DeclPrinter::VisitObjCProtocolDecl(ObjCProtocolDecl *PID) {
  if (!PID->isThisDeclarationADefinition()) {
    Out << "@protocol " << *PID << ";\n";
    return;
  }
  // Protocols?
  const ObjCList<ObjCProtocolDecl> &Protocols = PID->getReferencedProtocols();
  if (!Protocols.empty()) {
    Out << "@protocol " << *PID;
    for (ObjCList<ObjCProtocolDecl>::iterator I = Protocols.begin(),
         E = Protocols.end(); I != E; ++I)
      Out << (I == Protocols.begin() ? '<' : ',') << **I;
    Out << ">\n";
  } else
    Out << "@protocol " << *PID << '\n';
  VisitDeclContext(PID, false);
  Out << "@end";
}

void DeclPrinter::VisitObjCCategoryImplDecl(ObjCCategoryImplDecl *PID) {
  Out << "@implementation " << *PID->getClassInterface() << '(' << *PID <<")\n";

  VisitDeclContext(PID, false);
  Out << "@end";
  // FIXME: implement the rest...
}

void DeclPrinter::VisitObjCCategoryDecl(ObjCCategoryDecl *PID) {
  Out << "@interface " << *PID->getClassInterface();
  if (auto TypeParams = PID->getTypeParamList()) {
    PrintObjCTypeParams(TypeParams);
  }
  Out << "(" << *PID << ")\n";
  if (PID->ivar_size() > 0) {
    Out << "{\n";
    Indentation += Policy.Indentation;
    for (const auto *I : PID->ivars())
      Indent() << I->getASTContext().getUnqualifiedObjCPointerType(I->getType()).
                    getAsString(Policy) << ' ' << *I << ";\n";
    Indentation -= Policy.Indentation;
    Out << "}\n";
  }

  VisitDeclContext(PID, false);
  Out << "@end";

  // FIXME: implement the rest...
}

void DeclPrinter::VisitObjCCompatibleAliasDecl(ObjCCompatibleAliasDecl *AID) {
  Out << "@compatibility_alias " << *AID
      << ' ' << *AID->getClassInterface() << ";\n";
}

/// PrintObjCPropertyDecl - print a property declaration.
///
/// Print attributes in the following order:
/// - class
/// - nonatomic | atomic
/// - assign | retain | strong | copy | weak | unsafe_unretained
/// - readwrite | readonly
/// - getter & setter
/// - nullability
void DeclPrinter::VisitObjCPropertyDecl(ObjCPropertyDecl *PDecl) {
  if (PDecl->getPropertyImplementation() == ObjCPropertyDecl::Required)
    Out << "@required\n";
  else if (PDecl->getPropertyImplementation() == ObjCPropertyDecl::Optional)
    Out << "@optional\n";

  QualType T = PDecl->getType();

  Out << "@property";
  if (PDecl->getPropertyAttributes() != ObjCPropertyDecl::OBJC_PR_noattr) {
    bool first = true;
    Out << "(";
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_class) {
      Out << (first ? "" : ", ") << "class";
      first = false;
    }

    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_direct) {
      Out << (first ? "" : ", ") << "direct";
      first = false;
    }

    if (PDecl->getPropertyAttributes() &
        ObjCPropertyDecl::OBJC_PR_nonatomic) {
      Out << (first ? "" : ", ") << "nonatomic";
      first = false;
    }
    if (PDecl->getPropertyAttributes() &
        ObjCPropertyDecl::OBJC_PR_atomic) {
      Out << (first ? "" : ", ") << "atomic";
      first = false;
    }

    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_assign) {
      Out << (first ? "" : ", ") << "assign";
      first = false;
    }
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_retain) {
      Out << (first ? "" : ", ") << "retain";
      first = false;
    }

    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_strong) {
      Out << (first ? "" : ", ") << "strong";
      first = false;
    }
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_copy) {
      Out << (first ? "" : ", ") << "copy";
      first = false;
    }
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_weak) {
      Out << (first ? "" : ", ") << "weak";
      first = false;
    }
    if (PDecl->getPropertyAttributes()
        & ObjCPropertyDecl::OBJC_PR_unsafe_unretained) {
      Out << (first ? "" : ", ") << "unsafe_unretained";
      first = false;
    }

    if (PDecl->getPropertyAttributes() &
        ObjCPropertyDecl::OBJC_PR_readwrite) {
      Out << (first ? "" : ", ") << "readwrite";
      first = false;
    }
    if (PDecl->getPropertyAttributes() &
        ObjCPropertyDecl::OBJC_PR_readonly) {
      Out << (first ? "" : ", ") << "readonly";
      first = false;
    }

    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_getter) {
      Out << (first ? "" : ", ") << "getter = ";
      PDecl->getGetterName().print(Out);
      first = false;
    }
    if (PDecl->getPropertyAttributes() & ObjCPropertyDecl::OBJC_PR_setter) {
      Out << (first ? "" : ", ") << "setter = ";
      PDecl->getSetterName().print(Out);
      first = false;
    }

    if (PDecl->getPropertyAttributes() &
        ObjCPropertyDecl::OBJC_PR_nullability) {
      if (auto nullability = AttributedType::stripOuterNullability(T)) {
        if (*nullability == NullabilityKind::Unspecified &&
            (PDecl->getPropertyAttributes() &
               ObjCPropertyDecl::OBJC_PR_null_resettable)) {
          Out << (first ? "" : ", ") << "null_resettable";
        } else {
          Out << (first ? "" : ", ")
              << getNullabilitySpelling(*nullability, true);
        }
        first = false;
      }
    }

    (void) first; // Silence dead store warning due to idiomatic code.
    Out << ")";
  }
  std::string TypeStr = PDecl->getASTContext().getUnqualifiedObjCPointerType(T).
      getAsString(Policy);
  Out << ' ' << TypeStr;
  if (!StringRef(TypeStr).endswith("*"))
    Out << ' ';
  Out << *PDecl;
  if (Policy.PolishForDeclaration)
    Out << ';';
}

void DeclPrinter::VisitObjCPropertyImplDecl(ObjCPropertyImplDecl *PID) {
  if (PID->getPropertyImplementation() == ObjCPropertyImplDecl::Synthesize)
    Out << "@synthesize ";
  else
    Out << "@dynamic ";
  Out << *PID->getPropertyDecl();
  if (PID->getPropertyIvarDecl())
    Out << '=' << *PID->getPropertyIvarDecl();
}

void DeclPrinter::VisitUsingDecl(UsingDecl *D) {
  if (!D->isAccessDeclaration())
    Out << "using ";
  if (D->hasTypename())
    Out << "typename ";
  D->getQualifier()->print(Out, Policy);

  // Use the correct record name when the using declaration is used for
  // inheriting constructors.
  for (const auto *Shadow : D->shadows()) {
    if (const auto *ConstructorShadow =
            dyn_cast<ConstructorUsingShadowDecl>(Shadow)) {
      assert(Shadow->getDeclContext() == ConstructorShadow->getDeclContext());
      Out << *ConstructorShadow->getNominatedBaseClass();
      return;
    }
  }
  Out << *D;
}

void
DeclPrinter::VisitUnresolvedUsingTypenameDecl(UnresolvedUsingTypenameDecl *D) {
  Out << "using typename ";
  D->getQualifier()->print(Out, Policy);
  Out << D->getDeclName();
}

void DeclPrinter::VisitUnresolvedUsingValueDecl(UnresolvedUsingValueDecl *D) {
  if (!D->isAccessDeclaration())
    Out << "using ";
  D->getQualifier()->print(Out, Policy);
  Out << D->getDeclName();
}

void DeclPrinter::VisitUsingShadowDecl(UsingShadowDecl *D) {
  // ignore
}

void DeclPrinter::VisitOMPThreadPrivateDecl(OMPThreadPrivateDecl *D) {
  Out << "#pragma omp threadprivate";
  if (!D->varlist_empty()) {
    for (OMPThreadPrivateDecl::varlist_iterator I = D->varlist_begin(),
                                                E = D->varlist_end();
                                                I != E; ++I) {
      Out << (I == D->varlist_begin() ? '(' : ',');
      NamedDecl *ND = cast<DeclRefExpr>(*I)->getDecl();
      ND->printQualifiedName(Out);
    }
    Out << ")";
  }
}

void DeclPrinter::VisitOMPAllocateDecl(OMPAllocateDecl *D) {
  Out << "#pragma omp allocate";
  if (!D->varlist_empty()) {
    for (OMPAllocateDecl::varlist_iterator I = D->varlist_begin(),
                                           E = D->varlist_end();
         I != E; ++I) {
      Out << (I == D->varlist_begin() ? '(' : ',');
      NamedDecl *ND = cast<DeclRefExpr>(*I)->getDecl();
      ND->printQualifiedName(Out);
    }
    Out << ")";
  }
  if (!D->clauselist_empty()) {
    Out << " ";
    OMPClausePrinter Printer(Out, Policy);
    for (OMPClause *C : D->clauselists())
      Printer.Visit(C);
  }
}

void DeclPrinter::VisitOMPRequiresDecl(OMPRequiresDecl *D) {
  Out << "#pragma omp requires ";
  if (!D->clauselist_empty()) {
    OMPClausePrinter Printer(Out, Policy);
    for (auto I = D->clauselist_begin(), E = D->clauselist_end(); I != E; ++I)
      Printer.Visit(*I);
  }
}

void DeclPrinter::VisitOMPDeclareReductionDecl(OMPDeclareReductionDecl *D) {
  if (!D->isInvalidDecl()) {
    Out << "#pragma omp declare reduction (";
    if (D->getDeclName().getNameKind() == DeclarationName::CXXOperatorName) {
      const char *OpName =
          getOperatorSpelling(D->getDeclName().getCXXOverloadedOperator());
      assert(OpName && "not an overloaded operator");
      Out << OpName;
    } else {
      assert(D->getDeclName().isIdentifier());
      D->printName(Out);
    }
    Out << " : ";
    TypePrint(D->getType(),Out,Policy,Twine(),0,CTAList);
    Out << " : ";
    StmtPrinter P(Out, nullptr, Policy, 0, "\n", nullptr, CTAList);
    P.Visit(D->getCombiner());
    Out << ")";
    if (auto *Init = D->getInitializer()) {
      Out << " initializer(";
      switch (D->getInitializerKind()) {
      case OMPDeclareReductionDecl::DirectInit:
        Out << "omp_priv(";
        break;
      case OMPDeclareReductionDecl::CopyInit:
        Out << "omp_priv = ";
        break;
      case OMPDeclareReductionDecl::CallInit:
        break;
      }
      StmtPrinter P(Out, nullptr, Policy, 0, "\n", nullptr, CTAList);
      P.Visit(Init);
      if (D->getInitializerKind() == OMPDeclareReductionDecl::DirectInit)
        Out << ")";
      Out << ")";
    }
  }
}

void DeclPrinter::VisitOMPDeclareMapperDecl(OMPDeclareMapperDecl *D) {
  if (!D->isInvalidDecl()) {
    Out << "#pragma omp declare mapper (";
    D->printName(Out);
    Out << " : ";
    TypePrint(D->getType(),Out, Policy,Twine(),0,CTAList);
    Out << " ";
    Out << D->getVarName();
    Out << ")";
    if (!D->clauselist_empty()) {
      OMPClausePrinter Printer(Out, Policy);
      for (auto *C : D->clauselists()) {
        Out << " ";
        Printer.Visit(C);
      }
    }
  }
}

void DeclPrinter::VisitOMPCapturedExprDecl(OMPCapturedExprDecl *D) {
  StmtPrinter P(Out, nullptr, Policy, 0, "\n", nullptr, CTAList);
  P.Visit(D->getInit());
}

