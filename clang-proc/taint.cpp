#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h" 
#include "clang/Tooling/Tooling.h"
#include <map>
#include <set>

namespace json {
class JSON;
}

#include "dbjson.hpp"

using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace ast_matchers;

  typedef std::string name_t;
  typedef int to_index;
  typedef int from_index;
  typedef std::multimap<name_t,std::pair<to_index,from_index>> db_t;
  typedef std::vector<std::pair<to_index,from_index>> data_t;
  typedef std::unordered_map<name_t,data_t> ndb_t;

  ndb_t tracked_functions; 
  ndb_t tracked_functions_regex;

//database
  int load_taint_function_database(std::string filepath, ndb_t &exact, ndb_t &regex);
  //loads database from file (called from main)
  void load_database(std::string filepath){
    load_taint_function_database(filepath, tracked_functions,tracked_functions_regex);
  }

  //get taint info for function from database
  const data_t* match_database(name_t name){
    //fullname
    for(auto &f : tracked_functions){
      if(name.compare(f.first) == 0)
        return &f.second;
    }
    //regex
    for(auto &f : tracked_functions_regex){
      if(name.compare(0,f.first.length(),f.first) == 0)
        return &f.second;
    }
    //not found
    return nullptr;
  }

//declaration counting
  DeclarationMatcher DeclCountMatcher =
    functionDecl(
      forEachDescendant(
        varDecl(
          unless(
            parmVarDecl()
          )
        ).bind("v")
      )
    )
  ;
  //returns number of variable declarations within funtion's body
  //note, if passed decl has no body, will return 0
  int internal_declcount(const FunctionDecl *F){
    class : public MatchFinder::MatchCallback{
      public:
        virtual void run(const MatchFinder::MatchResult &Result) {
          ASTContext &Context = *Result.Context;
          SourceManager &SM = *Result.SourceManager;
          if(auto v = Result.Nodes.getNodeAs<VarDecl>("v")){
            count++;
          }
        }

        int count = 0;
    }Callback;
    
    MatchFinder ArgFinder;
    ArgFinder.addMatcher(DeclCountMatcher, &Callback);
    ArgFinder.match(*F,F->getASTContext());
    return Callback.count;
  }


