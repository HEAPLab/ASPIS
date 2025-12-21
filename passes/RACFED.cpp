/****************************************************************************************
 * @brief LLVM pass implementing Random Additive Control Flow Error Detection (RACFED).
 * 	  Original algorithm by Vankeirsbilck et Al. (DOI: 10.1007/978-3-319-99130-6_15)
 *
 * @author Martina Starone, Gabriele Santandrea, Politecnico di Milano, Italy
 * 	   (martina.starone@mail.polimi.it, gabriele.santandrea@mail.polimi.it)
 *
 * The RACFED algorithm is reported here.
 * It was splitted into sections in order to develop a more readeable code.
 *
 * initializeBlocksSignatures
 *  1:for all Basic Block (BB) in CFG do
 *  2: repeat compileTimeSig ← random number
 *  3: until compileTimeSig is unique
 *  4: repeat subRanPrevVal ← random number
 *  5: until (compileTimeSig + subRanPrevVal) is unique
 * insertIntraInstructionUpdates
 *  6:for all BB in CFG do
 *  7:   if NrInstrBB > 2 then
 *  8:    for all original instructions insert after
 *  9:      signature ← signature + random number
 * checkJumpSignature
 *  10:for all BB in CFG insert at beginning
 *  11:  signature ← signature − subRanPrevVal
 *  12:  if signature != compileTimeSig error()
 *  13:for all BB in CFG do
 * checkOnReturn
 *  14: if Last Instr. is return instr. and NrIntrBB > 1 then
 *  15:   Calculate needed variables
 *  16:     returnVal ← random number
 *  17:     adjustValue ← (compileTimeSigBB + SumIntraInstructions) -
 *  18:                 return Val
 *  19:   Insert signature update before return instr.
 *  20:     signature ← signature + adjustValue
 *  21:     if signature != returnVal error()
 *  22: else
 * updateBeforeJump
 *  23:   for all Successor of BB do
 *  24:    adjustValue ← (compileTimeSigBB + SumIntraInstructions) -
 *  25:     (compileTimeSigSuccs + subRanPrevValSuccs)
 *  26:   Insert signature update at BB end
 *  27:     signature ← signature + adjustValue
 *************************************************************************************/

#include "ASPIS.h"
#include "Utils/Utils.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <random>
#include <stdlib.h>

#define OPTIONAL_DEBUG false

using namespace llvm;


#define DISTR_START 1
#define DISTR_END 0x7fffffff
/// Uniform distribution for 32 bits numbers.
///
/// In each function using this distribution a different seed will be used.
/// Seeds are chosen to be constant for reproducibility.
///
/// The range doesn't encapsulate the complete representation of 32 bits unsigned,
/// this was a design choice since multiple 32 bits are sum between each
/// other throughout the algorithm thus no overflow should occur when summing.
std::uniform_int_distribution<uint32_t> dist32(DISTR_START, DISTR_END);

// -------- INITIALIZE BLOCKS SIGNATURES --------

/**
 * Checks whether the compile time signature is unique.
 *
 * @param bb_num 	 compile time signature of the current analysed basic block.
 * @param compileTimeSig already assigned compile time signatures
 */
bool isNotUniqueCompileTimeSig(
  const uint32_t bb_num,
  const std::unordered_map<BasicBlock*, uint32_t> &compileTimeSig
) {
  for (const auto &[_, other_bb_id] : compileTimeSig) {
    if ( other_bb_id == bb_num ) return true;
  }
  return false;
}

/**
 * Checks whether the compile time signature (already unique) 
 * + second unique identifier (subRanPrevVal).
 * 
 * @param current_id     compileTimeSignature + subRanPrevVal for the 
 * current basic block.
 * @param compileTimeSig already assigned compile time signatures.
 * @param subRanPrevVals already assigned subRanPrevVals.
 */
bool isNotUnique(
  const uint32_t current_id,
  const std::unordered_map<BasicBlock*, uint32_t> &compileTimeSig,
  const std::unordered_map<BasicBlock*, uint32_t> &subRanPrevVals
) {
  for (const auto &[other_bb, other_bb_num] : compileTimeSig) {
    uint32_t other_id = static_cast<uint32_t>(other_bb_num) + subRanPrevVals.at(other_bb);
    if ( other_id == current_id ) {
      return true;
    }
  }

  return false;
}

/*
 * initializeBlocksSignatures
 *  1:for all Basic Block (BB) in CFG do
 *  2: repeat compileTimeSig ← random number
 *  3: until compileTimeSig is unique
 *  4: repeat subRanPrevVal ← random number
 *  5: until (compileTimeSig + subRanPrevVal) is unique
 */
