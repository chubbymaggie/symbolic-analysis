#ifndef ALIASANALYSISCHECKERINTERFACE_H
#define ALIASANALYSISCHECKERINTERFACE_H

#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include <set>

namespace aachecker {

typedef char AbstractLoc;
typedef std::set<const AbstractLoc *> AbstractLocSet;

class AliasAnalysisCheckerInterface : public llvm::ModulePass {
public:
  static char ID;
  AliasAnalysisCheckerInterface() : llvm::ModulePass(ID) {}
  AliasAnalysisCheckerInterface(char &id) : llvm::ModulePass(id) {}

  //
  // For a given value in the module, returns a pointer to a set representing
  // the set of abstract memory locations that the value can point to.
  //
  virtual const AbstractLocSet *getAbstractLocSetForValue(const llvm::Value *V)=0;

  //
  // For a given value in the module, returns the set of all abstract memory
  // locations reachable from that value.
  //
  virtual const AbstractLocSet *getReachableAbstractLocSetForValue(const llvm::Value *V)=0;

  // Returns true if this is an allocation site
  virtual bool isAllocationSite(const llvm::Value *V)=0;

  // Returns the abstract location corresponding to external nodes.
  virtual const AbstractLoc *getExternalLoc()=0;

  // Return all abstract locations that may be created by an allocation site.
  virtual const AbstractLocSet &getAllocatableLocs(const llvm::Value *V)=0;

};

}

#endif
