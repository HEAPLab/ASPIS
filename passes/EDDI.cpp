/**
 * ************************************************************************************************
 * @brief  LLVM pass implementing Error Detection by Duplicate Instructions
 * (EDDI). Original algorithm by Oh et Al. (DOI: 10.1109/24.994913)
 *
 * @author Davide Baroffio, Politecnico di Milano, Italy
 * (davide.baroffio@polimi.it)
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
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <array>
#include <fstream>
#include <iostream>
#include <list>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Errc.h>
#include <llvm/Support/ModRef.h>
#include <map>
#include <queue>
#include <unordered_set>

#include "Utils/Utils.h"

using namespace llvm;

#define DEBUG_TYPE "eddi_verification"

/**
 * - 0: EDDI (Add checks at every basic block)
 * - 1: FDSC (Add checks only at basic blocks with more than one predecessor)
 */
// #define SELECTIVE_CHECKING 0

// #define CHECK_AT_STORES
// #define CHECK_AT_CALLS
// #define CHECK_AT_BRANCH

/**
 * Determines whether a instruction &I is used by store instructions different
 * than &Use
 * @param I is the operand that we want to check whether is used by store
 * @param Use is the instruction that has I as operand
 */
int EDDI::isUsedByStore(Instruction &I, Instruction &Use) {
  BasicBlock *BB = I.getParent();
  /* get I users and check whether the BB of I is in the successors of the user
   */
  for (User *U : I.users()) {
    if (isa<StoreInst>(U) && U != &Use) {
      Instruction *U_st = cast<StoreInst>(U);
      // find BB in U_st successors
      std::unordered_set<BasicBlock *> reachable;
      std::queue<BasicBlock *> worklist;
      worklist.push(U_st->getParent());
      while (!worklist.empty()) {
        BasicBlock *front = worklist.front();
        if (front == BB)
          return 1;
        worklist.pop();
        for (BasicBlock *succ : successors(front)) {
          if (reachable.count(succ) == 0) {
            /// We need the check here to ensure that we don't run
            /// infinitely if the CFG has a loop in it
            /// i.e. the BB reaches itself directly or indirectly
            worklist.push(succ);
            reachable.insert(succ);
          }
        }
      }
    }
  }
  return 0;
}

/**
 * Clones instruction `I` and adds the pair <I, IClone> to
 * DuplicatedInstructionMap, inserting the clone right after the original.
 */
Instruction *
EDDI::cloneInstr(Instruction &I,
                 std::map<Value *, Value *> &DuplicatedInstructionMap) {
  Instruction *IClone = I.clone();

  if (!I.getType()->isVoidTy() && I.hasName()) {
    IClone->setName(I.getName() + "_dup");
  }

  // if the instruction is an alloca and alternate-memmap is disabled, place it
  // at the end of the list of alloca instruction
  if (AlternateMemMapEnabled == false && isa<AllocaInst>(I)) {
    IClone->insertBefore(&*I.getParent()->getFirstNonPHIOrDbgOrAlloca());
  } // else place it right after the instruction we are working on
  else {
    IClone->insertAfter(&I);
  }
  DuplicatedInstructionMap.insert(
      std::pair<Instruction *, Instruction *>(&I, IClone));
  DuplicatedInstructionMap.insert(
      std::pair<Instruction *, Instruction *>(IClone, &I));
  return IClone;
}

/**
 * Takes instruction I and duplicates its operands. Then substitutes each
 * duplicated operand in the duplicated instruction IClone.
 *
 * @param DuplicatedInstructionMap is the map of duplicated instructions, needed
 * for the recursive duplicateInstruction call
 * @param ErrBB is the error basic block to jump to in case of error needed for
 * the recursive duplicateInstruction call
 */