void RACFED::initializeBlocksSignatures(Function &Fn) {
  std::random_device rd;
  std::mt19937 rng(rd()); 
  uint32_t randomBB;
  uint32_t randomSub;

  for (BasicBlock &BB : Fn) {
    do {
      randomBB = dist32(rng);
    } while ( isNotUniqueCompileTimeSig(randomBB, compileTimeSig) );

    do {
      randomSub = dist32(rng);
    } while ( isNotUnique(
      randomBB + randomSub,
      compileTimeSig,
      subRanPrevVals) );

    compileTimeSig.insert(std::pair(&BB, randomBB));
    subRanPrevVals.insert(std::pair(&BB, randomSub));
  }
}

// --------- INSERT UPDATES AFTER INSTRUCTIONS  -----------

/**
 * Computes the number of original instructions of the intermediate representation.
 */
void originalInstruction(BasicBlock &BB, std::vector<Instruction*> &OrigInstructions) {
  for (Instruction &I : BB) {
    if ( isa<PHINode>(&I) ) continue; // NOT ORIGINAL
    if ( I.isTerminator() ) continue; // NOT ORIGINAL
    if ( isa<DbgInfoIntrinsic>(&I) ) continue;
    OrigInstructions.push_back(&I);
  }
}

/*
 * insertIntraInstructionUpdates
 *  6:for all BB in CFG do
 *  7:   if NrInstrBB > 2 then
 *  8:    for all original instructions insert after
 *  9:      signature ← signature + random number
 */
void RACFED::insertIntraInstructionUpdates(Function &Fn,
				    GlobalVariable *RuntimeSigGV, 
				    Type *IntType) {
  std::random_device rd;
  std::mt19937 rng(rd()); 

  // 6: for all BB in CFG do
  for (auto &BB: Fn){
    std::vector<Instruction*> OrigInstructions;
    originalInstruction(BB, OrigInstructions);

    // 7: if NrInstrBB > 2 then
    if ( OrigInstructions.size() <= 2 ) continue;

    uint64_t partial_sum = 0;

    // 8: for all original instructions insert after
    for (Instruction *I : OrigInstructions) {
      Instruction *InsertPt = nullptr;

      if ( I->isTerminator() ) {
        InsertPt = I; // insert BEFORE terminator
      } else {
        InsertPt = I->getNextNode(); // insert BEFORE next instruction (equivalent to "after I")
      }

      IRBuilder<> InstrIR(InsertPt);

      // 9: signature ← signature + random number
      uint64_t K = dist32(rng);
      partial_sum += K;

      Value *Sig = InstrIR.CreateLoad(IntType, RuntimeSigGV);
      Value *NewSig = InstrIR.CreateAdd(Sig, ConstantInt::get(IntType, K), "sig_add");
      InstrIR.CreateStore(NewSig, RuntimeSigGV);
    }
    // Track total sum
    sumIntraInstruction[&BB] = partial_sum;
  }
}

// --------- CHECK BLOCKS AT JUMP END ---------

/*
 * checkJumpSignature
 *  10:for all BB in CFG insert at beginning
 *  11:  signature ← signature − subRanPrevVal
 *  12:  if signature != compileTimeSig error()
 *  13:for all BB in CFG do
 */
