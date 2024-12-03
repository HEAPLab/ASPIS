#ifndef ASPIS_H
#define ASPIS_H

#include "llvm/IR/PassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include <llvm/IR/Instructions.h>
#include <map>
#include <set>

using namespace llvm;

/**
 * - 0: Disabled
 * - 1: Enabled
*/
#define LOG_COMPILED_FUNCS 1

// DATA PROTECTION
class FuncRetToRef : public PassInfoMixin<FuncRetToRef> {
    private:
        Function* updateFnSignature(Function &Fn, Module &Md);
        void updateRetInstructions(Function &Fn);
        void updateFunctionCalls(Function &Fn, Function &NewFn);

    public:
        PreservedAnalyses run(Module &Md, ModuleAnalysisManager &AM);
        static bool isRequired() { return true; }
};

class EDDI : public PassInfoMixin<EDDI> {
    private:
        std::set<Function*> CompiledFuncs;
        std::map<Value*, StringRef> FuncAnnotations;
        std::set<Function*> OriginalFunctions;

        int isUsedByStore(Instruction &I, Instruction &Use);
        Instruction* cloneInstr(Instruction &I, std::map<Value *, Value *> &DuplicatedInstructionMap);
        void duplicateOperands (Instruction &I, std::map<Value *, Value *> &DuplicatedInstructionMap, BasicBlock &ErrBB);
        Value* getPtrFinalValue(Value &V);
        Value* comparePtrs(Value &V1, Value &V2, IRBuilder<> &B);
        void addConsistencyChecks(Instruction &I, std::map<Value *, Value *> &DuplicatedInstructionMap, BasicBlock &ErrBB);
        void fixFuncValsPassedByReference(Instruction &I, std::map<Value *, Value *> &DuplicatedInstructionMap, IRBuilder<> &B);
        Function *getFunctionDuplicate(Function *Fn);
        Function *getFunctionFromDuplicate(Function *Fn);
        void duplicateGlobals (Module &Md, std::map<Value *, Value *> &DuplicatedInstructionMap);
        bool isAllocaForExceptionHandling(AllocaInst &I);
        int duplicateInstruction (Instruction &I, std::map<Value *, Value *> &DuplicatedInstructionMap, BasicBlock &ErrBB);
        bool isValueDuplicated(std::map<Value *, Value *> &DuplicatedInstructionMap, Instruction &V);
        Function *duplicateFnArgs(Function &Fn, Module &Md, std::map<Value *, Value *> &DuplicatedInstructionMap);

        void fixGlobalCtors(Module &M);
    public:
        PreservedAnalyses run(Module &M,
                              ModuleAnalysisManager &);

        static bool isRequired() { return true; }
};

class DuplicateGlobals : public PassInfoMixin<DuplicateGlobals> {
    private: 
        std::map<GlobalVariable*, GlobalVariable*> DuplicatedGlobals;

        void getFunctionsToNotModify(std::string Filename, std::set<std::string> &FunctionsToNotModify);
        GlobalVariable* getDuplicatedGlobal(Module &Md, GlobalVariable &GV);
        void duplicateCall(Module &Md, CallBase* UCall, Value* Original, Value* Copy);
        void replaceCallsWithOriginalCalls(Module &Md, std::set<std::string> &FunctionsToNotModify);

    public:
        PreservedAnalyses run(Module &M,
                              ModuleAnalysisManager &);

        static bool isRequired() { return true; }
};


// CONTROL-FLOW CHECKING
class CFCSS : public PassInfoMixin<CFCSS> {
    private:
        std::map<Value*, StringRef> FuncAnnotations;

        #if (LOG_COMPILED_FUNCS == 1)
        std::set<Function*> CompiledFuncs;
        #endif

        void initializeBlocksSignatures(Module &Md, std::map<BasicBlock*, int> &BBSigs);
        BasicBlock* getFirstPredecessor(BasicBlock &BB, const std::map<BasicBlock*, int> &BBSigs);
        int getNeighborSig(BasicBlock &BB, const std::map<BasicBlock*, int> &BBSigs);
        bool hasNPredecessorsOrMore(BasicBlock &BB, int N, const std::map<BasicBlock*, int> &BBSigs);
        void sortBasicBlocks(const std::map<BasicBlock *, int> &BBSigs, const std::map<int, BasicBlock *> &NewBBs, const std::map<Function*, BasicBlock*> &FuncErrBBs);
        void createCFGVerificationBB (BasicBlock &BB,
                                 const std::map<BasicBlock *, int> &BBSigs,
                                 std::map<int, BasicBlock *> *NewBBs,
                                 BasicBlock &ErrBB,
                                 Value *G, Value *D);

    public:
        PreservedAnalyses run(Module &M,
                              ModuleAnalysisManager &);

        static bool isRequired() { return true; }
};

class RASM : public PassInfoMixin<RASM> {
    private:
        std::map<Value*, StringRef> FuncAnnotations;
        std::map<BasicBlock*, BasicBlock*> NewBBs;

        #if (LOG_COMPILED_FUNCS == 1)
        std::set<Function*> CompiledFuncs;
        #endif

        void initializeBlocksSignatures(Module &Md, std::map<BasicBlock*, int> &RandomNumberBBs, std::map<BasicBlock*, int> &SubRanPrevVals);
        void splitBBsAtCalls(Module &Md);
        CallBase *isCallBB (BasicBlock &BB);
        void initializeEntryBlocksMap(Module &Md);
        Value *getCondition(Instruction &I);
        void createCFGVerificationBB (  BasicBlock &BB, 
                                    std::map<BasicBlock*, int> &RandomNumberBBs, 
                                    std::map<BasicBlock*, int> &SubRanPrevVals, 
                                    Value &RuntimeSig, 
                                    Value &RetSig,
                                    BasicBlock &ErrBB);

    public:
        PreservedAnalyses run(Module &M,
                              ModuleAnalysisManager &);

        static bool isRequired() { return true; }

};

#endif