void EDDI::duplicateOperands(
    Instruction &I, std::map<Value *, Value *> &DuplicatedInstructionMap,
    BasicBlock &ErrBB) {
  Instruction *IClone = NULL;
  // see if I has a clone
  if (DuplicatedInstructionMap.find(&I) != DuplicatedInstructionMap.end()) {
    Value *VClone = DuplicatedInstructionMap.find(&I)->second;
    if (isa<Instruction>(VClone)) {
      IClone = cast<Instruction>(VClone);
    }
  }

  int J = 0;
  // iterate over the operands and switch them with their duplicates in the
  // duplicated instructions
  for (Value *V : I.operand_values()) {
    // if the operand has not been duplicated we need to duplicate it
    if (isa<Instruction>(V)) {
      Instruction *Operand = cast<Instruction>(V);
      if (!isValueDuplicated(DuplicatedInstructionMap, *Operand))
        duplicateInstruction(*Operand, DuplicatedInstructionMap, ErrBB);
    }
    // It may happen that we have a GEP as inline operand of a instruction. The
    // operands of the GEP are not duplicated leading to errors, so we manually
    // clone of the GEP for the clone of the original instruction.
    else if (isa<GEPOperator>(V) && isa<ConstantExpr>(V)) {
      if (IClone != NULL) {
        GEPOperator *GEPOperand = cast<GEPOperator>(IClone->getOperand(J));
        Value *PtrOperand = GEPOperand->getPointerOperand();
        // update the duplicate GEP operator using the duplicate of the pointer
        // operand
        if (DuplicatedInstructionMap.find(PtrOperand) !=
            DuplicatedInstructionMap.end()) {
          std::vector<Value *> indices;
          for (auto &Idx : GEPOperand->indices()) {
            indices.push_back(Idx);
          }
          Constant *CloneGEPOperand =
              cast<ConstantExpr>(GEPOperand)
                  ->getInBoundsGetElementPtr(
                      GEPOperand->getSourceElementType(),
                      cast<Constant>(
                          DuplicatedInstructionMap.find(PtrOperand)->second),
                      ArrayRef<Value *>(indices));
          IClone->setOperand(J, CloneGEPOperand);
        }
      }
    }

    if (IClone != NULL) {
      // use the duplicated instruction as operand of IClone
      auto Duplicate = DuplicatedInstructionMap.find(V);
      if (Duplicate != DuplicatedInstructionMap.end())
        IClone->setOperand(
            J,
            Duplicate->second); // set the J-th operand with the duplicate value
    }
    J++;
  }
}

// recursively follow store instructions to find the pointer final value,
// if the value cannot be found (e.g. when the pointer is passed as function
// argument) we return NULL.
Value *EDDI::getPtrFinalValue(Value &V) {
  Value *res = NULL;

  if (V.getType()->isPointerTy()) {
    // find the store using V as ptr
    for (User *U : V.users()) {
      if (isa<StoreInst>(U)) {
        StoreInst *SI = cast<StoreInst>(U);
        if (SI->getPointerOperand() == &V) { // we found the store

          // if the store saves a pointer we work recursively to find the
          // original value
          if (SI->getValueOperand()->getType()->isPointerTy()) {
            return getPtrFinalValue(*(SI->getValueOperand()));
          } else {
            return &V;
          }
        }
      }
    }
  }

  return res;
}

// Follows the pointers V1 and V2 using getPtrFinalValue() and adds a compare
// instruction using the IRBuilder B.
Value *EDDI::comparePtrs(Value &V1, Value &V2, IRBuilder<> &B) {
  /**
   * synthax `store val, ptr`
   *
   * There is the following case:
   * store a, b
   * store b, c
   *
   * If I have c, I need to perform 2 loads: one load for finding b and one load
   * for finding a _b = load c _a = load _b
   */

  Value *F1 = getPtrFinalValue(V1);
  Value *F2 = getPtrFinalValue(V2);

  if (F1 != NULL && F2 != NULL && !F1->getType()->isPointerTy()) {
    Instruction *L1 = B.CreateLoad(F1->getType(), F1);
    Instruction *L2 = B.CreateLoad(F2->getType(), F2);
    if (L1->getType()->isFloatingPointTy()) {
      return B.CreateCmp(CmpInst::FCMP_UEQ, L1, L2);
    } else {
      return B.CreateCmp(CmpInst::ICMP_EQ, L1, L2);
    }
  }
  return NULL;
}

/**
 * Adds a consistency check on the instruction I
 */
