//===--- PPCallbacksTracker.cpp - Preprocessor tracker -*--*---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Implementations for preprocessor tracking.
///
/// See the header for details.
///
// Modified by [2022] Samsung Electronics, co. Ltd.
//===----------------------------------------------------------------------===//

#include <MacroInfoExt.h>
#include "PPCallbacksTracker.h"
#include "clang/Lex/MacroArgs.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>

#include "main.hpp"

#define DBG(on,...)		do { if (on) { __VA_ARGS__; } } while(0)

int DEBUG_PP;

// Utility functions.

// Get a "file:line:column" source location string.
static std::string getSourceLocationString(clang::Preprocessor &PP,
                                           clang::SourceLocation Loc) {
  if (Loc.isInvalid())
    return std::string("(none)");

  if (Loc.isFileID()) {
    clang::PresumedLoc PLoc = PP.getSourceManager().getPresumedLoc(Loc);

    if (PLoc.isInvalid()) {
      return std::string("(invalid)");
    }

    std::string Str;
    llvm::raw_string_ostream SS(Str);

    // The macro expansion and spelling pos is identical for file locs.
    SS << "\"" << PLoc.getFilename() << ':' << PLoc.getLine() << ':'
       << PLoc.getColumn() << "\"";

    std::string Result = SS.str();

    // YAML treats backslash as escape, so use forward slashes.
    std::replace(Result.begin(), Result.end(), '\\', '/');

    return Result;
  }

  return std::string("(nonfile)");
}

// Enum string tables.

// FileChangeReason strings.
static const char *const FileChangeReasonStrings[] = {
  "EnterFile", "ExitFile", "SystemHeaderPragma", "RenameFile"
};

// CharacteristicKind strings.
static const char *const CharacteristicKindStrings[] = { "C_User", "C_System",
                                                         "C_ExternCSystem" };

// MacroDirective::Kind strings.
static const char *const MacroDirectiveKindStrings[] = {
  "MD_Define","MD_Undefine", "MD_Visibility"
};

// PragmaIntroducerKind strings.
static const char *const PragmaIntroducerKindStrings[] = { "PIK_HashPragma",
                                                           "PIK__Pragma",
                                                           "PIK___pragma" };

// PragmaMessageKind strings.
static const char *const PragmaMessageKindStrings[] = {
  "PMK_Message", "PMK_Warning", "PMK_Error"
};

// ConditionValueKind strings.
static const char *const ConditionValueKindStrings[] = {
  "CVK_NotEvaluated", "CVK_False", "CVK_True"
};

// Mapping strings.
static const char *const MappingStrings[] = { "0",          "MAP_IGNORE",
                                              "MAP_REMARK", "MAP_WARNING",
                                              "MAP_ERROR",  "MAP_FATAL" };

// PPCallbacksTracker functions.

PPCallbacksTracker::PPCallbacksTracker(clang::Preprocessor &PP, const struct main_opts& _opts,
		std::vector<MacroDefInfo>& mexps): PP(PP), opts(_opts), mexps(mexps) {}

PPCallbacksTracker::~PPCallbacksTracker() {}

// Callback functions.

// Callback invoked whenever a source file is entered or exited.
void PPCallbacksTracker::FileChanged(
    clang::SourceLocation Loc, clang::PPCallbacks::FileChangeReason Reason,
    clang::SrcMgr::CharacteristicKind FileType, clang::FileID PrevFID) {

	DBG(DEBUG_PP, llvm::outs() << "# FileChanged: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("Reason", Reason, FileChangeReasonStrings) << " | "
	<< getArgument("FileType", FileType, CharacteristicKindStrings) << " | "
	<< getArgument("PrevFID", PrevFID) << "\n" );
}

