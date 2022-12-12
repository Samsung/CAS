#include "main.hpp"

//option used in DeclPrinter
bool enable_sa = 0;

class IndexerPPCallbacks : public PPCallbacks {

	SourceManager& SM;
public:
	explicit IndexerPPCallbacks(SourceManager& sm): SM(sm) {
	}

	void Ifdef(SourceLocation Loc, const Token &MacroNameTok,const MacroDefinition &MD) override {
		//printf("@IndexerPPCallbacks:Ifdef()\n");
	}

};

class DbJSONClassAction : public clang::ASTFrontendAction {
public:
	DbJSONClassAction(const std::string* sourceFile, const struct main_opts& opts, const std::string* directory):
		_sourceFile(sourceFile), _opts(opts), _directory(directory) {}
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer (
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
    return std::unique_ptr<clang::ASTConsumer>(
        new DbJSONClassConsumer(Compiler.getASTContext(),_sourceFile,_directory,_opts,Compiler.getPreprocessor()));
  }
  bool BeginSourceFileAction(CompilerInstance &CI) override {
	  Preprocessor &PP = CI.getPreprocessor();
	  PP.addPPCallbacks(std::make_unique<IndexerPPCallbacks>(CI.getSourceManager()));
	  return true;
  }

  const std::string* _sourceFile;
  const struct main_opts& _opts;
  const std::string* _directory;
};

class FOPSClassAction : public clang::ASTFrontendAction {
public:
	FOPSClassAction(const std::string* sourceFile, const struct main_opts& opts, const std::string* directory):
	_sourceFile(sourceFile), _opts(opts), _directory(directory) {}
virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
  clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
  return std::unique_ptr<clang::ASTConsumer>(
	  new FOPSClassConsumer(Compiler.getASTContext(),_sourceFile,_directory,_opts));
}
bool BeginSourceFileAction(CompilerInstance &CI) override {
  Preprocessor &PP = CI.getPreprocessor();
  PP.addPPCallbacks(std::make_unique<IndexerPPCallbacks>(CI.getSourceManager()));
  return true;
}

const std::string* _sourceFile;
const struct main_opts& _opts;
const std::string* _directory;
};

template <class Action>
class DBFactory : public clang::tooling::FrontendActionFactory {
public:
	DBFactory(const std::string* sourceFile, const struct main_opts &opts, const std::string* directory)
		: _sourceFile(sourceFile), _opts(opts), _directory(directory) {}
	std::unique_ptr<clang::FrontendAction> create() override {
		return std::unique_ptr<clang::FrontendAction>( new Action(_sourceFile,_opts,_directory) );
	}
private:
	const std::string* _sourceFile;
  const struct main_opts _opts;
  const std::string* _directory;
};

using namespace std;
using namespace llvm;
using namespace clang;
using namespace clang::tooling;

#include <stdio.h>

