#include "MacroHandler.h"


class MacroRedefCallbacks : public PPCallbacks{
  public:
    MacroRedefCallbacks(Preprocessor &PP) : PP(PP) {}
  private:
    Preprocessor &PP;
    
    // replaces macro with provided custom definition
    void MacroDefined(const clang::Token &MacroNameTok, const clang::MacroDirective *MD) {
      auto OrigII = MacroNameTok.getIdentifierInfo();
      auto replacement = "__macro_replacement__" + OrigII->getName().str();
      auto &Identifiers = PP.getIdentifierTable();
      auto iterII = Identifiers.find(replacement);
      if(iterII != Identifiers.end()){
        auto ReplII = iterII->second;
        auto ReplMacroDef = PP.getMacroDefinition(ReplII);
        auto ReplMI = ReplMacroDef.getMacroInfo();
        PP.appendDefMacroDirective(OrigII,ReplMI);
      }
    }
};

class MacroExpCallbacks : public PPCallbacks{
  public:
    MacroExpCallbacks(Preprocessor &PP, RangeMap &ExpansionRanges, MacroExpMap &MacroExpansions)
      : PP(PP), ExpansionRanges(ExpansionRanges), MacroExpansions(MacroExpansions) {}
  private:
    Preprocessor &PP;
    RangeMap &ExpansionRanges;
    MacroExpMap &MacroExpansions;


    // collects location ranges of macro expansions
    void MacroExpands(const Token &MacroNameTok, const MacroDefinition &MD, SourceRange Range, const MacroArgs *Args) {
      //ignore directives and nested macros
      if(PP.isParsingIfOrElifDirective() || !PP.getCurrentLexer()) return;
      auto &SM = PP.getSourceManager();
      auto ExpCharRange = Lexer::getAsCharRange(SM.getExpansionRange(Range),SM,PP.getLangOpts());
      auto BeginLoc = ExpCharRange.getBegin();
      auto EndLoc = ExpCharRange.getEnd();
      auto it = ExpansionRanges.find(BeginLoc);
      if(it!= ExpansionRanges.end()){
        if(SM.isBeforeInTranslationUnit(it->second,EndLoc))
          it->second = EndLoc;
      }
      else{
        ExpansionRanges.emplace(BeginLoc,EndLoc);
        MacroExpansions.emplace(BeginLoc,"");
      }
      return;
    }

    //collect location ranges of if directives
    void If(SourceLocation Loc, SourceRange ConditionRange, ConditionValueKind ConditionValue) {
      auto &SM = PP.getSourceManager();
      auto BeginLoc = SM.getExpansionLoc(Loc).getLocWithOffset(-1);
      auto ExpCharRange = Lexer::getAsCharRange(SM.getExpansionRange(ConditionRange),SM,PP.getLangOpts());
      auto EndLoc = ExpCharRange.getEnd();
      auto it = ExpansionRanges.find(BeginLoc);
      if(it!= ExpansionRanges.end()){
        if(SM.isBeforeInTranslationUnit(it->second,EndLoc))
          it->second = EndLoc;
      }
      else{
        ExpansionRanges.emplace(BeginLoc,EndLoc);
        MacroExpansions.emplace(BeginLoc,"");
      }
      return;
    }

    void Ifdef(SourceLocation Loc, const Token &MacroNameTok, const MacroDefinition &MD) {
      auto &SM = PP.getSourceManager();
      auto BeginLoc = SM.getExpansionLoc(Loc).getLocWithOffset(-1);
      auto EndLoc = MacroNameTok.getEndLoc();
      auto it = ExpansionRanges.find(BeginLoc);
      if(it!= ExpansionRanges.end()){
        if(SM.isBeforeInTranslationUnit(it->second,EndLoc))
          it->second = EndLoc;
      }
      else{
        ExpansionRanges.emplace(BeginLoc,EndLoc);
        MacroExpansions.emplace(BeginLoc,"");
      }
      return;
    }

    void Ifndef(SourceLocation Loc, const Token &MacroNameTok, const MacroDefinition &MD) {
      auto &SM = PP.getSourceManager();
      auto BeginLoc = SM.getExpansionLoc(Loc).getLocWithOffset(-1);
      auto EndLoc = MacroNameTok.getEndLoc();
      auto it = ExpansionRanges.find(BeginLoc);
      if(it!= ExpansionRanges.end()){
        if(SM.isBeforeInTranslationUnit(it->second,EndLoc))
          it->second = EndLoc;
      }
      else{
        ExpansionRanges.emplace(BeginLoc,EndLoc);
        MacroExpansions.emplace(BeginLoc,"");
      }
      return;
    }

