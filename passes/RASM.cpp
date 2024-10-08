/**
 * ************************************************************************************************
 * @brief  LLVM pass implementing Random Additive Signature Monitoring (RASM).
 *         Original algorithm by Vankeirsbilck et Al. (DOI: 10.1109/TR.2017.2754548)
 * 
 * @author Davide Baroffio, Politecnico di Milano, Italy (davide.baroffio@polimi.it)
 * ************************************************************************************************
*/
#include "ASPIS.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "Utils/Utils.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <list>
#include <map>
#include <iostream>
#include <fstream>
using namespace llvm;

#define DEBUG_TYPE "rasm-verify"

/**
 * - 0: Disabled
 * - 1: Enabled
*/
//#define INTRA_FUNCTION_CFC 1
#define INIT_SIGNATURE -0xDEAD // The same value has to be used as initializer for the signatures in the code

void RASM::initializeBlocksSignatures(Module &Md, std::map<BasicBlock*, int> &RandomNumberBBs, std::map<BasicBlock*, int> &SubRanPrevVals) {
    int i = 0;
    for (Function &Fn : Md) {
        if (shouldCompile(Fn, FuncAnnotations)) {
            for (BasicBlock &BB : Fn) {
                if (!BB.getName().equals_insensitive("errbb")) {
                    RandomNumberBBs.insert(std::pair<BasicBlock*, int>(&BB, i));
                    SubRanPrevVals.insert(std::pair<BasicBlock*, int>(&BB, 1));
                    i=i+2;
                }
            }
        }
    }
    return;
}

#if (INTRA_FUNCTION_CFC == 1)

std::map<BasicBlock*, CallBase *> CallBBs;
std::map<Function*, BasicBlock*> FuncEntryBlocks;
std::map<BasicBlock*, BasicBlock*> SplitBBs;
/**
 * Navigates the module's (not declared for linker and not externally linked) functions.
 * For each such function, split all the basic blocks calling it before the
 * call instruction.
 * @param Md The module for which to perform the split.
 */
void RASM::splitBBsAtCalls(Module &Md) {
  std::set<CallBase*> CallInstructions;
  // perform the split
  for (Function &Fn : Md) {
    // we target only functions defined in the module (i.e. not the ones that
    // have to be linked because they are not in the scope of this compilation
    // instance)
    if (shouldCompile(Fn, FuncAnnotations)) {
      for (User *U : Fn.users()) {
        if (isa<CallBase>(U)) {
          // Split the basic block containing U, insert the user into the set of call instructions
          CallBase *Caller = cast<CallBase>(U);
          Instruction *SplitInstr = Caller->getNextNonDebugInstruction();
          Caller->getParent()->splitBasicBlockBefore(SplitInstr);
          CallInstructions.insert(Caller);
        }
      }
    }
  }
  
  // populate CallBBs 
  for (CallBase *Caller : CallInstructions) {
    CallBBs.insert(std::pair<BasicBlock*, CallBase*>(Caller->getParent(), Caller));
  }

  // populate SplitBBs
  for (Function &Fn : Md) {
    if (shouldCompile(Fn, FuncAnnotations)) {
      for (BasicBlock &BB : Fn) {
        if (isCallBB(BB) != nullptr) {
          SplitBBs.insert(std::pair<BasicBlock*, BasicBlock*>(&BB, BB.getUniqueSuccessor()));
        }
      }
    }
  }
}

// Returns the instruction performing the call if the BB is in CallBBs. Returns nullptr otherwise.
CallBase *RASM::isCallBB (BasicBlock &BB) {
  if (CallBBs.find(&BB) != CallBBs.end()) {
    return CallBBs.find(&BB)->second;
  }
  else {
    return nullptr;
  }
}

void RASM::initializeEntryBlocksMap(Module &Md) {
  for (Function &Fn : Md) {
    if (shouldCompile(Fn, FuncAnnotations)) 
      FuncEntryBlocks.insert(std::pair<Function*, BasicBlock*>(&Fn, &Fn.front()));
  }
}

