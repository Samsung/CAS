#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wpessimizing-move"

#include "utils.h"

namespace utils
{

std::vector<std::string> getSyntaxOnlyToolArgs(const std::vector<std::string> &extraArgs, llvm::StringRef fileName)
{
    std::vector<std::string> args;

    args.push_back("clang-tool");
    args.push_back("-fsyntax-only");

    args.insert(args.end(), extraArgs.begin(), extraArgs.end());
    args.push_back(fileName.str());

    return args;
}

bool fileExists(const std::string &file)
{
    return std::ifstream(file).good();
}

std::vector<std::string> getCompileArgs(const std::vector<clang::tooling::CompileCommand> &compileCommands)
{
    std::vector<std::string> compileArgs;

    for(auto &cmd : compileCommands)
    {
        for(auto &arg : cmd.CommandLine)
            compileArgs.push_back(arg);
    }

    if(compileArgs.empty() == false)
    {
        compileArgs.erase(begin(compileArgs));
        compileArgs.pop_back();
    }

    return compileArgs;
}

std::string getSourceCode(const std::string &sourceFile)
{
    std::string sourcetxt = "";
    std::string temp = "";

    std::ifstream file(sourceFile);

    while(std::getline(file, temp))
        sourcetxt += temp + "\n";

    return sourcetxt;
}

std::string getClangBuiltInIncludePath(const std::string &fullCallPath)
{
    return std::string(CLANG_BUILTIN_INCLUDE_PATH);
}

}
