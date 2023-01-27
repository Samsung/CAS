#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PPCallbacks.h"

using namespace clang;
using RangeMap = std::map<SourceLocation, SourceLocation>;
using MacroExpMap = std::map<SourceLocation, std::string>;

// helper class registering preprocessor callbacks and collecting data
class MacroHandler{
  public:
    MacroHandler(Preprocessor &PP, bool save_expansions);
    std::string getExpansionText(SourceLocation MacroExpLoc) const;
    auto& getRanges(){return ExpansionRanges;}
  private:
    Preprocessor &PP;
    MacroExpMap MacroExpansions;
    RangeMap ExpansionRanges;
    void onTokenLexed(const Token &Tok);
};