// Callback invoked whenever a source file is skipped as the result
// of header guard optimization.
#if CLANG_VERSION>9
void
PPCallbacksTracker::FileSkipped(const clang::FileEntryRef &SkippedFile,
								const clang::Token &FilenameTok,
								clang::SrcMgr::CharacteristicKind FileType) {
#else
void
PPCallbacksTracker::FileSkipped(const clang::FileEntry &SkippedFile,
                                const clang::Token &FilenameTok,
                                clang::SrcMgr::CharacteristicKind FileType) {
#endif
	DBG(DEBUG_PP, llvm::outs() << "# FileSkipped: "
#if CLANG_VERSION>9
	<< getArgument("ParentFile", SkippedFile) << " | "
#else
	<< getArgument("ParentFile", &SkippedFile) << " | "
#endif
	<< getArgument("FilenameTok", FilenameTok) << " | "
	<< getArgument("FileType", FileType, CharacteristicKindStrings) << "\n" );
}

// Callback invoked whenever an inclusion directive results in a
// file-not-found error.
bool
PPCallbacksTracker::FileNotFound(llvm::StringRef FileName,
                                 llvm::SmallVectorImpl<char> &RecoveryPath) {
	DBG(DEBUG_PP, llvm::outs() << "# FileNotFound: "
	<< getFilePathArgument("FileName", FileName) << "\n" );
	return false;
}

// Callback invoked whenever an inclusion directive of
// any kind (#include, #import, etc.) has been processed, regardless
// of whether the inclusion will actually result in an inclusion.
void PPCallbacksTracker::InclusionDirective(
    clang::SourceLocation HashLoc, const clang::Token &IncludeTok,
    llvm::StringRef FileName, bool IsAngled,
    clang::CharSourceRange FilenameRange, const clang::FileEntry *File,
    llvm::StringRef SearchPath, llvm::StringRef RelativePath,
	const clang::Module *Imported
#if CLANG_VERSION>6
    ,clang::SrcMgr::CharacteristicKind FileType
#endif
	) {
	DBG(DEBUG_PP, llvm::outs() << "# InclusionDirective: "
	<< getArgument("IncludeTok", IncludeTok) << " | "
	<< getFilePathArgument("FileName", FileName) << " | "
	<< getArgument("IsAngled", IsAngled) << " | "
	<< getArgument("FilenameRange", FilenameRange) << " | "
	<< getArgument("File", File) << " | "
	<< getFilePathArgument("SearchPath", SearchPath) << " | "
	<< getFilePathArgument("RelativePath", RelativePath) << " | "
	<< getArgument("Imported", Imported) << "\n" );
}

// Callback invoked whenever there was an explicit module-import
// syntax.
void PPCallbacksTracker::moduleImport(clang::SourceLocation ImportLoc,
                                      clang::ModuleIdPath Path,
                                      const clang::Module *Imported) {
	DBG(DEBUG_PP, llvm::outs() << "# moduleImport: "
	<< getArgument("ImportLoc", ImportLoc) << " | "
	<< getArgument("Path", Path) << " | "
	<< getArgument("Imported", Imported) << "\n" );
}

// Callback invoked when the end of the main file is reached.
// No subsequent callbacks will be made.
void PPCallbacksTracker::EndOfMainFile() {
	DBG(DEBUG_PP, llvm::outs() << "# EndOfMainFile\n");
}

// Callback invoked when a #ident or #sccs directive is read.
void PPCallbacksTracker::Ident(clang::SourceLocation Loc, llvm::StringRef Str) {
	DBG(DEBUG_PP, llvm::outs() << "# Ident: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("Str", Str) << "\n" );
}

// Callback invoked when start reading any pragma directive.
void
PPCallbacksTracker::PragmaDirective(clang::SourceLocation Loc,
                                    clang::PragmaIntroducerKind Introducer) {
	DBG(DEBUG_PP, llvm::outs() << "# PragmaDirective: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("Introducer", Introducer, PragmaIntroducerKindStrings) << "\n" );
}

// Callback invoked when a #pragma comment directive is read.
void PPCallbacksTracker::PragmaComment(clang::SourceLocation Loc,
                                       const clang::IdentifierInfo *Kind,
                                       llvm::StringRef Str) {
	DBG(DEBUG_PP, llvm::outs() << "# PragmaComment: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("Kind", Kind) << " | "
	<< getArgument("Str", Str) << "\n" );
}

// Callback invoked when a #pragma detect_mismatch directive is
// read.
void PPCallbacksTracker::PragmaDetectMismatch(clang::SourceLocation Loc,
                                              llvm::StringRef Name,
                                              llvm::StringRef Value) {
	DBG(DEBUG_PP, llvm::outs() << "# PragmaDetectMismatch: "

			<< getArgument("Loc", Loc) << " | "
	<< getArgument("Name", Name) << " | "
	<< getArgument("Value", Value) << "\n" );
}

// Callback invoked when a #pragma clang __debug directive is read.
void PPCallbacksTracker::PragmaDebug(clang::SourceLocation Loc,
                                     llvm::StringRef DebugType) {
	DBG(DEBUG_PP, llvm::outs() << "# PragmaDebug: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("DebugType", DebugType) << "\n" );
}

// Callback invoked when a #pragma message directive is read.
void PPCallbacksTracker::PragmaMessage(
    clang::SourceLocation Loc, llvm::StringRef Namespace,
    clang::PPCallbacks::PragmaMessageKind Kind, llvm::StringRef Str) {
	DBG(DEBUG_PP, llvm::outs() << "# PragmaMessage: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("Namespace", Namespace) << " | "
	<< getArgument("Kind", Kind, PragmaMessageKindStrings) << " | "
	<< getArgument("Str", Str) << "\n" );
}

// Callback invoked when a #pragma gcc dianostic push directive
// is read.
void PPCallbacksTracker::PragmaDiagnosticPush(clang::SourceLocation Loc,
                                              llvm::StringRef Namespace) {
	DBG(DEBUG_PP, llvm::outs() << "# PragmaDiagnosticPush: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("Namespace", Namespace) << "\n" );
}

// Callback invoked when a #pragma gcc dianostic pop directive
// is read.
void PPCallbacksTracker::PragmaDiagnosticPop(clang::SourceLocation Loc,
                                             llvm::StringRef Namespace) {
	DBG(DEBUG_PP, llvm::outs() << "# PragmaDiagnosticPop: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("Namespace", Namespace) << "\n" );
}

// Callback invoked when a #pragma gcc dianostic directive is read.
void PPCallbacksTracker::PragmaDiagnostic(clang::SourceLocation Loc,
                                          llvm::StringRef Namespace,
                                          clang::diag::Severity Mapping,
                                          llvm::StringRef Str) {
	DBG(DEBUG_PP, llvm::outs() << "# PragmaDiagnostic: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("Namespace", Namespace) << " | "
	<< getArgument("Mapping", (unsigned)Mapping, MappingStrings) << " | "
	<< getArgument("Str", Str) << "\n" );
}

// Called when an OpenCL extension is either disabled or
// enabled with a pragma.
void PPCallbacksTracker::PragmaOpenCLExtension(
    clang::SourceLocation NameLoc, const clang::IdentifierInfo *Name,
    clang::SourceLocation StateLoc, unsigned State) {

	DBG(DEBUG_PP, llvm::outs() << "# PragmaOpenCLExtension: "
	<< getArgument("NameLoc", NameLoc) << " | "
	<< getArgument("Name", Name) << " | "
	<< getArgument("StateLoc", StateLoc) << " | "
	<< getArgument("State", (int)State) << "\n" );
}

// Callback invoked when a #pragma warning directive is read.
void PPCallbacksTracker::PragmaWarning(clang::SourceLocation Loc,
                                       llvm::StringRef WarningSpec,
                                       llvm::ArrayRef<int> Ids) {
	std::string Str;
		  llvm::raw_string_ostream SS(Str);
		  SS << "[";
		  for (int i = 0, e = Ids.size(); i != e; ++i) {
			if (i)
			  SS << ", ";
			SS << Ids[i];
		  }
		  SS << "]";

	DBG(DEBUG_PP, llvm::outs() << "# PragmaWarning: "
	 << getArgument("Loc", Loc) << " | "
	<< getArgument("WarningSpec", WarningSpec) << " | "
	<< getArgument("Ids", SS.str()) << "\n" );
}

// Callback invoked when a #pragma warning(push) directive is read.
void PPCallbacksTracker::PragmaWarningPush(clang::SourceLocation Loc,
                                           int Level) {
	DBG(DEBUG_PP, llvm::outs() << "# PragmaWarningPush: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("Level", Level) << "\n" );
}

// Callback invoked when a #pragma warning(pop) directive is read.
void PPCallbacksTracker::PragmaWarningPop(clang::SourceLocation Loc) {
	DBG(DEBUG_PP, llvm::outs() << "# PragmaWarningPop: "
	<< getArgument("Loc", Loc) << "\n" );
}

void dump_MacroInfo(const clang::MacroInfo * __this, llvm::raw_ostream& Out) {

  if (__this->isFunctionLike()) {
    Out << "(";
    for (unsigned I = 0; I != __this->getNumParams(); ++I) {
      if (I) Out << ", ";
      Out << __this->params()[I]->getName();
    }
    if (__this->isC99Varargs() || __this->isGNUVarargs()) {
      if (__this->getNumParams() && __this->isC99Varargs()) Out << ", ";
      Out << "...";
    }
    Out << ")";
  }

  bool First = true;
  for (const clang::Token &Tok : __this->tokens()) {
    // Leading space is semantically meaningful in a macro definition,
    // so preserve it in the dump output.
    if (First || Tok.hasLeadingSpace())
      Out << " ";
    First = false;

    if (const char *Punc = clang::tok::getPunctuatorSpelling(Tok.getKind()))
      Out << Punc;
    else if (Tok.isLiteral() && Tok.getLiteralData())
      Out << clang::StringRef(Tok.getLiteralData(), Tok.getLength());
    else if (auto *II = Tok.getIdentifierInfo())
      Out << II->getName();
    else
      Out << Tok.getName();
  }
}

// Get the string for the Unexpanded macro instance.
// The soureRange is expected to end at the last token
// for the macro instance, which in the case of a function-style
// macro will be a ')', but for an object-style macro, it
// will be the macro name itself.
static std::string getMacroUnexpandedString(clang::SourceRange Range,
                                            clang::Preprocessor &PP,
                                            llvm::StringRef MacroName,
                                            const clang::MacroInfo *MI) {
  clang::SourceLocation BeginLoc(Range.getBegin());
  const char *BeginPtr = PP.getSourceManager().getCharacterData(BeginLoc);
  size_t Length;
  std::string Unexpanded;
  if (MI->isFunctionLike()) {
    clang::SourceLocation EndLoc(Range.getEnd());
    const char *EndPtr = PP.getSourceManager().getCharacterData(EndLoc) + 1;
    Length = (EndPtr - BeginPtr) + 1; // +1 is ')' width.
  } else
    Length = MacroName.size();
  return llvm::StringRef(BeginPtr, Length).trim().str();
}

// Get the expansion for a macro instance, given the information
// provided by PPCallbacks.
// FIXME: This doesn't support function-style macro instances
// passed as arguments to another function-style macro. However,
// since it still expands the inner arguments, it still
// allows modularize to effectively work with respect to macro
// consistency checking, although it displays the incorrect
// expansion in error messages.
static std::string getMacroExpandedString(clang::Preprocessor &PP,
                                          llvm::StringRef MacroName,
                                          const clang::MacroInfo *MI,
                                          const clang::MacroArgs *Args) {
  std::string Expanded;
  // Walk over the macro Tokens.
  for (const auto &T : MI->tokens()) {
    clang::IdentifierInfo *II = T.getIdentifierInfo();
    int ArgNo = (II && Args ? MI->getParameterNum(II) : -1);
    if (ArgNo == -1) {
      // This isn't an argument, just add it.
      if (II == nullptr)
        Expanded += PP.getSpelling(T); // Not an identifier.
      else {
        // Token is for an identifier.
        std::string Name = II->getName().str();
        // Check for nexted macro references.
        clang::MacroInfo *MacroInfo = PP.getMacroInfo(II);
        if (MacroInfo && (Name != MacroName))
          Expanded += getMacroExpandedString(PP, Name, MacroInfo, nullptr);
        else
          Expanded += Name;
      }
      continue;
    }
    // We get here if it's a function-style macro with arguments.
    const clang::Token *ResultArgToks;
    const clang::Token *ArgTok = Args->getUnexpArgument(ArgNo);
    if (Args->ArgNeedsPreexpansion(ArgTok, PP))
      ResultArgToks = &(const_cast<clang::MacroArgs *>(Args))
          ->getPreExpArgument(ArgNo, PP)[0];
    else
      ResultArgToks = ArgTok; // Use non-preexpanded Tokens.
    // If the arg token didn't expand into anything, ignore it.
    if (ResultArgToks->is(clang::tok::eof))
      continue;
    unsigned NumToks = clang::MacroArgs::getArgLength(ResultArgToks);
    // Append the resulting argument expansions.
    for (unsigned ArgumentIndex = 0; ArgumentIndex < NumToks; ++ArgumentIndex) {
      const clang::Token &AT = ResultArgToks[ArgumentIndex];
      clang::IdentifierInfo *II = AT.getIdentifierInfo();
      if (II == nullptr)
        Expanded += PP.getSpelling(AT); // Not an identifier.
      else {
        // It's an identifier.  Check for further expansion.
        std::string Name = II->getName().str();
        clang::MacroInfo *MacroInfo = PP.getMacroInfo(II);
        if (MacroInfo)
          Expanded += getMacroExpandedString(PP, Name, MacroInfo, nullptr);
        else
          Expanded += Name;
      }
    }
  }
  return Expanded;
}

// Called by Preprocessor::HandleMacroExpandedIdentifier when a
// macro invocation is found.
void
PPCallbacksTracker::MacroExpands(const clang::Token &MacroNameTok,
                                 const clang::MacroDefinition &MacroDefinition,
                                 clang::SourceRange Range,
                                 const clang::MacroArgs *Args) {

	DBG(DEBUG_PP, llvm::outs() << "# MacroExpands: "
	<< getArgument("MacroNameTok", MacroNameTok) << " | "
	<< getArgument("MacroDefinition", MacroDefinition) << " | "
	<< getArgument("Range", Range) << " | "
	<< getArgument("Args", Args) << "\n" );

	clang::DefMacroDirective* MD = MacroDefinition.getLocalDirective();
	if (MD) {
		std::string MacroName = PP.getSpelling(MacroNameTok);
		std::string MacroReplacementDef;
		std::stringstream MacroReplacementDefStream(MacroReplacementDef);
		MacroReplacementDefStream << "__macro_replacement__" << MacroName << "__";
		clang::IdentifierInfo* II = PP.getIdentifierInfo(llvm::StringRef(MacroReplacementDefStream.str()));
		clang::MacroDefinition replacemetMD = PP.getMacroDefinition(II);
		clang::MacroInfo* replacemetMI = 0;
		if (replacemetMD) {
			replacemetMI = replacemetMD.getMacroInfo();
			DBG(DEBUG_PP, replacemetMI->dump(); llvm::errs() << " : " << II->getName().str() << "\n"; );
		}

		clang::MacroInfo* MI = MD->getInfo();

		if (replacemetMI) {
			MI->clearTokens();
			for (auto TI = replacemetMI->tokens_begin(); TI!=replacemetMI->tokens_end(); ++TI) {
				MI->AddTokenToBody((*TI),&PP.getSourceManager());
			}
		}

		DBG(DEBUG_PP, MI->dump(); llvm::errs() << " : " << PP.getSpelling(MacroNameTok) << "\n"; );
	}

	clang::SourceLocation Loc = Range.getBegin();
	// Ignore macro argument expansions.
	if (!Loc.isFileID())
		return;

	clang::IdentifierInfo *II = MacroNameTok.getIdentifierInfo();
	const clang::MacroInfo *MI = MacroDefinition.getMacroInfo();
	std::string MacroName = II->getName().str();

	if (opts.macroExpansionNames.find(MacroName)!=opts.macroExpansionNames.end()) {
		std::string Unexpanded(getMacroUnexpandedString(Range, PP, MacroName, MI));
		std::string Expanded(getMacroExpandedString(PP, MacroName, MI, Args));
		mexps.push_back(MacroDefInfo(MacroName,Expanded,Loc.printToString(PP.getSourceManager())));
	}
}

// Hook called whenever a macro definition is seen.
void
PPCallbacksTracker::MacroDefined(const clang::Token &MacroNameTok,
                                 const clang::MacroDirective *MacroDirective) {

	DBG(DEBUG_PP, llvm::outs() << "# MacroDefined: "
	<< getArgument("MacroNameTok", MacroNameTok) << " | "
	<< getArgument("MacroDirective", MacroDirective) << "\n" );

}

// Hook called whenever a macro #undef is seen.
void PPCallbacksTracker::MacroUndefined(
    const clang::Token &MacroNameTok,
    const clang::MacroDefinition &MacroDefinition,
    const clang::MacroDirective *Undef) {

	DBG(DEBUG_PP, llvm::outs() << "# MacroUndefined: "
	<< getArgument("MacroNameTok", MacroNameTok) << " | "
	<< getArgument("MacroDefinition", MacroDefinition) << "\n" );
}

// Hook called whenever the 'defined' operator is seen.
void PPCallbacksTracker::Defined(const clang::Token &MacroNameTok,
                                 const clang::MacroDefinition &MacroDefinition,
                                 clang::SourceRange Range) {

	DBG(DEBUG_PP, llvm::outs() << "# Defined: "
	<< getArgument("MacroNameTok", MacroNameTok) << " | "
	<< getArgument("MacroDefinition", MacroDefinition) << " | "
	<< getArgument("Range", Range) << "\n" );
}

// Hook called when a source range is skipped.
void PPCallbacksTracker::SourceRangeSkipped(clang::SourceRange Range,
                                            clang::SourceLocation EndifLoc) {

	DBG(DEBUG_PP, llvm::outs() << "# SourceRangeSkipped: "
	<< getArgument("Range", clang::SourceRange(Range.getBegin(), EndifLoc)) << "\n" );
}

// Hook called whenever an #if is seen.
void PPCallbacksTracker::If(clang::SourceLocation Loc,
                            clang::SourceRange ConditionRange,
                            ConditionValueKind ConditionValue) {

	DBG(DEBUG_PP, llvm::outs() << "# If: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("ConditionRange", ConditionRange) << " | "
	<< getArgument("ConditionValue", ConditionValue, ConditionValueKindStrings) << "\n" );
}

// Hook called whenever an #elif is seen.
void PPCallbacksTracker::Elif(clang::SourceLocation Loc,
                              clang::SourceRange ConditionRange,
                              ConditionValueKind ConditionValue,
                              clang::SourceLocation IfLoc) {

	DBG(DEBUG_PP, llvm::outs() << "# Elif: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("ConditionRange", ConditionRange) << " | "
	<< getArgument("ConditionValue", ConditionValue, ConditionValueKindStrings) << " | "
	<< getArgument("IfLoc", IfLoc) << "\n" );
}

// Hook called whenever an #ifdef is seen.
void PPCallbacksTracker::Ifdef(clang::SourceLocation Loc,
                               const clang::Token &MacroNameTok,
                               const clang::MacroDefinition &MacroDefinition) {

	DBG(DEBUG_PP, llvm::outs() << "# Ifdef: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("MacroNameTok", MacroNameTok) << " | "
	<< getArgument("MacroDefinition", MacroDefinition) << "\n" );
}

// Hook called whenever an #ifndef is seen.
void PPCallbacksTracker::Ifndef(clang::SourceLocation Loc,
                                const clang::Token &MacroNameTok,
                                const clang::MacroDefinition &MacroDefinition) {

	DBG(DEBUG_PP, llvm::outs() << "# Ifndef: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("MacroNameTok", MacroNameTok) << " | "
	<< getArgument("MacroDefinition", MacroDefinition) << "\n" );
}

// Hook called whenever an #else is seen.
void PPCallbacksTracker::Else(clang::SourceLocation Loc,
                              clang::SourceLocation IfLoc) {

	DBG(DEBUG_PP, llvm::outs() << "# Else: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("IfLoc", IfLoc) << "\n" );
}

// Hook called whenever an #endif is seen.
void PPCallbacksTracker::Endif(clang::SourceLocation Loc,
                               clang::SourceLocation IfLoc) {

	DBG(DEBUG_PP, llvm::outs() << "# Endif: "
	<< getArgument("Loc", Loc) << " | "
	<< getArgument("IfLoc", IfLoc) << "\n" );
}

// Append a bool argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name, bool Value) {
  if (Value)
	  return "true";
  else
	  return "false";
}

// Append an int argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name, int Value) {
  std::string Str;
  llvm::raw_string_ostream SS(Str);
  SS << Value;
  return SS.str();
}

// Append a string argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name, const char *Value) {
  return Value;
}

// Append a string object argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        llvm::StringRef Value) {
  return Value.str();
}

// Append a token argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        const clang::Token &Value) {
  return PP.getSpelling(Value);
}

// Append an enum argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name, int Value,
                                        const char *const Strings[]) {
  return Strings[Value];
}

// Append a FileID argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name, clang::FileID Value) {
  if (Value.isInvalid()) {
    return "(invalid)";
  }
  const clang::FileEntry *FileEntry =
      PP.getSourceManager().getFileEntryForID(Value);
  if (!FileEntry) {
    return "(getFileEntryForID failed)";
  }
  return getFilePathArgument(Name, FileEntry->getName());
}

#if CLANG_VERSION>9
// Append a FileEntry argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        const clang::FileEntryRef& Value) {
  return getFilePathArgument(Name, Value.getName());
}
#else
// Append a FileEntry argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        const clang::FileEntry *Value) {
  if (!Value) {
    return "(null)";
  }
  return getFilePathArgument(Name, Value->getName());
}
#endif