void EDDI::addConsistencyChecks(
    Instruction &I, std::map<Value *, Value *> &DuplicatedInstructionMap,
    BasicBlock &ErrBB) {
  std::vector<Value *> CmpInstructions;

  // split and add the verification BB
  I.getParent()->splitBasicBlockBefore(&I);
  BasicBlock *VerificationBB =
      BasicBlock::Create(I.getContext(), "VerificationBB",
                         I.getParent()->getParent(), I.getParent());
  I.getParent()->replaceUsesWithIf(VerificationBB, IsNotAPHINode);
  IRBuilder<> B(VerificationBB);

  // add a comparison for each operand
  for (Value *V : I.operand_values()) {
    // we compare the operands if they are instructions
    if (isa<Instruction>(V)) {
      // get the duplicate of the operand
      Instruction *Operand = cast<Instruction>(V);

      // If the operand is a pointer and is not used by any store, we skip the
      // operand
      if (Operand->getType()->isPointerTy() && !isUsedByStore(*Operand, I)) {
        continue;
      }

      auto Duplicate = DuplicatedInstructionMap.find(Operand);

      // if the duplicate exists we perform a compare
      if (Duplicate != DuplicatedInstructionMap.end()) {
        Value *Original = Duplicate->first;
        Value *Copy = Duplicate->second;

        // if the operand is a pointer we try to get a compare on pointers
        if (Original->getType()->isPointerTy()) {
          Value *CmpInstr = comparePtrs(*Original, *Copy, B);
          if (CmpInstr != NULL) {
            CmpInstructions.push_back(CmpInstr);
          }
        }
        // if the operand is an array we have to compare all its elements
        else if (Original->getType()->isArrayTy()) {
          if (!Original->getType()->getArrayElementType()->isAggregateType()) {
            int arraysize = Original->getType()->getArrayNumElements();

            for (int i = 0; i < arraysize; i++) {
              Value *OriginalElem = B.CreateExtractValue(Original, i);
              Value *CopyElem = B.CreateExtractValue(Copy, i);
              DuplicatedInstructionMap.insert(
                  std::pair<Value *, Value *>(OriginalElem, CopyElem));
              DuplicatedInstructionMap.insert(
                  std::pair<Value *, Value *>(CopyElem, OriginalElem));

              if (OriginalElem->getType()->isPointerTy()) {
                Value *CmpInstr = comparePtrs(*OriginalElem, *CopyElem, B);
                if (CmpInstr != NULL) {
                  CmpInstructions.push_back(CmpInstr);
                }
              } else {
                if (OriginalElem->getType()->isFloatingPointTy()) {
                  CmpInstructions.push_back(
                      B.CreateCmp(CmpInst::FCMP_UEQ, OriginalElem, CopyElem));
                } else {
                  CmpInstructions.push_back(
                      B.CreateCmp(CmpInst::ICMP_EQ, OriginalElem, CopyElem));
                }
              }
            }
          }
        }
        // else we just add a compare
        else {
          if (Original->getType()->isFloatingPointTy()) {
            CmpInstructions.push_back(
                B.CreateCmp(CmpInst::FCMP_UEQ, Original, Copy));
          } else {
            CmpInstructions.push_back(
                B.CreateCmp(CmpInst::ICMP_EQ, Original, Copy));
          }
        }
      }
    }
  }

  // if in the end we have a set of compare instructions, we check that all of
  // them are true
  if (!CmpInstructions.empty()) {
    // all comparisons must be true
    Value *AndInstr = B.CreateAnd(CmpInstructions);
    B.CreateCondBr(AndInstr, I.getParent(), &ErrBB)
        ->setDebugLoc(I.getDebugLoc());
  }

  if (VerificationBB->size() == 0) {
    B.CreateBr(I.getParent())->setDebugLoc(I.getDebugLoc());
  }
}

// Given an instruction, loads and stores the pointers passed to the
// instruction. This is useful in the case I is a CallBase, since the function
// called might not be in the compilation unit, and the function called may
// modify the content of the pointer passed as argument. This function has the
// objective of synchronize pointers after some non-duplicated instruction
// execution.
void EDDI::fixFuncValsPassedByReference(
    Instruction &I, std::map<Value *, Value *> &DuplicatedInstructionMap,
    IRBuilder<> &B) {
  int numOps = I.getNumOperands();
  for (int i = 0; i < numOps; i++) {
    Value *V = I.getOperand(i);
    if (isa<Instruction>(V) && V->getType()->isPointerTy()) {
      Instruction *Operand = cast<Instruction>(V);
      auto Duplicate = DuplicatedInstructionMap.find(Operand);
      if (Duplicate != DuplicatedInstructionMap.end()) {
        Value *Original = Duplicate->first;
        Value *Copy = Duplicate->second;

        Type *OriginalType = Original->getType();
        Instruction *TmpLoad = B.CreateLoad(OriginalType, Original);
        Instruction *TmpStore = B.CreateStore(TmpLoad, Copy);
        DuplicatedInstructionMap.insert(
            std::pair<Instruction *, Instruction *>(TmpLoad, TmpLoad));
        DuplicatedInstructionMap.insert(
            std::pair<Instruction *, Instruction *>(TmpStore, TmpStore));
      }
    }
  }
}

