#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "AAEval.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include <iostream>
#include <set>

using namespace llvm;
using namespace std;

class AAIntermediatePass : public FunctionPass {
    std::unique_ptr<AAEval> P;
public:
    static char ID;
    AAIntermediatePass() : FunctionPass(ID) { }
    ~AAIntermediatePass() { }

    // We don't modify the program, so we preserve all analyses
    //Run AAResultsWrapper prior to this
    void getAnalysisUsage(AnalysisUsage &AU) const override {
        AU.addRequired<AAResultsWrapperPass>();
        AU.setPreservesAll();
    }

    //Initialise
    bool doInitialization(Module &M) override {
        errs() << "15745 Project Alias Analysis Pass\n";
        P.reset(new AAEval());
        return false;
    }

    // Main run function
    bool runOnFunction(Function &F) override {
        errs() << F;
        P->run(F, getAnalysis<AAResultsWrapperPass>().getAAResults());
        return false;
    }

    //Invoke destructor
    bool doFinalization(Module &M) override {
        for (auto elem : P->MustAliasPairs){
            errs() << *elem.first << *elem.second << "\n";
        }
        P.reset();
        return false;
    } 
};

// LLVM uses the address of this static member to identify the pass, so the
// initialization value is unimportant.
char AAIntermediatePass::ID = 0;
static RegisterPass<AAIntermediatePass> X("my-aa-intermediate-pass", "15745: Intermediate Alias Analysis Pass", false, false);
