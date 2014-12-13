//===- Contech.cpp - Based on Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "Contech"

#include "llvm/Config/config.h"
#if LLVM_VERSION_MAJOR==2
#error LLVM Version 3.2 or greater required
#else
#if LLVM_VERSION_MINOR>=3
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Attributes.h"
#if LLVM_VERSION_MINOR>=5
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/DebugLoc.h"
#else
#include "llvm/DebugInfo.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/Support/DebugLoc.h"
#endif
#define ALWAYS_INLINE (Attribute::AttrKind::AlwaysInline)
#else
#include "llvm/Constants.h"
#include "llvm/DataLayout.h"
#include "llvm/Instructions.h"
#include "llvm/GlobalVariable.h"
#include "llvm/Type.h"
#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include "llvm/Metadata.h"
#include "llvm/Function.h"
#include "llvm/Attributes.h"
#include "llvm/Analysis/DebugInfo.h"
#define ALWAYS_INLINE (Attributes::AttrVal::AlwaysInline)
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/Support/DebugLoc.h"
#endif
#endif
#include "llvm/Support/raw_ostream.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Instrumentation.h"

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include <cxxabi.h>

#include "Contech.h"
using namespace llvm;
using namespace std;

#define __ctStrCmp(x, y) strncmp(x, y, sizeof(y) - 1)

//#define DEBUG_PRINT_CALLINST
#ifdef DEBUG_PRINT_CALLINST
    #define debugLog(s) errs() << s << "\n"
#else
    #define debugLog(s)
#endif
//#define SPLIT_DEBUG

map<BasicBlock*, llvm_basic_block*> cfgInfoMap;
cl::opt<string> ContechStateFilename("ContechState", cl::desc("File with current Contech state"), cl::value_desc("filename"));
cl::opt<bool> ContechMarkFrontend("ContechMarkFE", cl::desc("Generate a minimal marked output"));
cl::opt<bool> ContechMinimal("ContechMinimal", cl::desc("Generate a minimally instrumented output"));

namespace llvm {
#define STORE_AND_LEN(x) x, sizeof(x)
#define FUNCTIONS_INSTRUMENT_SIZE 43
// NB Order matters in this array.  Put the most specific function names first, then 
//  the more general matches.
    llvm_function_map functionsInstrument[FUNCTIONS_INSTRUMENT_SIZE] = {
                                            // If main has OpenMP regions, the derived functions
                                            //    will begin with main or MAIN__
                                           {STORE_AND_LEN("main\0"), MAIN},
                                           {STORE_AND_LEN("MAIN__\0"), MAIN},
                                           {STORE_AND_LEN("pthread_create"), THREAD_CREATE},
                                           {STORE_AND_LEN("pthread_join"), THREAD_JOIN},
                                           {STORE_AND_LEN("parsec_barrier_wait"), BARRIER_WAIT},
                                           {STORE_AND_LEN("pthread_barrier_wait"), BARRIER_WAIT},
                                           {STORE_AND_LEN("parsec_barrier"), BARRIER},
                                           {STORE_AND_LEN("pthread_barrier"), BARRIER},
                                           {STORE_AND_LEN("malloc"), MALLOC},
                                           {STORE_AND_LEN("xmalloc"), MALLOC},
                                           {STORE_AND_LEN("valloc"), MALLOC},
                                           {STORE_AND_LEN("memalign"), MALLOC2},
                                           {STORE_AND_LEN("operator new"), MALLOC},
                                           // Splash2.raytrace has a free_rayinfo, so \0 added
                                           {STORE_AND_LEN("free\0"), FREE},
                                           {STORE_AND_LEN("operator delete"),FREE},
                                           {STORE_AND_LEN("exit"), EXIT},
                                           {STORE_AND_LEN("pthread_mutex_lock"), SYNC_ACQUIRE},
                                           {STORE_AND_LEN("pthread_mutex_trylock"), SYNC_ACQUIRE},
                                           {STORE_AND_LEN("pthread_mutex_unlock"), SYNC_RELEASE},
                                           {STORE_AND_LEN("_mutex_lock_"), SYNC_ACQUIRE},
                                           {STORE_AND_LEN("_mutex_unlock_"), SYNC_RELEASE},
                                           {STORE_AND_LEN("pthread_cond_wait"), COND_WAIT},
                                           {STORE_AND_LEN("pthread_cond_signal"), COND_SIGNAL},
                                           {STORE_AND_LEN("pthread_cond_broadcast"), COND_SIGNAL},
                                           {STORE_AND_LEN("GOMP_parallel_start"), OMP_CALL},
                                           {STORE_AND_LEN("GOMP_parallel_end"), OMP_END},
                                           {STORE_AND_LEN("GOMP_atomic_start"), GLOBAL_SYNC_ACQUIRE},
                                           {STORE_AND_LEN("GOMP_atomic_end"), GLOBAL_SYNC_RELEASE},
                                           {STORE_AND_LEN("__kmpc_fork_call"), OMP_FORK},
                                           {STORE_AND_LEN("__kmpc_dispatch_next"), OMP_FOR_ITER},
                                           {STORE_AND_LEN("__kmpc_barrier"), OMP_BARRIER},
                                           {STORE_AND_LEN("MPI_Send"), MPI_SEND_BLOCKING},
                                           {STORE_AND_LEN("mpi_send_"), MPI_SEND_BLOCKING},
                                           {STORE_AND_LEN("MPI_Recv"), MPI_RECV_BLOCKING},
                                           {STORE_AND_LEN("mpi_recv_"), MPI_RECV_BLOCKING},
                                           {STORE_AND_LEN("MPI_Isend"), MPI_SEND_NONBLOCKING},
                                           {STORE_AND_LEN("mpi_isend_"), MPI_SEND_NONBLOCKING},
                                           {STORE_AND_LEN("MPI_Irecv"), MPI_RECV_NONBLOCKING},
                                           {STORE_AND_LEN("mpi_irecv_"), MPI_RECV_NONBLOCKING},
                                           {STORE_AND_LEN("MPI_Barrier"), BARRIER_WAIT},
                                           {STORE_AND_LEN("mpi_barrier_"), BARRIER_WAIT},
                                           {STORE_AND_LEN("MPI_Wait"), MPI_TRANSFER_WAIT},
                                           {STORE_AND_LEN("mpi_wait_"), MPI_TRANSFER_WAIT}};

    //
    // Contech - First record every load or store in a program
    //
    class Contech : public ModulePass {
    public:
        static char ID; // Pass identification, replacement for typeid
        Constant* storeBasicBlockFunction;
        Constant* storeBasicBlockCompFunction;
        Constant* storeMemOpFunction;
        Constant* threadInitFunction;
        Constant* allocateBufferFunction;
        Constant* checkBufferFunction;
        Constant* storeThreadCreateFunction;
        Constant* storeSyncFunction;
        Constant* storeMemoryEventFunction;
        Constant* queueBufferFunction;
        Constant* storeBarrierFunction;
        Constant* allocateCTidFunction;
        Constant* getThreadNumFunction;
        Constant* storeThreadJoinFunction;
        Constant* storeThreadInfoFunction;
        Constant* storeBulkMemoryOpFunction;
        Constant* getCurrentTickFunction;
        Constant* createThreadActualFunction;
        Constant* checkBufferLargeFunction;
        
        Constant* storeBasicBlockMarkFunction;
        Constant* storeMemReadMarkFunction;
        Constant* storeMemWriteMarkFunction;

        Constant* storeMPITransferFunction;
        Constant* storeMPIWaitFunction;
        
        Constant* ompThreadCreateFunction;
        Constant* ompThreadJoinFunction;
        Constant* ompTaskCreateFunction;
        Constant* ompTaskJoinFunction;
        Constant* ompPushParentFunction;
        Constant* ompPopParentFunction;
        Constant* ctPeekParentIdFunction;
        
        Constant* ompGetParentFunction;
        
        Constant* pthreadExitFunction;
        GlobalVariable* threadLocalNumber;
        DataLayout* currentDataLayout;
        Type* int8Ty;
        Type* int32Ty;
        Type* voidTy;
        PointerType* voidPtrTy;
        Type* int64Ty;
        Type* pthreadTy;
        Type* threadArgsTy;  // needed when wrapping pthread_create
        std::set<Function*> contechAddedFunctions;
        std::set<Function*> ompMicroTaskFunctions;
        
        Contech() : ModulePass(ID) {
        }
        
        virtual bool doInitialization(Module &M);
        virtual bool runOnModule(Module &M);
        virtual bool internalRunOnBasicBlock(BasicBlock &B,  Module &M, int bbid, bool markOnly, const char* fnName);
        virtual bool internalSplitOnCall(BasicBlock &B, CallInst**, int*);
        void addCheckAfterPhi(BasicBlock* B);
        void internalAddAllocate(BasicBlock& B);
        pllvm_mem_op insertMemOp(Instruction* li, Value* addr, bool isWrite, unsigned int memOpPos, Value*);
        unsigned int getSizeofType(Type*);
        unsigned int getSimpleLog(unsigned int);
        unsigned int getCriticalPathLen(BasicBlock& B);
        Function* createMicroTaskWrapStruct(Function* ompMicroTask, Type* arg, Module &M);
        Function* createMicroTaskWrap(Function* ompMicroTask, Module &M);
        Value* castSupport(Type*, Value*, Instruction*);
        void insertMPITransfer(bool, bool, Value*, CallInst*);
        
        virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        
        }
    };
    ModulePass* createContechPass() { return new Contech(); }
}    
    //
    // Create any globals requied for this module
    //
    bool Contech::doInitialization(Module &M)
    {
        // Function types are named fun(Return type)(arg1 ... argN)Ty
        FunctionType* funVoidPtrI32Ty;
        FunctionType* funVoidVoidPtrI32VoidPtrTy;
        FunctionType* funVoidVoidPtrI32I32I64Ty;
        FunctionType* funVoidPtrVoidPtrTy;
        FunctionType* funVoidPtrVoidTy;
        FunctionType* funVoidVoidTy;
        FunctionType* funVoidI8I64VoidPtrTy;
        FunctionType* funVoidI64VoidPtrVoidPtrTy;
        FunctionType* funVoidI8Ty;
        FunctionType* funVoidI32Ty;
        FunctionType* funVoidI8VoidPtrI64Ty;
        FunctionType* funVoidVoidPtrI32Ty;
        FunctionType* funVoidI64I64Ty;
        FunctionType* funVoidI8I8I32I32I32I32VoidPtrI64VoidPtrTy;
        FunctionType* funI32I32Ty;
        FunctionType* funVoidVoidPtrI64Ty;

        LLVMContext &ctx = M.getContext();
        #if LLVM_VERSION_MINOR>=5
        currentDataLayout = M.getDataLayout();
        #else
        currentDataLayout = new DataLayout(&M);
        #endif
        int8Ty = Type::getInt8Ty(ctx);
        int32Ty = Type::getInt32Ty(ctx);
        int64Ty = Type::getInt64Ty(ctx);
        voidTy = Type::getVoidTy(ctx);
        voidPtrTy = int8Ty->getPointerTo();

        Type* funVoidPtrVoidTypes[] = {voidPtrTy};
        funVoidPtrVoidTy = FunctionType::get(voidPtrTy, ArrayRef<Type*>(funVoidPtrVoidTypes, 1), false);

        Type* threadStructTypes[] = {static_cast<Type *>(funVoidPtrVoidTy)->getPointerTo(), voidPtrTy, int32Ty, int32Ty};
        threadArgsTy = StructType::create(ArrayRef<Type*>(threadStructTypes, 4), "contech_thread_create", false);
        
        Type* argsBB[] = {int32Ty};
        funVoidPtrI32Ty = FunctionType::get(voidPtrTy, ArrayRef<Type*>(argsBB, 1), false);
        storeBasicBlockFunction = M.getOrInsertFunction("__ctStoreBasicBlock", funVoidPtrI32Ty);
        Type* argsMO[] = {voidPtrTy, int32Ty, voidPtrTy};
        funVoidVoidPtrI32VoidPtrTy = FunctionType::get(voidTy, ArrayRef<Type*>(argsMO, 3), false);
        storeMemOpFunction = M.getOrInsertFunction("__ctStoreMemOp", funVoidVoidPtrI32VoidPtrTy);
        Type* argsInit[] = {voidPtrTy};//threadArgsTy->getPointerTo()};
        funVoidPtrVoidPtrTy = FunctionType::get(voidPtrTy, ArrayRef<Type*>(argsInit, 1), false);
        threadInitFunction = M.getOrInsertFunction("__ctInitThread", funVoidPtrVoidPtrTy);

        // void (void) functions:
        funVoidVoidTy = FunctionType::get(voidTy, false);
        allocateBufferFunction = M.getOrInsertFunction("__ctAllocateLocalBuffer", funVoidVoidTy);
        storeMemReadMarkFunction = M.getOrInsertFunction("__ctStoreMemReadMark", funVoidVoidTy);
        storeMemWriteMarkFunction = M.getOrInsertFunction("__ctStoreMemWriteMark", funVoidVoidTy);
        ompPushParentFunction = M.getOrInsertFunction("__ctOMPPushParent", funVoidVoidTy);
        ompPopParentFunction = M.getOrInsertFunction("__ctOMPPopParent", funVoidVoidTy);
        
        allocateCTidFunction = M.getOrInsertFunction("__ctAllocateCTid", FunctionType::get(int32Ty, false));
        getThreadNumFunction = M.getOrInsertFunction("__ctGetLocalNumber", FunctionType::get(int32Ty, false));
        getCurrentTickFunction = M.getOrInsertFunction("__ctGetCurrentTick", FunctionType::get(int64Ty, false));
        ompGetParentFunction = M.getOrInsertFunction("__kmpc_get_parent_taskid", FunctionType::get(int64Ty, false));
        ctPeekParentIdFunction = M.getOrInsertFunction("__ctPeekParent", FunctionType::get(int32Ty, false));
        
        Type* argsSSync[] = {voidPtrTy, int32Ty/*type*/, int32Ty/*retVal*/, int64Ty /*ct_tsc_t*/};
        funVoidVoidPtrI32I32I64Ty = FunctionType::get(voidTy, ArrayRef<Type*>(argsSSync, 4), false);
        storeSyncFunction = M.getOrInsertFunction("__ctStoreSync", funVoidVoidPtrI32I32I64Ty);
        
        Type* argsTC[] = {int32Ty};
        
        // TODO: See how one might flag a function as having the attribute of "does not return", for exit()
        funVoidI32Ty = FunctionType::get(voidTy, ArrayRef<Type*>(argsTC, 1), false);
        storeBasicBlockMarkFunction = M.getOrInsertFunction("__ctStoreBasicBlockMark", funVoidI32Ty);
        pthreadExitFunction = M.getOrInsertFunction("pthread_exit", funVoidI32Ty);
        ompThreadCreateFunction = M.getOrInsertFunction("__ctOMPThreadCreate", funVoidI32Ty);
        ompThreadJoinFunction = M.getOrInsertFunction("__ctOMPThreadJoin", funVoidI32Ty);
        ompTaskCreateFunction = M.getOrInsertFunction("__ctOMPTaskCreate", funVoidI32Ty);
        checkBufferFunction = M.getOrInsertFunction("__ctCheckBufferSize", funVoidI32Ty);
        checkBufferLargeFunction = M.getOrInsertFunction("__ctCheckBufferBySize", funVoidI32Ty);
        
        funI32I32Ty = FunctionType::get(int32Ty, ArrayRef<Type*>(argsTC, 1), false);
        storeBasicBlockCompFunction = M.getOrInsertFunction("__ctStoreBasicBlockComplete", funI32I32Ty);
       
        Type* argsQB[] = {int8Ty};
        funVoidI8Ty = FunctionType::get(voidTy, ArrayRef<Type*>(argsQB, 1), false);
        queueBufferFunction = M.getOrInsertFunction("__ctQueueBuffer", funVoidI8Ty);
        ompTaskJoinFunction = M.getOrInsertFunction("__ctOMPTaskJoin", funVoidVoidTy);
        
        Type* argsSB[] = {int8Ty, voidPtrTy, int64Ty};
        funVoidI8VoidPtrI64Ty = FunctionType::get(voidTy, ArrayRef<Type*>(argsSB, 3), false);
        storeBarrierFunction = M.getOrInsertFunction("__ctStoreBarrier", funVoidI8VoidPtrI64Ty);
        
        Type* argsATI[] = {voidPtrTy, int32Ty};
        funVoidVoidPtrI32Ty = FunctionType::get(voidTy, ArrayRef<Type*>(argsATI, 2), false);
        storeThreadInfoFunction = M.getOrInsertFunction("__ctAddThreadInfo", funVoidVoidPtrI32Ty);
        
        Type* argsSMPIXF[] = {int8Ty, int8Ty, int32Ty, int32Ty, int32Ty, int32Ty, voidPtrTy, int64Ty, voidPtrTy};
        funVoidI8I8I32I32I32I32VoidPtrI64VoidPtrTy = FunctionType::get(voidTy, ArrayRef<Type*>(argsSMPIXF, 9), false);
        storeMPITransferFunction = M.getOrInsertFunction("__ctStoreMPITransfer", funVoidI8I8I32I32I32I32VoidPtrI64VoidPtrTy);
        
        Type* argsMPIW[] = {voidPtrTy, int64Ty};
        funVoidVoidPtrI64Ty = FunctionType::get(voidTy, ArrayRef<Type*>(argsMPIW, 2), false);
        storeMPIWaitFunction = M.getOrInsertFunction("__ctStoreMPIWait", funVoidVoidPtrI64Ty);
        
        // This needs to be machine type here
        //
        #if LLVM_VERSION_MINOR>=5
        // LLVM 3.5 removed module::getPointerSize() -> DataLayout::getPointerSize()
        //   Let's find malloc instead, which takes size_t and size_t is the size we need
        //auto mFunc = M.getFunction("malloc");
        //FunctionType* mFuncTy = mFunc->getFunctionType();
        //pthreadTy = mFuncTy->getParamType(0);
        if (currentDataLayout->getPointerSize() == llvm::Module::Pointer64)
        {
            pthreadTy = int64Ty;
        }
        else
        {
            pthreadTy = int32Ty;
        }
        #else
        if (M.getPointerSize() == llvm::Module::Pointer64)
        {
            pthreadTy = int64Ty;
        }
        else
        {
            pthreadTy = int32Ty;
        }
        #endif
        
        Type* argsSTJ[] = {pthreadTy, int64Ty};
        funVoidI64I64Ty = FunctionType::get(voidTy, ArrayRef<Type*>(argsSTJ, 2), false);
        
        Type* argsME[] = {int8Ty, pthreadTy, voidPtrTy};
        funVoidI8I64VoidPtrTy = FunctionType::get(voidTy, ArrayRef<Type*>(argsME, 3), false);
        storeMemoryEventFunction = M.getOrInsertFunction("__ctStoreMemoryEvent", funVoidI8I64VoidPtrTy);
        Type* argsBulkMem[] = {pthreadTy, voidPtrTy, voidPtrTy};
        funVoidI64VoidPtrVoidPtrTy = FunctionType::get(voidTy, ArrayRef<Type*>(argsBulkMem, 3), false);
        storeBulkMemoryOpFunction = M.getOrInsertFunction("__ctStoreBulkMemoryEvent", funVoidI64VoidPtrVoidPtrTy);
        
        storeThreadJoinFunction = M.getOrInsertFunction("__ctStoreThreadJoin", funVoidI64I64Ty);
        
        Type* pthreadTyPtr = pthreadTy->getPointerTo();
        Type* argsCTA[] = {pthreadTyPtr, 
                           voidPtrTy, 
                           static_cast<Type *>(funVoidPtrVoidTy)->getPointerTo(),
                           voidPtrTy};
        FunctionType* funIntPthreadPtrVoidPtrVoidPtrFunVoidPtr = FunctionType::get(int32Ty, ArrayRef<Type*>(argsCTA,4), false);
        createThreadActualFunction = M.getOrInsertFunction("__ctThreadCreateActual", funIntPthreadPtrVoidPtrVoidPtrFunVoidPtr);
		
        return true;
    }
    
    _CONTECH_FUNCTION_TYPE classifyFunctionName(const char* fn)
    {
        for (unsigned int i = 0; i < FUNCTIONS_INSTRUMENT_SIZE; i++)
        {
            if (strncmp(fn, functionsInstrument[i].func_name, functionsInstrument[i].str_len - 1) == 0)
            {
                return functionsInstrument[i].typeID;
            }
        }
    
        return NONE;
    }
    
    //
    // addCheckAfterPhi
    //   Adds a check buffer function call after the last Phi instruction in a basic block
    //   Trying to add the check call as early as possible, but some instructions must come first
    //
    void Contech::addCheckAfterPhi(BasicBlock* B)
    {
        if (B == NULL) return;
        
        for (BasicBlock::iterator I = B->begin(), E = B->end(); I != E; ++I){
            if (/*PHINode *pn = */dyn_cast<PHINode>(&*I)) {
                continue;
            }
            else if (/*LandingPadInst *lpi = */dyn_cast<LandingPadInst>(&*I)){
                continue;
            }
            else
            {
                debugLog("checkBufferFunction @" << __LINE__);
                CallInst::Create(checkBufferFunction, "", I);
                return;
            }
        }
    }
    
    //
    // Scan through each instruction in the basic block
    //   For each op used by the instruction, this instruction is 1 deeper than it
    //
    unsigned int Contech::getCriticalPathLen(BasicBlock& B)
    {
        map<Instruction*, unsigned int> depthOfInst;
        unsigned int maxDepth = 0;
        
        for (BasicBlock::iterator it = B.begin(), et = B.end(); it != et; ++it)
        {
            unsigned int currIDepth = 0;
            for (User::op_iterator itUse = it->op_begin(), etUse = it->op_end(); 
                                 itUse != etUse; ++itUse)
            {
                unsigned int tDepth;
                Instruction* inU = dyn_cast<Instruction>(itUse->get());
                map<Instruction*, unsigned int>::iterator depI = depthOfInst.find(inU);
                
                if (depI == depthOfInst.end())
                {
                    // Dependent on value from a different basic block
                    tDepth = 1;
                }
                else
                {
                    // The path in this block is 1 more than the dependent instruction's depth
                    tDepth = depI->second + 1;
                }
                
                if (tDepth > currIDepth) currIDepth = tDepth;
            }
            
            depthOfInst[&*it] = currIDepth;
            if (currIDepth > maxDepth) maxDepth = currIDepth;
        }
    
        return maxDepth;
    }
    
    //
    //  Wrapper call that appropriately adds the operations to record the memory operation
    //
    pllvm_mem_op Contech::insertMemOp(Instruction* li, Value* addr, bool isWrite, unsigned int memOpPos, Value* pos)
    {
        pllvm_mem_op tMemOp = new llvm_mem_op;
        
        tMemOp->addr = NULL;
        tMemOp->next = NULL;
        tMemOp->isWrite = isWrite;
        tMemOp->size = getSimpleLog(getSizeofType(addr->getType()->getPointerElementType()));
        
        if (tMemOp->size > 4)
        {
            errs() << "MemOp of size: " << tMemOp->size << "\n";
        }
        
        if (GlobalValue* gv = dyn_cast<GlobalValue>(addr))
        {
            tMemOp->isGlobal = true;
            //errs() << "Is global - " << *addr << "\n";
            //return tMemOp; // HACK!
        }
        else
        {
            tMemOp->isGlobal = false;
            //errs() << "Is not global - " << *addr << "\n";
        }
        
        //Constant* cIsWrite = ConstantInt::get(int8Ty, isWrite);
        //Constant* cSize = ConstantInt::get(int8Ty, tMemOp->size);
        Constant* cPos = ConstantInt::get(int32Ty, memOpPos);
        Value* addrI = new BitCastInst(addr, voidPtrTy, Twine("Cast as void"), li);
        Value* argsMO[] = {addrI, cPos, pos};
        debugLog("storeMemOpFunction @" << __LINE__);
        CallInst* smo = CallInst::Create(storeMemOpFunction, ArrayRef<Value*>(argsMO, 3), "", li);
        assert(smo != NULL);
        smo->getCalledFunction()->addFnAttr( ALWAYS_INLINE );
        
        return tMemOp;
    }
    
    void Contech::insertMPITransfer(bool isSend, bool isBlocking, Value* startTime, CallInst* ci)
    {
        Constant* cSend = ConstantInt::get(int8Ty, isSend);
        Constant* cBlock = ConstantInt::get(int8Ty, isBlocking);
        Value* reqArg = NULL;
        
        if (isBlocking == true)
        {
            reqArg = ConstantPointerNull::get(voidPtrTy);
        }
        else
        {
            reqArg = castSupport(voidPtrTy, ci->getArgOperand(6), ci);
        }
        
        // Fortran sometimes passes with pointers instead of values
        Value* argsMPIXF[] = {cSend, 
                              cBlock, 
                              new LoadInst(ci->getArgOperand(1), "", ci),  
                              castSupport(int32Ty, ci->getArgOperand(2), ci), // datatype, constant
                              new LoadInst(ci->getArgOperand(3), "", ci), 
                              new LoadInst(ci->getArgOperand(4), "", ci), 
                              castSupport(voidPtrTy, ci->getArgOperand(0), ci), 
                              startTime,
                              reqArg};
        
        errs() << "Adding MPI Transfer call\n";
        
        // Need to insert after ci...
        CallInst* ciStore = CallInst::Create(storeMPITransferFunction, 
                                             ArrayRef<Value*>(argsMPIXF, 9), 
                                             "", ci);
        ci->moveBefore(ciStore);
    }
    
    void Contech::internalAddAllocate(BasicBlock& B)
    {
        debugLog("allocateBufferFunction @" << __LINE__);
        CallInst::Create(allocateBufferFunction, "", B.begin());
    }
    
    //
    // Go through the module to get the basic blocks
    //
    bool Contech::runOnModule(Module &M)
    {
        unsigned int bb_count = 0;
        int length = 0;
        char* buffer = NULL;
        doInitialization(M);
        
        ifstream* icontechStateFile = new ifstream(ContechStateFilename.c_str(), ios_base::in | ios_base::binary);
        if (icontechStateFile != NULL && icontechStateFile->good())
        {
            icontechStateFile->seekg(0, icontechStateFile->end);
            length = icontechStateFile->tellg();
            icontechStateFile->seekg(0, icontechStateFile->beg);
            buffer = new char[length];
            icontechStateFile->read(buffer, length);
            bb_count = *(unsigned int*)buffer;
        }
        else
        {
            //errs() << contechStateFile->rdstate() << "\t" << contechStateFile->fail() << "\n";
            //errs() << ContechStateFilename.c_str() << "\n";
        }
        
        // for unknown reasons, a file that does not exist needs to clear all bits and not
        // just eof for writing
        icontechStateFile->close();
        delete icontechStateFile;
        
        for (Module::iterator F = M.begin(), FE = M.end(); F != FE; ++F) {
            int status;
            const char* fmn = F->getName().data();
            char* fn = abi::__cxa_demangle(fmn, 0, 0, &status);
            bool inMain = false;

            // If status is 0, then we demangled the name
            if (status != 0)
            {
                // fmn is original name string
            }
            else
            {
                fmn = fn;
            }
            
            // Replace Main with a different main, if Contech is inserting the runtime
            //   and associated instrumentation
            if (__ctStrCmp(fmn, "main\0") == 0)
            {
                // Only rename main if this is not the marker front end
                if (ContechMarkFrontend == false)
                {
                    // This invalidates F->getName(), ie possibly fmn is invalid
                    //F->setName(Twine("ct_orig_main"));
                    inMain = true;
                }
            }
            // Add other functions that Contech should not instrument here
            // NB Main is checked above and is special cased
            else if (classifyFunctionName(fmn) != NONE)
            {
                errs() << "SKIP: " << fmn << "\n";
                if (fmn == fn)
                {
                    free(fn);
                }
                continue;
            }
            // If this function is one Contech adds when instrumenting for OpenMP
            // then it can be skipped
            else if (contechAddedFunctions.find(&*F) != contechAddedFunctions.end())
            {
                errs() << "SKIP: " << fmn << "\n";
                if (fmn == fn)
                {
                    free(fn);
                }
                continue;
            }
            
            if (F->size() == 0)
            {
                if (fmn == fn)
                {
                    free(fn);
                }
                continue;
            }
            errs() << fmn << "\n";
        
            // "Normalize" every basic block to have only one function call in it
            for (Function::iterator B = F->begin(), BE = F->end(); B != BE; ) {
                BasicBlock &pB = *B;
                CallInst *ci;
                int status = 0;

                if (internalSplitOnCall(pB, &ci, &status) == false)
                {
                    B++;
                    #ifdef SPLIT_DEBUG
                    if (ci != NULL)
                        errs() << status << "\t" << *ci << "\n";
                    #endif
                }
                else {
                    
                }
            }
        
            // Now instrument each basic block in the function
            for (Function::iterator B = F->begin(), BE = F->end(); B != BE; ++B) {
                BasicBlock &pB = *B;
                
                internalRunOnBasicBlock(pB, M, bb_count, ContechMarkFrontend, fmn);
                bb_count++;
            }
            
            // If fmn is fn, then it was allocated by the demangle routine and we are required to free
            if (fmn == fn)
            {
                free(fn);
            }
            
            // Renaming invalidates the current name of the function
            if (inMain == true)
            {
                F->setName(Twine("ct_orig_main"));
            }
        }
        
        if (ContechMarkFrontend == true) goto cleanup;
        
cleanup:     
        ofstream* contechStateFile = new ofstream(ContechStateFilename.c_str(), ios_base::out | ios_base::binary);
        //contechStateFile->seekp(0, ios_base::beg);
        if (buffer == NULL)
        {
            // New state file starts with the basic block count
            contechStateFile->write((char*)&bb_count, sizeof(unsigned int));
        }
        else
        {
            // Write the existing data back out
            //   First, put a new basic block count at the start of the existing data
            *(unsigned int*) buffer = bb_count;
            contechStateFile->write(buffer, length);
        }
        //contechStateFile->seekp(0, ios_base::end);
        
        if (ContechMarkFrontend == false && ContechMinimal == false)
        {
            int wcount = 0;
            char evTy = ct_event_basic_block_info;
            for (map<BasicBlock*, llvm_basic_block*>::iterator bi = cfgInfoMap.begin(), bie = cfgInfoMap.end(); bi != bie; ++bi)
            {
                pllvm_mem_op t = bi->second->first_op;
                
                // Write out basic block info events
                //   Then runtime can directly pass the events to the event list
                contechStateFile->write((char*)&evTy, sizeof(char));
                contechStateFile->write((char*)&bi->second->id, sizeof(unsigned int));
                // This is the flags field, which is currently 0 or 1 for containing a call
                unsigned int flags = ((unsigned int)bi->second->containCall) | 
                                     ((unsigned int)bi->second->containGlobalAccess << 1);
                contechStateFile->write((char*)&flags, sizeof(unsigned int));
                contechStateFile->write((char*)&bi->second->lineNum, sizeof(unsigned int));
                contechStateFile->write((char*)&bi->second->numIROps, sizeof(unsigned int));
                contechStateFile->write((char*)&bi->second->critPathLen, sizeof(unsigned int));
                
                int strLen = bi->second->fnName.length();//(bi->second->fnName != NULL)?strlen(bi->second->fnName):0;
                contechStateFile->write((char*)&strLen, sizeof(int));
                *contechStateFile << bi->second->fnName;
                //contechStateFile->write(bi->second->fnName, strLen * sizeof(char));
                
                
                strLen = (bi->second->fileName != NULL)?strlen(bi->second->fileName):0;
                contechStateFile->write((char*)&strLen, sizeof(int));
                contechStateFile->write(bi->second->fileName, strLen * sizeof(char));
                contechStateFile->write((char*)&bi->second->len, sizeof(unsigned int));
                
                while (t != NULL)
                {
                    pllvm_mem_op tn = t->next;
                    contechStateFile->write((char*)&t->isWrite, sizeof(bool));
                    contechStateFile->write((char*)&t->size, sizeof(char));
                    delete (t);
                    t = tn;
                }
                wcount++;
                free(bi->second);
            }
        }
        //errs() << "Wrote: " << wcount << " basic blocks\n";
        cfgInfoMap.clear();
        contechStateFile->close();
        delete contechStateFile;
        
        return true;
    }
    
    // returns size in bytes
    unsigned int Contech::getSizeofType(Type* t)
    {
        unsigned int r = t->getPrimitiveSizeInBits();
        if (r > 0) return r / 8;
        else if (t->isPointerTy()) { return 8;}
        else if (t->isVectorTy()) { return t->getVectorNumElements() * t->getScalarSizeInBits();}
        else if (t->isPtrOrPtrVectorTy()) { errs() << *t << " is pointer vector\n";}
        else if (t->isArrayTy()) { errs() << *t << " is array\n";}
        else if (t->isStructTy()) { errs() << *t << " is struct\n";}
        
        // DataLayout::getStructLayout(StructType*)->getSizeInBytes()
        StructType* st = dyn_cast<StructType>(t);
        if (st == NULL)
        {
            errs() << "Failed get size - " << *t << "\n";
            return 0;
        }
        auto stLayout = currentDataLayout->getStructLayout(st);
        if (stLayout == NULL)
        {
            errs() << "Failed get size - " << *t << "\n";
        }
        else
        {
            return stLayout->getSizeInBytes();
        }
        
        return 0;
    }    
    
    // base 2 log of a value
    unsigned int Contech::getSimpleLog(unsigned int i)
    {
        if (i > 128) {return 8;}
        if (i > 64) { return 7;}
        if (i > 32) { return 6;}
        if (i > 16) { return 5;}
        if (i > 8) { return 4;}
        if (i > 4) { return 3;}
        if (i > 2) { return 2;}
        if (i > 1) { return 1;}
        return 0;
    }

    bool Contech::internalSplitOnCall(BasicBlock &B, CallInst** tci, int* st)
    {
        *tci = NULL;
        for (BasicBlock::iterator I = B.begin(), E = B.end(); I != E; ++I){
            if (CallInst *ci = dyn_cast<CallInst>(&*I)) {
                *tci = ci;
                if (ci->isTerminator()) {*st = 1; return false;}
                if (ci->doesNotReturn()) {*st = 2; return false;}
                Function* f = ci->getCalledFunction();
                
                //
                // If F == NULL, then f is indirect
                //   O.w. this function may just be an annotation and can be ignored
                //
                if (f != NULL)
                {
                    const char* fn = f->getName().data();
                    if (0 == __ctStrCmp(fn, "llvm.dbg") ||
                        0 == __ctStrCmp(fn, "llvm.lifetime")) 
                    {
                        *st = 3;
                        continue;
                    }
                
                }
                
                I++;
                
                // At this point, the call instruction returns and was not the last in the block
                //   If the next instruction is a terminating, unconditional branch then splitting
                //   is redundant.  (as splits create a terminating, unconditional branch)
                if (I->isTerminator())
                {
                	if (BranchInst *bi = dyn_cast<BranchInst>(&*I))
                	{
                		if (bi->isUnconditional()) {*st = 4; return false;}
                	}
                    else if (/* ReturnInst *ri = */ dyn_cast<ReturnInst>(&*I))
                    {
                        *st = 5;
                        return false;
                    }
                }
                B.splitBasicBlock(I, "");
                return true;
            }
        }
    
        return false;
    }
    
    //
    // For each basic block
    //
    bool Contech::internalRunOnBasicBlock(BasicBlock &B,  Module &M, int bbid, const bool markOnly, const char* fnName)
    {
        Instruction* iPt = B.getTerminator();
        std::vector<pllvm_mem_op> opsInBlock;
        ct_event_id containingEvent = ct_event_basic_block;
        unsigned int memOpCount = 0;
        Instruction* aPhi = B.begin();
        bool getNextI = false;
        bool containCall = false, containQueueBuf = false;
        bool hasUninstCall = false;
        bool containKeyCall = false;
        Value* posValue = NULL;
        unsigned int lineNum = 0, numIROps = B.size();
        
        //errs() << "BB: " << bbid << "\n";
        
        for (BasicBlock::iterator I = B.begin(), E = B.end(); I != E; ++I){
            MDNode *N;
            if (lineNum == 0 && (N = I->getMetadata("dbg"))) {
                DILocation Loc(N);
                
                lineNum = Loc.getLineNumber();
            }
        
            // TODO: Use BasicBlock->getFirstNonPHIOrDbgOrLifetime as insertion point
            //   compare with getFirstInsertionPt
            if (/*PHINode *pn = */dyn_cast<PHINode>(&*I)) {
                getNextI = true;
                numIROps --;
                continue;
            }
            else if (/*LandingPadInst *lpi = */dyn_cast<LandingPadInst>(&*I)){
                getNextI = true;
                numIROps --;
                continue;
            }
            else if (/*LoadInst *li = */dyn_cast<LoadInst>(&*I)){
                memOpCount ++;
            }
            else if (/*StoreInst *si = */dyn_cast<StoreInst>(&*I)) {
                memOpCount ++;
            }
            else if (ContechMinimal == true)
            {
                if (CallInst* ci = dyn_cast<CallInst>(&*I)) {
                    Function *f = ci->getCalledFunction();
                    
                    // call is indirect
                    // TODO: add dynamic check on function called
                    if (f == NULL) { continue; }

                    int status;
                    const char* fmn = f->getName().data();
                    char* fdn = abi::__cxa_demangle(fmn, 0, 0, &status);
                    const char* fn = fdn;
                    if (status != 0) 
                    {
                        fn = fmn;
                    }
                    
                    CONTECH_FUNCTION_TYPE tID = classifyFunctionName(fn);
                    if (tID == EXIT || // We need to replace exit otherwise the trace is corrupt
                        tID == SYNC_ACQUIRE ||
                        tID == SYNC_RELEASE ||
                        tID == BARRIER_WAIT ||
                        tID == THREAD_CREATE ||
                        tID == THREAD_JOIN ||
                        tID == COND_WAIT ||
                        tID == COND_SIGNAL)
                    {
                        containKeyCall = true;
                    }
                    
                    if (status == 0)
                    {
                        free(fdn);
                    }
                }
            
            }
            
            // LLVM won't do insertAfter, so we have to get the instruction after the instruction
            // to insert before it
            if (getNextI == true)
            {
                aPhi = I;
                getNextI = false;
            }
        }
        
        if (ContechMinimal == true && containKeyCall == false)
        {
            return false;
        }
        
        llvm_basic_block* bi = new llvm_basic_block;
        if (bi == NULL)
        {
            errs() << "Cannot record CFG in Contech\n";
            return true;
        }
        bi->id = bbid;
        bi->first_op = NULL;
        bi->containGlobalAccess = false;
        bi->lineNum = lineNum;
        bi->numIROps = numIROps;
        bi->fnName.assign(fnName);
        bi->fileName = M.getModuleIdentifier().data();
        bi->critPathLen = getCriticalPathLen(B);
        
        //errs() << "Basic Block - " << bbid << " -- " << memOpCount << "\n";
        //debugLog("checkBufferFunction @" << __LINE__);
        //CallInst::Create(checkBufferFunction, "", aPhi);
        Constant* llvm_bbid;
        Constant* llvm_nops = NULL;
        CallInst* sbb;
        CallInst* sbbc = NULL;
        unsigned int memOpPos = 0;
        
        if (markOnly == true)
        {
            llvm_bbid = ConstantInt::get(int32Ty, bbid);
            Value* argsBB[] = {llvm_bbid};
            debugLog("storeBasicBlockMarkFunction @" << __LINE__);
            sbb = CallInst::Create(storeBasicBlockMarkFunction, ArrayRef<Value*>(argsBB, 1), "", aPhi);
        }
        else {
            llvm_bbid = ConstantInt::get(int32Ty, bbid);
            Value* argsBB[] = {llvm_bbid};
            debugLog("storeBasicBlockFunction for BBID: " << bbid << " @" << __LINE__);
            sbb = CallInst::Create(storeBasicBlockFunction, 
                                   ArrayRef<Value*>(argsBB, 1), 
                                   string("storeBlock") + to_string(bbid), 
                                   aPhi);
            sbb->getCalledFunction()->addFnAttr( ALWAYS_INLINE);
            posValue = sbb;

//#define TSC_IN_BB
#ifdef TSC_IN_BB
            Value* stTick = CallInst::Create(getCurrentTickFunction, "tick", aPhi);
            
            //pllvm_mem_op tMemOp = insertMemOp(aPhi, stTick, true, memOpPos, posValue);
            pllvm_mem_op tMemOp = new llvm_mem_op;
        
            tMemOp->isWrite = true;
            tMemOp->size = 7;
            
            Constant* cPos = ConstantInt::get(int32Ty, memOpPos);
            Value* addrI = castSupport(voidPtrTy, stTick, aPhi);
            Value* argsMO[] = {addrI, cPos, sbb};
            debugLog("storeMemOpFunction @" << __LINE__);
            CallInst* smo = CallInst::Create(storeMemOpFunction, ArrayRef<Value*>(argsMO, 3), "", aPhi);
            assert(smo != NULL);
            smo->getCalledFunction()->addFnAttr( ALWAYS_INLINE );
            
            tMemOp->addr = NULL;
            tMemOp->next = NULL;
            
            memOpPos ++;
            memOpCount++;
            if (bi->first_op == NULL) bi->first_op = tMemOp;
            else
            {
                pllvm_mem_op t = bi->first_op;
                while (t->next != NULL)
                {
                    t = t->next;
                }
                t->next = tMemOp;
            }
#endif
            
            // In LLVM 3.3+, switch to Monotonic and not Acquire
            new FenceInst(M.getContext(), Acquire, SingleThread, sbb);
        }

        
        bool hasInstAllMemOps = false;
        for (BasicBlock::iterator I = B.begin(), E = B.end(); I != E; ++I){
        
            // After all of the known memOps have been instrumented, close out the basic
            //   block event based on the number of memOps
            if (hasInstAllMemOps == false && memOpPos == memOpCount && markOnly == false)
            {
                llvm_nops = ConstantInt::get(int32Ty, memOpCount);
                Value* argsBBc[] = {llvm_nops};
                #ifdef TSC_IN_BB
                if (memOpCount == 1)
                #else
                if (memOpCount == 0)
                #endif
                {
                    debugLog("storeBasicBlockCompFunction @" << __LINE__);
                    sbbc = CallInst::Create(storeBasicBlockCompFunction, ArrayRef<Value*>(argsBBc, 1), "", aPhi);
                    
                    new FenceInst(M.getContext(), Release, SingleThread, aPhi);
                    iPt = aPhi;
                }
                else
                {
                    debugLog("storeBasicBlockCompFunction @" << __LINE__);
                    sbbc = CallInst::Create(storeBasicBlockCompFunction, ArrayRef<Value*>(argsBBc, 1), "", I);
                    new FenceInst(M.getContext(), Release, SingleThread, I);
                    iPt = I;
                }
                sbbc->getCalledFunction()->addFnAttr( ALWAYS_INLINE);
                bi->len = memOpCount;
                hasInstAllMemOps = true;
            }

            // If this block is only being marked, then only memops are needed
            if (markOnly == true)
            {
                // Don't bother maintaining a list of memory ops for the basic block
                //   at this time
                bi->len = 0;
                if (LoadInst *li = dyn_cast<LoadInst>(&*I)){
                    debugLog("storeMemReadMarkFunction @" << __LINE__);
                    CallInst::Create(storeMemReadMarkFunction, "", li);
                }
                else if (StoreInst *si = dyn_cast<StoreInst>(&*I)) {
                    debugLog("storeMemWriteMarkFunction @" << __LINE__);
                    CallInst::Create(storeMemWriteMarkFunction, "", si);
                }
                if (CallInst *ci = dyn_cast<CallInst>(&*I)) {
                
                    if (ci->doesNotReturn())
                    {
                        iPt = ci;
                    }
                }
                continue;
            }
            
            // <result> = load [volatile] <ty>* <pointer>[, align <alignment>][, !nontemporal !<index>][, !invariant.load !<index>]
            // Load and store are identical except the cIsWrite is set accordingly.
            // 
            if (LoadInst *li = dyn_cast<LoadInst>(&*I)) {
                if (memOpPos >= memOpCount) continue;
                pllvm_mem_op tMemOp = insertMemOp(li, li->getPointerOperand(), false, memOpPos, posValue);
                memOpPos ++;
                if (tMemOp->isGlobal)
                {
                    bi->containGlobalAccess = true;
                }
                
                if (bi->first_op == NULL) bi->first_op = tMemOp;
                else
                {
                    pllvm_mem_op t = bi->first_op;
                    while (t->next != NULL)
                    {
                        t = t->next;
                    }
                    t->next = tMemOp;
                }
            }
            //  store [volatile] <ty> <value>, <ty>* <pointer>[, align <alignment>][, !nontemporal !<index>]
            else if (StoreInst *si = dyn_cast<StoreInst>(&*I)) {
                if (memOpPos >= memOpCount) continue;
                pllvm_mem_op tMemOp = insertMemOp(si, si->getPointerOperand(), true, memOpPos, posValue);
                memOpPos ++;
                
                if (tMemOp->isGlobal)
                {
                    bi->containGlobalAccess = true;
                }
                
                if (bi->first_op == NULL) bi->first_op = tMemOp;
                else
                {
                    pllvm_mem_op t = bi->first_op;
                    while (t->next != NULL)
                    {
                        t = t->next;
                    }
                    t->next = tMemOp;
                }
            }
            else if (CallInst *ci = dyn_cast<CallInst>(&*I)) {
                Function *f = ci->getCalledFunction();
                
                // call is indirect
                // TODO: add dynamic check on function called
                if (f == NULL) 
                { 
                    Value* v = ci->getCalledValue();
                    f = dyn_cast<Function>(v->stripPointerCasts());
                    if (f == NULL)
                    {
                        hasUninstCall = true; 
                        continue; 
                    }
                }

                int status;
                const char* fmn = f->getName().data();
                char* fdn = abi::__cxa_demangle(fmn, 0, 0, &status);
                const char* fn = fdn;
                if (status != 0) 
                {
                    fn = fmn;
                }
                if (ci->doesNotReturn())
                {
                    iPt = ci;
                }
                
                CONTECH_FUNCTION_TYPE funTy = classifyFunctionName(fn);
                //errs() << funTy << "\n";
                switch(funTy)
                {
                
                // Check for call to exit(n), replace with pthread_exit(n)
                //  Splash benchmarks like to exit on us which pthread_cleanup doesn't catch
                //  Also check that this "...exit..." is at least a do not return function
                case(EXIT):
                    if (ci->getCalledFunction()->doesNotReturn())
                    {
                        ci->setCalledFunction(pthreadExitFunction);
                    }
                    break;
                case(MALLOC):
                    if (!(ci->getCalledFunction()->getReturnType()->isVoidTy()))
                    {
                        Value* cArg[] = {ConstantInt::get(int8Ty, 1), ci->getArgOperand(0), ci};
                        debugLog("storeMemoryEventFunction @" << __LINE__);
                        CallInst* nStoreME = CallInst::Create(storeMemoryEventFunction, ArrayRef<Value*>(cArg, 3),
                                                            "", ++I);                                 
                        I = nStoreME;
                        hasUninstCall = true;
                    }
                    break;
                case(MALLOC2):
                    if (!(ci->getCalledFunction()->getReturnType()->isVoidTy()))
                    {
                        Value* cArg[] = {ConstantInt::get(int8Ty, 1), ci->getArgOperand(1), ci};
                        debugLog("storeMemoryEventFunction @" << __LINE__);
                        CallInst* nStoreME = CallInst::Create(storeMemoryEventFunction, ArrayRef<Value*>(cArg, 3),
                                                            "", ++I);
                        I = nStoreME;
                        hasUninstCall = true;
                    }
                    break;
                case (FREE):
                {
                    Value* cz = ConstantInt::get(int8Ty, 0);
                    Value* cz32 = ConstantInt::get(pthreadTy, 0);
                    Value* cArg[] = {cz, cz32, ci->getArgOperand(0)};
                    debugLog("storeMemoryEventFunction @" << __LINE__);
                    CallInst* nStoreME = CallInst::Create(storeMemoryEventFunction, ArrayRef<Value*>(cArg, 3),
                                                        "", ++I);
                    I = nStoreME;
                    hasUninstCall = true;
                }
                break;
                case (SYNC_ACQUIRE):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    Value* con1 = ConstantInt::get(int32Ty, 1);
                     // If sync_acquire returns int, pass it, else pass 0 - success
                    Value* retV;
                    if (ci->getType() == int32Ty)
                        retV = ci;
                    else 
                        retV = ConstantInt::get(int32Ty, 0);
                    Value* cArg[] = {new BitCastInst(ci->getArgOperand(0), voidPtrTy, "locktovoid", ci),
                                     con1, 
                                     retV,
                                     nGetTick};
                    debugLog("storeSyncFunction @" << __LINE__);
                    // ++I moves the insertion point to after the sync call
                    CallInst* nStoreSync = CallInst::Create(storeSyncFunction, ArrayRef<Value*>(cArg,4),
                                                        "", ++I);
                    I = nStoreSync;
                    containingEvent = ct_event_sync;
                    iPt = nStoreSync;
                    hasUninstCall = true;
                }
                break;
                case (SYNC_RELEASE):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    // ++I moves the insertion point to after the sync call
                    BitCastInst* bci = new BitCastInst(ci->getArgOperand(0), voidPtrTy, "locktovoid", ++I);
                    Value* cArg[] = {bci, ConstantInt::get(int32Ty, 0), ConstantInt::get(int32Ty, 0), nGetTick};
                    debugLog("storeSyncFunction @" << __LINE__);
                    CallInst* nStoreSync = CallInst::Create(storeSyncFunction, ArrayRef<Value*>(cArg,4),
                                                        "", I);
                    I = nStoreSync;
                    if (hasInstAllMemOps == false)
                    {
                        errs() << "Failed to close storeBasicBlock before storeSync\n";
                    }
                    containingEvent = ct_event_sync;
                    iPt = ++I;
                    I = nStoreSync;
                    hasUninstCall = true;
                }
                break;
                case (GLOBAL_SYNC_ACQUIRE):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    Value* con1 = ConstantInt::get(int32Ty, 1);
                     // If sync_acquire returns int, pass it, else pass 0 - success
                    Value* retV;
                    if (ci->getType() == int32Ty)
                        retV = ci;
                    else 
                        retV = ConstantInt::get(int32Ty, 0);
                    // ++I moves the insertion point to after the sync call
                    Value* cinst = castSupport(voidPtrTy, ConstantInt::get(int64Ty, 0), ++I);
                    Value* cArg[] = {cinst, // No argument, "NULL" is lock
                                     con1, 
                                     retV,
                                     nGetTick};
                    debugLog("storeSyncFunction @" << __LINE__);
                    CallInst* nStoreSync = CallInst::Create(storeSyncFunction, ArrayRef<Value*>(cArg,4),
                                                        "", I); // Insert after sync call
                    I = nStoreSync;
                    containingEvent = ct_event_sync;
                    iPt = nStoreSync;
                    hasUninstCall = true;
                }
                break;
                case (GLOBAL_SYNC_RELEASE):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    // ++I moves the insertion point to after the sync call
                    Value* cinst = castSupport(voidPtrTy, ConstantInt::get(int64Ty, 0), ++I);
                    Value* cArg[] = {cinst, ConstantInt::get(int32Ty, 0), ConstantInt::get(int32Ty, 0), nGetTick};
                    debugLog("storeSyncFunction @" << __LINE__);
                    CallInst* nStoreSync = CallInst::Create(storeSyncFunction, ArrayRef<Value*>(cArg,4),
                                                        "", I);
                    I = nStoreSync;
                    if (hasInstAllMemOps == false)
                    {
                        errs() << "Failed to close storeBasicBlock before storeSync\n";
                    }
                    containingEvent = ct_event_sync;
                    iPt = ++I;
                    I = nStoreSync;
                    hasUninstCall = true;
                }
                break;
                case (COND_WAIT):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    BitCastInst* bciCV = new BitCastInst(ci->getArgOperand(0), voidPtrTy, "locktovoid", ++I);
                    BitCastInst* bciMut = new BitCastInst(ci->getArgOperand(1), voidPtrTy, "locktovoid", ci);
                    Value* cArg[] = {bciMut, ConstantInt::get(int32Ty, 0), ConstantInt::get(int32Ty, 0), nGetTick};
                    debugLog("storeSyncFunction @" << __LINE__);
                    CallInst::Create(storeSyncFunction, ArrayRef<Value*>(cArg,4), "", ci);
                    // TODO: Can avoid queuing after the first sync?
                    if (ContechMinimal == false)
                    {
                        cArg[0] = ConstantInt::get(int8Ty, 1);
                        debugLog("queueBufferFunction @" << __LINE__);
                        CallInst::Create(queueBufferFunction, ArrayRef<Value*>(cArg, 1),
                                                            "", ci);
                        containQueueBuf = true;
                    }
                    
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick2 = CallInst::Create(getCurrentTickFunction, "tick2", I); 
                    Value* retV;
                    if (ci->getType() == int32Ty)
                        retV = ci;
                    else 
                        retV = ConstantInt::get(int32Ty, 0);                    
                    Value* cArgCV[] = {bciCV, ConstantInt::get(int32Ty, 2), retV, nGetTick2};
                    debugLog("storeSyncFunction @" << __LINE__);
                    CallInst* nStoreCV = CallInst::Create(storeSyncFunction, ArrayRef<Value*>(cArgCV, 4), "", I);
                    I = nStoreCV;
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick3 = CallInst::Create(getCurrentTickFunction, "tick3", ++I);                                    
                    Value* cArgMut[] = {bciMut, ConstantInt::get(int32Ty, 1), ConstantInt::get(int32Ty, 0), nGetTick3};
                    debugLog("storeSyncFunction @" << __LINE__);
                    CallInst* nStoreMut = CallInst::Create(storeSyncFunction, ArrayRef<Value*>(cArgMut, 4), "", I);
                    I = nStoreMut;
                    containingEvent = ct_event_sync;
                    iPt = ++I;
                    I = nStoreMut;
                    hasUninstCall = true;
                }
                break;
                case (COND_SIGNAL):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    BitCastInst* bciCV = new BitCastInst(ci->getArgOperand(0), voidPtrTy, "locktovoid", ++I);
                    Value* retV;
                    if (ci->getType() == int32Ty)
                        retV = ci;
                    else 
                        retV = ConstantInt::get(int32Ty, 0);
                    Value* cArgCV[] = {bciCV, ConstantInt::get(int32Ty, 3), retV, nGetTick};
                    debugLog("storeSyncFunction @" << __LINE__);
                    CallInst* nStoreCV = CallInst::Create(storeSyncFunction, ArrayRef<Value*>(cArgCV, 4), "", I);
                    I = nStoreCV;
                    containingEvent = ct_event_sync;
                    iPt = ++I;
                    I = nStoreCV;
                    hasUninstCall = true;
                }
                break;
                case (BARRIER_WAIT):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    BitCastInst* bci = new BitCastInst(ci->getArgOperand(0), voidPtrTy, "locktovoid", I);
                    Value* c1 = ConstantInt::get(int8Ty, 1);
                    Value* cArgs[] = {c1, bci, nGetTick};
                    // Record the barrier entry
                    debugLog("storeBarrierFunction @" << __LINE__);
                    CallInst* nStoreBarEn = CallInst::Create(storeBarrierFunction, ArrayRef<Value*>(cArgs,3),
                                                        "", I);
                    Value* cArg[] = {c1};
                    debugLog("queueBufferFunction @" << __LINE__);
                    /*CallInst* nQueueBuf = */CallInst::Create(queueBufferFunction, ArrayRef<Value*>(cArg, 1),
                                                        "", I);                   
                    ++I;
                    cArgs[0] = ConstantInt::get(int8Ty, 0);
                    // Record the barrier exit
                    debugLog("storeBarrierFunction @" << __LINE__);
                    CallInst* nStoreBarEx = CallInst::Create(storeBarrierFunction, ArrayRef<Value*>(cArgs,3),
                                                        "", I);
                    I = nStoreBarEx;
                    containingEvent = ct_event_barrier;
                    containQueueBuf = true;
                    iPt = nStoreBarEn;
                    hasUninstCall = true;
                }
                break;
                case (THREAD_JOIN):
                {
                    //Value* vPtr = new BitCastInst(ci->getOperand(0), voidPtrTy, "hideInPtr", ci);
                    //errs() << *ci->getOperand(0) << "\t" << *ci->getOperand(0)->getType() << "\n";
                    //errs() << *storeThreadJoinFunction->getType() << "\n";
                    Value* c1 = ConstantInt::get(int8Ty, 1);
                    Value* cArgQB[] = {c1};
                    debugLog("queueBufferFunction @" << __LINE__);
                    /*CallInst* nQueueBuf = */CallInst::Create(queueBufferFunction, ArrayRef<Value*>(cArgQB, 1),
                                                        "", ci);
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    Value* cArg[] = {ci->getOperand(0), nGetTick};
                    debugLog("storeThreadJoinFunction @" << __LINE__);
                    I++;
                    CallInst* nStoreJ = CallInst::Create(storeThreadJoinFunction, ArrayRef<Value*>(cArg, 2), 
                                                         Twine(""), I);
                    containingEvent = ct_event_task_join;
                    I = nStoreJ;
                    iPt = nGetTick;
                    containQueueBuf = true;
                    hasUninstCall = true;
                }
                break;
                //int pthread_create(pthread_t * thread, const pthread_attr_t * attr,
                //                   void * (*start_routine)(void *), void * arg);
                //
                case (THREAD_CREATE):
                {
                    Value* cTcArg[] = {ci->getArgOperand(0), 
                                       new BitCastInst(ci->getArgOperand(1), voidPtrTy, "", ci), 
                                       ci->getArgOperand(2), 
                                       ci->getArgOperand(3)};
                    debugLog("createThreadActualFunction @" << __LINE__);
                    CallInst* nThreadCreate = CallInst::Create(createThreadActualFunction,
                                                               ArrayRef<Value*>(cTcArg, 4), "", ci);
                    ci->replaceAllUsesWith(nThreadCreate);
                    ci->eraseFromParent();
                    I = nThreadCreate;
                    hasUninstCall = true;
                    //ci->setCalledFunction(createThreadActualFunction);
                }
                break;
                case (OMP_END):
                {
                    // End of a parallel region, restore parent stack
                    ++I;
                    debugLog("ompPopParentFunction @" << __LINE__);
                    // Also __ctOMPThreadJoin
                    Value* parentId = CallInst::Create(ctPeekParentIdFunction, "", I);
                    Value* cArg[] = {parentId};
                    CallInst::Create(ompThreadJoinFunction, ArrayRef<Value*>(cArg, 1), "", I);
                    CallInst* nPopParent = CallInst::Create(ompPopParentFunction, "", I);
                    I = nPopParent;
                    hasUninstCall = true;
                }
                break;
                case (OMP_CALL):
                {
                    // Simple case, push and pop the parent id
                    // And Transform the arguments to the function call
                    // The GOMP_parallel_start has a parallel_end routine, so the master thread
                    //   returns immediately
                    // Thus the pop parent should be delayed until the end routine executes
                    Value* c1 = ConstantInt::get(int8Ty, 1);
                    Value* cArgQB[] = {c1};
                    debugLog("queueBufferFunction @" << __LINE__);
                    /*CallInst* nQueueBuf = */CallInst::Create(queueBufferFunction, ArrayRef<Value*>(cArgQB, 1),
                                                        "", ci);
                    debugLog("ompPushParentFunction @" << __LINE__);                                    
                    CallInst::Create(ompPushParentFunction, "", ci);
                    
                    // __ctOMPThreadCreate after entering the parallel region
                    Value* parentId = CallInst::Create(ctPeekParentIdFunction, "", ++I);
                    Value* cArg[] = {parentId};
                    I = CallInst::Create(ompThreadCreateFunction, ArrayRef<Value*>(cArg, 1), "", I);
                    
                    // Change the function called to a wrapper routine
                    Value* arg0 = ci->getArgOperand(0);
                    Function* ompMicroTask = NULL;
                    ConstantExpr* bci = NULL;
                    if ((bci = dyn_cast<ConstantExpr>(arg0)) != NULL)
                    {
                        if (bci->isCast())
                        {
                            ompMicroTask = dyn_cast<Function>(bci->getOperand(0));
                        }
                    }
                    else if ((ompMicroTask = dyn_cast<Function>(arg0)) != NULL)
                    {
                        // Cast success
                        errs() << "OmpMicroTask - " << ompMicroTask;
                    }
                    else
                    {
                        errs() << *(arg0->getType());
                        errs() << "Need new casting route for GOMP Parallel call\n";
                    }
                    ompMicroTaskFunctions.insert(ompMicroTask);
                    // Alloca Type and store pid and arg into alloc'd space
                    Type* strTy[] = {int32Ty, voidPtrTy};
                    Type* t = StructType::create(strTy);
                    Value* nArg = new AllocaInst(t, "Wrapper Struct", ci);
                    // Add Store insts here
                    Value* gepArgs[2] = {ConstantInt::get(int32Ty, 0), ConstantInt::get(int32Ty, 0)};
                    Value* ppid = GetElementPtrInst::Create(nArg, ArrayRef<Value*>(gepArgs, 2), "ParentIdPtr", ci);
                    debugLog("getThreadNumFunction @" << __LINE__);
                    Value* tNum = CallInst::Create(getThreadNumFunction, "", ci);
                    Value* pid = new StoreInst(tNum, ppid, ci);
                    gepArgs[1] = ConstantInt::get(int32Ty, 1);
                    Value* parg = GetElementPtrInst::Create(nArg, ArrayRef<Value*>(gepArgs, 2), "ArgPtr", ci);
                    new StoreInst(ci->getArgOperand(1), parg, ci);
                    Function* wrapMicroTask = createMicroTaskWrapStruct(ompMicroTask, t, M);
                    contechAddedFunctions.insert(wrapMicroTask);
                    ci->setArgOperand(0, new BitCastInst(wrapMicroTask, arg0->getType(), "", ci));
                    ci->setArgOperand(1, new BitCastInst(nArg, voidPtrTy, "", ci));
                    hasUninstCall = true;                                          
                    //ci->setArgOperand(2, bci->getWithOperandReplaced(0,wrapMicroTask));
                }
                break;
                case (OMP_FORK):
                {
                    // Simple case, push and pop the parent id
                    // And Transform the arguments to the function call
                    debugLog("ompPushParentFunction @" << __LINE__);
                    CallInst* nPushParent = CallInst::Create(ompPushParentFunction, "", ci);
                    ++I;
                    debugLog("ompPopParentFunction @" << __LINE__);
                    CallInst* nPopParent = CallInst::Create(ompPopParentFunction, "", &*I);
                    I = nPopParent;
                    
                    // Add one to the number of arguments
                    //   TODO: Make this a ConstantExpr
                    ci->setArgOperand(1, BinaryOperator::Create(Instruction::Add,
                                                ci->getArgOperand(1),
                                                ConstantInt::get(int32Ty, 1),
                                                "", ci));
                    
                    // Change the function called to a wrapper routine
                    Value* arg2 = ci->getArgOperand(2);
                    Function* ompMicroTask = NULL;
                    ConstantExpr* bci = NULL;
                    if ((bci = dyn_cast<ConstantExpr>(arg2)) != NULL)
                    {
                        if (bci->isCast())
                        {
                            ompMicroTask = dyn_cast<Function>(bci->getOperand(0));
                        }
                    }
                    else
                    {
                        errs() << "Need new casting route for omp fork call\n";
                    }
                    ompMicroTaskFunctions.insert(ompMicroTask);
                    Function* wrapMicroTask = createMicroTaskWrap(ompMicroTask, M);
                    contechAddedFunctions.insert(wrapMicroTask);
                    ci->setArgOperand(2, ConstantExpr::getBitCast(wrapMicroTask, bci->getType()));
                                                              
                    // One cannot simply add an argument to an instruction
                    // Instead we have to copy the arguments over and create a new instruction
                    Value** cArg = new Value*[ci->getNumArgOperands() + 1];
                    for (unsigned int i = 0; i < ci->getNumArgOperands(); i++)
                    {
                        cArg[i] = ci->getArgOperand(i);
                    }
                    
                    // Now add a new argument
                    debugLog("getThreadNumFunction @" << __LINE__);
                    cArg[ci->getNumArgOperands()] = CallInst::Create(getThreadNumFunction, "", ci);
                    Value* cArgQB[] = {ConstantInt::get(int8Ty, 1)};
                    debugLog("queueBufferFunction @" << __LINE__);
                    CallInst::Create(queueBufferFunction, ArrayRef<Value*>(cArgQB, 1),
                                                            "", ci);
                    debugLog("kmpc_fork_call @" << __LINE__);
                    CallInst* nForkCall = CallInst::Create(ci->getCalledFunction(),
                                                           ArrayRef<Value*>(cArg, 1 + ci->getNumArgOperands()),
                                                           ci->getName(), ci);
                    ci->replaceAllUsesWith(nForkCall);
                    // Erase is dangerous, e.g. iPt could point to ci
                    if (iPt == ci)
                        iPt = nPushParent;
                    ci->eraseFromParent();
                    hasUninstCall = true;
                    delete [] cArg;
                }
                break;
                case (OMP_FOR_ITER):
                {
                    debugLog("ompTaskJoinFunction @" << __LINE__);
                    CallInst::Create(ompTaskJoinFunction, "", ci);
                    ++I;
                    Value* cArg[] = {ci};
                    debugLog("ompTaskCreateFunction @" << __LINE__);
                    CallInst* nCreate = CallInst::Create(ompTaskCreateFunction, ArrayRef<Value*>(cArg, 1), "", I);
                    I = nCreate;
                    hasUninstCall = true;
                }
                break;
                case (OMP_BARRIER):
                {
                    // OpenMP barriers use argument 1 for barrier ID
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    //IntToPtrInst* bci = new IntToPtrInst(ci->getArgOperand(1), voidPtrTy, "locktovoid", I);
                    debugLog("ompGetParentFunction @" << __LINE__);
                    IntToPtrInst* bci = new IntToPtrInst(CallInst::Create(ompGetParentFunction, "", I), voidPtrTy, "locktovoid", I);
                    Value* c1 = ConstantInt::get(int8Ty, 1);
                    Value* cArgs[] = {c1, bci, nGetTick};
                    // Record the barrier entry
                    debugLog("storeBarrierFunction @" << __LINE__);
                    CallInst* nStoreBarEn = CallInst::Create(storeBarrierFunction, ArrayRef<Value*>(cArgs,3),
                                                        "", I);
                    Value* cArg[] = {c1};
                    debugLog("queueBufferFunction @" << __LINE__);
                    /*CallInst* nQueueBuf = */CallInst::Create(queueBufferFunction, ArrayRef<Value*>(cArg, 1),
                                                        "", I);                   
                    ++I;
                    cArgs[0] = ConstantInt::get(int8Ty, 0);
                    // Record the barrier exit
                    debugLog("storeBarrierFunction @" << __LINE__);
                    CallInst* nStoreBarEx = CallInst::Create(storeBarrierFunction, ArrayRef<Value*>(cArgs,3),
                                                        "", I);
                    I = nStoreBarEx;
                    containingEvent = ct_event_barrier;
                    containQueueBuf = true;
                    iPt = nStoreBarEn;
                    hasUninstCall = true;
                }
                break;
                case(MPI_SEND_BLOCKING):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    insertMPITransfer(true, true, nGetTick, ci);
                }
                break;
                case(MPI_RECV_BLOCKING):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    insertMPITransfer(false, true, nGetTick, ci);
                }
                break;
                case(MPI_SEND_NONBLOCKING):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    insertMPITransfer(true, false, nGetTick, ci);
                }
                break;
                case(MPI_RECV_NONBLOCKING):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    insertMPITransfer(false, false, nGetTick, ci);
                }
                break;
                case(MPI_TRANSFER_WAIT):
                {
                    debugLog("getCurrentTickFunction @" << __LINE__);
                    CallInst* nGetTick = CallInst::Create(getCurrentTickFunction, "tick", ci);
                    Value* argWait[] = {castSupport(voidPtrTy, ci->getOperand(0), I), nGetTick};
                    ++I;
                    CallInst* storeWait = CallInst::Create(storeMPIWaitFunction, ArrayRef<Value*>(argWait, 2), "", I);
                    I = storeWait;
                }
                break;
                default:
                    // TODO: Function->isIntrinsic()
                    if (0 == __ctStrCmp(fn, "memcpy")  ||
                        0 == __ctStrCmp(fn, "memmove"))
                    {
                        Value* cArgS[] = {ci->getArgOperand(2), ci->getArgOperand(0), ci->getArgOperand(1)};
                        debugLog("storeBulkMemoryOpFunction @" << __LINE__);
                        CallInst::Create(storeBulkMemoryOpFunction, ArrayRef<Value*>(cArgS, 3), "", I);
                        hasUninstCall = true;
                    }
                    else if (0 == __ctStrCmp(fn, "llvm."))
                    {
                        if (0 == __ctStrCmp(fn + 5, "memcpy"))
                        {
                            // LLVM.memcpy can take i32 or i64 for size of copy
                            Value* castSize = castSupport(pthreadTy, ci->getArgOperand(2), I);
                            Value* cArgS[] = {castSize, ci->getArgOperand(0), ci->getArgOperand(1)};
                            debugLog("storeBulkMemoryOpFunction @" << __LINE__);
                            CallInst::Create(storeBulkMemoryOpFunction, ArrayRef<Value*>(cArgS, 3), "", I);
                            hasUninstCall = true;
                        }
                        else if (0 == __ctStrCmp(fn + 5, "dbg") ||
                                 0 == __ctStrCmp(fn + 5, "lifetime"))
                        {
                            // IGNORE
                        }
                        else
                        {
                            errs() << "Builtin - " << fn << "\n";
                        }
                    }
                    else if (0 != __ctStrCmp(fn, "__ct"))
                    {
                        // The function called is not something added by the instrumentation
                        //   and also not one that needs special treatment.
                        containCall = true;
                        hasUninstCall = true;
                    }
                    else
                    {
                    }
                }
                if (status == 0)
                {
                    free(fdn);
                }
            }
        }
        
        bi->containCall = hasUninstCall;
        
        // If there are more than 170 memops, then "prealloc" space
        if (memOpCount > ((1024 - 4) / 6))
        {
            // TODO: Function not defined in ct_runtime
            Value* argsCheck[] = {llvm_nops};
            debugLog("checkBufferLargeFunction @" << __LINE__);
            CallInst::Create(checkBufferLargeFunction, ArrayRef<Value*>(argsCheck, 1), "", sbb);
            containCall = true;
        }
        //
        // Being conservative, if another function was called, then
        // the instrumentation needs to check that the buffer isn't full
        //
        // Being really conservative every block has a check, this also
        //   requires disabling the dominator tree traversal in the runOnModule routine
        //
        //if (/*containCall == true && */containQueueBuf == false && markOnly == false)
        else if (B.getTerminator()->getNumSuccessors() != 1 && markOnly == false)
        {
            // Since calls terminate basic blocks
            //   These blocks would have only 1 successor
            Value* argsCheck[] = {sbbc};
            debugLog("checkBufferFunction @" << __LINE__);
            CallInst::Create(checkBufferFunction, ArrayRef<Value*>(argsCheck, 1), "", iPt);
            containCall = true;
        }
        
        
        
        // Finally record the information about this basic block
        //  into the CFG structure, so that targets can be matched up
        //  once all basic blocks have been parsed
        {
            // Does this basic block check its buffer?
            bi->hasCheckBuffer = (containCall)?2:0;
            bi->ev = containingEvent;
            {
                TerminatorInst* t = B.getTerminator();
                unsigned i = t->getNumSuccessors();
                unsigned j;
                
                // Let's assume that a basic block can only go to at most two other blocks
                for (j = 0; j < 2; j++)
                {
                    if (j < i)
                    {
                        bi->tgts[j] = t->getSuccessor(j);
                    }
                    else
                    {
                        bi->tgts[j] = NULL;
                    }
                }
                
            }
            cfgInfoMap.insert(pair<BasicBlock*, llvm_basic_block*>(&B, bi));
            
        }
        
        debugLog("Return from BBID: " << bbid);
        
        return true;
    }