// Given Fn, it returns the version of the function with duplicated arguments,
// or the function Fn itself if it is already the version with duplicated
// arguments
Function *EDDI::getFunctionDuplicate(Function *Fn) {
  // If Fn ends with "_dup" we have already the duplicated function.
  // If Fn is NULL, it means that we don't have a duplicate
  if (Fn == NULL || Fn->getName().endswith("_dup")) {
    return Fn;
  }

  // Otherwise, we try to get the "_dup" version or the "_ret_dup" version
  Function *FnDup = Fn->getParent()->getFunction(Fn->getName().str() + "_dup");
  if (FnDup == NULL) {
    FnDup = Fn->getParent()->getFunction(Fn->getName().str() + "_ret_dup");
  }
  return FnDup;
}

// Given Fn, it returns the version of the function without the duplicated
// arguments, or the function Fn itself if it is already the version without
// duplicated arguments
Function *EDDI::getFunctionFromDuplicate(Function *Fn) {
  // If Fn ends with "_dup" we have already the duplicated function.
  // If Fn is NULL, it means that we don't have a duplicate
  if (Fn == NULL || !Fn->getName().endswith("_dup")) {
    return Fn;
  }

  // Otherwise, we try to get the non-"_dup" version
  Function *FnDup = Fn->getParent()->getFunction(
      Fn->getName().str().substr(0, Fn->getName().str().length() - 8));
  if (FnDup == NULL) {
    FnDup = Fn->getParent()->getFunction(
        Fn->getName().str().substr(0, Fn->getName().str().length() - 4));
  }
  return FnDup;
}

void EDDI::duplicateGlobals(
    Module &Md, std::map<Value *, Value *> &DuplicatedInstructionMap) {
  Value *RuntimeSig;
  Value *RetSig;
  std::list<GlobalVariable *> GVars;
  for (GlobalVariable &GV : Md.globals()) {
    GVars.push_back(&GV);
  }
  for (auto GV : GVars) {
    if (!isa<Function>(GV) &&
        FuncAnnotations.find(GV) != FuncAnnotations.end()) {
      if ((FuncAnnotations.find(GV))->second.startswith("runtime_sig") ||
          (FuncAnnotations.find(GV))->second.startswith("run_adj_sig")) {
        continue;
      }
    }
    /**
     * The global variable is duplicated if all the following hold:
     * - It is not a function
     * - It is not constant (i.e. read only)
     * - It is not a struct
     * - Doesn't end with "_dup" (i.e. has already been duplicated)
     * - Has internal linkage and either:
     *        a) It is not an array
     *        b) It is an array but its elements are neither structs nor arrays
     */
    bool isFunction = GV->getType()->isFunctionTy();
    bool isConstant = GV->isConstant();
    bool isStruct = GV->getValueType()->isStructTy();
    bool isArray = GV->getValueType()->isArrayTy();
    bool isPointer = GV->getValueType()->isOpaquePointerTy();
    bool endsWithDup = GV->getName().endswith("_dup");
    bool hasInternalLinkage = GV->hasInternalLinkage();
    bool isMetadataInfo = GV->getSection() == "llvm.metadata";
    bool toExclude = !isa<Function>(GV) &&
                     FuncAnnotations.find(GV) != FuncAnnotations.end() &&
                     (FuncAnnotations.find(GV))->second.startswith("exclude");

    if (! (isFunction || isConstant || endsWithDup || isMetadataInfo || toExclude) // is not function, constant, struct and does not end with _dup
        /* && ((hasInternalLinkage && (!isArray || (isArray && !cast<ArrayType>(GV.getValueType())->getArrayElementType()->isAggregateType() ))) // has internal linkage and is not an array, or is an array but the element type is not aggregate
            || !isArray) */ // if it does not have internal linkage, it is not an array or a pointer
        ) {
      Constant *Initializer = nullptr;
      if (GV->hasInitializer()) {
        Initializer = GV->getInitializer();
      }

      GlobalVariable *InsertBefore;

      if (AlternateMemMapEnabled == false) {
        InsertBefore = GVars.front();
      } else {
        InsertBefore = GV;
      }

      // get a copy of the global variable
      GlobalVariable *GVCopy = new GlobalVariable(
          Md, GV->getValueType(), false, GV->getLinkage(), Initializer,
          GV->getName() + "_dup", InsertBefore, GV->getThreadLocalMode(),
          GV->getAddressSpace(), GV->isExternallyInitialized());

      if (AlternateMemMapEnabled == false && !GV->hasSection() &&
          !GV->hasInitializer()) {
        GVCopy->setSection(DuplicateSecName);
      }

      GVCopy->setAlignment(GV->getAlign());
      GVCopy->setDSOLocal(GV->isDSOLocal());
      // Save the duplicated global so that the duplicate can be used as operand
      // of other duplicated instructions
      DuplicatedInstructionMap.insert(std::pair<Value *, Value *>(GV, GVCopy));
      DuplicatedInstructionMap.insert(std::pair<Value *, Value *>(GVCopy, GV));
    }
  }
}

  bool EDDI::isAllocaForExceptionHandling(AllocaInst &I){
    for (auto e : I.users())
    {
      if (isa<StoreInst>(e)){
        StoreInst *storeInst=cast<StoreInst>(e);
        auto *valueOperand =storeInst->getValueOperand();
        if(isa<CallBase>(valueOperand)){
          CallBase *callInst = cast<CallBase>(valueOperand);
          if (callInst->getCalledFunction()->getName().equals("__cxa_begin_catch"))
          {return true;}
        }
        
      }
    }
    return false;
}