void RACFED::checkJumpSignature(BasicBlock &BB,
				GlobalVariable *RuntimeSigGV, Type *IntType,
				BasicBlock &ErrBB) {
  if ( BB.isEntryBlock() ) return;

  // In this case BB is not the first Basic Block of the function, 
  // so it has to update RuntimeSig and check it
  auto FirstNonPHI = BB.getFirstNonPHIIt();
  if ( (FirstNonPHI != BB.end() && isa<LandingPadInst>(FirstNonPHI)) ||
     BB.getName().contains_insensitive("verification") ) {

    if ( BB.getFirstInsertionPt() == BB.end() ) return; // Skip empty/invalid blocks

    int randomNumberBB = compileTimeSig.find(&BB)->second;
    IRBuilder<> BChecker(&*BB.getFirstInsertionPt());
    BChecker.CreateStore(llvm::ConstantInt::get(IntType, randomNumberBB), RuntimeSigGV);
  } else if ( !BB.getName().contains_insensitive("errbb") ) {
    // Get compile signatures
    int compileTimeSigCurrBB = compileTimeSig.find(&BB)->second;
    int subRanPrevValCurrBB = subRanPrevVals.find(&BB)->second;
    // Create verification basic block
    BasicBlock *VerificationBB = BasicBlock::Create(
      BB.getContext(), "RACFED_Verification_BB", BB.getParent(), &BB
    );
    IRBuilder<> BChecker(VerificationBB);

    // Add instructions for the first runtime signature update
    Value *InstrRuntimeSig =
    BChecker.CreateLoad(IntType, RuntimeSigGV);

    // 11: signature ← signature − subRanPrevVal
    Value *RuntimeSignatureVal = BChecker.CreateSub(
    InstrRuntimeSig, llvm::ConstantInt::get(IntType, subRanPrevValCurrBB));
    BChecker.CreateStore(RuntimeSignatureVal, RuntimeSigGV);

    // update phi placing them in the new block
    while (isa<PHINode>(&BB.front())) {
      Instruction *PhiInst = &BB.front();
      PhiInst->removeFromParent();
      PhiInst->insertInto(VerificationBB, VerificationBB->getFirstInsertionPt());
    }

    // replace the uses of BB with VerificationBB
    BB.replaceAllUsesWith(VerificationBB);

    // Fix PHI nodes in successors
    // replaceAllUsesWith updates PHI nodes in successors to point to VerificationBB,
    // but the actual control flow is VerificationBB -> BB -> Succ, so Succ still sees BB
    // as predecessor.
    for (BasicBlock *Succ : successors(&BB)) {
      for (PHINode &Phi : Succ->phis()) {
        for (unsigned i = 0; i < Phi.getNumIncomingValues(); ++i) {
          if ( Phi.getIncomingBlock(i) == VerificationBB ) {
            Phi.setIncomingBlock(i, &BB);
          }
        }
      }
    }

    // 12: if signature != compileTimeSig error()
    // add instructions for checking the runtime signature
    Value *CmpVal = BChecker.CreateCmp(
      llvm::CmpInst::ICMP_EQ, RuntimeSignatureVal,
      llvm::ConstantInt::get(IntType, compileTimeSigCurrBB)
    );
    BChecker.CreateCondBr(CmpVal, &BB, &ErrBB);

    // Map NewBB to the same signature requirements as BB so predecessors can
    // target it correctly
    compileTimeSig[VerificationBB] = compileTimeSigCurrBB;
    subRanPrevVals[VerificationBB] = subRanPrevValCurrBB;
  }
}

// --- UPDATE BRANCH SIGNATURE BEFORE JUMP ---

Value *RACFED::getCondition(Instruction &I) {
  if ( isa<BranchInst>(I) && cast<BranchInst>(I).isConditional() ) {
    if ( !cast<BranchInst>(I).isConditional() ) {
      return nullptr;
    } else {
      return cast<BranchInst>(I).getCondition();
    }
  } else if ( isa<SwitchInst>(I) ) {
    errs() << "There is a switch!\n";
    abort();
    return cast<SwitchInst>(I).getCondition();
  } else {
    assert(false && "Tried to get a condition on a function that is not a "
                    "branch or a switch");
  }
}

#if OPTIONAL_DEBUG
/**
 * Adds a call to printf to check the signature
 */
static void printSig(Module &Md, IRBuilder<> &B, Value *SigVal, const char *Msg) {
  LLVMContext &Ctx = Md.getContext();

  // int printf(const char*, ...)
  FunctionCallee Printf = Md.getOrInsertFunction(
      "printf",
      FunctionType::get(IntegerType::getInt32Ty(Ctx),
                        PointerType::getUnqual(Ctx),
                        true));

  // Generates global string "Msg: %ld\n"
  std::string Fmt = std::string(Msg) + ": %ld\n";
  Value *FmtStr = B.CreateGlobalString(Fmt);


  if ( SigVal->getType()->isIntegerTy(32) ) {
    SigVal = B.CreateZExt(SigVal, Type::getInt64Ty(Ctx));
  }

  B.CreateCall(Printf, {FmtStr, SigVal});
}
#endif

/*
 * updateBeforeJump
 *  23:   for all Successor of BB do
 *  24:    adjustValue ← (compileTimeSigBB + SumIntraInstructions) -
 *  25:     (compileTimeSigSuccs + subRanPrevValSuccs)
 *  26:   Insert signature update at BB end
 *  27:     signature ← signature + adjustValue
 */
