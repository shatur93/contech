#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "AAEval.h"
#include "AAPass.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include <iostream>
#include <set>

using namespace llvm;
using namespace std;

namespace llvm {

    // We don't modify the program, so we preserve all analyses
    //Run AAResultsWrapper prior to this

    //Initialise
    bool AAPass::InitializePass() {
        errs() << "15745 Project Alias Analysis Pass\n";
        P.reset(new AAEval());
        return false;
    }

    // Main run function
    bool AAPass::runOnFunction(Function &F) {
        errs() << F;
        auto LI = ctThis->getAnalysisAliasInfo(F);
        P->run(F, *LI);
        return false;
    }

    //Invoke destructor
    bool AAPass::DumpPassOutput() {
        for (auto elem : P->MustAliasPairs){
            errs() << *elem.first << *elem.second << "\n";
        }
        P.reset();
        return false;
    } 
};