/**
 * Performs a duplication of the instruction I. Performing the following
 * operations depending on the class of I:
 * - Clone the instruction;
 * - Duplicate the instruction operands;
 * - Add consistency checks on the operands (if I is a synchronization point).
 * @returns 1 if the cloned instruction has to be removed, 0 otherwise
 */
int EDDI::duplicateInstruction(
    Instruction &I, std::map<Value *, Value *> &DuplicatedInstructionMap,
    BasicBlock &ErrBB) {
  if (isValueDuplicated(DuplicatedInstructionMap, I)) {
    return 0;
  }

  int res = 0;

  // if the instruction is an alloca instruction we need to duplicate it
  if (isa<AllocaInst>(I)) {
    
    if (!isAllocaForExceptionHandling(cast<AllocaInst>(I))){
      
      cloneInstr(I, DuplicatedInstructionMap);

    };

    
  }

  // if the instruction is a binary/unary instruction we need to duplicate it
  // checking for its operands
  else if (isa<BinaryOperator, UnaryInstruction, LoadInst, GetElementPtrInst,
               CmpInst, PHINode, SelectInst,InsertValueInst>(I)) {
    // duplicate the instruction
    cloneInstr(I, DuplicatedInstructionMap);

    // duplicate the operands
    duplicateOperands(I, DuplicatedInstructionMap, ErrBB);
  }

  // if the instruction is a store instruction we need to duplicate it and its
  // operands (if not duplicated already) and add consistency checks
  else if (isa<StoreInst, AtomicRMWInst, AtomicCmpXchgInst>(I)) {
    Instruction *IClone = cloneInstr(I, DuplicatedInstructionMap);

    // duplicate the operands
    duplicateOperands(I, DuplicatedInstructionMap, ErrBB);

    // add consistency checks on I

#ifdef CHECK_AT_STORES
#if (SELECTIVE_CHECKING == 1)
    if (I.getParent()->getTerminator()->getNumSuccessors() > 1)
#endif
      addConsistencyChecks(I, DuplicatedInstructionMap, ErrBB);
#endif
    // it may happen that I duplicate a store but don't change its operands, if
    // that happens I just remove the duplicate
    if (IClone->isIdenticalTo(&I)) {
      IClone->eraseFromParent();
      DuplicatedInstructionMap.erase(DuplicatedInstructionMap.find(&I));
    }
  }

  // if the instruction is a branch/switch/return instruction, we need to
  // duplicate its operands (if not duplicated already) and add consistency
  // checks
  else if (isa<BranchInst, SwitchInst, ReturnInst, IndirectBrInst>(I)) {
    // duplicate the operands
    duplicateOperands(I, DuplicatedInstructionMap, ErrBB);

// add consistency checks on I
#ifdef CHECK_AT_BRANCH
    if (I.getParent()->getTerminator()->getNumSuccessors() > 1)
      addConsistencyChecks(I, DuplicatedInstructionMap, ErrBB);
#endif
  }

  // if the istruction is a call, we duplicate the operands and add consistency
  // checks
  else if (isa<CallBase>(I)) {
    CallBase *CInstr = cast<CallBase>(&I);
    // there are some instructions that can be annotated with "to_duplicate" in
    // order to tell the pass to duplicate the function call.
    Function *Callee = CInstr->getCalledFunction();
    Callee = getFunctionFromDuplicate(Callee);
    // check if the function call has to be duplicated
    if ((FuncAnnotations.find(Callee) != FuncAnnotations.end() &&
         (*FuncAnnotations.find(Callee)).second.startswith("to_duplicate")) ||
        isIntrinsicToDuplicate(CInstr)) {
      // duplicate the instruction
      cloneInstr(*CInstr, DuplicatedInstructionMap);

      // duplicate the operands
      duplicateOperands(I, DuplicatedInstructionMap, ErrBB);

// add consistency checks on I
#ifdef CHECK_AT_CALLS
#if (SELECTIVE_CHECKING == 1)
      if (I.getParent()->getTerminator()->getNumSuccessors() > 1)
#endif
        addConsistencyChecks(I, DuplicatedInstructionMap, ErrBB);
#endif
    }

    else {
      // duplicate the operands
      duplicateOperands(I, DuplicatedInstructionMap, ErrBB);

// add consistency checks on I
#ifdef CHECK_AT_CALLS
#if (SELECTIVE_CHECKING == 1)
      if (I.getParent()->getTerminator()->getNumSuccessors() > 1)
#endif
        addConsistencyChecks(I, DuplicatedInstructionMap, ErrBB);
#endif

      IRBuilder<> B(CInstr);
      if (!isa<InvokeInst>(CInstr)) {
        B.SetInsertPoint(I.getNextNonDebugInstruction());
      } else {
        B.SetInsertPoint(
            &*cast<InvokeInst>(CInstr)->getNormalDest()->getFirstInsertionPt());
      }
      // get the function with the duplicated signature, if it exists
      Function *Fn = getFunctionDuplicate(CInstr->getCalledFunction());
      // if the _dup function exists, we substitute the call instruction with a
      // call to the function with duplicated arguments
      if (Fn != NULL) {
        std::vector<Value *> args;
        int i = 0;
        for (Value *Original : CInstr->args()) {
          Value *Copy = Original;
          // see if Original has a copy
          if (DuplicatedInstructionMap.find(Original) !=
              DuplicatedInstructionMap.end()) {
            Copy = DuplicatedInstructionMap.find(Original)->second;
          }

          if (AlternateMemMapEnabled == false) {
            args.insert(args.begin() + i, Original);
            args.push_back(Copy);
          } else {
            args.push_back(Original);
            args.push_back(Copy);
          }
          i++;
        }

        if (Fn != CInstr->getCalledFunction()) {
          Instruction *NewCInstr;
          IRBuilder<> CallBuilder(CInstr);
          if (isa<InvokeInst>(CInstr)) {
            InvokeInst *IInst=cast<InvokeInst>(CInstr);
            NewCInstr = CallBuilder.CreateInvoke(Fn->getFunctionType(), Fn,IInst->getNormalDest(),IInst->getUnwindDest(), args);
          } else {
            NewCInstr =  CallBuilder.CreateCall(Fn->getFunctionType(), Fn, args);
          }

          NewCInstr->setDebugLoc(CInstr->getDebugLoc());
          res = 1;
          CInstr->replaceNonMetadataUsesWith(NewCInstr);
        }
      } else {
        fixFuncValsPassedByReference(*CInstr, DuplicatedInstructionMap, B);
      }
    }
  }

 /*  else {
    errs() << I <<"\n";
  } */

/*   if (!isa<InvokeInst>(I)) {
    errs() << "it's an invoke";
    errs() << *I.getParent();
  } */
  return res;
}

