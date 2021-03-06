#include "symbexchecks/SimpleReachAnalysis.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CallSite.h"
#include <stack>

using namespace llvm;

namespace symbexchecks {

char SimpleReachAnalysis::ID;

static RegisterPass<SimpleReachAnalysis> SimpleReachAnalysisPI(
  "simplereach", "Simple reachability analysis from "
                 "inputs to LLVM values", false, true);

void  SimpleReachAnalysis::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<PostDominatorTree>();
}

const char *SimpleReachAnalysis::getPassName() const {
  return "Simple reachability analysis from inputs to LLVM values";
}

bool SimpleReachAnalysis::runOnModule(Module &M) {

  errs() << "SimpleReachAnalysis runs!!!\n";

  // Reset pass.
  MemoryWrites.clear();
  AddressTakenFunctions.clear();
  OneStepReachGraph.clear();
  PDFs.clear();

  // Initialize pass.
  collectAddressTakenFunctions(M);
  collectMemoryWrites(M);
  calculatePostDominanceFrontiers(M);
  initOneStepReachGraph(M);

  // Does not modify module.
  return false;
}

void SimpleReachAnalysis::collectAddressTakenFunctions(Module &M) {
  Module::iterator funIt = M.begin(), funItEnd = M.end();
  for (; funIt != funItEnd; ++funIt) {
    const Function *f = &(*funIt);
    if (f->hasAddressTaken())
      AddressTakenFunctions.insert(f);
  }
}

void SimpleReachAnalysis::collectMemoryWrites(Module &M) {
  // initializers of global variables are memory writes
  Module::global_iterator gvIt = M.global_begin(), gvItEnd = M.global_end();
  for (; gvIt != gvItEnd; ++gvIt) {
    MemoryWrite MW;
    GlobalVariable &GV = *gvIt;
    if (GV.hasInitializer()) {
      Value *initValue = GV.getInitializer();
      MW.memwrite = &GV;
      MW.ptr = &GV;
      MW.val = initValue;
      MemoryWrites.insert(MW);
    }
  }

  Module::iterator funIt = M.begin(), funItEnd = M.end();
  for (; funIt != funItEnd; ++funIt) {
    Function &F = *funIt;
    Function::iterator bbIt = F.begin(), bbItEnd = F.end();
    for (; bbIt != bbItEnd; ++bbIt) {
      BasicBlock &BB = *bbIt;
      BasicBlock::iterator instIt = BB.begin(), instItEnd = BB.end();
      for (; instIt != instItEnd; ++instIt) {
        MemoryWrite MW;
        Instruction &I = *instIt;

        // store instructions are memory writes
        if (StoreInst *storeInst = dyn_cast<StoreInst>(&I)) {
          MW.memwrite = storeInst;
          MW.ptr = storeInst->getPointerOperand();
          MW.val = storeInst->getValueOperand();
          MemoryWrites.insert(MW);
        }
        // atomiccmpxchg instructions contain a possible memory write
        else if (AtomicCmpXchgInst *acxInst = dyn_cast<AtomicCmpXchgInst>(&I)) {
          MW.memwrite = acxInst;
          MW.ptr = acxInst->getPointerOperand();
          MW.val = acxInst->getNewValOperand();
          MemoryWrites.insert(MW);

          MW.memwrite = acxInst;
          MW.ptr = acxInst->getPointerOperand();
          MW.val = acxInst->getCompareOperand();
          MemoryWrites.insert(MW);
        }
        // atomicrmw instructions are memory writes
        else if (AtomicRMWInst *armwInst = dyn_cast<AtomicRMWInst>(&I)) {
          MW.memwrite = armwInst;
          MW.ptr = armwInst->getPointerOperand();
          MW.val = armwInst->getValOperand();
          MemoryWrites.insert(MW);
        }

        // calls to vararg functions are treated as memory writes
        // for the variadic arguments
        else if (isa<CallInst>(&I) || isa<InvokeInst>(&I)) {
          ImmutableCallSite CS(&I);

          // collect vararg functions that are possible targets of this call
          std::vector<const Function *> CalledVFuncs;
          if (const Function *f = CS.getCalledFunction()) {
            // direct function call
            if (f->isVarArg()) CalledVFuncs.push_back(f);
          }
          else {
            // indirect function call - use all address taken vararg
            // functions as possible targets
            FunctionSet::iterator it = AddressTakenFunctions.begin(),
                                  itEnd = AddressTakenFunctions.end();
            for (; it != itEnd; ++it) {
              const Function *indf = *it;
              if (indf->isVarArg()) CalledVFuncs.push_back(indf);
            }
          }

          // for each vararg function found, add a memory write for
          // each of its variadic arguments in the call
          for (unsigned i = 0; i < CalledVFuncs.size(); ++i) {
            const Function *vfunc = CalledVFuncs[i];
            ImmutableCallSite::arg_iterator it = CS.arg_begin(),
                                            itEnd = CS.arg_end();
            unsigned argno = 0;
            for (; it != itEnd; ++it, ++argno) {
              if (argno < vfunc->arg_size()) continue;
              Value *actualArg = *it;
              MW.memwrite = &I;
              MW.ptr = &I;
              MW.val = actualArg;
              MemoryWrites.insert(MW);
            }
          }
        }
      } 
    }
  }
}