#endif

Value *RASM::getCondition(Instruction &I) {
  if (isa<BranchInst>(I) && cast<BranchInst>(I).isConditional()) {
    if (!cast<BranchInst>(I).isConditional()) {
      return nullptr;
    }
    else {
      return cast<BranchInst>(I).getCondition();
    }
  }
  else if (isa<SwitchInst>(I)) {
    return cast<SwitchInst>(I).getCondition();
  }
  else {
    assert(false && "Tried to get a condition on a function that is not a branch or a switch");
  }
}

void RASM::createCFGVerificationBB (  BasicBlock &BB, 
                                std::map<BasicBlock*, int> &RandomNumberBBs, 
                                std::map<BasicBlock*, int> &SubRanPrevVals, 
                                Value &RuntimeSig, 
                                Value &RetSig,
                                BasicBlock &ErrBB) {

    auto *IntType = llvm::Type::getInt32Ty(BB.getContext());

    int randomNumberBB = RandomNumberBBs.find(&BB)->second;
    int subRanPrevVal = SubRanPrevVals.find(&BB)->second;
    // in this case BB is not the first Basic Block of the function, so it has to update RuntimeSig and check it
    if (!BB.isEntryBlock()) {
        if (isa<LandingPadInst>(BB.getFirstNonPHI())) {
          IRBuilder<> BChecker(&*BB.getFirstInsertionPt());
          BChecker.CreateStore(llvm::ConstantInt::get(IntType, randomNumberBB),&RuntimeSig);
        }
        else {
        BasicBlock *NewBB = BasicBlock::Create(BB.getContext(), "RASM_Verification_BB", BB.getParent(), &BB);
        IRBuilder<> BChecker(NewBB);

        // add instructions for the first runtime signature update
        Value *InstrRuntimeSig = BChecker.CreateLoad(IntType, &RuntimeSig);

        Value *RuntimeSignatureVal = BChecker.CreateSub(InstrRuntimeSig, llvm::ConstantInt::get(IntType, subRanPrevVal));
        BChecker.CreateStore(RuntimeSignatureVal, &RuntimeSig);

        // update phi placing them in the new block
        while (isa<PHINode>(&BB.front())) {
          Instruction *PhiInst = &BB.front();
          PhiInst->removeFromParent();
          PhiInst->insertBefore(&NewBB->front());
        }

        // replace the uses of BB with NewBB
        for (BasicBlock &BB_ : *BB.getParent()) {
          if (&BB_ != NewBB) {
            BB_.getTerminator()->replaceSuccessorWith(&BB, NewBB);
          }
        }

        // add instructions for checking the runtime signature
        Value *CmpVal = BChecker.CreateCmp(llvm::CmpInst::ICMP_EQ, RuntimeSignatureVal, llvm::ConstantInt::get(IntType, randomNumberBB));
        BChecker.CreateCondBr(CmpVal, &BB, &ErrBB);

        // add NewBB and BB into the NewBBs map
        NewBBs.insert(std::pair<BasicBlock*, BasicBlock*>(NewBB, &BB));
    }
    }

    // Add instructions for the second runtime signature update.
    // There are three cases:
    /**
     * A) the basic block ends with a call instruction
     * B) the basic block ends with a return instruction
     * C) all the other cases
    */

    #if (INTRA_FUNCTION_CFC == 1) 
    // Case A, we need to update the RetSig
    CallBase *CallIn = isCallBB(BB);
    if (CallIn != nullptr && (*CallIn).getCalledFunction() != nullptr && shouldCompile(*(*CallIn).getCalledFunction(), FuncAnnotations)) {
      // Get the signature of the called basic block after the call
      BasicBlock *SuccBB = SplitBBs.find(&BB)->second;
      int randomNumberSuccBB = RandomNumberBBs.find(SuccBB)->second;
      int subRanPrevValSuccBB = SubRanPrevVals.find(SuccBB)->second;
      int retSig = randomNumberSuccBB + subRanPrevValSuccBB;
      
      // Get the signature of the first basic block of the called function
      BasicBlock *CalledBB = FuncEntryBlocks.find(CallIn->getCalledFunction())->second;
      int randomNumberCalledBB = RandomNumberBBs.find(CalledBB)->second;
      int subRanPrevValCalledBB = SubRanPrevVals.find(CalledBB)->second;
      
      IRBuilder<> B(CallIn);
      
      // Backup the ret signature so that we don't overwrite it
      Value* RetSigBackup = B.CreateLoad(IntType, &RetSig);

      // Set the runtime signature as the input signature of the first basic block of the called function
      B.CreateStore(llvm::ConstantInt::get(IntType, randomNumberCalledBB+subRanPrevValCalledBB), &RuntimeSig);
      
      // Set the ret signature as the signature of the basic block after the call
      B.CreateStore(llvm::ConstantInt::get(IntType, retSig), &RetSig);

      // Restore the ret signature after the call
      B.SetInsertPoint(CallIn->getNextNonDebugInstruction());
      B.CreateStore(RetSigBackup, &RetSig);
    }
    else
    #endif
    // Case B, we need to add a check on the RetSig and update the RuntimeSig
    if (isa<ReturnInst>(BB.getTerminator())) {
      // add a control basic block before the return instruction
      BB.splitBasicBlockBefore(BB.getTerminator());
      BasicBlock *NewBB = BasicBlock::Create(BB.getContext(), "RASM_ret_Verification_BB", BB.getParent(), &BB);
      // replace the uses of BB with NewBB
      for (BasicBlock &BB_ : *BB.getParent()) {
        if (&BB_ != NewBB) {
          BB_.getTerminator()->replaceSuccessorWith(&BB, NewBB);
        }
      }
      int primeNum = randomNumberBB - subRanPrevVal;

      // compute the adjustment value as AdjVal = primeNum+SubRanPrevVal-RetSig = randomNumberBB-subRanPrevVal+SubRanPrevVal-RetSig = randomNumberBB-RetSig
      IRBuilder<> B(NewBB);
      Value *InstrRetSig = B.CreateLoad(IntType, &RetSig);
      Value* AdjVal = B.CreateSub(llvm::ConstantInt::get(IntType, randomNumberBB), InstrRetSig);

      // update the signature
      Value *InstrRuntimeSig = B.CreateLoad(IntType, &RuntimeSig);
      Value* NewSig = B.CreateSub(InstrRuntimeSig, AdjVal);
      B.CreateStore(NewSig, &RuntimeSig);

      // compare the new signature with RetSig
      Value *CmpValRet = B.CreateCmp(llvm::CmpInst::ICMP_EQ, NewSig, InstrRetSig);
      B.CreateCondBr(CmpValRet, &BB, &ErrBB);
    }
    // Case C, we need to update the signature depending on the target basic block
    else {
      IRBuilder<> B(BB.getTerminator());
      Instruction *Terminator = BB.getTerminator();
      
      /**
       * We have three cases:
       * 1) one successor -> the branch is unconditional, so we use one single successor
       * 2) two successors -> the branch is conditional, so we use a `select` instruction
       * 3) more than three successors -> we have a switch: impossible since we lower switches in a previous pass
      */
      int numSuccessors = Terminator->getNumSuccessors();
      if (numSuccessors>1 and Terminator->isExceptionalTerminator()){
        numSuccessors=1;
      }
      switch (numSuccessors)
      {
        case 1: {
          BasicBlock *Successor = Terminator->getSuccessor(0);
          if (NewBBs.find(Successor) != NewBBs.end()) { // we want to find the correct successor, i.e. if Successor is in NewBBs, it means that it has been added now so it doesn't have a signature... Therefore we take the successor's successor
            Successor = NewBBs.find(Successor)->second;
          }
          int succRandomNumberBB = RandomNumberBBs.find(Successor)->second;
          int succSubRanPrevVal = SubRanPrevVals.find(Successor)->second;
          int adjVal = randomNumberBB - (succRandomNumberBB + succSubRanPrevVal);

          Value *InstrRuntimeSig = B.CreateLoad(IntType, &RuntimeSig);
          Value *NewSig = B.CreateSub(InstrRuntimeSig, llvm::ConstantInt::get(IntType, adjVal));
          B.CreateStore(NewSig, &RuntimeSig);
          break;
        }
        case 2: {
          BasicBlock *Successor_1 = Terminator->getSuccessor(0);
          if (NewBBs.find(Successor_1) != NewBBs.end()) {
            Successor_1 = NewBBs.find(Successor_1)->second;
          }
          int succRandomNumberBB_1 = RandomNumberBBs.find(Successor_1)->second;
          int succSubRanPrevVal_1 = SubRanPrevVals.find(Successor_1)->second;
          int adjVal_1 = randomNumberBB - (succRandomNumberBB_1 + succSubRanPrevVal_1);

          BasicBlock *Successor_2 = Terminator->getSuccessor(1);
          if (NewBBs.find(Successor_2) != NewBBs.end()) {
            Successor_2 = NewBBs.find(Successor_2)->second;
          }
          int succRandomNumberBB_2 = RandomNumberBBs.find(Successor_2)->second;
          int succSubRanPrevVal_2 = SubRanPrevVals.find(Successor_2)->second;
          int adjVal_2 = randomNumberBB - (succRandomNumberBB_2 + succSubRanPrevVal_2);

          Value *BrCondition = getCondition(*Terminator);

          Value *AdjustValue = B.CreateSelect(BrCondition, llvm::ConstantInt::get(IntType, adjVal_1)
                                    , llvm::ConstantInt::get(IntType, adjVal_2));
          Value *InstrRuntimeSig = B.CreateLoad(IntType, &RuntimeSig);
          Value *NewSig = B.CreateSub(InstrRuntimeSig, AdjustValue);
          B.CreateStore(NewSig, &RuntimeSig);
          break;
        }
        
        default:{ 
          break;
        }
      }
    }
}