// Append a SourceLocation argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        clang::SourceLocation Value) {
  if (Value.isInvalid()) {
    return "(invalid)";
  }
  return getSourceLocationString(PP, Value);
}

// Append a SourceRange argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        clang::SourceRange Value) {
  if (Value.isInvalid()) {
    return "(invalid)";
  }
  std::string Str;
  llvm::raw_string_ostream SS(Str);
  SS << "[" << getSourceLocationString(PP, Value.getBegin()) << ", "
     << getSourceLocationString(PP, Value.getEnd()) << "]";
  return SS.str();
}

// Append a CharSourceRange argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        clang::CharSourceRange Value) {
  if (Value.isInvalid()) {
    return "(invalid)";
  }
  return getSourceString(Value).str();
}

// Append a SourceLocation argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        clang::ModuleIdPath Value) {
  std::string Str;
  llvm::raw_string_ostream SS(Str);
  SS << "[";
  for (int I = 0, E = Value.size(); I != E; ++I) {
    if (I)
      SS << ", ";
    SS << "{"
       << "Name: " << Value[I].first->getName() << ", "
       << "Loc: " << getSourceLocationString(PP, Value[I].second) << "}";
  }
  SS << "]";
  return SS.str();
}

// Append an IdentifierInfo argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        const clang::IdentifierInfo *Value) {
  if (!Value) {
    return "(null)";
  }
  return Value->getName().str();
}