int main(int argc, const char **argv)
{
	llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
	llvm::cl::OptionCategory ctCategory("clang-proc options");
    cl::opt<std::string> JSONRecordOption("R", cl::cat(ctCategory));
    cl::list<std::string> MacroReplaceOption("M", cl::cat(ctCategory));
    MacroReplaceOption.setNumOccurrencesFlag(llvm::cl::ZeroOrMore);
    cl::list<std::string> SaveMacroExpansionOption("X", cl::cat(ctCategory));
    SaveMacroExpansionOption.setNumOccurrencesFlag(llvm::cl::ZeroOrMore);
    cl::list<std::string> AdditionalDefinesOption("D", cl::cat(ctCategory));
    AdditionalDefinesOption.setNumOccurrencesFlag(llvm::cl::ZeroOrMore);
    cl::opt<bool> FopsOption("f", cl::cat(ctCategory));
    cl::opt<bool> CallOption("c", cl::cat(ctCategory));
    cl::opt<bool> AssertOption("a", cl::cat(ctCategory));
    cl::opt<bool> DebugOption("d", cl::cat(ctCategory));
    cl::opt<bool> Debug2Option("dd", cl::cat(ctCategory));
    cl::opt<bool> Debug3Option("ddd", cl::cat(ctCategory));
    cl::opt<bool> DebugNoticeOption("dn", cl::cat(ctCategory));
    cl::opt<bool> DebugPPOption("dp", cl::cat(ctCategory));
    cl::opt<bool> DebugDereference("du", cl::cat(ctCategory));
    cl::opt<bool> DebugBuildString("db", cl::cat(ctCategory));
    cl::opt<bool> DebugME("dme", cl::cat(ctCategory));
    cl::opt<std::string> BreakFunctionPlaceholder("B", cl::cat(ctCategory));
    cl::opt<bool> BreakOption("k", cl::cat(ctCategory));
    cl::opt<bool> IncludeOption("i", cl::cat(ctCategory));
    cl::list<std::string> AdditionalIncludePathsOption("A", cl::cat(ctCategory));
    cl::opt<bool> BodyOption("b", cl::cat(ctCategory));
    cl::opt<bool> DefsOption("F", cl::cat(ctCategory));
    cl::opt<bool> SwitchOption("s", cl::cat(ctCategory));
    cl::opt<bool> CSOption("t", cl::cat(ctCategory));
    cl::opt<std::string> TaintOption("pt", cl::cat(ctCategory),cl::desc("Generate taint info for functions' parameters"),cl::value_desc("database"));
    cl::opt<bool> TUDumpOption("u", cl::cat(ctCategory));
    cl::opt<bool> TUDumpContOption("U", cl::cat(ctCategory));
    cl::opt<bool> RecordLocOption("L", cl::cat(ctCategory));
    cl::opt<bool> TUDumpWithSrcOption("uS", cl::cat(ctCategory));
    cl::opt<bool> OnlySrcOption("S", cl::cat(ctCategory));
    cl::opt<bool> ExitOnErrorOption("E", cl::cat(ctCategory));
    cl::opt<bool> CustomStructDefs("csd",cl::cat(ctCategory));
    cl::opt<bool> ptrMEOption("pm", cl::cat(ctCategory));
    cl::opt<bool> enableStaticAssert("sa", cl::cat(ctCategory));


    // CommonOptionsParser optionsParser(argc, argv, ctCategory);
    auto optionsParser = std::move(CommonOptionsParser::create(argc,argv,ctCategory,cl::OneOrMore).get());

    if(TaintOption.getValue().size()){
      load_database(TaintOption.getValue());
    }
    
    for(auto &sourceFile : optionsParser.getSourcePathList())
    {
        if(utils::fileExists(sourceFile) == false)
        {
            llvm::errs() << "File: " << sourceFile << " does not exist!\n";
            return -1;
        }
        ClangTool Tool(optionsParser.getCompilations(),sourceFile);
        auto directory = optionsParser.getCompilations().getCompileCommands(sourceFile).front().Directory;

        if (IncludeOption.getValue()) {
          auto arg = "-I" + utils::getClangBuiltInIncludePath(argv[0]);
          Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(arg.c_str()));
        }

        for (unsigned i = 0; i != AdditionalIncludePathsOption.size(); ++i) {
          auto arg = "-I" + AdditionalIncludePathsOption[i];
          Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(arg.c_str()));
        }

        enable_sa = enableStaticAssert.getValue();
        struct main_opts opts;
        opts.JSONRecord = JSONRecordOption.getValue();
        opts.BreakFunPlaceholder = BreakFunctionPlaceholder.getValue();
        opts.call = CallOption.getValue();
        opts.assert = AssertOption.getValue();
        opts.debug = DebugOption.getValue();
        opts.debug2 = Debug2Option.getValue();
        opts.debug3 = Debug3Option.getValue();
        opts.debugNotice = DebugNoticeOption.getValue();
        DEBUG_NOTICE = opts.debugNotice;
        opts.debugPP = DebugPPOption.getValue();
        DEBUG_PP = opts.debugPP;
        opts.debugDeref = DebugDereference.getValue();
        opts.debugbuild = DebugBuildString.getValue();
        opts.debugME = DebugME.getValue();
        opts.brk = BreakOption.getValue();
        opts.addbody = BodyOption.getValue();
        opts.switchopt = SwitchOption.getValue();
        opts.adddefs = DefsOption.getValue();
        opts.cstmt = CSOption.getValue();
        opts.taint = TaintOption.getValue().size();
        opts.tudump = TUDumpOption.getValue();
        opts.tudumpcont = TUDumpContOption.getValue();
        opts.tudumpwithsrc = TUDumpWithSrcOption.getValue();
        opts.onlysrc = OnlySrcOption.getValue();
        opts.recordLoc = RecordLocOption.getValue();
        opts.exit_on_error = ExitOnErrorOption.getValue();
        opts.csd = CustomStructDefs.getValue();
        opts.ptrMEonly = ptrMEOption.getValue();
        if (opts.JSONRecord=="*") {
        	opts.fops_all = true;
        }
        else {
        	opts.fops_all = false;
        }
        std::string fopsRecord;
        std::stringstream recordList(opts.JSONRecord);
        while(std::getline(recordList, fopsRecord, ':'))
        {
        	opts.fopsRecords.insert(fopsRecord);
        }
        opts.fops = FopsOption.getValue();

        for (unsigned i = 0; i != MacroReplaceOption.size(); ++i) {
        	size_t delim = MacroReplaceOption[i].find(":");
        	if (delim==std::string::npos) continue;
        	std::string macroReplacementName = MacroReplaceOption[i].substr(0,delim);
        	std::string macroReplacementValue = MacroReplaceOption[i].substr(delim+1);
        	opts.macroReplacementTokens[macroReplacementName] = macroReplacementValue;
        	std::string MacroReplacementDef;
        	std::stringstream MacroReplacementDefStream(MacroReplacementDef);
        	MacroReplacementDefStream << "-D__macro_replacement__" << macroReplacementName << "__=" << macroReplacementValue << "";
        	Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(MacroReplacementDefStream.str().c_str()));
                Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-implicit-function-declaration"));
        }

        for (unsigned i = 0; i != SaveMacroExpansionOption.size(); ++i) {
        	opts.macroExpansionNames.insert(SaveMacroExpansionOption[i]);
        }

        for (unsigned i = 0; i != AdditionalDefinesOption.size(); ++i) {
        	std::string def = AdditionalDefinesOption[i];
        	std::string AdditionalDef;
        	std::stringstream AdditionalDefStream(AdditionalDef);
        	AdditionalDefStream << "-D" << def;
        	Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(AdditionalDefStream.str().c_str()));
        }

        if (opts.fops) {
          DBFactory<FOPSClassAction> Factory(&sourceFile,opts,&directory);
		      Tool.run(&Factory);
        }
        else {
          DBFactory<DbJSONClassAction> Factory(&sourceFile,opts,&directory);
		      Tool.run(&Factory);
        }
        break;
    }
    return 0;
}