// OpenMP is calling ompMicroTask with a void* struct
//   Create a new routine that is invoked with a different struct that
//   will invoke the original routine with the original parameter
Function* Contech::createMicroTaskWrapStruct(Function* ompMicroTask, Type* argTy, Module &M)
{
    FunctionType* baseFunType = ompMicroTask->getFunctionType();
    Type* argTyAr[] = {voidPtrTy};
    FunctionType* extFunType = FunctionType::get(ompMicroTask->getReturnType(), 
                                                 ArrayRef<Type*>(argTyAr, 1),
                                                 false);
    
    Function* extFun = Function::Create(extFunType, 
                                        ompMicroTask->getLinkage(),
                                        Twine("__ct", ompMicroTask->getName()),
                                        &M);
                                        
    BasicBlock* soloBlock = BasicBlock::Create(M.getContext(), "entry", extFun);
    
    Function::ArgumentListType& argList = extFun->getArgumentList();
    
    Value* addrI = new BitCastInst(argList.begin(), argTy->getPointerTo(), Twine("Cast to Type"), soloBlock);
    
    // getElemPtr 0, 0 -> arg 0 of type*
    
    Value* args[2] = {ConstantInt::get(int32Ty, 0), ConstantInt::get(int32Ty, 0)};
    Value* ppid = GetElementPtrInst::Create(addrI, ArrayRef<Value*>(args, 2), "ParentIdPtr", soloBlock);
    Value* pid = new LoadInst(ppid, "ParentId", soloBlock);
    
    // getElemPtr 0, 1 -> arg 1 of type*
    args[1] = ConstantInt::get(int32Ty, 1);
    Value* parg = GetElementPtrInst::Create(addrI, ArrayRef<Value*>(args, 2), "ArgPtr", soloBlock);
    Value* argP = new LoadInst(parg, "Arg", soloBlock);
    Value* argV = new BitCastInst(argP, baseFunType->getParamType(0), "Cast to ArgTy", soloBlock);
    
    Value* cArg[] = {pid};
    CallInst::Create(ompThreadCreateFunction, ArrayRef<Value*>(cArg, 1), "", soloBlock);
    Value* cArgCall[] = {argV};
    CallInst* wrappedCall = CallInst::Create(ompMicroTask, ArrayRef<Value*>(cArgCall, 1), "", soloBlock);
    CallInst::Create(ompThreadJoinFunction, ArrayRef<Value*>(cArg, 1), "", soloBlock);
    if (ompMicroTask->getReturnType() != voidTy)
        ReturnInst::Create(M.getContext(), wrappedCall, soloBlock);
    else
        ReturnInst::Create(M.getContext(), soloBlock);
    
    return extFun;
}
    