// Append a MacroDirective argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        const clang::MacroDirective *Value) {
  if (!Value) {
    return "(null)";
  }
  return MacroDirectiveKindStrings[Value->getKind()];
}

// Append a MacroDefinition argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        const clang::MacroDefinition &Value) {
  std::string Str;
  llvm::raw_string_ostream SS(Str);
  SS << "[";
  bool Any = false;
  if (Value.getLocalDirective()) {
    SS << "(local)";
    Any = true;
  }
  for (auto *MM : Value.getModuleMacros()) {
    if (Any) SS << ", ";
    SS << MM->getOwningModule()->getFullModuleName();
  }
  SS << "]";
  return SS.str();
}

// Append a MacroArgs argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        const clang::MacroArgs *Value) {
  if (!Value) {
    return "(null)";
  }
  std::string Str;
  llvm::raw_string_ostream SS(Str);
  SS << "[";

  // Each argument is is a series of contiguous Tokens, terminated by a eof.
  // Go through each argument printing tokens until we reach eof.
  for (unsigned I = 0; I < Value->getNumMacroArguments(); ++I) {
    const clang::Token *Current = Value->getUnexpArgument(I);
    if (I)
      SS << ", ";
    bool First = true;
    while (Current->isNot(clang::tok::eof)) {
      if (!First)
        SS << " ";
      // We need to be careful here because the arguments might not be legal in
      // YAML, so we use the token name for anything but identifiers and
      // numeric literals.
      if (Current->isAnyIdentifier() ||
          Current->is(clang::tok::numeric_constant)) {
        SS << PP.getSpelling(*Current);
      } else {
        SS << "<" << Current->getName() << ">";
      }
      ++Current;
      First = false;
    }
  }
  SS << "]";
  return SS.str();
}

// Append a Module argument to the top trace item.
std::string PPCallbacksTracker::getArgument(const char *Name,
                                        const clang::Module *Value) {
  if (!Value) {
    return "(null)";
  }
  return Value->Name;
}

// Append a double-quoted argument to the top trace item.
std::string PPCallbacksTracker::getQuotedArgument(const char *Name,
                                              const std::string &Value) {
  std::string Str;
  llvm::raw_string_ostream SS(Str);
  SS << "\"" << Value << "\"";
  return SS.str();
}

// Append a double-quoted file path argument to the top trace item.
std::string PPCallbacksTracker::getFilePathArgument(const char *Name,
                                                llvm::StringRef Value) {
  std::string Path(Value);
  // YAML treats backslash as escape, so use forward slashes.
  std::replace(Path.begin(), Path.end(), '\\', '/');
  return Path;
}

// Get the raw source string of the range.
llvm::StringRef
PPCallbacksTracker::getSourceString(clang::CharSourceRange Range) {
  const char *B = PP.getSourceManager().getCharacterData(Range.getBegin());
  const char *E = PP.getSourceManager().getCharacterData(Range.getEnd());
  return llvm::StringRef(B, E - B);
}