//param tainting
    //matches initializations with given variable
  //all variables initialized with expression referencing given variable will be matched
  DeclarationMatcher InitMatcher(const Decl *arg){
    return
      functionDecl(
        forEachDescendant(
          varDecl(
            hasDescendant(
              declRefExpr(
                to(
                  varDecl(
                    equalsNode(arg)
                  ).bind("source_arg")
                )
              )
            )
          ).bind("init_arg")
        )
      ).bind("f")
    ;
  }

  //matches assignments with given variable
  //all variables which are assigned to with expression referencing given variable will be matched
  DeclarationMatcher AssignMatcher(const Decl *arg){
    return
      functionDecl(
        forEachDescendant(
          binaryOperator(
            anyOf(
              hasOperatorName("="),
              hasOperatorName("+="),
              hasOperatorName("-="),
              hasOperatorName("*="),
              hasOperatorName("/="),
              hasOperatorName("%="),
              hasOperatorName("<<="),
              hasOperatorName(">>="),
              hasOperatorName("&="),
              hasOperatorName("^="),
              hasOperatorName("|=")
            ),
            hasLHS(
              anyOf(
                declRefExpr(
                  to(
                    varDecl().bind("assign_arg")
                  )
                ),
                hasDescendant(
                  declRefExpr(
                    to(
                      varDecl().bind("assign_arg")
                    )
                  )
                )
              )
            ),
            hasRHS(
              hasDescendant(
                declRefExpr(
                  to(
                    varDecl(
                      equalsNode(arg)
                    ).bind("source_arg")
                  )
                )
              )

            )
          )
        )
      ).bind("f")
    ;
  }

  //matches call expressions with given variable used in any argument
  DeclarationMatcher CallMatcher(const Decl *arg){
    return
      functionDecl(
        forEachDescendant(
          callExpr(
            callee(
              functionDecl().bind("call_func")
            ),
            hasAnyArgument(
              anyOf(
                declRefExpr(
                  to(
                    varDecl(
                      equalsNode(arg)
                    ).bind("source_arg")
                  )
                ),
                hasDescendant(
                  declRefExpr(
                    to(
                      varDecl(
                        equalsNode(arg)
                      ).bind("source_arg")
                    )
                  )
                )
              )
            )
          ).bind("call")
        )
      ).bind("f")
    ;
  }
  //matches variables passed to function as certain argument
  StatementMatcher ArgMatcher(to_index to_index, from_index from_index, const Decl *arg){
    return
      callExpr(
        hasArgument(
          to_index,
          anyOf(
            declRefExpr(
              to(
                varDecl().bind("function_arg")
              )
            ),
            hasDescendant(
              declRefExpr(
                to(
                  varDecl().bind("function_arg")
                )
              )
            )
          )
        ),
        hasArgument(
          from_index,
          anyOf(
            declRefExpr(
              to(
                varDecl(
                  equalsNode(arg)
                )
              )
            ),
            hasDescendant(
              declRefExpr(
                to(
                  varDecl(
                    equalsNode(arg)
                  )
                )
              )
            )
          )
        )
      )
    ;
  }

  void taint_params(const FunctionDecl *f, DbJSONClassVisitor::FuncData &func_data){
	 DbJSONClassVisitor::taintdata_t& data = func_data.taintdata;
	 for(auto param : f->parameters()){
      int param_index = param->getFunctionScopeIndex();

      class : public MatchFinder::MatchCallback{
        public:
          virtual void run(const MatchFinder::MatchResult &Result) {
            ASTContext &Context = *Result.Context;
            SourceManager &SM = *Result.SourceManager;
            
            if(auto local_arg = Result.Nodes.getNodeAs<VarDecl>("init_arg")){
              if(!local_arg->isDefinedOutsideFunctionOrMethod() && !already(local_arg)) 
                tainted.push_back(local_arg);
            }

            if(auto local_arg = Result.Nodes.getNodeAs<VarDecl>("assign_arg")){
              if(!local_arg->isDefinedOutsideFunctionOrMethod() && !already(local_arg)) 
                tainted.push_back(local_arg);
            }

            if(auto local_arg = Result.Nodes.getNodeAs<VarDecl>("function_arg")){
              if(!local_arg->isDefinedOutsideFunctionOrMethod() && !already(local_arg)) 
                tainted.push_back(local_arg);
            }


            if(auto call_func = Result.Nodes.getNodeAs<FunctionDecl>("call_func")){
              auto source_arg = Result.Nodes.getNodeAs<VarDecl>("source_arg");
              auto call_expr = Result.Nodes.getNodeAs<CallExpr>("call");
              const data_t *data = match_database(call_func->getNameAsString());
              if(!data) return;
              for(auto taint : *data){
                MatchFinder ArgFinder;
                //normal function taint
                if(taint.first && taint.second){
                  ArgFinder.addMatcher(ArgMatcher(taint.first-1,taint.second-1,source_arg),this);
                  ArgFinder.match(*call_expr,Context);
                }
                //to variadic argument
                else if(taint.first == 0){
                  if(!call_func->isVariadic()) errs()<<call_func->getNameAsString()<<" IS NOT VARIADIC!\n";
                  for(unsigned i = call_func->getNumParams();i<call_expr->getNumArgs();i++){
                    ArgFinder.addMatcher(ArgMatcher(i,taint.second-1,source_arg),this);
                  }
                  ArgFinder.match(*call_expr,Context);
                }
                //from variadic argument
                else if(taint.second == 0){
                  if(!call_func->isVariadic()) errs()<<call_func->getNameAsString()<<" IS NOT VARIADIC!\n";
                  for(unsigned i = call_func->getNumParams();i<call_expr->getNumArgs();i++){
                    ArgFinder.addMatcher(ArgMatcher(taint.first-1,i,source_arg),this);
                  }
                  ArgFinder.match(*call_expr,Context);
                }
              }


            }

          }
          std::vector<const VarDecl*> tainted = {};

        private:
          bool already(const VarDecl* new_taint){
            for(auto taint : this->tainted){
              if(new_taint == taint) return true;
              if(new_taint->getNameAsString()==taint->getNameAsString() &&
                 new_taint->getLocation().printToString(taint->getASTContext().getSourceManager())==taint->getLocation().printToString(taint->getASTContext().getSourceManager())){
                  //  errs()<<new_taint->getNameAsString()<<"\n";
                   return true;
                 }
            }
            return false;
          }
      }Callback;    
      Callback.tainted.push_back(param);

      int depth = 0;
      size_t index = 0;
      while(index<Callback.tainted.size()){
        for(size_t size = Callback.tainted.size();index<size;index++){
          const VarDecl *var = Callback.tainted[index];
          std::string name = var->getNameAsString();
          std::string loc = var->getLocation().printToString(var->getASTContext().getSourceManager());
          data[param_index].insert({depth,var});
          MatchFinder ArgFinder;
          ArgFinder.addMatcher(InitMatcher(var),&Callback);
          ArgFinder.addMatcher(AssignMatcher(var),&Callback);
          ArgFinder.addMatcher(CallMatcher(var),&Callback);
          ArgFinder.match(*f,f->getASTContext());
        }
        depth++;
      }
    }
  }
