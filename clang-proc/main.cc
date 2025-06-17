#include "main.hpp"
#include "dbjson.hpp"

#include <thread>

//option used in DeclPrinter
bool enable_sa = 0;

using namespace llvm;
using namespace clang;
using namespace clang::tooling;

void clang_prepare(){
  // prevents buffer access issue with multiple threads
  llvm::outs().SetUnbuffered();
  // forces intitialization of clang internal object SttmClassInfo
  ((Stmt*)0)->addStmtClass((Stmt::StmtClass)0);
}

ArgumentsAdjuster getStripWarningsAdjuster() {
  return [](const CommandLineArguments &Args, StringRef /*unused*/) {
    CommandLineArguments AdjustedArgs;
    for (size_t i = 0, e = Args.size(); i < e; ++i) {
      StringRef Arg = Args[i];

      if(Arg.startswith("-Werror")){
        continue;
      }
      AdjustedArgs.push_back(Args[i]);
    }
    AdjustedArgs.push_back("-w");
    return AdjustedArgs;
  };
}

ArgumentsAdjuster getClangStripDependencyFileAdjusterFixed() {
  return [](const CommandLineArguments &Args, StringRef /*unused*/) {
    CommandLineArguments AdjustedArgs;
    for (size_t i = 0, e = Args.size(); i < e; ++i) {
      StringRef Arg = Args[i];

      // Include cases where option is passed by -Wp,
      if(Arg.startswith("-Wp,")){
        Arg = Arg.substr(4);
      }
      // All dependency-file options begin with -M. These include -MM,
      // -MF, -MG, -MP, -MT, -MQ, -MD, and -MMD.
      if (!Arg.startswith("-M")) {
        AdjustedArgs.push_back(Args[i]);
        continue;
      }

      if (Arg == "-MF" || Arg == "-MT" || Arg == "-MQ")
        // These flags take an argument: -MX foo. Skip the next argument also.
        ++i;
    }
    return AdjustedArgs;
  };
}

class DbJSONClassAction : public clang::ASTFrontendAction {
public:
	DbJSONClassAction(size_t fid): file_id(fid) {}
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer (
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
    return std::unique_ptr<clang::ASTConsumer>(
        new DbJSONClassConsumer(Compiler.getASTContext(),file_id,Compiler.getPreprocessor(),opts.save_expansions));
  }
  bool BeginSourceFileAction(CompilerInstance &CI) override {
	  Preprocessor &PP = CI.getPreprocessor();
    return true;
  }

  size_t file_id;
};

template <class Action>
class DBFactory : public clang::tooling::FrontendActionFactory {
public:
  template <class... Args>
  DBFactory(Args ...args): build([=](){return std::unique_ptr<clang::FrontendAction>(new Action(args...));}) {}

	std::unique_ptr<clang::FrontendAction> create() override {
		// return std::unique_ptr<clang::FrontendAction>(action);
    return build();
	}
private:
  std::function<std::unique_ptr<clang::FrontendAction>()> build;
};

llvm::cl::OptionCategory ctCategory("clang-proc options");
cl::list<std::string> MacroReplaceOption("M", cl::cat(ctCategory),cl::ZeroOrMore);
cl::list<std::string> AdditionalDefinesOption("D", cl::cat(ctCategory),cl::ZeroOrMore);
cl::opt<bool> SaveMacroExpansionOption("X", cl::cat(ctCategory));
cl::opt<bool> CallOption("c", cl::cat(ctCategory));
cl::opt<bool> AssertOption("a", cl::cat(ctCategory));
cl::opt<bool> DebugOption("d", cl::cat(ctCategory));
cl::opt<bool> Debug2Option("dd", cl::cat(ctCategory));
cl::opt<bool> Debug3Option("ddd", cl::cat(ctCategory));
cl::opt<bool> DebugNoticeOption("dn", cl::cat(ctCategory));
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
cl::opt<bool> enableStaticAssert("sa", cl::cat(ctCategory));

cl::opt<bool> MultiOption("multi", cl::cat(ctCategory));
cl::opt<unsigned int> ThreadCount("tc",cl::cat(ctCategory),cl::init(0));

std::string builtInIncludePath;
std::map<std::string,std::string> macroReplacementTokens;
struct main_opts opts;

