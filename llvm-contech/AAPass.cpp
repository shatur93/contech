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
        //errs() << F;
        for (Function::iterator B = F.begin(), BE = F.end(); B != BE; ++B) {
            BasicBlock &pB = *B;
            for (BasicBlock::iterator I = pB.begin(), E = pB.end(); I != E; ++I) {
                errs() << *I  << " Value " << (dyn_cast<Value>(I)) << "\n";
                /*if (LoadInst *li = dyn_cast<LoadInst>(I)) {
                    errs() << "Load instruction" << li->getPointerOperand() << "\n";
                }*/
            }
        }
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
                    errs() << *(contech_elem.first) 
                        << *contech_elem.second << "\n";
                //}
            }
            errs() << "\n";
        //}
        P.reset();
    }

    //Add instructions to set
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

    //Invoke destructor
    bool AAPass::DumpPassOutput() {
        errs() << "MustAliasPairs" << "\n";
        for (auto elem : P->MustAliasPairs){
            errs() << *elem.first << *elem.second << "\n" ;
            AddToSet(elem.first, elem.second);
        }
        errs() << "\n";

        errs() << "PartialAliasPairs" << "\n";
        for (auto elem : P->PartialAliasPairs){
            errs() << *elem.first << *elem.second << "\n" ;
            AddToSet(elem.first, elem.second);
        }
        errs() << "\n";
        //PrintAliasSets();
        VisitedAliasSets.resize(AliasSetVector.size());
        for (int i = 0; i < VisitedAliasSets.size(); i++) {
            VisitedAliasSets[i] = false;
        }
        //P.reset();
        return false;
    } 

    int AAPass::IsPresentInAASet(Value *addr) {
        for (int i = 0; i < AliasSetVector.size(); i++) {
            errs() << "Set " << i << "\n";
            if (AliasSetVector[i].find(addr) != AliasSetVector[i].end()) {
                return i;
            }
        }
        return -1;   
    }

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

