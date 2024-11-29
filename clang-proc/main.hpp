#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Support/Signals.h>

namespace json {
class JSON;
}

extern int DEBUG_NOTICE;

struct main_opts {
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
	bool save_expansions;
};

extern main_opts opts;