/**
 * @returns True if the value V is present in the DuplicatedInstructionMap
 * either as a key or as value
 */
bool EDDI::isValueDuplicated(
    std::map<Value *, Value *> &DuplicatedInstructionMap, Instruction &V) {
  for (auto Elem : DuplicatedInstructionMap) {
    if (Elem.first == &V || Elem.second == &V) {
      return true;
    }
  }
  return false;
}

Function *
EDDI::duplicateFnArgs(Function &Fn, Module &Md,
                      std::map<Value *, Value *> &DuplicatedInstructionMap) {
  Type *RetType = Fn.getReturnType();
  FunctionType *FnType = Fn.getFunctionType();

  // create the param type lists
  std::vector<Type *> paramTypeVec;
  for (int i = 0; i < Fn.arg_size(); i++) {
    Type *ParamType = FnType->params()[i];
    if (AlternateMemMapEnabled == false) { // sequential
      paramTypeVec.insert(paramTypeVec.begin() + i, ParamType);
      paramTypeVec.push_back(ParamType);
    } else {
      paramTypeVec.push_back(ParamType);
      paramTypeVec.push_back(ParamType); // two times
    }
  }

  // update the function type adding the duplicated args
  FunctionType *NewFnType = FnType->get(RetType,             // returntype
                                        paramTypeVec,        // params
                                        FnType->isVarArg()); // vararg

  // create the function and clone the old one
  Function *ClonedFunc = Fn.Create(NewFnType, Fn.getLinkage(),
                                   Fn.getName() + "_dup", Fn.getParent());
  ValueToValueMapTy Params;
  for (int i = 0; i < Fn.arg_size(); i++) {
    if (AlternateMemMapEnabled == false) {
      Params[Fn.getArg(i)] = ClonedFunc->getArg(Fn.arg_size() + i);
    } else {
      Params[Fn.getArg(i)] = ClonedFunc->getArg(i * 2);
    }
  }
  SmallVector<ReturnInst *, 8> returns;
  CloneFunctionInto(ClonedFunc, &Fn, Params,
                    CloneFunctionChangeType::GlobalChanges, returns);

  return ClonedFunc;
}

