//===--- tools/qualify/Qualify.cpp - Add namespace qualifiers -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Rewrite/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Signals.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory QualifyCategory("qualify options");
static std::unique_ptr<opt::OptTable> Options(createDriverOptTable());
static cl::opt<std::string>
NamespaceName("namespace", cl::desc("Namespace to qualify references into"),
              cl::cat(QualifyCategory));

class FoundCallback : public ast_matchers::MatchFinder::MatchCallback {
public:
  FoundCallback(tooling::Replacements *Replace) : Replace(Replace) {}

  virtual void run(const ast_matchers::MatchFinder::MatchResult &Result) {
    const NamedDecl *ND = Result.Nodes.getNodeAs<NamedDecl>("name");
    // ND->dump();
    Replace->insert(Replacement(*Result.SourceManager, ND->getLocStart(), 0,
                                NamespaceName + "::"));
  }

private:
  Replacements *Replace;
};

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();
  CommonOptionsParser OptionsParser(argc, argv, QualifyCategory);
  tooling::RefactoringTool Tool(OptionsParser.getCompilations(),
                                OptionsParser.getSourcePathList());

  ast_matchers::MatchFinder Finder;
  FoundCallback Callback(&Tool.getReplacements());
  Finder.addMatcher(
      namedDecl(matchesName("^::" + NamespaceName + ".*")).bind("name"),
      &Callback);
  return Tool.runAndSave(newFrontendActionFactory(&Finder));
}