PreservedAnalyses RASM::run(Module &Md, ModuleAnalysisManager &AM) {
    getFuncAnnotations(Md, FuncAnnotations);
    LinkageMap linkageMap=mapFunctionLinkageNames(Md);
    // Collection of <BB, RandomSign
    std::map<BasicBlock*, int> RandomNumberBBs;
    std::map<BasicBlock*, int> SubRanPrevVals;

    std::map<Function*, BasicBlock*> ErrBBs;

    auto *IntType = llvm::Type::getInt32Ty(Md.getContext());

    #if (INTRA_FUNCTION_CFC == 1)
      splitBBsAtCalls(Md);
      GlobalVariable *RuntimeSig;
      GlobalVariable *RetSig;
      // find the global variables required for the runtime signatures
      for (GlobalVariable &GV : Md.globals()) {
        if (!isa<Function>(GV) && FuncAnnotations.find(&GV) != FuncAnnotations.end()) {
          if ((FuncAnnotations.find(&GV))->second.startswith("runtime_sig")) {
            RuntimeSig = &GV;
          }
          else if ((FuncAnnotations.find(&GV))->second.startswith("run_adj_sig")) {
            RetSig = &GV;
          }
        }
      } 
    #endif

    initializeBlocksSignatures(Md, RandomNumberBBs, SubRanPrevVals);

    #if (INTRA_FUNCTION_CFC == 1)
    initializeEntryBlocksMap(Md);
    #endif

    for (Function &Fn : Md) {
      if (shouldCompile(Fn, FuncAnnotations)) {
        DebugLoc debugLoc;
        for (auto &I : Fn.front()) {
          if (I.getDebugLoc()) {
            debugLoc = I.getDebugLoc();
            break;
          } 
        }
        #if (LOG_COMPILED_FUNCS == 1)
          CompiledFuncs.insert(&Fn);
        #endif
        int currSig = RandomNumberBBs.find(&Fn.front())->second;
        #if (INTRA_FUNCTION_CFC == 0)
          IRBuilder<> B(&*(Fn.front().getFirstInsertionPt()));
          // initialize the runtime signature for the first basic block of the function
          Value *RuntimeSig = B.CreateAlloca(IntType);
          Value *RetSig = B.CreateAlloca(IntType);
          B.CreateStore(llvm::ConstantInt::get(IntType, currSig), RuntimeSig);
          B.CreateStore(llvm::ConstantInt::get(IntType, RandomNumberBBs.size() + currSig), RetSig);
        #elif (INTRA_FUNCTION_CFC == 1)
          int subCurrSig = SubRanPrevVals.find(&Fn.front())->second;
          // add instructions for initializing the runtime signatures in case they have not been initialized
          
          // put this computation inside a basic block at the beginning of the function
          BasicBlock *FrontBB = &Fn.front();
          BasicBlock *NewBB = BasicBlock::Create(Fn.getContext(), "RASM_prequel_BB", &Fn, FrontBB);
          IRBuilder<> B(NewBB);
          
          // load the runtime signatures
          Value* RuntimeSigInstr = B.CreateLoad(IntType, RuntimeSig);
          Value* RetSigInstr = B.CreateLoad(IntType, RetSig);

          // compare them with their initialization value INIT_SIGNATURE
          Value* Cond1 = B.CreateCmp(llvm::CmpInst::ICMP_EQ, RuntimeSigInstr, RetSigInstr);
          Value* Cond2 = B.CreateCmp(llvm::CmpInst::ICMP_EQ, RuntimeSigInstr, llvm::ConstantInt::get(IntType, INIT_SIGNATURE));
          Value* CondAnd = B.CreateAnd(Cond1, Cond2);

          // if they contain the initialization values, update them using sig and num_bbs+sig
          Value* NewRuntimeSig = B.CreateSelect(CondAnd, llvm::ConstantInt::get(IntType, currSig + subCurrSig), RuntimeSigInstr);
          Value* NewRetSig = B.CreateSelect(CondAnd, llvm::ConstantInt::get(IntType, RandomNumberBBs.size() + currSig), RetSigInstr);
          B.CreateStore(NewRuntimeSig, RuntimeSig);
          B.CreateStore(NewRetSig, RetSig);

          // add the branch to the previous frontBB
          B.CreateBr(FrontBB);
        #endif
        // create the ErrBB
        BasicBlock *ErrBB = BasicBlock::Create(Fn.getContext(), "ErrBB", &Fn);
        IRBuilder<> ErrB(ErrBB);

        assert(!getLinkageName(linkageMap,"SigMismatch_Handler").empty() && "Function SigMismatch_Handler is missing!");
        auto CalleeF = ErrBB->getModule()->getOrInsertFunction(
            getLinkageName(linkageMap,"SigMismatch_Handler"), FunctionType::getVoidTy(Md.getContext()));
        ErrB.CreateCall(CalleeF)->setDebugLoc(debugLoc);
        ErrB.CreateUnreachable();

        for (auto &Elem : RandomNumberBBs) {
          BasicBlock *BB = Elem.first;
          if (BB->getParent() == &Fn) {
            createCFGVerificationBB(*BB, RandomNumberBBs, SubRanPrevVals, *RuntimeSig, *RetSig, *ErrBB);
          }
        }
      }
    }

    #if (LOG_COMPILED_FUNCS == 1)
      persistCompiledFunctions(CompiledFuncs, "compiled_rasm_functions.csv");
    #endif

    return PreservedAnalyses::none();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getRASMPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "rasm-verify", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "rasm-verify") {
                    FPM.addPass(RASM());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getRASMPluginInfo();
}