void SimpleReachAnalysis::calculatePostDominanceFrontiers(Module &M) {
  Module::iterator funIt = M.begin(), funItEnd = M.end();
  for (; funIt != funItEnd; ++funIt) {
    Function &F = *funIt;
    if (!F.isDeclaration()) {
      PostDominatorTree &PDT = getAnalysis<PostDominatorTree>(F);
      PostDominanceFrontiers PDF;
      PDF.calculate(PDT);
      PDFs.insert(std::pair<const Function *, PostDominanceFrontiers>(&F, PDF));
    }
  }
}

void SimpleReachAnalysis::initOneStepReachGraph(Module &M) {
  visit(M);
}

void SimpleReachAnalysis::visitBinaryOperator(BinaryOperator &I) {
  ValueSet &Successors = OneStepReachGraph[&I];
  Successors.insert(I.getOperand(0));
  Successors.insert(I.getOperand(1));
}

void SimpleReachAnalysis::visitCmpInst(CmpInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];
  Successors.insert(I.getOperand(0));
  Successors.insert(I.getOperand(1));
}

void SimpleReachAnalysis::visitCastInst(CastInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];
  Successors.insert(I.getOperand(0));
}

void SimpleReachAnalysis::visitSelectInst(SelectInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];
  Successors.insert(I.getCondition());
  Successors.insert(I.getTrueValue());
  Successors.insert(I.getFalseValue());
}

void SimpleReachAnalysis::visitGetElementPtrInst(GetElementPtrInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];

  Successors.insert(I.getPointerOperand());

  GetElementPtrInst::const_op_iterator GEPIt = I.idx_begin(),
                                       GEPItEnd = I.idx_end();
  for (; GEPIt != GEPItEnd; ++GEPIt)
    Successors.insert(*GEPIt);
}

void SimpleReachAnalysis::visitExtractElementInst(ExtractElementInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];
  Successors.insert(I.getVectorOperand());
  Successors.insert(I.getIndexOperand());
}

void SimpleReachAnalysis::visitInsertElementInst(InsertElementInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];
  Successors.insert(I.getOperand(0));
  Successors.insert(I.getOperand(1));
  Successors.insert(I.getOperand(2));
}

void SimpleReachAnalysis::visitShuffleVectorInst(ShuffleVectorInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];
  Successors.insert(I.getOperand(0));
  Successors.insert(I.getOperand(1));
  Successors.insert(I.getOperand(2));
}

void SimpleReachAnalysis::visitExtractValueInst(ExtractValueInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];
  Successors.insert(I.getAggregateOperand());
}

void SimpleReachAnalysis::visitInsertValueInst(InsertValueInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];
  Successors.insert(I.getAggregateOperand());
  Successors.insert(I.getInsertedValueOperand());
}

void SimpleReachAnalysis::visitLoadInst(LoadInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];

  Successors.insert(I.getPointerOperand());

  MemoryWriteSet::iterator MWIt = MemoryWrites.begin(),
                           MWItEnd = MemoryWrites.end();
  for (; MWIt != MWItEnd; ++MWIt)
    Successors.insert(MWIt->val);
}

void SimpleReachAnalysis::visitAtomicCmpXchgInst(AtomicCmpXchgInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];

  Successors.insert(I.getPointerOperand());

  MemoryWriteSet::iterator MWIt = MemoryWrites.begin(),
                           MWItEnd = MemoryWrites.end();
  for (; MWIt != MWItEnd; ++MWIt)
    Successors.insert(MWIt->val);
}

