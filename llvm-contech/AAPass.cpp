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
        auto LI = ctThis->getAnalysisAliasInfo(F);
        P->run(F, *LI);
        return false;
    }

    void AAPass::PrintContechInstructions(vector<pair<Value*, Value*>> &contechAliasPairs) {
        errs() << "ContechAliasPairs" << "\n";
            for (auto contech_elem: contechAliasPairs) {
                    errs() << *(contech_elem.first) 
                        << *contech_elem.second << "\n";
            }
            errs() << "\n";
        P.reset();
    }

    //Add instructions to Alias Set
    void AAPass::AddToSet(Value *first, Value *second) {
        for (int i = 0; i < AliasSetVector.size(); i++) {
            if ((AliasSetVector[i].find(first) != AliasSetVector[i].end()) && 
                (AliasSetVector[i].find(second) != AliasSetVector[i].end())) {
                    //both addresses already in a set, do nothing
                    return;
            }
            else if ((AliasSetVector[i].find(first) != AliasSetVector[i].end()) && 
                    (AliasSetVector[i].find(second) == AliasSetVector[i].end())) {
                AliasSetVector[i].insert(second);
                return;
            }
            else if ((AliasSetVector[i].find(first) == AliasSetVector[i].end()) && 
                    (AliasSetVector[i].find(second) != AliasSetVector[i].end())) {
                AliasSetVector[i].insert(first);
                return;
            }
        }
        set<Value *> new_set;
        new_set.insert(first);
        new_set.insert(second);
        AliasSetVector.push_back(new_set);
    }

    void AAPass::PrintAliasSets() {
        for (int i = 0; i < AliasSetVector.size(); i++) {
            errs() << "Set " << i << "\n";
            for(set<Value *> :: iterator it = AliasSetVector[i].begin(); 
                        it != AliasSetVector[i].end();++it) {
                errs() << *(*it) << "\n";
            }
            errs() << "\n";
        }
    }

    //Creates Alias sets on Must Alias and Partial Alias instructions
    bool AAPass::DumpPassOutput() {
        for (auto elem : P->MustAliasPairs){
            AddToSet(elem.first, elem.second);
        }

        for (auto elem : P->PartialAliasPairs){
            AddToSet(elem.first, elem.second);
        }

        VisitedAliasSets.resize(AliasSetVector.size());
        for (int i = 0; i < VisitedAliasSets.size(); i++) {
            VisitedAliasSets[i] = false;
        }
        return false;
    } 

    //check to see if this instruction is already part
    //of an alias set. Return the set if found
    int AAPass::IsPresentInAASet(Value *addr) {
        for (int i = 0; i < AliasSetVector.size(); i++) {
            if (AliasSetVector[i].find(addr) != AliasSetVector[i].end()) {
                return i;
            }
        }
        return -1;   
    }

    //Check to see if an instruction from this set index is already visited
    bool AAPass::IsSetVisited(int i) {
        if (VisitedAliasSets[i]) {
            return true;
        }
        else {
            VisitedAliasSets[i] = true;
            return false;
        }
    }
};

