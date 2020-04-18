// 15-745 S16 Assignment 1: FunctionInfo.cpp
// Group:
////////////////////////////////////////////////////////////////////////////////

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include <iostream>
#include <set>

using namespace llvm;
using namespace std;

class FunctionInfo : public FunctionPass {
public:
    static char ID;
    FunctionInfo() : FunctionPass(ID) { }
    ~FunctionInfo() { }

    // We don't modify the program, so we preserve all analyses
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
      AU.addRequired<AAResultsWrapperPass>();
    }

    // Do some initialization
    bool doInitialization(Module &M) override {
      errs() << "15745 Function Information Pass\n"; // TODO: remove this.
      // outs() << "Name,\tArgs,\tCalls,\tBlocks,\tInsns\n";
      //for (Module::iterator F = M.begin(), FE = M.end(); F != FE; ++F) {
   }

    // Print output for each function
    bool runOnFunction(Function &F) override {
      // outs() << "name" << ",\t" << "args" << ",\t" << "calls" << ",\t" << "bbs" << ",\t" << "insts" << "\n";
      auto AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
      set<Value*> all_addr;
      for (BasicBlock &BB: F) {
          for (Instruction &I: BB) {
            outs() << I << "\n";
              Value *new_addr = NULL;
              if (isa<LoadInst>(I)){
                  new_addr = cast<LoadInst>(I).getPointerOperand();
              } else if (isa<StoreInst>(I)){
                  new_addr = cast<StoreInst>(I).getPointerOperand();
              }
              
              if (new_addr != NULL) all_addr.insert(new_addr);
          }
	    }

      outs() << "\n\n\n";
      for (auto e1 : all_addr){
        for (auto e2 : all_addr){
          if (e1 != e2){
            outs() << *e1 << " " << AA->alias(e1, e2) << " " << *e2 << "\n";
          }
        } 
      }




      return false;
   } 
};

// LLVM uses the address of this static member to identify the pass, so the
// initialization value is unimportant.
char FunctionInfo::ID = 0;
static RegisterPass<FunctionInfo> X("function-info", "15745: Function Information", false, false);
