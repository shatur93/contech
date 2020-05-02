#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/raw_ostream.h"
#include "AAEval.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include <iostream>
#include <set>
#include "ContechDef.h"

using namespace llvm;
using namespace std;

namespace llvm {
    class AAPass {
        public:
            AAPass(Contech* _ctThis) { 
                ctThis = _ctThis;
            }
            ~AAPass();
            std::unique_ptr<AAEval> P;
            // We don't modify the program, so we preserve all analyses
            //Run AAResultsWrapper prior to this

            //Initialise
            bool InitializePass();

            // Main run function
            virtual bool runOnFunction(Function &F);

            //Invoke destructor
            bool DumpPassOutput();
            void CompareInstructions(std::vector<std::pair<Value*, Value*>> &);
        private:
            Contech* ctThis;
    };
}   


