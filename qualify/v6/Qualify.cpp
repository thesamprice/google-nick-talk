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

  void qualify(SourceLocation SL, SourceManager *SM) {
    if (!SkipList.insert(FullSourceLoc(SL, *SM)).second)
      return;
    SL.dump(*SM);
    const char *data = SM->getCharacterData(SL);
    errs() << ' ' << std::string(data, data + (std::min(strlen(data), 16ul)))
           << "\n";
    Replace->insert(Replacement(*SM, SL, 0, NamespaceName + "::"));
  }

  virtual void run(const ast_matchers::MatchFinder::MatchResult &Result) {
    SourceManager *SM = Result.SourceManager;
    if (const DeclRefExpr *DRE =
            Result.Nodes.getNodeAs<DeclRefExpr>("skipdre")) {
      errs() << "skipdre\n";
      if (DRE->hasQualifier())
        return;
      SkipList.insert(FullSourceLoc(DRE->getLocStart(), *SM));
      return;
    }
    if (const TypeLoc *TL = Result.Nodes.getNodeAs<TypeLoc>("skiptype")) {
      errs() << "skiptype\n";
      SkipList.insert(FullSourceLoc(
          TL->getUnqualifiedLoc().castAs<TagTypeLoc>().getNameLoc(), *SM));
      return;
    }
    if (const TypeLoc *TL = Result.Nodes.getNodeAs<TypeLoc>("skipelabtype")) {
      errs() << "skipelabtype\n";
      ElaboratedTypeLoc ETL =
          TL->getUnqualifiedLoc().castAs<ElaboratedTypeLoc>();
      TypeLoc NTL = ETL.getNamedTypeLoc().getUnqualifiedLoc();
      SkipList.insert(FullSourceLoc(NTL.getBeginLoc(), *SM));
      return;
    }
    if (const DeclRefExpr *DRE = Result.Nodes.getNodeAs<DeclRefExpr>("dre")) {
      errs() << "dre\n";
      if (DRE->hasQualifier())
        return;
      qualify(DRE->getLocStart(), SM);
      return;
    }
    if (const TypeLoc *TL = Result.Nodes.getNodeAs<TypeLoc>("type")) {
      errs() << "type\n";
      TagTypeLoc TTL = TL->getUnqualifiedLoc().castAs<TagTypeLoc>();
      qualify(TTL.getNameLoc(), SM);
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
      functionDecl(isTemplateInstantiation(),
                 forEachDescendant(declRefExpr(to(name)).bind("skipdre"))),
      &Callback);
  Finder.addMatcher(
      varDecl(isTemplateInstantiation(),
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