void RACFED::updateBeforeJump(Module &Md, BasicBlock &BB,  GlobalVariable *RuntimeSigGV,
			   Type *IntType) {
  Instruction *Term = BB.getTerminator();
  IRBuilder<> B(&BB);
  B.SetInsertPoint(Term);
  auto *BI = dyn_cast<BranchInst>(Term);
  if ( !BI ) return;


  // Calculate Source Static Signature: CT_BB + SumIntra
  uint64_t SourceStatic =
      static_cast<uint64_t>(compileTimeSig[&BB]) + sumIntraInstruction[&BB];

  Value *Current = B.CreateLoad(IntType, RuntimeSigGV, "current");
  #if OPTIONAL_DEBUG
  printSig(Md, B, Current, "current");
  #endif

  //define if conditional or unconditional branch
  //Conditional: expected= CT_succ+subRan_succ
  //adj = CTB-exp--> new signature = RT -adj
  if ( BI->isUnconditional() ) {  // only one successor
    BasicBlock *Succ = BI->getSuccessor(0);
    uint64_t SuccExpected =
      static_cast<uint64_t>(compileTimeSig[Succ] + subRanPrevVals[Succ]);
    // adj = expected - current
    long int adj_value = SuccExpected - SourceStatic;
    Value *Adj = ConstantInt::get(IntType, adj_value);
    Value *NewSig = B.CreateAdd(Current, Adj, "racfed_newsig");
    B.CreateStore(NewSig, RuntimeSigGV);
    #if OPTIONAL_DEBUG
    printSig(Md,B, NewSig, "newsig");
    #endif

    return;
  }

  if ( BI-> isConditional()) {
    BasicBlock *SuccT = BI->getSuccessor(0);
    BasicBlock *SuccF = BI->getSuccessor(1);
    Instruction *Terminator = BB.getTerminator();
    Value *BrCondition = getCondition(*Terminator);

    // Target T
    uint64_t expectedT =
        static_cast<uint64_t>(compileTimeSig[SuccT] + subRanPrevVals[SuccT]);
    long int adj1 = expectedT - SourceStatic;

    // Target F
    uint64_t expectedF =
        static_cast<uint64_t>(compileTimeSig[SuccF] + subRanPrevVals[SuccF]);
    long int adj2 = expectedF - SourceStatic;

    Value *Adj = B.CreateSelect(BrCondition, ConstantInt::get(IntType, adj1), ConstantInt::get(IntType, adj2));
    Value *NewSig = B.CreateAdd(Current, Adj, "racfed_newsig");
    B.CreateStore(NewSig, RuntimeSigGV);

    #if OPTIONAL_DEBUG
    printSig(Md, B, NewSig, "SIG after cond");
    #endif
  }
}

// --- CHECK ON RETURN ---

/*
 * checkOnReturn
 *  14: if Last Instr. is return instr. and NrIntrBB > 1 then
 *  15:   Calculate needed variables
 *  16:     returnVal ← random number
 *  17:     adjustValue ← (compileTimeSigBB + SumIntraInstructions) -
 *  18:                 return Val
 *  19:   Insert signature update before return instr.
 *  20:     signature ← signature + adjustValue
 *  21:     if signature != returnVal error()
 */
Instruction *RACFED::checkOnReturn(BasicBlock &BB,
			      GlobalVariable *RuntimeSigGV, 
			      Type* IntType, BasicBlock &ErrBB,
			      Value *BckupRunSig) {

  // Uniform distribution for 64 bits numbers.
  std::uniform_int_distribution<uint64_t> dist64(DISTR_START, DISTR_END);
  // Constant seed for 64 bits.
  // Fixed for reproducibility.
  std::random_device rd;
  std::mt19937 rng64(rd()); 

  Instruction *Term = BB.getTerminator();

  if ( !isa<ReturnInst>(Term) ) return nullptr;

  std::vector<Instruction*> org_instr;
  originalInstruction(BB, org_instr);
  // 14: if Last Instr. is return instr. and NrIntrBB > 1 then
  // Since the implementation is slightly different
  // than the theoretical one, due to the possible
  // interpretations of a basic block 
  // this check is skipped

  // if ( org_instr.size() > 1 ) {

  // Splits the BB that contains the return instruction into
  // two basic blocks:
  // BB will contain the return instruction
  // BeforeRetBB will contain all of the instructions before the return one
  //
  // These two BBs will be linked meaning that BeforeRetBB->successor == BB
  BasicBlock *BeforeRetBB = BB.splitBasicBlockBefore(Term);

  // Creating control basic block to insert before
  // the return instruction
  BasicBlock *ControlBB = BasicBlock::Create(
    BB.getContext(), 
    "RAFCED_ret_verification_BB", 
    BB.getParent(),
    &BB
  );

  // Relinking the basic blocks so that the structure 
  // results in: BeforeRetBB->ControlBB->BB
  BeforeRetBB->getTerminator()->replaceSuccessorWith(&BB, ControlBB);

  // Inserting instructions into ControlBB
  IRBuilder<> ControlIR(ControlBB);
  // 16:     returnVal ← random number
  uint64_t random_ret_value = dist64(rng64);
  // 17:     adjustValue ← (compileTimeSigBB + Sum) -
  // 18:                 returnVal
  //
  // This is a 64 bit SIGNED integer (cause a subtraction happens
  // and it cannot previously be established that it will be positive)
  long int adj_value = static_cast<uint64_t>(compileTimeSig[&BB]) 
    + sumIntraInstruction[&BB] - random_ret_value;

  // 19:   Insert signature update before return instr.
  // 20:     signature ← signature + adjustValue // wrong must be subtracted
  // 21:     if signature != returnVal error()
  Value *Sig = ControlIR.CreateLoad(IntType, RuntimeSigGV, "checking_sign");
  Value *CmpVal = ControlIR.CreateSub(Sig, llvm::ConstantInt::get(IntType, adj_value), "checking_value");
  Value *CmpSig = ControlIR.CreateCmp(llvm::CmpInst::ICMP_EQ, CmpVal, 
			      llvm::ConstantInt::get(IntType, random_ret_value));

  ControlIR.CreateCondBr(CmpSig, &BB, &ErrBB);
  // }

  return Term;
}

