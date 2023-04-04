#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PPCallbacks.h"

#include <set>
using namespace clang;
using RangeMap = std::map<SourceLocation, SourceLocation>;
using MacroExpMap = std::map<SourceLocation, std::string>;
using SkippedMap = std::map<uint64_t, std::set<std::pair<unsigned int, unsigned int>>>;

// helper class registering preprocessor callbacks and collecting data
class MacroHandler{
  public:
    MacroHandler(Preprocessor &PP, bool save_expansions);
    const char *getExpansionText(SourceLocation MacroExpLoc) const;
    auto& getExpansionRanges(){return ExpansionRanges;}
    auto& getSkippedRanges(){return SkippedRanges;}

  private:
    Preprocessor &PP;
    MacroExpMap MacroExpansions;
    RangeMap ExpansionRanges;
    SkippedMap SkippedRanges;
    void onTokenLexed(const Token &Tok);
};