    void Endif(SourceLocation Loc, SourceLocation IfLoc) {
      auto &SM = PP.getSourceManager();
      auto ExpCharRange = Lexer::getAsCharRange(SM.getExpansionRange(Loc),SM,PP.getLangOpts());
      auto BeginLoc = ExpCharRange.getBegin().getLocWithOffset(-1);
      auto EndLoc = ExpCharRange.getEnd();
      ExpansionRanges.emplace(BeginLoc,EndLoc);
      MacroExpansions.emplace(BeginLoc,"");
    }

    void SourceRangeSkipped(SourceRange Range, SourceLocation EndifLoc) {
      auto &SM = PP.getSourceManager();
      auto ExpCharRange = Lexer::getAsCharRange(SM.getExpansionRange(Range),SM,PP.getLangOpts());
      auto BeginLoc = ExpCharRange.getBegin();
      auto EndLoc = ExpCharRange.getEnd().getLocWithOffset(FTDB_COMPAT_MACRO_OFFSET);
      //remove endif if contained
      ExpansionRanges.erase(ExpansionRanges.lower_bound(BeginLoc),ExpansionRanges.upper_bound(EndLoc));
        
      auto it = ExpansionRanges.find(BeginLoc);
      if(it!= ExpansionRanges.end()){
        if(SM.isBeforeInTranslationUnit(it->second,EndLoc))
          it->second = EndLoc;
      }
      else{
        ExpansionRanges.emplace(BeginLoc,EndLoc);
        MacroExpansions.emplace(BeginLoc,"");
      }
      return;
    }
};

class SkippedRangesCallbacks : public PPCallbacks{
  public:
    SkippedRangesCallbacks(Preprocessor &PP, SkippedMap &SkippedRanges) : PP(PP), SkippedRanges(SkippedRanges) {}
  private:
    Preprocessor &PP;
    SkippedMap &SkippedRanges;

    // tracks source ranges skipped by preprocessor (behind ifdef)
    void SourceRangeSkipped(SourceRange Range, SourceLocation EndifLoc) {
      auto &SM = PP.getSourceManager();
      auto Begin = Range.getBegin();
      auto uniqueID = SM.getFileEntryForID(SM.getFileID(Begin))->getUniqueID().getFile();
      SkippedRanges[uniqueID].emplace(SM.getFileOffset(Begin),SM.getFileOffset(Range.getEnd()));
    }
};

MacroHandler::MacroHandler(Preprocessor &PP,bool save_expansions) : PP(PP) {
  PP.addPPCallbacks(std::make_unique<MacroRedefCallbacks>(PP));
  PP.addPPCallbacks(std::make_unique<SkippedRangesCallbacks>(PP,SkippedRanges));
  if(save_expansions){
    PP.addPPCallbacks(std::make_unique<MacroExpCallbacks>(PP,ExpansionRanges,MacroExpansions));
    PP.setTokenWatcher([this](const Token &Tok){onTokenLexed(Tok);});
  }
}

const char *MacroHandler::getExpansionText(SourceLocation MacroExpLoc) const {
  const auto it = MacroExpansions.find(MacroExpLoc);
  if (it == MacroExpansions.end())
    return nullptr;
  return it->second.c_str();
}

static std::string TokenToString(const Preprocessor &PP, Token Tok) {
  assert(Tok.isNot(tok::raw_identifier));
  std::string tok_str;
  llvm::raw_string_ostream OS(tok_str);

  // Ignore annotation tokens
  if (Tok.isAnnotation())
    return "";

  if (IdentifierInfo *II = Tok.getIdentifierInfo()) {
    OS << II->getName() << ' ';
  } else {
    SmallString<40> tmp;
    OS << PP.getSpelling(Tok,tmp);
  }
  return OS.str();
}

void MacroHandler::onTokenLexed(const Token &Tok){
  auto &SM = PP.getSourceManager();
  SourceLocation SLoc = Tok.getLocation();

  // not a macro expansion token
  if (SLoc.isFileID())
    return;

  SourceLocation ELoc = SM.getExpansionLoc(SLoc);
  auto it = MacroExpansions.find(ELoc);
  if(it!= MacroExpansions.end())
    it->second.append(TokenToString(PP, Tok));
  else
    assert(0 && "Entry should already exist at this point\n");
    MacroExpansions.emplace(ELoc,TokenToString(PP, Tok));
}