Function* Contech::createMicroTaskWrap(Function* ompMicroTask, Module &M)
{
    if (ompMicroTask == NULL) {errs() << "Cannot create wrapper from NULL function\n"; return NULL;}
    FunctionType* baseFunType = ompMicroTask->getFunctionType();
    if (ompMicroTask->isVarArg()) { errs() << "Cannot create wrapper for varg function\n"; return NULL;}
    
    Type** argTy = new Type*[1 + baseFunType->getNumParams()];
    for (unsigned int i = 0; i < baseFunType->getNumParams(); i++)
    {
        argTy[i] = baseFunType->getParamType(i);
    }
    argTy[baseFunType->getNumParams()] = int32Ty;
    FunctionType* extFunType = FunctionType::get(ompMicroTask->getReturnType(), 
                                                 ArrayRef<Type*>(argTy, 1 + baseFunType->getNumParams()),
                                                 false);
    
    Function* extFun = Function::Create(extFunType, 
                                        ompMicroTask->getLinkage(),
                                        Twine("__ct", ompMicroTask->getName()),
                                        &M);
    
    BasicBlock* soloBlock = BasicBlock::Create(M.getContext(), "entry", extFun);
    
    Function::ArgumentListType& argList = extFun->getArgumentList();
    unsigned argListSize = argList.size();
    
    Value** cArgExt = new Value*[argListSize - 1];
    auto it = argList.begin();
    for (unsigned i = 0; i < argListSize - 1; i ++)
    {
        cArgExt[i] = it;
        ++it;
    }
    
    Value* cArg[] = {--(argList.end())};
    CallInst::Create(ompThreadCreateFunction, ArrayRef<Value*>(cArg, 1), "", soloBlock);
    CallInst* wrappedCall = CallInst::Create(ompMicroTask, ArrayRef<Value*>(cArgExt, argListSize - 1), "", soloBlock);
    CallInst::Create(ompThreadJoinFunction, ArrayRef<Value*>(cArg, 1), "", soloBlock);
    if (ompMicroTask->getReturnType() != voidTy)
        ReturnInst::Create(M.getContext(), wrappedCall, soloBlock);
    else
        ReturnInst::Create(M.getContext(), soloBlock);
    
    delete [] argTy;
    delete [] cArgExt;
    
    return extFun;
}
    
Value* Contech::castSupport(Type* castType, Value* sourceValue, Instruction* insertBefore)
{
    auto castOp = CastInst::getCastOpcode (sourceValue, false, castType, false);
    debugLog("CastInst @" << __LINE__);
    return CastInst::Create(castOp, sourceValue, castType, "Cast to Support Type", insertBefore);
}
    
char Contech::ID = 0;
static RegisterPass<Contech> X("Contech", "Contech Pass", false, false);
