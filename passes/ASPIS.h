#pragma once

#include <llvm/IR/Instructions.h>

#include <map>
#include <set>

#include "Duplication/ASPISValues.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

using namespace llvm;

/**
 * - 0: Disabled
 * - 1: Enabled
 */
#define LOG_COMPILED_FUNCS 1

// DATA PROTECTION
/* class FuncRetToRef : public PassInfoMixin<FuncRetToRef> {
    private:
        Function* updateFnSignature(Function &Fn, Module &Md);
        void updateRetInstructions(Function &Fn);
        void updateFunctionCalls(Function &Fn, Function &NewFn);

    public:
        PreservedAnalyses run(Module &Md, ModuleAnalysisManager &AM);
        static bool isRequired() { return true; }
}; */

class ASPISValue;

class EDDI : public PassInfoMixin<EDDI> {
private:
  std::set<Value *> SphereOfReplication;
  std::set<Value *> SphereOfDuplication;
  std::set<Value *> SphereOfExclusion;

  std::map<Value *, ASPISValue *> ValuesToASPISValues;

  BasicBlock *ErrBB;

  /**
   * This function performs the following operations:
   * - Resolves aliases.
   * - Gathers annotations.
   * - Defines spheres.
   */
  void PreprocessModule(Module &Md);

  /**
   * Transforms functions with a return value into functions where the return
   * value is passed by reference as a parameter.
   */
  void FuncRetToRef(Module &Md);

  /**
   * Duplicates the global variables of the module in the SOR.
   */
  void DuplicateGlobals(Module &Md);

  /**
   * Creates the signatures for the _dup functions in the SOR.
   */
  void CreateDupFunctions(Module &Md);

  /**
   * Create the skeleton of the ErrBB.
   */
  void CreateErrBB(Module &Md);

public:
  const std::set<Value *> *getSOR() const { return &SphereOfReplication; }
  const std::set<Value *> *getSOD() const { return &SphereOfReplication; }
  const std::set<Value *> *getSOE() const { return &SphereOfExclusion; }
  const std::map<Value *, ASPISValue *> *getValuesToASPISValues() const {
    return &ValuesToASPISValues;
  }
  const BasicBlock *getErrBB() const { return ErrBB; }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);

  static bool isRequired() { return true; }
};

/* class DuplicateGlobals : public PassInfoMixin<DuplicateGlobals> {
    private:
        std::map<GlobalVariable*, GlobalVariable*> DuplicatedGlobals;

        void getFunctionsToNotModify(std::string Filename, std::set<std::string>
&FunctionsToNotModify); GlobalVariable* getDuplicatedGlobal(Module &Md,
GlobalVariable &GV); void duplicateCall(Module &Md, CallBase* UCall, Value*
Original, Value* Copy); void replaceCallsWithOriginalCalls(Module &Md,
std::set<std::string> &FunctionsToNotModify);

    public:
        PreservedAnalyses run(Module &M,
                              ModuleAnalysisManager &);

        static bool isRequired() { return true; }
}; */

class ASPISCheckProfiler : public PassInfoMixin<ASPISCheckProfiler> {
private:
  std::map<Value *, StringRef> FuncAnnotations;

public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);

  static bool isRequired() { return true; }
};

class ASPISInsertCheckProfiler
    : public PassInfoMixin<ASPISInsertCheckProfiler> {
private:
  std::map<Value *, StringRef> FuncAnnotations;

public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);

  static bool isRequired() { return true; }
};

// CONTROL-FLOW CHECKING
class CFCSS : public PassInfoMixin<CFCSS> {
private:
  std::map<Value *, StringRef> FuncAnnotations;

#if (LOG_COMPILED_FUNCS == 1)
  std::set<Function *> CompiledFuncs;
#endif

  void initializeBlocksSignatures(Module &Md,
                                  std::map<BasicBlock *, int> &BBSigs);
  BasicBlock *getFirstPredecessor(BasicBlock &BB,
                                  const std::map<BasicBlock *, int> &BBSigs);
  int getNeighborSig(BasicBlock &BB, const std::map<BasicBlock *, int> &BBSigs);
  bool hasNPredecessorsOrMore(BasicBlock &BB, int N,
                              const std::map<BasicBlock *, int> &BBSigs);
  void sortBasicBlocks(const std::map<BasicBlock *, int> &BBSigs,
                       const std::map<int, BasicBlock *> &NewBBs,
                       const std::map<Function *, BasicBlock *> &FuncErrBBs);
  void createCFGVerificationBB(BasicBlock &BB,
                               const std::map<BasicBlock *, int> &BBSigs,
                               std::map<int, BasicBlock *> *NewBBs,
                               BasicBlock &ErrBB, Value *G, Value *D);

public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);

  static bool isRequired() { return true; }
};

class RASM : public PassInfoMixin<RASM> {
private:
  std::map<Value *, StringRef> FuncAnnotations;
  std::map<BasicBlock *, BasicBlock *> NewBBs;

#if (LOG_COMPILED_FUNCS == 1)
  std::set<Function *> CompiledFuncs;
#endif

  void initializeBlocksSignatures(Module &Md,
                                  std::map<BasicBlock *, int> &RandomNumberBBs,
                                  std::map<BasicBlock *, int> &SubRanPrevVals);
  void splitBBsAtCalls(Module &Md);
  CallBase *isCallBB(BasicBlock &BB);
  void initializeEntryBlocksMap(Module &Md);
  Value *getCondition(Instruction &I);
  void createCFGVerificationBB(BasicBlock &BB,
                               std::map<BasicBlock *, int> &RandomNumberBBs,
                               std::map<BasicBlock *, int> &SubRanPrevVals,
                               Value &RuntimeSig, Value &RetSig,
                               BasicBlock &ErrBB);

public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);

  static bool isRequired() { return true; }
};
