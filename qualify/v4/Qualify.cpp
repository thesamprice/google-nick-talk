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
#include <set>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

namespace clang {
namespace ast_matchers {
AST_TYPE_MATCHER(TagType, tagType);
}
}

static cl::OptionCategory QualifyCategory("qualify options");
static std::unique_ptr<opt::OptTable> Options(createDriverOptTable());
static cl::opt<std::string>
NamespaceName("namespace", cl::desc("Namespace to qualify references into"),
              cl::cat(QualifyCategory));

struct CompareSourceLoc {
  bool operator()(FullSourceLoc A, FullSourceLoc B) const {
    return A.isBeforeInTranslationUnitThan(B);
  }
};

class FoundCallback : public ast_matchers::MatchFinder::MatchCallback {
public:
  FoundCallback(tooling::Replacements *Replace) : Replace(Replace) {}

  virtual void onStartOfTranslationUnit() { SkipList.clear(); }

  virtual void run(const ast_matchers::MatchFinder::MatchResult &Result) {
    if (const DeclRefExpr *DRE =
            Result.Nodes.getNodeAs<DeclRefExpr>("skipdre")) {
      if (DRE->hasQualifier())
        return;
      SkipList.insert(FullSourceLoc(DRE->getLocStart(), *Result.SourceManager));
      return;
    }
    if (const TypeLoc *TL = Result.Nodes.getNodeAs<TypeLoc>("skiptype")) {
      SkipList.insert(FullSourceLoc(
          TL->getUnqualifiedLoc().castAs<TagTypeLoc>().getNameLoc(),
          *Result.SourceManager));
      return;
    }
    if (const TypeLoc *TL = Result.Nodes.getNodeAs<TypeLoc>("skipelabtype")) {
      ElaboratedTypeLoc ETL =
          TL->getUnqualifiedLoc().castAs<ElaboratedTypeLoc>();
      TypeLoc NTL = ETL.getNamedTypeLoc().getUnqualifiedLoc();
      SkipList.insert(FullSourceLoc(NTL.getBeginLoc(), *Result.SourceManager));
      return;
    }
    if (const DeclRefExpr *DRE = Result.Nodes.getNodeAs<DeclRefExpr>("dre")) {
      if (DRE->hasQualifier())
        return;
      if (SkipList.count(
              FullSourceLoc(DRE->getLocStart(), *Result.SourceManager)))
        return;
      Replace->insert(Replacement(*Result.SourceManager, DRE->getLocStart(), 0,
                                  NamespaceName + "::"));
      return;
    }
    if (const TypeLoc *TL = Result.Nodes.getNodeAs<TypeLoc>("type")) {
      TagTypeLoc TTL = TL->getUnqualifiedLoc().castAs<TagTypeLoc>();
      if (SkipList.count(
              FullSourceLoc(TTL.getNameLoc(), *Result.SourceManager)))
        return;
      Replace->insert(Replacement(*Result.SourceManager, TTL.getNameLoc(), 0,
                                  NamespaceName + "::"));
      return;
    }
    assert(false && "Unhandled match!");
  }

private:
  Replacements *Replace;
  std::set<FullSourceLoc, CompareSourceLoc> SkipList;
};

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();
  CommonOptionsParser OptionsParser(argc, argv, QualifyCategory);
  tooling::RefactoringTool Tool(OptionsParser.getCompilations(),
                                OptionsParser.getSourcePathList());

  ast_matchers::MatchFinder Finder;
  FoundCallback Callback(&Tool.getReplacements());

  auto name = namedDecl(matchesName("^::" + NamespaceName + ".*"));
  auto ns_name = matchesName("^::" + NamespaceName);
  Finder.addMatcher(
      namespaceDecl(ns_name,
                    forEachDescendant(declRefExpr(to(name)).bind("skipdre"))),
      &Callback);
  Finder.addMatcher(
      namespaceDecl(ns_name,
                    forEachDescendant(
                        loc(tagType(hasDeclaration(name))).bind("skiptype"))),
      &Callback);
  Finder.addMatcher(declRefExpr(to(name)).bind("dre"), &Callback);
  Finder.addMatcher(loc(elaboratedType(hasQualifier(
                            specifiesNamespace(ns_name)))).bind("skipelabtype"),
                    &Callback);
  Finder.addMatcher(loc(tagType(hasDeclaration(name))).bind("type"), &Callback);
  return Tool.runAndSave(newFrontendActionFactory(&Finder));
}