/**
 * I have to duplicate all instructions except function calls and branches
 * @param Md
 * @return
 */
PreservedAnalyses EDDI::run(Module &Md, ModuleAnalysisManager &AM) {
  LLVM_DEBUG(dbgs() << "Initializing EDDI...\n");

  LLVM_DEBUG(dbgs() << "Getting annotations... ");
  getFuncAnnotations(Md, FuncAnnotations);
  LLVM_DEBUG(dbgs() << "[done]\n");

  LinkageMap linkageMap = mapFunctionLinkageNames(Md);

  std::map<Value *, Value *>
      DuplicatedInstructionMap; // is a map containing the instructions
                                // and their duplicates

  LLVM_DEBUG(dbgs() << "Duplicating globals... ");
  duplicateGlobals(Md, DuplicatedInstructionMap);
  LLVM_DEBUG(dbgs() << "[done]\n");

  // store the functions that are currently in the module
  std::list<Function *> FnList;
  std::set<Function *> DuplicatedFns;

  LLVM_DEBUG(dbgs() << "Retrieving functions to compile... ");
  // first store the instructions to compile in the current module
  int cnt = 0;
  for (Function &Fn : Md) {
    if (shouldCompile(Fn, FuncAnnotations, OriginalFunctions)) {
      cnt++;
      // LLVM_DEBUG(dbgs() << "Found: " << cnt << "\r");
      FnList.push_back(&Fn);
      ValueToValueMapTy Params;
      Function *OriginalFn = CloneFunction(&Fn, Params);
      OriginalFunctions.insert(OriginalFn);
      OriginalFn->setName(Fn.getName().str() + "_original");
    }
  }

  LLVM_DEBUG(dbgs() << "Found: " << FnList.size() << "\n");

  // then duplicate the function arguments using FnList populated earlier
  for (Function *Fn : FnList) {
    Function *newFn = duplicateFnArgs(*Fn, Md, DuplicatedInstructionMap);
    DuplicatedFns.insert(newFn);
  }

  // list of duplicated instructions to remove since they are equal to the
  // original
  std::list<Instruction *> InstructionsToRemove;
  int i = -1;
  int tot_funcs = 0;
  for (Function &Fn : Md) {
    if (shouldCompile(Fn, FuncAnnotations, OriginalFunctions)) {
      tot_funcs++;
    }
  }
  LLVM_DEBUG(dbgs() << "Iterating over the module functions...\n");
  for (Function &Fn : Md) {
    if (shouldCompile(Fn, FuncAnnotations, OriginalFunctions)) {
      i++;
      LLVM_DEBUG(dbgs() << "Compiling " << i << "/" << tot_funcs << ": "
                        << Fn.getName() << "\n");
      //"                                                              \r" );
      CompiledFuncs.insert(&Fn);
      BasicBlock *ErrBB = BasicBlock::Create(Fn.getContext(), "ErrBB", &Fn);

      // If the function is a duplicated one, we need to
      // iterate over the function arguments and duplicate
      // them in order to access them during the instruction
      // duplication phase
      if (DuplicatedFns.find(&Fn) != DuplicatedFns.end()) {
        // save the function arguments and their duplicates
        for (int i = 0; i < Fn.arg_size(); i++) {
          Value *Arg, *ArgClone;
          if (AlternateMemMapEnabled == false) {
            if (i >= Fn.arg_size() / 2) {
              break;
            }
            Arg = Fn.getArg(i);
            ArgClone = Fn.getArg(i + Fn.arg_size() / 2);
          } else {
            if (i % 2 == 1)
              continue;
            Arg = Fn.getArg(i);
            ArgClone = Fn.getArg(i + 1);
          }
          DuplicatedInstructionMap.insert(
              std::pair<Value *, Value *>(Arg, ArgClone));
          DuplicatedInstructionMap.insert(
              std::pair<Value *, Value *>(ArgClone, Arg));
          for (User *U : Arg->users()) {
            if (isa<Instruction>(U)) {
              // duplicate the uses of each argument
              duplicateInstruction(cast<Instruction>(*U),
                                   DuplicatedInstructionMap, *ErrBB);
            }
          }
        }
      }

      for (BasicBlock &BB : Fn) {
        for (Instruction &I : BB) {
          if (!isValueDuplicated(DuplicatedInstructionMap, I)) {
            // perform the duplication
            int shouldDelete =
                duplicateInstruction(I, DuplicatedInstructionMap, *ErrBB);
            // the instruction duplicated may be equal to the original, so we
            // return shouldDelete in order to drop the duplicates
            if (shouldDelete) {
              InstructionsToRemove.push_back(&I);
            }
          }
        }
      }

      // insert the code for calling the error basic block in case of a mismatch
      IRBuilder<> ErrB(ErrBB);

      assert(!getLinkageName(linkageMap, "DataCorruption_Handler").empty() &&
             "Function DataCorruption_Handler is missing!");
      auto CalleeF = ErrBB->getModule()->getOrInsertFunction(
          getLinkageName(linkageMap, "DataCorruption_Handler"),
          FunctionType::getVoidTy(Md.getContext()));

      auto *CallI = ErrB.CreateCall(CalleeF);
      ErrB.CreateUnreachable();

      std::list<Instruction *> errBranches;
      for (User *U : ErrBB->users()) {
        Instruction *I = cast<Instruction>(U);
        errBranches.push_back(I);
      }
      for (Instruction *I : errBranches) {
        ValueToValueMapTy VMap;
        BasicBlock *ErrBBCopy = CloneBasicBlock(ErrBB, VMap);
        ErrBBCopy->insertInto(ErrBB->getParent(), I->getParent());
        // set the debug location to the instruction the ErrBB is related to
        for (Instruction &ErrI : *ErrBBCopy) {
          if (!I->getDebugLoc()) {
            ErrI.setDebugLoc(findNearestDebugLoc(*Fn.back().getTerminator()));
          } else {
            ErrI.setDebugLoc(I->getDebugLoc());
          }
        }
        I->replaceSuccessorWith(ErrBB, ErrBBCopy);
      }
      ErrBB->eraseFromParent();
    }
  }

  // Drop the instructions that have been marked for removal earlier
  for (Instruction *I2rm : InstructionsToRemove) {
    I2rm->eraseFromParent();
  }

  persistCompiledFunctions(CompiledFuncs, "compiled_eddi_functions.csv");

/*   if (Function *mainFunc = Md.getFunction("main")) {
    errs() << *mainFunc;
  } else {
    errs() << "Function 'main' not found!\n";
  }
 */
  return PreservedAnalyses::none();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getEDDIPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "eddi-verify", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "func-ret-to-ref") {
                    FPM.addPass(FuncRetToRef());
                    return true;
                  }
                  return false;
                });
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "eddi-verify") {
                    FPM.addPass(EDDI());
                    return true;
                  }
                  return false;
                });
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "duplicate-globals") {
                    FPM.addPass(DuplicateGlobals());
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
  return getEDDIPluginInfo();
}