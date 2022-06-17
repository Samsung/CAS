#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Support/Signals.h>

namespace json {
class JSON;
}

extern int DEBUG_NOTICE;
extern int DEBUG_PP;

#include "dbjson.hpp"
#include "fops.hpp"

struct main_opts {
	bool fops;
	bool fops_all;
	bool call;
	bool assert;
	bool debug;
	bool debug2;
	bool debug3;
	bool debugNotice;
	bool debugPP;
	bool debugDeref;
	bool debugbuild;
	bool debugME;
	bool brk;
	bool include;
	bool addbody;
	bool adddefs;
	bool switchopt;
	bool cstmt;
	bool taint;
	bool tudump;
	bool tudumpcont;
	bool tudumpwithsrc;
	bool onlysrc;
	bool recordLoc;
	bool exit_on_error;
	bool csd;
	bool ptrMEonly;
	std::string JSONRecord;
	std::string BreakFunPlaceholder;
	std::set<std::string> fopsRecords;
	std::map<std::string,std::string> macroReplacementTokens;
	std::set<std::string> macroExpansionNames;
};