void SimpleReachAnalysis::visitVAArgInst(VAArgInst &I) {
  ValueSet &Successors = OneStepReachGraph[&I];

  Successors.insert(I.getPointerOperand());

  MemoryWriteSet::iterator MWIt = MemoryWrites.begin(),
                           MWItEnd = MemoryWrites.end();
  for (; MWIt != MWItEnd; ++MWIt)
    Successors.insert(MWIt->val);
}

void SimpleReachAnalysis::visitPHINode(PHINode &I) {
  ValueSet &Successors = OneStepReachGraph[&I];

  for (unsigned i = 0; i < I.getNumIncomingValues(); ++i)
    Successors.insert(I.getIncomingValue(i));

  // handle control deps
  Function *parentFunc = I.getParent()->getParent();
  PostDominanceFrontiers &PDF = PDFs.at(parentFunc);

  PostDominanceFrontiers::DomSetType AllControlDeps;
  PHINode::block_iterator phiIt = I.block_begin(), phiItEnd = I.block_end();
  for (; phiIt != phiItEnd; ++phiIt) {
    BasicBlock *bb = *phiIt;
    PostDominanceFrontiers::iterator pdfIt = PDF.find(bb);
    assert(pdfIt != PDF.end());
    PostDominanceFrontiers::DomSetType &ControlDeps = pdfIt->second;
    AllControlDeps.insert<PostDominanceFrontiers::DomSetType::iterator>(
      ControlDeps.begin(), ControlDeps.end());
  }

  PostDominanceFrontiers::DomSetType::iterator
    it = AllControlDeps.begin(), itEnd = AllControlDeps.end();
  for (; it != itEnd; ++it) {
    TerminatorInst *inst = (*it)->getTerminator();

    if (BranchInst * brInst = dyn_cast<BranchInst>(inst)) {
      if (brInst->isConditional()) Successors.insert(brInst->getCondition());
    }
    else if (IndirectBrInst * inbrInst = dyn_cast<IndirectBrInst>(inst)) {
      Successors.insert(inbrInst->getAddress());
    }
    else if (SwitchInst * swInst = dyn_cast<SwitchInst>(inst)) {
      Successors.insert(swInst->getCondition());
    }
    else if (isa<InvokeInst>(inst)) {
      // do nothing: we are missing some dependencies here.
    }
    else {
      // remaining cases (ReturnInst, ResumeInst, Unreachable) terminate
      // basic blocks without outgoing edges, so they should not appear
      // here
      assert(false);
    }
  }
}

void SimpleReachAnalysis::visitCallSite(CallSite CS) {
  const Function *f = CS.getCalledFunction();
  if (f)
    // direct function call
    visitFunctionCall(f, CS);
  else {
    // indirect function call - use all address taken functions as
    // possible targets
    FunctionSet::iterator it = AddressTakenFunctions.begin(),
                          itEnd = AddressTakenFunctions.end();
    for (; it != itEnd; ++it) {
      const Function *f = *it;
      // before visiting the function, we make sure its number of
      // formal arguments is compatible with the actual arguments
      // used in the callsite
      if (!f->isVarArg() && f->arg_size() != CS.arg_size()) continue;
      if (f->isVarArg() && f->arg_size() > CS.arg_size()) continue;

      visitFunctionCall(*it, CS);
    }
  }
}

void SimpleReachAnalysis::visitIntrinsicInst(IntrinsicInst &I) {
  // TODO handle important intrinsics
  return;
}

