#ifndef _ALIAS_ANALYSIS_EVALUATOR_H_
#define _ALIAS_ANALYSIS_EVALUATOR_H_
 
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Pass.h"
#include <vector>

using namespace std;
using namespace llvm;

class AAEval {
    int64_t FunctionCount;
    int64_t NoAliasCount, MayAliasCount, PartialAliasCount, MustAliasCount;
    int64_t NoModRefCount, ModCount, RefCount, ModRefCount;
    int64_t MustCount, MustRefCount, MustModCount, MustModRefCount;
 
public:
    vector<pair<Value*, Value*>> MustAliasPairs;
    vector<pair<Value*, Value*>> PartialAliasPairs;

    AAEval()
       : FunctionCount(), NoAliasCount(), MayAliasCount(), PartialAliasCount(),
         MustAliasCount(), NoModRefCount(), ModCount(), RefCount(),
         ModRefCount(), MustCount(), MustRefCount(), MustModCount(),
         MustModRefCount() {};

    ~AAEval();
    
    void run(Function &F, AAResults &AA);
};

 
#endif