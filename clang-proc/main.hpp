#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Support/Signals.h>

namespace json {
class JSON;
}

extern int DEBUG_NOTICE;
// extern int DEBUG_PP;

#include "dbjson.hpp"
#include "fops.hpp"
#include "compat.h"

namespace multi{
  extern std::string directory;
  extern std::vector<std::string> files;
  void registerVar(DbJSONClassVisitor::VarData&);
  void registerType(DbJSONClassVisitor::TypeData&);
  void registerFuncDecl(DbJSONClassVisitor::FuncDeclData&);
  void registerFuncInternal(DbJSONClassVisitor::FuncData&);
  void registerFunc(DbJSONClassVisitor::FuncData&);
  void registerFops(DbJSONClassVisitor::FopsData&);
  void handleRefs(void *rv, std::vector<int> rIds,std::vector<std::string> rDef);
  void processDatabase();
  void emitDatabase(llvm::raw_ostream&);
  void report();
}

struct main_opts {
	bool fops;
	bool fops_all;
	bool call;
	bool assert;
	bool debug;
	bool debug2;
	bool debug3;
	bool debugNotice;
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
	bool save_expansions;
	std::string JSONRecord;
	std::string BreakFunPlaceholder;
	std::set<std::string> fopsRecords;
};

extern main_opts opts;
