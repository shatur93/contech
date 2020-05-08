#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include <iostream>
#include <set>

using namespace llvm;
using namespace std;

class AABasicPass : public FunctionPass {
public:
    static char ID;
    int NoAliasCount = 0;
    int PartialAliasCount = 0;
    int MustAliasCount = 0;
    AABasicPass() : FunctionPass(ID) { }
    ~AABasicPass() { }

    // We don't modify the program, so we preserve all analyses
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
      AU.addRequired<AAResultsWrapperPass>();
    }

    // Do some initialization
    bool doInitialization(Module &M) override {
      errs() << "15745 AA Basic Pass\n";
   }

    // Print output for each function
    bool runOnFunction(Function &F) override {
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
            if ((AA->alias(e1, e2) == NoAlias) || (AA->alias(e1,e2) == MayAlias)) {
              NoAliasCount++;
            }
            else if (AA->alias(e1, e2) == PartialAlias) {
              PartialAliasCount++;
            }
            else if (AA->alias(e1, e2) == MustAlias) {
              MustAliasCount++;
            }
          }
        } 
      }
      outs() << "Must Alias " << MustAliasCount << " PartialAlias " << PartialAliasCount \
      << " No/MayAlias " << NoAliasCount << "\n";
      return false;
   } 
};

// LLVM uses the address of this static member to identify the pass, so the
// initialization value is unimportant.
char AABasicPass::ID = 0;
static RegisterPass<AABasicPass> X("my-aa-basic-pass", "15745: Basic Alias Analysis Pass", false, false);