PreservedAnalyses RACFED::run(Module &Md, ModuleAnalysisManager &AM) {
  auto *I64 = llvm::Type::getInt64Ty(Md.getContext());

  // Runtime signature defined as a global variable
  GlobalVariable *RuntimeSig = new GlobalVariable(
    Md, I64,
    /*isConstant=*/false,
    GlobalValue::ExternalLinkage,
    ConstantInt::get(I64, 0),
    "signature"
  );

  createFtFuncs(Md);
  getFuncAnnotations(Md, FuncAnnotations);
  LinkageMap linkageMap = mapFunctionLinkageNames((Md));
  
  for (Function &Fn: Md) {
    if (!shouldCompile(Fn, FuncAnnotations)) continue;

    initializeBlocksSignatures(Fn);
    if (!(Fn.isDeclaration() || Fn.empty()))
      insertIntraInstructionUpdates(Fn, RuntimeSig, I64);

    // Search debug location point
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

    assert(!getLinkageName(linkageMap,"SigMismatch_Handler").empty() 
	   && "Function SigMismatch_Handler is missing!");

    // Create error basic block
    BasicBlock *ErrBB = BasicBlock::Create(Fn.getContext(), "ErrBB", &Fn);
    // Define instruction call to SigMismatch_Handler
    auto CalleeF = ErrBB->getModule()->getOrInsertFunction(
      getLinkageName(linkageMap,"SigMismatch_Handler"),
      FunctionType::getVoidTy(Md.getContext())
    );

    IRBuilder<> ErrIR(ErrBB);
    // Add call instruction to function SigMismatch_Handler
    ErrIR.CreateCall(CalleeF)->setDebugLoc(debugLoc);
    ErrIR.CreateUnreachable();
    
    // Initialize runtime signature backup
    Value *runtime_sign_bkup = nullptr;
    // Initialize return instruction: used to reinstate 
    // the runtime signature of the callee
    Instruction *ret_inst = nullptr;

    for (BasicBlock &BB : Fn) {
      // Backup of compile time sign when entering a function
      if ( BB.isEntryBlock() ) {
        IRBuilder<> InstrIR(&*BB.getFirstInsertionPt());
	if ( Fn.getName() != "main" ) {
	  runtime_sign_bkup =
	    InstrIR.CreateLoad(I64, RuntimeSig, "backup_run_sig");
	}
	// Set runtime signature to compile time signature 
	// of the function's entry block.
	InstrIR.CreateStore(llvm::ConstantInt::get(I64, compileTimeSig[&BB]),
	  RuntimeSig);
      }

      checkJumpSignature(BB, RuntimeSig, I64, *ErrBB);
      ret_inst = checkOnReturn(BB, RuntimeSig, I64, *ErrBB, runtime_sign_bkup);
      updateBeforeJump(Md, BB, RuntimeSig, I64);

      // Restore signature on return
      if ( ret_inst != nullptr && Fn.getName() != "main") {
	      IRBuilder<> RetInstIR(ret_inst);
	      RetInstIR.CreateStore(runtime_sign_bkup, RuntimeSig);
      }
    }
  }

  #if (LOG_COMPILED_FUNCS == 1)
  persistCompiledFunctions(CompiledFuncs, "compiled_racfed_functions.csv");
  #endif

  // There is nothing that this pass preserved
  return PreservedAnalyses::none();
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK

llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "RACFED", "v0.1", [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "racfed-verify") {
                    MPM.addPass(RACFED());
                    return true;
                  }
                  return false;
                });
          }};
}

