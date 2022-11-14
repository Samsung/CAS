#ifndef UTILS_HPP
#define UTILS_HPP

#include <fstream>
#include <vector>
#include <string>

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>

#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Frontend/FrontendActions.h>

namespace utils
{
    std::vector<std::string> getSyntaxOnlyToolArgs(const std::vector<std::string> &ExtraArgs, llvm::StringRef FileName);

    bool fileExists(const std::string &file);
    std::vector<std::string> getCompileArgs(const std::vector<clang::tooling::CompileCommand> &compileCommands);
    std::string getSourceCode(const std::string &sourceFile);

    std::string getClangBuiltInIncludePath(const std::string &fullCallPath);
}

#endif