std::atomic_int32_t counter(0);
void run(const CompilationDatabase &compilations,const CommandLineArguments &sources){
  if(unshare(CLONE_FS)){
    llvm::errs()<<"Failed to clone fs\n";
    return;
  }
  int max = sources.size();
  int current = counter++;
  std::string buf;
  llvm::raw_string_ostream dbg(buf);

  while(current<max){
    std::string file = sources.at(current);
    multi::files[current] = file;
    std::string directory = compilations.getCompileCommands(file).at(0).Directory;
    ClangTool Tool(compilations,file);
    IgnoringDiagConsumer Diag;
    // Tool.setDiagnosticConsumer(&Diag);

    if (IncludeOption.getValue()) {
      auto arg = "-I" + builtInIncludePath;
      Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(arg.c_str()));
    }

    for (unsigned i = 0; i != AdditionalIncludePathsOption.size(); ++i) {
      auto arg = "-I" + AdditionalIncludePathsOption[i];
      Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(arg.c_str()));
    }

    for (auto &token : macroReplacementTokens) {
      auto arg = "-D__macro_replacement__" + token.first + "=" + token.second;
      Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(arg.c_str()));
    }
    Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-Wno-implicit-function-declaration"));

    for (unsigned i = 0; i != AdditionalDefinesOption.size(); ++i) {
      auto arg = "-D" + AdditionalDefinesOption[i];
      Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(arg.c_str()));
    }

    Tool.appendArgumentsAdjuster(getStripWarningsAdjuster());
    Tool.appendArgumentsAdjuster(getClangStripDependencyFileAdjusterFixed());

    dbg<<llvm::format_decimal(current,6)<<"  "<<file<<"\n";
    dbg.flush();
    // llvm::errs()<<"LOG: "+buf;

    DBFactory<DbJSONClassAction> Factory(current);
    Tool.run(&Factory);

    llvm::errs()<<"LOG: "+buf;
    buf.clear();

    current = counter++;
    if(!MultiOption.getValue())
      break;
  }
}



int main(int argc, const char **argv)
{
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    // CommonOptionsParser optionsParser(argc, argv, ctCategory);
    auto optionsParser = std::move(CommonOptionsParser::create(argc,argv,ctCategory,cl::OneOrMore).get());
    auto AllFiles = optionsParser.getSourcePathList();
    if(AllFiles.size() == 1 && AllFiles[0] == "__all__")
      AllFiles = optionsParser.getCompilations().getAllFiles();

    enable_sa = enableStaticAssert.getValue();
    opts.call = CallOption.getValue();
    opts.assert = AssertOption.getValue();
    opts.debug = DebugOption.getValue();
    opts.debug2 = Debug2Option.getValue();
    opts.debug3 = Debug3Option.getValue();
    opts.debugNotice = DebugNoticeOption.getValue();
    DEBUG_NOTICE = opts.debugNotice;
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
    opts.save_expansions = opts.addbody && SaveMacroExpansionOption.getValue();

    if (IncludeOption.getValue()) {
      builtInIncludePath = utils::getClangBuiltInIncludePath(argv[0]);
    }

    for (unsigned i = 0; i != MacroReplaceOption.size(); ++i) {
      size_t delim = MacroReplaceOption[i].find(":");
      if (delim==std::string::npos) continue;
      std::string macroReplacementName = MacroReplaceOption[i].substr(0,delim);
      std::string macroReplacementValue = MacroReplaceOption[i].substr(delim+1);
      macroReplacementTokens[macroReplacementName] = macroReplacementValue;
    }

    if(TaintOption.getValue().size()){
      load_database(TaintOption.getValue());
    }

    multi::directory = optionsParser.getCompilations().getCompileCommands(optionsParser.getSourcePathList().front())[0].Directory;
    multi::files.resize(AllFiles.size());
    if(MultiOption.getValue()){
      unsigned int threadcount = ThreadCount.getValue() ?: std::thread::hardware_concurrency();
      if(AllFiles.size()<threadcount)
        threadcount = AllFiles.size();
      llvm::errs()<<"LOG: "<<"Number of threads: "<<threadcount<<'\n';

      clang_prepare();

      std::vector<std::thread> ts;
      while(threadcount--){
        ts.emplace_back(run,std::ref(optionsParser.getCompilations()),std::ref(AllFiles));
      }

      for(auto &t :ts){
        t.join();
      }
      llvm::errs()<<"LOG: Done.\n";

      multi::processDatabase();
      multi::emitDatabase(llvm::outs());
      // multi::report();
    }
    else{ //normal pass for backwards compatibility
      run(std::ref(optionsParser.getCompilations()),std::ref(AllFiles));
      multi::processDatabase();
      multi::emitDatabase(llvm::outs());
    }
    return 0;
}