void SimpleReachAnalysis::visitFunctionCall(const Function *f, CallSite CS) {
  assert(!f->isIntrinsic());

  ValueSet &Successors = OneStepReachGraph[CS.getInstruction()];

  // if the called function is external, we consider it as an input
  // TODO: handle important library functions
  if (f->isDeclaration()) {
    Successors.insert(f);
    return;
  }

  // if there is a return value, we consider it depends on any value found
  // as an argument of a ret instruction inside the called function
  if (!CS.getType()->isVoidTy()) {
    Function::const_iterator bbIt = f->begin(), bbItEnd = f->end();
    for (; bbIt != bbItEnd; ++bbIt) {
      const BasicBlock &BB = *bbIt;
      BasicBlock::const_iterator instIt = BB.begin(), instItEnd = BB.end();
      for (; instIt != instItEnd; ++instIt) {
        const Instruction &I = *instIt;
        if (const ReturnInst *retInst = dyn_cast<ReturnInst>(&I))
          Successors.insert(retInst->getReturnValue());
      }
    } 
  }
 
  // each formal argument of the called function depends on the corresponding
  // actual argument
  Function::const_arg_iterator formalIt = f->arg_begin(),
                               formalItEnd = f->arg_end();
  CallSite::arg_iterator actualIt = CS.arg_begin();
  for (; formalIt != formalItEnd; ++formalIt, ++actualIt) {
    const Value *formalArg = &*formalIt;
    const Value *actualArg = *actualIt;
    if (actualArg) { // actualArg is null for formals argc argv of main
      OneStepReachGraph[formalArg].insert(actualArg);
    }
  }

}

void
SimpleReachAnalysis::getReachingInputs(const ValueSet &Values,
                                       ValueSet &Inputs,
                                       const Module &M) const {
  std::stack<const Value *> WorkList;
  ValueSet Explored;

  // insert given values in the list
  ValueSet::const_iterator it = Values.begin(), itEnd = Values.end();
  for (; it != itEnd; ++it) {
    WorkList.push(*it);
  }

  // while the list is not empty, pop a value and process it
  while (!WorkList.empty()) {
    const Value *v = WorkList.top();
    WorkList.pop();

    // if exlpored v continue
    if (Explored.find(v) != Explored.end()) continue;

    // mark v explored
    Explored.insert(v);

    // if v is an input continue
    if (isInput(v, M)) {
      Inputs.insert(v);
      continue;
    }

    // add successors of v to the list
    ReachGraph::const_iterator gIt = OneStepReachGraph.find(v);
    
    if (gIt == OneStepReachGraph.end()) continue; // eventually it should
                                                  // become an assertion
    //assert(gIt != OneStepReachGraph.end());

    const ValueSet &Successors = gIt->second;
    it = Successors.begin(), itEnd = Successors.end();
    for (; it != itEnd; ++it)
      WorkList.push(*it);
  }
}

bool SimpleReachAnalysis::isInput(const Value *v, const Module &M) const {
  // constants are inputs
  if (isa<Constant>(v)) return true;

  // external functions are inputs
  if (const Function *f = dyn_cast<Function>(v)) {
    return (f->isDeclaration());
  }

  // arguments of main are inputs
  if (const Argument *arg = dyn_cast<Argument>(v)) {

    llvm::Function* userMain = M.getFunction("__user_main");
	if (!userMain) {
		userMain = M.getFunction("main");
    }

    if (userMain && arg->getParent() == userMain) return true;

    return false;
  }

  return false;
}

void SimpleReachAnalysis::printValueSet(raw_ostream &O,
                                        const ValueSet &S) const {
  if (S.empty())
    O << "\tNo reaching inputs found.\n";
  else {
    ValueSet::const_iterator it = S.begin(), itEnd = S.end();
    for (; it != itEnd; ++it) {
      O << "\t";
      const Value *v = *it;
      if (v) O << *v; else O << "null found";
      O << "\n";
    }
  }
}

void SimpleReachAnalysis::printReachGraph(raw_ostream &O,
                                          const ReachGraph &G) const {
  ReachGraph::const_iterator it = G.begin(), itEnd = G.end();
  for (; it != itEnd; ++it) {
    const Value *v = it->first;
    const ValueSet &VS = it->second;
    O << "\n======================================================\n";
    O << "Values affecting " << *v << "\n";
    O << "------------------------------------------------------\n";
    printValueSet(O, VS);
    O << "------------------------------------------------------\n";
  }
}

void SimpleReachAnalysis::printPostDominanceFrontiers(raw_ostream &O) const {
  PDFMap::const_iterator it = PDFs.begin(), itEnd = PDFs.end();
  for (; it != itEnd; ++it) {
    const Function *f = it->first;
    const PostDominanceFrontiers &PDF = it->second;

    O << "\n======================================================\n";
    O << "Post Dominance Frontiers for Function " << f->getName() << "\n";
    O << "------------------------------------------------------\n";
    PDF.print(O);
    O << "------------------------------------------------------\n";
  }
}

}
