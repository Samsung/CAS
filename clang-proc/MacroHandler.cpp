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
    MacroExpCallbacks(Preprocessor &PP, RangeMap &ExpansionRanges) : PP(PP), ExpansionRanges(ExpansionRanges) {}
  private:
    Preprocessor &PP;
    RangeMap &ExpansionRanges;

    // collects location ranges of macro expansions
    void MacroExpands(const Token &MacroNameTok, const MacroDefinition &MD, SourceRange Range, const MacroArgs *Args) {
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
      }
      return;
    }
};

MacroHandler::MacroHandler(Preprocessor &PP,bool save_expansions) : PP(PP) {
  PP.addPPCallbacks(std::make_unique<MacroRedefCallbacks>(PP));
  if(save_expansions){
    PP.addPPCallbacks(std::make_unique<MacroExpCallbacks>(PP,ExpansionRanges));
    PP.setTokenWatcher([this](const Token &Tok){onTokenLexed(Tok);});
  }
}

std::string MacroHandler::getExpansionText(SourceLocation MacroExpLoc) const {
  const auto it = MacroExpansions.find(MacroExpLoc);
  if (it == MacroExpansions.end())
    return {};
  return it->second;
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
    MacroExpansions.emplace(ELoc,TokenToString(PP, Tok));
}