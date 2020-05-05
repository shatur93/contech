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

    void AAPass::CompareInstructions(vector<pair<Value*, Value*>> &contechAliasPairs) {
        errs() << "ContechAliasPairs" << "\n";
        //for (auto elem : P->MustAliasPairs){
            //TODO - comparison not working, Contech seems to be modifying the
            //program
            for (auto contech_elem: contechAliasPairs) {
                //if (elem == contech_elem) {
                    errs() << *contech_elem.first << *contech_elem.second << "\n";
                //}
            }
            errs() << "\n";
        //}
        P.reset();
    }

    //Invoke destructor
    bool AAPass::DumpPassOutput() {
        errs() << "MustAliasPairs" << "\n";
        for (auto elem : P->MustAliasPairs){
            errs() << *elem.first << *elem.second << "\n" ;
        }
        errs() << "\n";

        errs() << "PartialAliasPairs" << "\n";
        for (auto elem : P->PartialAliasPairs){
            errs() << *elem.first << *elem.second << "\n" ;
        }
        errs() << "\n";


        //P.reset();
        return false;
    } 

};

