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
#include "llvm/Demangle/Demangle.h"
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
#include <regex>
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
// #include "../TypeDeductionAnalysis/TypeDeductionAnalysis.hpp"

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

// Regex to match constructors: the class name should be the same of the function name
std::regex ConstructorRegex(R"(.*([\w]+)::\1\((.*?)\))");

std::set<InvokeInst *> toFixInvokes;

/**
 * @brief Check if the passed store is the one which saves the vtable in the object.
 * In case it is, return the pointer to the GV of the vtable.
 * 
 * @param SInst Reference to the store instruction to analyze.
 * @return The pointer to the vtable global variable, if found; nullptr otherwise.
 */
GlobalVariable* isVTableStore(StoreInst &SInst) {
  if(isa<GetElementPtrInst>(SInst.getValueOperand())) {
    // TODO: Should see the uses of the valueOperand to find this inst in case it happens
    errs() << "this is a GEP instruction\n";
    auto *V = cast<GetElementPtrInst>(SInst.getValueOperand())->getOperand(0);
    if(isa<GlobalVariable>(V)) {
      auto *GV = cast<GlobalVariable>(V);
      auto vtableName = demangle(GV->getName().str());
      // Found "vtable" in name
      if(vtableName.find("vtable") != vtableName.npos) {
        // LLVM_DEBUG(dbgs() << "[REDDI] GEP Vtable name: " << vtableName << " of function " << Fn->getName() << "\n");
        return GV;
      }
    }
  } else if(isa<ConstantExpr>(SInst.getValueOperand())) {
    auto *CE = cast<ConstantExpr>(SInst.getValueOperand());
    if(CE->getOpcode() == Instruction::GetElementPtr && isa<GlobalVariable>(CE->getOperand(0))) {
      auto *GV = cast<GlobalVariable>(CE->getOperand(0));
      auto vtableName = demangle(GV->getName().str());
      // Found "vtable" in name
      if(vtableName.find("vtable") != vtableName.npos) {
        return GV;
      }
    }
  }
  
  return nullptr;
}

/**
 * @brief Retrieve all the virtual methods present in the vtable from the pointer to the constructor.
 * 
 * @param Fn pointer to a function.
 * @return A set containing the virtual functions referenced in the vtable (could be empty).
 */
std::set<Function *> EDDI::getVirtualMethodsFromConstructor(Function *Fn) {
  std::set<Function *> virtualMethods;

  if(!Fn) {
    errs() << "Fn is not a valid function.\n";
    return virtualMethods;
  }

  // Find vtable
  GlobalVariable *vtable = nullptr;
  for(auto &BB : *Fn) {
    for(auto &I : BB) {
      if(isa<StoreInst>(I)){
        auto &SInst = cast<StoreInst>(I);
        vtable = isVTableStore(SInst);
      }

      if(vtable)
        break;
    }

    if(vtable)
      break;
  }
  
  // Get all the virtual methods
  if(vtable) {
    // Ensure the vtable global variable has an initializer
    if(!vtable->hasInitializer()) {
      errs() << "Vtable does not have an initializer.\n";
      return virtualMethods;
    }

    Constant *Initializer = vtable->getInitializer();
    if (!Initializer || !isa<ConstantStruct>(Initializer)) {
      errs() << "Vtable initializer is not a ConstantStruct.\n";
      return virtualMethods;
    }

    // Extract the array field from the struct
    ConstantStruct *VTableStruct = cast<ConstantStruct>(Initializer);

    for(int i = 0; i < VTableStruct->getNumOperands(); i++) {
      Constant *ArrayField = VTableStruct->getOperand(i);
      if (!isa<ConstantArray>(ArrayField)) {
        errs() << "Vtable field " << i << " is not a ConstantArray.\n";
        continue;
      }

      // get virtual functions to harden from vtable
      for (Value *Elem : cast<ConstantArray>(ArrayField)->operands()) {
        if (isa<Function>(Elem)) {
          virtualMethods.insert(cast<Function>(Elem));
          // LLVM_DEBUG(dbgs() << "[REDDI] Found virtual method " << cast<Function>(Elem)->getName() <<  " in " << Fn->getName() << "\n");
        }
      }
    }
  }

  return virtualMethods;
}

/**
 * @brief For each toHardenConstructors, modifies the store for the vtable so that is used 
 * the `_dup` version of that vtable.
 * 
 * identifies the store which saves the vtable in the object (if exists). Found it, 
 * duplicates the vtable (uses all the virtual `_dup` methods) and uses this new vtable 
 * (global variable) in the store.
 * 
 * @param Md The module we are analyzing.
 */
void EDDI::fixDuplicatedConstructors(Module &Md) {
  for(Function *Fn : toHardenConstructors) {
    GlobalVariable *vtable = nullptr;
    GlobalVariable *NewVtable = nullptr;
    StoreInst *SInstVtable = nullptr;
    Function *FnDup = getFunctionDuplicate(Fn);

    if(!FnDup) {
      errs() << "Doesn't exist the dup version of " << Fn->getName() << "\n";
      continue;
    }

    // Find vtable
    LLVM_DEBUG(dbgs() << "[REDDI] Finding vtable for " << Fn->getName() << "\n");
    for(auto &BB : *Fn) {
      for(auto &I : BB) {
        if(isa<StoreInst>(I)){
          auto &SInst = cast<StoreInst>(I);
          vtable = isVTableStore(SInst);
        }
      
        if(vtable)
          break;
      }

      if(vtable) 
        break;
    }

    // Duplicate vtable
    if(vtable && vtable->hasInitializer()) {
      // Ensure the vtable global variable has an initializer
      Constant *Initializer = vtable->getInitializer();
      if (!Initializer || !isa<ConstantStruct>(Initializer)) {
        errs() << "Vtable initializer is not a ConstantStruct.\n";
        return;
      }

      // Extract the array field from the struct
      ConstantStruct *VTableStruct = cast<ConstantStruct>(Initializer);

      std::vector<Constant *> NewArrayRef;

      for(int i = 0; i < VTableStruct->getNumOperands(); i++) {
        Constant *ArrayField = VTableStruct->getOperand(i);
        if (!isa<ConstantArray>(ArrayField)) {
          errs() << "Vtable field " << i << " is not a ConstantArray.\n";
          continue;
        }

        ConstantArray *FunctionArray = cast<ConstantArray>(ArrayField);

        // Iterate over elements of the array and modify function pointers
        std::vector<Constant *> ModifiedElements;
        for (Value *Elem : FunctionArray->operands()) {
          if (isa<Function>(Elem)) {
            Function *Func = cast<Function>(Elem);
            // Replace with the _dup version of the function
            std::string DupName = Func->getName().str() + "_dup";
            Function *DupFunction = Md.getFunction(DupName);

            if (DupFunction) {
              // LLVM_DEBUG(dbgs() << "Getting _dup function: " << DupFunction->getName() << "\n");
              ModifiedElements.push_back(DupFunction);
            } else {
              errs() << "Missing _dup function for: " << Func->getName() << "\n";
              ModifiedElements.push_back(cast<Constant>(Elem)); // Keep the original
            }
          } else {
            // Retain non-function elements
            ModifiedElements.push_back(cast<Constant>(Elem));
          }
        }

        // Create a new ConstantArray with the modified elements
        ArrayType *ArrayType = FunctionArray->getType();
        Constant *NewArray = ConstantArray::get(ArrayType, ModifiedElements);
        NewArrayRef.push_back(NewArray);
      }

      // Create a new ConstantStruct for the vtable
      Constant *NewVTableStruct = ConstantStruct::get(VTableStruct->getType(), NewArrayRef);

      // Create a new global variable for the modified vtable
      NewVtable = new GlobalVariable(
        Md,
        NewVTableStruct->getType(),
        vtable->isConstant(),
        GlobalValue::ExternalLinkage,
        NewVTableStruct,
        vtable->getName() + "_dup"
      );
      NewVtable->setSection(vtable->getSection());
      LLVM_DEBUG(dbgs() << "[REDDI] Created new vtable: " << NewVtable->getName() << "\n");
    }

    // In the dup constructor, change the relative store
    if(NewVtable) {
      for(auto &BB : *FnDup) {
        for(auto &I : BB) {
          if(isa<StoreInst>(I)) {
            auto &SInst = cast<StoreInst>(I);
            if(isVTableStore(SInst)) {
              if(isa<GetElementPtrInst>(SInst.getValueOperand())) {
                // TODO: Should see the uses of the valueOperand to find this inst in case it happens
                errs() << "this is a GEP instruction\n";
              } else if(isa<ConstantExpr>(SInst.getValueOperand())) {
                auto *CE = cast<ConstantExpr>(SInst.getValueOperand());
                if (CE->getOpcode() == Instruction::GetElementPtr) {
                  // Extract the indices and base type
                  std::vector<Constant *> Indices = {
                                  ConstantInt::get(Type::getInt32Ty(Md.getContext()), 0),
                                  ConstantInt::get(Type::getInt32Ty(Md.getContext()), 0),
                                  ConstantInt::get(Type::getInt32Ty(Md.getContext()), 2)
                                };

                  // Create a new GEP ConstantExpr with the new vtable
                  auto *NewGEP = ConstantExpr::getGetElementPtr(
                      cast<GEPOperator>(CE)->getSourceElementType(), 
                      NewVtable,
                      Indices,
                      cast<GEPOperator>(CE)->isInBounds()
                  );

                  // Update the store instruction
                  SInst.setOperand(0, NewGEP);
                }
                LLVM_DEBUG(dbgs() << "[REDDI] Changed vtable_dup store with new vtable: " << NewVtable->getName() << "\n");
              }
            }
          }
        }
      }
    }
  }
}

/**
 * @brief Fill toHardenFunctions and toHardenVariables sets with all the functions and 
 * global variables that will need to be hardened.
 * 
 * The rules to enter in toHardenFunctions set are:
 * - Explicitely marked as `to_harden`
 * - Called by a `to_harden` function and not an `exclude` or `to_duplicate` function
 * - Used by a `to_harden` GlobalVariable
 * - Present in a vtable of a `to_harden` object
 * 
 * The rule to enter in toHardenVariables set is that it is a global variable explicitly
 * marked as `to_harden`
 * 
 * @param Md The module we are analyzing.
 */
void EDDI::preprocess(Module &Md) {
  // Replace all uses of alias to aliasee
  LLVM_DEBUG(dbgs() << "[REDDI] Replacing aliases\n");
  for (auto &alias : Md.aliases()) {
    auto aliasee = alias.getAliaseeObject();
    if(isa<Function>(aliasee)){
      alias.replaceAllUsesWith(aliasee);
    }
  }
  LLVM_DEBUG(dbgs() << "\n");

  LLVM_DEBUG(dbgs() << "Getting annotations... ");
  getFuncAnnotations(Md, FuncAnnotations);
  LLVM_DEBUG(dbgs() << "[done]\n\n");

  // Getting the explicit `to_harden` functions and Values
  LLVM_DEBUG(dbgs() << "[REDDI] Getting all the functions and Global variables to harden\n");

  // Choose between EDDI and REDDI
  if(duplicateAll) {
    outs() << "EDDI!\n";

    // All the functions are to be hardened except the ones explicitly marked as `exclude` or `to_duplicate`
    for(auto &F : Md) {
      if((!F.hasName() || !isToDuplicateName(F.getName())) && (FuncAnnotations.find(&F) == FuncAnnotations.end() || 
        (!FuncAnnotations.find(&F)->second.starts_with("exclude") && !FuncAnnotations.find(&F)->second.starts_with("to_duplicate")))) {
        toHardenFunctions.insert(&F);
      }
    }
    
    // All the Global Variables are to be hardened except the ones explicitly marked as `exclude` or `to_duplicate`
    for(auto &GV : Md.globals()) {
      if(GV.hasName() && !isToDuplicateName(GV.getName())) {
        if(FuncAnnotations.find(&GV) == FuncAnnotations.end() || 
          (!FuncAnnotations.find(&GV)->second.starts_with("exclude") && !FuncAnnotations.find(&GV)->second.starts_with("to_duplicate"))) {
          toHardenVariables.insert(&GV);
        }
      }
    }
  } else {
    outs() << "REDDI!\n";
    for(auto x : FuncAnnotations) {
      if(x.second.starts_with("to_harden")) {
        if(isa<Function>(x.first) && getFunctionDuplicate(cast<Function>(x.first)) == NULL) {
          // If is a function and it isn't/hasn't a duplicate version already
          toHardenFunctions.insert(cast<Function>(x.first));
        } else if(isa<Value>(x.first)) {
          toHardenVariables.insert(cast<Value>(x.first));
        }
      }
    }
    LLVM_DEBUG(dbgs() << "\n");
  }

  // Getting the explicit `to_harden` functions and Values
  LLVM_DEBUG(dbgs() << "[REDDI] Getting all the global variables to harden from explicitly to_harden functions\n");
  for(auto *Fn : toHardenFunctions) {
    for(auto &BB : *Fn) {
      for(auto &I : BB) {
        for(auto &V : I.operands()) {

          if(isa<GlobalVariable>(V) && cast<GlobalVariable>(V)->hasInitializer() && toHardenVariables.find(V) == toHardenVariables.end()) {
            toHardenVariables.insert(V);
            outs() << "Inserting GV from explicit toHarden: " << *V << "\n";
          } 
          else if(isa<GEPOperator>(V)) {
            for(auto &U : cast<GEPOperator>(V)->operands()) {
              // if(isa<GlobalVariable>(U) && U->hasName() && !isToDuplicateName(U->getName())) {
              if(isa<GlobalVariable>(U) && cast<GlobalVariable>(U)->hasInitializer() && toHardenVariables.find(U) == toHardenVariables.end()) {
                toHardenVariables.insert(U);
                outs() << "Inserting GV from explicit toHarden from GEPOp: " << *U << "\n";
              }
            }
          }

        }
      }
    }
  }
  LLVM_DEBUG(dbgs() << "\n");

  // Collecting all the functions called by a value to be hardened
  LLVM_DEBUG(dbgs() << "[REDDI] Getting all the functions to harden called by a Global Variable\n");
  std::set<Value *> toCheckVariables{toHardenVariables};
  while(!toCheckVariables.empty()){
    std::set<Value *> toAddVariables; // support set to contain new to-be-checked values
    for(Value *V : toCheckVariables) {
      // outs() << "Instruction to check: " << *V << "\n";
      // Just protect the return value of the call, not the operands
      if((isa<Instruction>(V) || isa<GEPOperator>(V)) && !isa<CallBase>(V)) {
        auto Instr = cast<User>(V);

        // Check parameters of function
        for(int i = 0; i < Instr->getNumOperands(); i++) {
          Value *operand = nullptr;

          // Get operand
          if(isa<PHINode>(Instr)) {
            auto PhiInst = cast<PHINode>(Instr);
            operand = PhiInst->getIncomingValue(i);
            // outs() << "phi operand: " << *operand << "\n";
          } else if(isa<Instruction>(Instr->getOperand(i)) || isa<GlobalVariable>(Instr->getOperand(i)) || isa<GEPOperator>(Instr->getOperand(i))) {
            operand = Instr->getOperand(i);
            // outs() << "operand: " << *operand << "\n";
          }
          
          // Check if to add operand to toAddVariables
          if(operand != NULL && operand != V && isa<Instruction>(operand) &&
                toHardenVariables.find(operand) == toHardenVariables.end() && 
                toCheckVariables.find(operand) == toCheckVariables.end() && 
                (FuncAnnotations.find(operand) == FuncAnnotations.end() || !FuncAnnotations.find(operand)->second.starts_with("exclude")) && 
                (!operand->hasName() || !isToDuplicateName(operand->getName())) && 
                (!isa<AllocaInst>(operand) || !isAllocaForExceptionHandling(*cast<AllocaInst>(operand)))) {
            toAddVariables.insert(operand);
            // outs() << "* To be hardened operand: " << *operand << "\n";
          }
        }
      }

      for(User *U : V->users()) {
        if(isa<Instruction>(U) || isa<GEPOperator>(U)) {
        // if(isa<StoreInst>(U) || isa<LoadInst>(U) || isa<GetElementPtrInst>(U) || isa<BinaryOperator>(U) || isa<PHINode>(U) || isa<SelectInst>(U) || isa<CastInst>(U)) {
          if(U != NULL && U != V && 
                toHardenVariables.find(U) == toHardenVariables.end() && 
                toCheckVariables.find(U) == toCheckVariables.end() && 
                (FuncAnnotations.find(U) == FuncAnnotations.end() || !FuncAnnotations.find(U)->second.starts_with("exclude")) && 
                (!U->hasName() || !isToDuplicateName(U->getName())) && 
                (!isa<AllocaInst>(U) || !isAllocaForExceptionHandling(*cast<AllocaInst>(U)))) {
            // outs() << "* To be hardened user: " << *U << "\n";
            // If it is a call, add also the called function in the toHardenFunction set
            if(isa<CallBase>(U)) {
              CallBase *CallI = cast<CallBase>(U);     
              Function *Fn = CallI->getCalledFunction();  
              if (Fn != NULL && getFunctionDuplicate(Fn) == NULL && 
                    (FuncAnnotations.find(Fn) == FuncAnnotations.end() || 
                      (!FuncAnnotations.find(Fn)->second.starts_with("exclude") && !FuncAnnotations.find(Fn)->second.starts_with("to_duplicate"))) && 
                    !isToDuplicateName(Fn->getName()) && !Fn->getName().starts_with("__clang_call_terminate")) {
                // If it isn't/hasn't a duplicate version already
                toHardenFunctions.insert(Fn);
                toAddVariables.insert(U);
              } else {
                errs() << "[REDDI] Indirect Function to harden (called by " << V->getName() << ")\n";
                // continue;
              }
            } else {
              toAddVariables.insert(U);
            }
          }
        }
      }
    }
    toHardenVariables.merge(toCheckVariables);
    toCheckVariables = toAddVariables;
  }
  LLVM_DEBUG(dbgs() << "\n");

  // Recursively retrieve functions to harden
  LLVM_DEBUG(dbgs() << "[REDDI] Getting all the functions to harden recursively\n");
  std::set<Function *> JustAddedFns{toHardenFunctions};
  while(!JustAddedFns.empty()) {
    // New discovered functions
    std::set<Function *> toAddFns;
    for(Function *Fn : JustAddedFns) {
      // Check if it is a constructor
      std::string DemangledName = demangle(Fn->getName().str());
      if(std::regex_match(DemangledName, ConstructorRegex)) {
        // Add it to the toHardenConstructors set and retrieve all its virtualMethods
        // if it isn't/hasn't a duplicate version already
        toHardenConstructors.insert(Fn);
        toAddFns.merge(getVirtualMethodsFromConstructor(Fn));
      }

      // Retrieve all the other called functions
      for(BasicBlock &BB : *Fn) {
        for(Instruction &I : BB) {
          if(isa<CallBase>(I)) {
            if(Function *CalledFn = cast<CallBase>(I).getCalledFunction()) {
              auto CalledFnEntry = FuncAnnotations.find(CalledFn);
              bool to_harden = (CalledFnEntry == FuncAnnotations.end()) || 
                !(CalledFnEntry->second.starts_with("exclude") || CalledFnEntry->second.starts_with("to_duplicate"));
              LLVM_DEBUG(dbgs() << "[REDDI] " << Fn->getName() << " called " << CalledFn->getName() << 
                ((CalledFnEntry == FuncAnnotations.end()) ? " (not annotated)" : "") <<
                ((CalledFnEntry != FuncAnnotations.end() && CalledFnEntry->second.starts_with("exclude")) ? " (exclude)" : "") <<
                (toHardenFunctions.find(CalledFn) != toHardenFunctions.end() ? " (already in toHardenFunctions)" : "") <<
                (JustAddedFns.find(CalledFn) != JustAddedFns.end() ? " (already in JustAddedFns)" : "") <<
                "\n");
              if(to_harden && toHardenFunctions.find(CalledFn) == toHardenFunctions.end() && 
                JustAddedFns.find(CalledFn) == JustAddedFns.end() && 
                getFunctionDuplicate(CalledFn) == NULL && 
                (FuncAnnotations.find(CalledFn) == FuncAnnotations.end() || !FuncAnnotations.find(CalledFn)->second.starts_with("exclude")) &&
                !CalledFn->getName().starts_with("__clang_call_terminate")) {
                // If is a new function to and it isn't/hasn't a duplicate version
                toAddFns.insert(CalledFn);
                // LLVM_DEBUG(dbgs() << "[REDDI] Added: " << CalledFn->getName() << "\n");
              }
            } else {
              // errs() << "[REDDI] Indirect Function to harden (called by " << Fn->getName() << ")\n";
              // I.print(errs());
              // errs() << "\n";
            }
          }
        }
      }
    }

    // Add the just analyzed functions to the `toHardenFunctions` set
    toHardenFunctions.merge(JustAddedFns);
    // Now analyze the just discovered functions
    JustAddedFns = toAddFns;
  }

  LLVM_DEBUG(dbgs() << "[REDDI] preprocess done\n\n");
}

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
      if (!isValueDuplicated(DuplicatedInstructionMap, *Operand)) {
        if(duplicateInstruction(*Operand, DuplicatedInstructionMap, ErrBB)) {
          if(InstructionsToRemove.find(Operand) == InstructionsToRemove.end()) {
            InstructionsToRemove.insert(Operand);
          }
        }
      }
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
    } else if (isa<Function>(V)) {
      // if the operand is a function we need to set the duplicate function as
      // operand of the clone instruction
      Function *FnOperand = cast<Function>(V);
      auto DuplicateFn = getFunctionDuplicate(FnOperand);
      if (DuplicateFn != NULL) {
        I.setOperand(J, DuplicateFn);
        if (IClone != NULL) {
          IClone->setOperand(J, DuplicateFn);
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

  if(InstructionsToRemove.find(&I) != InstructionsToRemove.end()) {
    return ;
  }

  std::vector<Value *> CmpInstructions;

  // split and add the verification BB
  auto BBpred = I.getParent()->splitBasicBlockBefore(&I);
  BasicBlock *VerificationBB =
      BasicBlock::Create(I.getContext(), "VerificationBB",
                         I.getParent()->getParent(), I.getParent());
  I.getParent()->replaceUsesWithIf(BBpred, IsNotAPHINode);
  auto BI = cast<BranchInst>(BBpred->getTerminator());
  BI->setSuccessor(0, VerificationBB);
  IRBuilder<> B(VerificationBB);

  // if the instruction is a call with indirect function, we try to get a compare
  if(isa<CallBase>(I) && cast<CallBase>(I).isIndirectCall()) {
    auto Duplicate = DuplicatedInstructionMap.find(cast<CallBase>(I).getCalledOperand());
    if (Duplicate != DuplicatedInstructionMap.end()) {
      Value *Original = Duplicate->first;
      Value *Copy = Duplicate->second;
      if (Original->getType()->isIntOrIntVectorTy() || Original->getType()->isPtrOrPtrVectorTy()) {
        // DuplicatedInstructionMap.insert(std::pair<Value *, Value *>(&I, &I));
        CmpInstructions.push_back(B.CreateCmp(CmpInst::ICMP_EQ, Original, Copy));
      }
    }
  }

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
                } else if (OriginalElem->getType()->isIntOrIntVectorTy() || OriginalElem->getType()->isPtrOrPtrVectorTy()) {
                  CmpInstructions.push_back(
                      B.CreateCmp(CmpInst::ICMP_EQ, OriginalElem, CopyElem));
                } else {
                  errs() << "Didn't create a comparison for ";
                  OriginalElem->getType()->print(errs());
                  errs() << " type\n";
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
          } else if (Original->getType()->isIntOrIntVectorTy() || Original->getType()->isPtrOrPtrVectorTy()) {
            CmpInstructions.push_back(
                B.CreateCmp(CmpInst::ICMP_EQ, Original, Copy));
          } else {
            errs() << "Didn't create a comparison for " << Original->getType() << " type\n";
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
    auto CondBrInst = B.CreateCondBr(AndInstr, I.getParent(), &ErrBB);
    if (DebugEnabled) {
      CondBrInst->setDebugLoc(I.getDebugLoc());
    }
  }

  if (VerificationBB->size() == 0) {
    auto BrInst = B.CreateBr(I.getParent());
    if (DebugEnabled) {
      BrInst->setDebugLoc(I.getDebugLoc());
    }
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
    if (isa<Instruction>(V)) {
      Instruction *Operand = cast<Instruction>(V);
      auto Duplicate = DuplicatedInstructionMap.find(Operand);
      if (Duplicate != DuplicatedInstructionMap.end()) {
        Value *Original = Duplicate->first;
        Value *Copy = Duplicate->second;
        if(Original->getType()->isPointerTy() && Copy->getType()->isPointerTy()) {
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
}

// Given Fn, it returns the version of the function with duplicated arguments,
// or the function Fn itself if it is already the version with duplicated
// arguments
Function *EDDI::getFunctionDuplicate(Function *Fn) {
  // If Fn ends with "_dup" we have already the duplicated function.
  // If Fn is NULL, it means that we don't have a duplicate
  if (Fn == NULL || Fn->getName().ends_with("_dup")) {
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
  if (Fn == NULL || !Fn->getName().ends_with("_dup")) {
    return Fn;
  }

  // Otherwise, we try to get the non-"_dup" version
  Function *FnDup = Fn->getParent()->getFunction(
      Fn->getName().str().substr(0, Fn->getName().str().length() - 8));
  if (FnDup == NULL || FnDup == Fn) {
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
  for (auto *V : toHardenVariables) {
    if(isa<GlobalVariable>(V)) {
      GVars.push_back(cast<GlobalVariable>(V));
    }
  }
  for (auto GV : GVars) {
    auto GVAnnotation = FuncAnnotations.find(GV);
    if (!isa<Function>(GV) &&
        GVAnnotation != FuncAnnotations.end()) {
      // What does these annotations do?
      if (GVAnnotation->second.starts_with("runtime_sig") ||
          GVAnnotation->second.starts_with("run_adj_sig")) {
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
    bool isPointer = GV->getValueType()->isPointerTy();
    bool ends_withDup = GV->getName().ends_with("_dup");
    bool hasInternalLinkage = GV->hasInternalLinkage();
    bool isMetadataInfo = GV->getSection() == "llvm.metadata";
    bool isReservedName = GV->getName().starts_with("llvm.");
    bool toExclude = !isa<Function>(GV) &&
                     GVAnnotation != FuncAnnotations.end() &&
                     GVAnnotation->second.starts_with("exclude");

    if (! (isFunction || isConstant || ends_withDup || isMetadataInfo || isReservedName || toExclude) // is not function, constant, struct and does not end with _dup
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
        if (callInst->getCalledFunction() != NULL && callInst->getCalledFunction()->getName() == "__cxa_begin_catch")
        {return true;}
      }
      
    }
  }
  return false;
}

int EDDI::transformCallBaseInst(CallBase *CInstr, std::map<Value *, Value *> &DuplicatedInstructionMap,
    IRBuilder<> &B, BasicBlock &ErrBB) {
  int res = 0;
  SmallVector<Value *, 6> args;
  SmallVector<Type *, 6> ParamTypes;
  
  Function *Callee = CInstr->getCalledFunction();
  Function *Fn = getFunctionDuplicate(Callee);

  if(Callee != NULL && (Fn == NULL || Fn == Callee)) {
    errs() << "Doesn't exist or already duplicated function: " << *CInstr << "\n";
    return 0;
  }

  for (unsigned i = 0; i < CInstr->arg_size(); i++) {
    // Populate args and ParamTypes from the original instruction
    Value *Arg = CInstr->getArgOperand(i);
    Value *Copy = Arg;

    // see if Original has a copy
    if(DuplicatedInstructionMap.find(Arg) != DuplicatedInstructionMap.end()) {
      Copy = DuplicatedInstructionMap.find(Arg)->second;
    }

    // Duplicating only fixed parameters, passing just one time the variadic arguments
    if(Callee != NULL && Callee->getFunctionType() != NULL && i >= Callee->getFunctionType()->getNumParams()) {
      args.push_back(Arg);
      if(Callee == NULL) {
        ParamTypes.push_back(Arg->getType());
      }
    } else {
      if (!AlternateMemMapEnabled) {
        args.insert(args.begin() + i, Copy);
        args.push_back(Arg);
        if(Callee == NULL) {
          ParamTypes.insert(ParamTypes.begin() + i, Arg->getType());
          ParamTypes.push_back(Arg->getType());
        }
      } else {
        args.push_back(Copy);
        args.push_back(Arg);
        if(Callee == NULL) {
          ParamTypes.push_back(Arg->getType());
          ParamTypes.push_back(Arg->getType());
        }
      }
    }
  }

  Instruction *NewCInstr = nullptr;
  IRBuilder<> CallBuilder(CInstr);

  // In case of duplication of an indirect call, call the function with doubled parameters
  if (Callee == NULL) {
    // Create the new function type
    Type *ReturnType = CInstr->getType();
    FunctionType *FuncType = FunctionType::get(ReturnType, ParamTypes, false);

    // Create a dummy function pointer (Fn) for the new call
    Value *Fn = CallBuilder.CreateBitCast(CInstr->getCalledOperand(), FuncType->getPointerTo());

    // Create the new call or invoke instruction
    if (isa<InvokeInst>(CInstr)) {
      InvokeInst *IInst=cast<InvokeInst>(CInstr);
      NewCInstr = CallBuilder.CreateInvoke(
          FuncType, Fn, IInst->getNormalDest(), IInst->getUnwindDest(), args);
    } else {
      NewCInstr = CallBuilder.CreateCall(FuncType, Fn, args);
    }

    // Transfer parameter attributes
    for (unsigned i = 0; i < CInstr->arg_size(); ++i) {
      AttributeSet ParamAttrs = CInstr->getAttributes().getParamAttrs(i);
      for(auto &attr : ParamAttrs) {
        if(attr.getKindAsEnum() != Attribute::AttrKind::StructRet){
          // Assuming that indirect function calls aren't variadic
          if (!AlternateMemMapEnabled) {
            cast<CallBase>(NewCInstr)->addParamAttr(i, attr);
            cast<CallBase>(NewCInstr)->addParamAttr(i + CInstr->arg_size(), attr);
          } else {
            cast<CallBase>(NewCInstr)->addParamAttr(i*2, attr);
            cast<CallBase>(NewCInstr)->addParamAttr(i*2 + 1 , attr);
          }
        }
      }
    }

    // Copy metadata and debug location
    if (DebugEnabled) {
      NewCInstr->setDebugLoc(CInstr->getDebugLoc());
    }

    // Replace the old instruction with the new one
    CInstr->replaceNonMetadataUsesWith(NewCInstr);

    // Remove original instruction since we created the duplicated version
    res = 1;
  } else {
    if (isa<InvokeInst>(CInstr)) {
      InvokeInst *IInst=cast<InvokeInst>(CInstr);
      NewCInstr = CallBuilder.CreateInvoke(Fn->getFunctionType(), Fn,IInst->getNormalDest(),IInst->getUnwindDest(), args);
    } else {
      NewCInstr =  CallBuilder.CreateCall(Fn->getFunctionType(), Fn, args);
    }

    if (DebugEnabled) {
      NewCInstr->setDebugLoc(CInstr->getDebugLoc());
    }
    res = 1;
    CInstr->replaceNonMetadataUsesWith(NewCInstr);
  }

  if(NewCInstr) {
    DuplicatedCalls.insert(NewCInstr);
  }

  return res;
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
    if(I.getParent()->getTerminator() == NULL) {
      errs() << "Malformed block!\n";
      I.getParent()->print(errs());
      errs() << "\n";
    } else if (I.getParent()->getTerminator()->getNumSuccessors() > 1)
#endif
      addConsistencyChecks(I, DuplicatedInstructionMap, ErrBB);
#endif
    // it may happen that I duplicate a store but don't change its operands, if
    // that happens I just remove the duplicate
    if (IClone->isIdenticalTo(&I)) {
      IClone->eraseFromParent();
      if(DuplicatedInstructionMap.find(&I) != DuplicatedInstructionMap.end()) {
        DuplicatedInstructionMap.erase(DuplicatedInstructionMap.find(&I));
      }
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
    if(I.getParent()->getTerminator() == NULL) {
      errs() << "Malformed block!\n";
      I.getParent()->print(errs());
      errs() << "\n";
    } else if (I.getParent()->getTerminator()->getNumSuccessors() > 1)
      addConsistencyChecks(I, DuplicatedInstructionMap, ErrBB);
#endif
  }

  // if the istruction is a non-already-duplicated call, we duplicate the operands and add consistency
  // checks
  else if (isa<CallBase>(I) && DuplicatedCalls.find(&I) == DuplicatedCalls.end()) {
    DuplicatedCalls.insert(&I);
    CallBase *CInstr = cast<CallBase>(&I);
    // there are some instructions that can be annotated with "to_duplicate" in
    // order to tell the pass to duplicate the function call.
    Function *Callee = CInstr->getCalledFunction();
    Callee = getFunctionFromDuplicate(Callee);

    if(CInstr->getCalledFunction() != NULL && isToExcludeName(CInstr->getCalledFunction()->getName())) {
      return 0;
    }

    // check if the function call has to be duplicated
    if ((FuncAnnotations.find(Callee) != FuncAnnotations.end() && FuncAnnotations.find(Callee)->second.starts_with("to_duplicate")) ||
        isToDuplicate(CInstr)) {
      // duplicate the instruction
      cloneInstr(*CInstr, DuplicatedInstructionMap);

      // duplicate the operands
      duplicateOperands(I, DuplicatedInstructionMap, ErrBB);

      if(isa<InvokeInst>(I)) {
        // In case of an invoke instruction, we have to fix the first invoke since 
        // it would jump to the next BB and not to the duplicated invoke instruction
        auto *IInstr = &cast<InvokeInst>(I);
        toFixInvokes.insert(IInstr);
      }

// add consistency checks on I
#ifdef CHECK_AT_CALLS
#if (SELECTIVE_CHECKING == 1)
    if(I.getParent()->getTerminator() == NULL) {
      errs() << "Malformed block!\n";
      I.getParent()->print(errs());
      errs() << "\n";
    } else if (I.getParent()->getTerminator()->getNumSuccessors() > 1)
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
    if(I.getParent()->getTerminator() == NULL) {
      errs() << "Malformed block!\n";
      I.getParent()->print(errs());
      errs() << "\n";
    } else if (I.getParent()->getTerminator()->getNumSuccessors() > 1)
#endif
        addConsistencyChecks(I, DuplicatedInstructionMap, ErrBB);
#endif

      IRBuilder<> B(CInstr);
      if (!isa<InvokeInst>(CInstr) && I.getNextNonDebugInstruction()) {
        B.SetInsertPoint(I.getNextNonDebugInstruction());
      } else if(isa<InvokeInst>(CInstr) && cast<InvokeInst>(CInstr)->getNormalDest()) {
        B.SetInsertPoint(
            &*cast<InvokeInst>(CInstr)->getNormalDest()->getFirstInsertionPt());
      } else {
        errs() << "Can't set insert point! " << I << "\n";
        abort();
      }
      // get the function with the duplicated signature, if it exists
      Function *Fn = getFunctionDuplicate(CInstr->getCalledFunction());
      // if the _dup function exists (and it is not itself the dup version) or is an indirect call, 
      // we substitute the call instruction with a call to the function with duplicated arguments
      // Inline assembly calls are excluded: their constraint strings are fixed and cannot accept
      // the doubled argument list that transformCallBaseInst would produce.
      if ((CInstr->getCalledFunction() == NULL && !CInstr->isInlineAsm()) || (Fn != NULL && Fn != CInstr->getCalledFunction())) {
        res = transformCallBaseInst(CInstr, DuplicatedInstructionMap, B, ErrBB);
      } else {
        fixFuncValsPassedByReference(*CInstr, DuplicatedInstructionMap, B);
      }
    }
  }

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

    // Passing just one time the variadic arguments while passing two times the fixed ones
    if(i >= FnType->getNumParams()) {
      paramTypeVec.push_back(ParamType);
    } else if (!AlternateMemMapEnabled) { // sequential
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
    if (Fn.getArg(i)->hasStructRetAttr()) {
      Fn.getArg(i)->removeAttr(Attribute::AttrKind::StructRet);
    }

    if (!AlternateMemMapEnabled) {
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
 * @brief Recursively searches for the value type, returning its type and alignment
 * @param Arg [In] Pointer to the value we want to analyze
 * @param ArgAlign [Out] The found alignment 
 * @return The Type of Arg, if found. VoidTy otherwise
 */
Type *getValueType(Value *Arg, Align *ArgAlign) {
  // https://llvm.org/docs/OpaquePointers.html
  while(true) {
    if(isa<CallInst>(Arg) && !cast<CallInst>(Arg)->isIndirectCall() && demangle(cast<CallInst>(Arg)->getCalledFunction()->getName().str()).find("operator new") == 0) {
      Value *Size = cast<CallInst>(Arg)->getArgOperand(0);
      if(isa<ConstantInt>(Size)) {
        // Use the size to create a type
        LLVMContext &Ctx = Arg->getContext();

        // Assume the allocated memory is for an array of bytes
        Type *ElementType = Type::getInt8Ty(Ctx); // Byte type
        return ArrayType::get(ElementType, cast<ConstantInt>(Size)->getZExtValue());
      }
      errs() << "Call not supported" << *Arg << "\n";
      return Type::getVoidTy(Arg->getContext());
    } else if(isa<GlobalValue>(Arg)) {
      Type *ArgType = cast<GlobalValue>(Arg)->getValueType();
      if(ArgType->isPointerTy()) {
        bool foundNewValue = false;
        for(Value *ArgUsers : cast<GlobalValue>(Arg)->users()) {
          if (isa<StoreInst>(ArgUsers) && cast<StoreInst>(ArgUsers)->getPointerOperand() == Arg) {
            Arg = cast<StoreInst>(ArgUsers)->getValueOperand();
            *ArgAlign = cast<StoreInst>(ArgUsers)->getAlign();
            errs() << "Store found: " << *ArgUsers << " with align " << ArgAlign->value() << "\n";
            foundNewValue = true;
            break;
          }
        }

        if(!foundNewValue) {
          errs() << "Global Type not supported" << *Arg << "\n";
          return Type::getVoidTy(Arg->getContext());
        }
      } else {
        return ArgType;
      }
    } else if(isa<PHINode>(Arg)) {
      Arg = cast<PHINode>(Arg)->getIncomingValue(0);
    } else if(isa<AllocaInst>(Arg)) {
      *ArgAlign = cast<AllocaInst>(Arg)->getAlign();
      return cast<AllocaInst>(Arg)->getAllocatedType();
    } else if(isa<GetElementPtrInst>(Arg)) {
      *ArgAlign = cast<GetElementPtrInst>(Arg)->getPointerAlignment(cast<GetElementPtrInst>(Arg)->getModule()->getDataLayout());
      return cast<GetElementPtrInst>(Arg)->getSourceElementType();
    } else if(isa<Function>(Arg)) {
      return cast<Function>(Arg)->getFunctionType();
    }  else if(isa<LoadInst>(Arg)) {
      *ArgAlign = cast<LoadInst>(Arg)->getAlign();
      Arg = cast<LoadInst>(Arg)->getPointerOperand();
    } else if(isa<StoreInst>(Arg)) {
      *ArgAlign = cast<StoreInst>(Arg)->getAlign();
      Arg = cast<StoreInst>(Arg)->getValueOperand();
    } else  {
      errs() << "Type not supported" << *Arg << "\n";
      return Type::getVoidTy(Arg->getContext());
    }
  }
}

/**
 * @brief I have to duplicate all instructions except function calls and branches
 * 
 * 0. Replacing aliases to aliasees
 * 1. getting function annotations
 * 2. Creating fault tolerance functions
 * 3. Create map of subprogram and linkage names
 * 4. Duplicate globals
 *    4.1. 
 * 5. For each function in module, if it should NOT compile (the function is neither null nor empty, 
 *    it does not have to be marked as excluded or to_duplicate nor it is one of the original functions) skip
 * 6. If the function is a duplicated one, we need to iterate over the function arguments and duplicate them in order to access them during the instruction duplication phase 
 *    6.1. Call duplicateInstruction on all uses of each argument
 * 7. For each Instruction, duplicate the instruction and then save for delete after if the duplicated instruction is the same as the original
 * 8. Generate error branches
 * 9. Delete the marked duplicated instructions
 * 
 * 
 *
 * 1. Duplicate Globals
 * 2. Duplicate functions
 * 3. Duplicate Constructors
 *
 * 
 * @param Md
 * @return
 */
PreservedAnalyses EDDI::run(Module &Md, ModuleAnalysisManager &AM) {
  LLVM_DEBUG(dbgs() << "Initializing EDDI...\n");

  preprocess(Md);
  LLVM_DEBUG(dbgs() << "[REDDI] Preprocess finished\n");

  createFtFuncs(Md);
  linkageMap = mapFunctionLinkageNames(Md);

  // fix debug information in the first BB of each function
  if(DebugEnabled) {
    for (auto &Fn : Md) {
      // if the first instruction after the allocas does not have a debug location
      if (shouldCompile(Fn, FuncAnnotations, OriginalFunctions) && !(*Fn.begin()).getFirstNonPHIOrDbgOrAlloca()->getDebugLoc()) {
        auto I = &*(*Fn.begin()).getFirstNonPHIOrDbgOrAlloca();
        auto NextI = I;
        
        // iterate over the next instructions finding the first debug loc
        while (NextI = NextI->getNextNode()) {
          if (NextI->getDebugLoc()) {
            I->setDebugLoc(NextI->getDebugLoc());
            break;
          }
        }
      }
    }
  }

  std::map<Value *, Value *>
      DuplicatedInstructionMap; // is a map containing the instructions
                                // and their duplicates

  LLVM_DEBUG(dbgs() << "Duplicating globals... ");
  duplicateGlobals(Md, DuplicatedInstructionMap);
  LLVM_DEBUG(dbgs() << "[done]\n");

  // store the duplicated functions that are currently in the module
  std::set<Function *> DuplicatedFns;

#ifdef DUPLICATE_ALL
  // Insert in the set of "duplicated functions" the original "entrypoint" 
  // function, to protect it "in place" and staring all the execution from 
  // the Sphere of Replication.
  if(Function *entryPointFn = Md.getFunction(entryPoint)) {
    DuplicatedFns.insert(entryPointFn);
  } else {
    errs() << "[EDDI] Entry point function not found: " << entryPoint << "\n";
  }
#endif

  // then duplicate the function arguments using toHardenFunctions
  LLVM_DEBUG(dbgs() << "Creating _dup functions\n");
  for (Function *Fn : toHardenFunctions) {
    // Create dup functions only if the function is declared in this module
    // and isn't just to be duplicated
    if(!Fn->isDeclaration() && !isToDuplicateName(Fn->getName())) {
      Function *newFn = duplicateFnArgs(*Fn, Md, DuplicatedInstructionMap);
      DuplicatedFns.insert(newFn);
    }
  }
  LLVM_DEBUG(dbgs() << "Creating _dup functions [done]\n");

  // Fixing the duplicated constructors
  fixDuplicatedConstructors(Md);

  // list of duplicated instructions to remove since they are equal to the original
  std::set<CallBase *> GrayAreaCallsToFix;
  int iFn = 1;
  LLVM_DEBUG(dbgs() << "Iterating over the functions...\n");

  for (Function *Fn : DuplicatedFns) {
    LLVM_DEBUG(dbgs() << "Compiling " << iFn++ << "/" << DuplicatedFns.size() << ": "
                      << Fn->getName() << "\n");
    CompiledFuncs.insert(Fn);

    BasicBlock *ErrBB = BasicBlock::Create(Fn->getContext(), "ErrBB", Fn);

    LLVM_DEBUG(dbgs() << "function arguments");
    // save the function arguments and their duplicates
    for (int i = 0; i < Fn->arg_size(); i++) {
      Value *Arg, *ArgClone;
      if (!AlternateMemMapEnabled) {
        if (i >= Fn->arg_size() / 2) {
          break;
        }
        Arg = Fn->getArg(i);
        ArgClone = Fn->getArg(i + Fn->arg_size() / 2);
      } else {
        if (i % 2 == 1)
          continue;
        Arg = Fn->getArg(i);
        ArgClone = Fn->getArg(i + 1);
      }
      DuplicatedInstructionMap.insert(
          std::pair<Value *, Value *>(Arg, ArgClone));
      DuplicatedInstructionMap.insert(
          std::pair<Value *, Value *>(ArgClone, Arg));
      for (User *U : Arg->users()) {
        if (isa<Instruction>(U)) {
          Instruction *I = cast<Instruction>(U);
          // duplicate the uses of each argument
          if (duplicateInstruction(*I, DuplicatedInstructionMap, *ErrBB)) {
            if(InstructionsToRemove.find(I) == InstructionsToRemove.end()) {
              InstructionsToRemove.insert(I);
              errs() << "Remove instr ( " << *I << " ) from " << *I->getParent()->getParent() << " while duplicating fn args\n";
            } else {
              errs() << "Duplicated to remove instr ( " << *I << " ) from " << *I->getParent()->getParent() << " while duplicating fn args\n";
            }
          }
        }
      }
    }
    LLVM_DEBUG(dbgs() << " [done]\n");

    LLVM_DEBUG(dbgs() << "Duplicate instructions");

    std::set<Instruction *> InstToDuplicate;
    for (BasicBlock &BB : *Fn) {
      for (Instruction &I : BB) {
        InstToDuplicate.insert(&I);
      }
    }

    for (Instruction *I : InstToDuplicate) {
      if (!isValueDuplicated(DuplicatedInstructionMap, *I)) {
        // perform the duplication
        int shouldDelete = 
              duplicateInstruction(*I, DuplicatedInstructionMap, *ErrBB);

        // the instruction duplicated may be equal to the original, so we
        // return shouldDelete in order to drop the duplicates

        // TODO: Why to be done in another phase and not in duplciateInstruction? 
        if (shouldDelete) {
          if(InstructionsToRemove.find(I) == InstructionsToRemove.end()) {
            InstructionsToRemove.insert(I);
          }
        }
      }
    }
    LLVM_DEBUG(dbgs() << " [done]\n");

    // insert the code for calling the error basic block in case of a mismatch
    CreateErrBB(Md, *Fn, ErrBB);
  }
  
  LLVM_DEBUG(dbgs() << "Iterating over variables...\n");
  // Duplicate usages of global variables to harden only if not in a _dup function 
  // (already handled in a duplicated function)
  for (Value *V : toHardenVariables) {
    if(V == NULL) {
      errs() << "To harden a null var\n";
      continue;
    }

    for(User *U : V->users()) {
      if(!isa<Instruction>(U)) {
        // If User is not an instruction continue to next user
        continue;
      }

      Instruction *I = cast<Instruction>(U);        
      Function *Fn = I->getFunction();
      
      // Duplicate instruction only if this isn't an already duplicated function
      if(!Fn->getName().ends_with("_dup")) {
        BasicBlock *ErrBB = nullptr;
        bool newErrBB = true;

        // Search pre-existant ErrBB if single basic block error handling is enabled
        if(!MultipleErrBBEnabled) {
          for(BasicBlock &BB : *Fn) {
            if(BB.getName().starts_with("ErrBB")) {
              ErrBB = &BB;
              newErrBB = false; // ErrBB already present
            }
          }
        }

        if(newErrBB) {
          ErrBB = BasicBlock::Create(Fn->getContext(), "ErrBB", Fn);
        }

        if(!isa<CallBase>(I)) {
          if(duplicateInstruction(*I, DuplicatedInstructionMap, *ErrBB)) {
            if(InstructionsToRemove.find(I) == InstructionsToRemove.end()) {
              InstructionsToRemove.insert(I);
            }
          }
        } else {
          GrayAreaCallsToFix.insert(cast<CallBase>(I));
        }

        // insert the code for calling the error basic block in case of a mismatch
        CreateErrBB(Md, *Fn, ErrBB);
      }
    }
  }
  
  // Protect only the explicitly marked `to_harden` functions
  LLVM_DEBUG(dbgs() << "Getting all GrayAreaCallsToFix...\n");
  for(auto annot : FuncAnnotations) {
    if(annot.second.starts_with("to_harden")) {
      if(isa<Function>(annot.first)) {
        auto Fn = cast<Function>(annot.first);
        outs() << "Adding to GrayAreaCallsToFix all calls of " << Fn->getName() << "\n";
        // Get function calls in gray area
        for(auto U : getFunctionFromDuplicate(Fn)->users()) {
          if(isa<CallBase>(U)) {
            auto caller = cast<CallBase>(U)->getFunction();
            // Protect this call if it's not in toHardenFunction and is not marked as `exclude`
            if(toHardenFunctions.find(caller) == toHardenFunctions.end() && 
                  (FuncAnnotations.find(caller) == FuncAnnotations.end() || !FuncAnnotations.find(caller)->second.starts_with("exclude"))) {
              outs() << "GrayAreaCallsToFix added: " << *U << "\n";
              GrayAreaCallsToFix.insert(cast<CallBase>(U));
            }
          }
        }
      }
    }
  }
  
  LLVM_DEBUG(dbgs() << "Fixing gray area calls\n");
  // Add alloca and memcpy of non duplicated instructions and use that as duplciated instr
  for(CallBase *CInstr : GrayAreaCallsToFix) {
    if(FuncAnnotations.find(CInstr->getCalledFunction()) != FuncAnnotations.end() && 
        FuncAnnotations.find(CInstr->getCalledFunction())->second.starts_with("exclude")) {
      // Maybe check if have to fix operands and return after the call
      errs() << "About to duplicate a call not to duplciate: " << *CInstr << "\n";
      continue;
    }


    // Map with the duplicated instructions, including the temporary load ones
    std::map<Value *, Value *> TmpDuplicatedInstructionMap{DuplicatedInstructionMap};
    Function *Fn = CInstr->getFunction();
    BasicBlock *ErrBB = nullptr;
    bool newErrBB = true;        

    // Search pre-existant ErrBB if single basic block error handling is enabled
    if(!MultipleErrBBEnabled) {
      for(BasicBlock &BB : *Fn) {
        if(BB.getName().starts_with("ErrBB")) {
          ErrBB = &BB;
          newErrBB = false; // ErrBB already present
        }
      }
    }

    if(newErrBB) {
      ErrBB = BasicBlock::Create(Fn->getContext(), "ErrBB", Fn);
    }

    // Set insertion point for the load instructions
    IRBuilder<> B(CInstr);
    B.SetInsertPoint(CInstr);

    for (unsigned i = 0; i < CInstr->arg_size(); i++) {
      // Populate args and ParamTypes from the original instruction
      Value *Arg = CInstr->getArgOperand(i);

      // If argument has already a duplicate, nothing to do
      if(TmpDuplicatedInstructionMap.find(Arg) != TmpDuplicatedInstructionMap.end() || !isa<Instruction>(Arg)) {
        // If Argument already duplicated continue to next argument
        continue;
      }
      
      // Create alloca and memcpy only if ptr since if it is a value, we can just pass two times the same value
      if(Arg->getType()->isPointerTy() && !CInstr->isByValArgument(i) && isa<Instruction>(Arg) && !isa<CallInst>(Arg))
      {
        const llvm::DataLayout &DL = Md.getDataLayout();
        Type *ArgType;
        
        Align ArgAlign;
        ArgType = getValueType(Arg, &ArgAlign);

        // If can't find type, do not duplicate argument
        if(ArgType->isVoidTy()) {
          continue;
        }

        uint64_t SizeInBytes = DL.getTypeAllocSize(ArgType);
        Value *Size = llvm::ConstantInt::get(B.getInt64Ty(), SizeInBytes);
        
        // Alignment (assuming alignment of 1 here; adjust as necessary)
        llvm::ConstantInt *Align = B.getInt32(ArgAlign.value());

        // Volatility (non-volatile in this example)
        llvm::ConstantInt *IsVolatile = B.getInt1(false);

        // Create the memcpy call
        auto CopyArg = B.CreateAlloca(ArgType);

        llvm::CallInst *memcpy_call = B.CreateMemCpy(CopyArg, Arg->getPointerAlignment(DL), Arg, Arg->getPointerAlignment(DL), Size);

        TmpDuplicatedInstructionMap.insert(std::pair<Value *, Value *>(CopyArg, Arg));
        TmpDuplicatedInstructionMap.insert(std::pair<Value *, Value *>(Arg, CopyArg));
      } else {
        // Otherwise pass two times the same arg
        TmpDuplicatedInstructionMap.insert(std::pair<Value *, Value *>(Arg, Arg));
      }
    }

    // Finally, duplicate the call with the temporary DuplicatedInstructionMap
    if(duplicateInstruction(*CInstr, TmpDuplicatedInstructionMap, *ErrBB)) {
      if(InstructionsToRemove.find(CInstr) == InstructionsToRemove.end()) {
        InstructionsToRemove.insert(CInstr);
      }
    }

    // insert the code for calling the error basic block in case of a mismatch
    CreateErrBB(Md, *Fn, ErrBB);
  }

  LLVM_DEBUG(dbgs() << "Fixing invokes\n");
  for(InvokeInst *IInstr : toFixInvokes) {
    if(IInstr == NULL) {
      errs() << "To fix a null invoke\n";
      continue;
    }

    // Split every toFixInvoke in two different BBs, with the first having the normal continuation 
    // to the next invoke and both having the same landingpad
    auto *NewBB = IInstr->getParent()->splitBasicBlockBefore(IInstr->getNextNonDebugInstruction());
    auto *BrI = NewBB->getTerminator();
    BrI->removeFromParent();
    BrI->deleteValue();

    // Update the first invoke's normal destination
    IInstr->setNormalDest(NewBB->getNextNode());
  }
  
  LLVM_DEBUG(dbgs() << "Remove instructions\n");
  // Drop the instructions that have been marked for removal earlier
  for (Instruction *I2rm : InstructionsToRemove) {
    if(I2rm == NULL) {
      errs() << "To remove a null instruction\n";
      continue;
    }

    I2rm->eraseFromParent();
  }

  LLVM_DEBUG(dbgs() << "Fixing global ctors\n");
  fixGlobalCtors(Md);

  // Fixing calls to default handlers
  if(DebugEnabled){
    LLVM_DEBUG(dbgs() << "Fixing DataCorruptionHandlers\n");
    auto *DataCorruptionH = Md.getFunction(getLinkageName(linkageMap, "DataCorruption_Handler"));
    for(User *U : DataCorruptionH->users()) {
      if(isa<CallBase>(U)) {
        if(auto *CallI = cast<CallBase>(U)) {
          if(auto dbgLoc = findNearestDebugLoc(*CallI)) {
            CallI->setDebugLoc(dbgLoc);
          }
        }
      }
    }
  }

  LLVM_DEBUG(dbgs() << "Persisting Compiled Functions...\n");
  persistCompiledFunctions(CompiledFuncs, "compiled_eddi_functions.csv");

/*   if (Function *mainFunc = Md.getFunction("main")) {
    errs() << *mainFunc;
  } else {
    errs() << "Function 'main' not found!\n";
  }
 */
  return PreservedAnalyses::none();
}

Instruction *getSingleReturnInst(Function &F) {
  for (BasicBlock &BB : F) {
    if (auto *retInst = llvm::dyn_cast<llvm::ReturnInst>(BB.getTerminator())) {
      return retInst;
    }
  }
  return nullptr;
}

void EDDI::CreateErrBB(Module &Md, Function &Fn, BasicBlock *ErrBB){
  if(ErrBB->getNumUses() == 0) {
    ErrBB->eraseFromParent();
    return;
  }

  IRBuilder<> ErrB(ErrBB);

  assert(!getLinkageName(linkageMap, "DataCorruption_Handler").empty() &&
          "Function DataCorruption_Handler is missing!");
  auto CalleeF = ErrBB->getModule()->getOrInsertFunction(
      getLinkageName(linkageMap, "DataCorruption_Handler"),
      FunctionType::getVoidTy(Md.getContext()));

  auto *CallI = ErrB.CreateCall(CalleeF);

  if(MultipleErrBBEnabled) {
    // Insert one error block for each consistency check so that a specific 
    // recovery and continuation is possible

    std::list<Instruction *> errBranches;
    for (User *U : ErrBB->users()) {
      Instruction *I = cast<Instruction>(U);
      errBranches.push_back(I);
    }

    // For each consistency check branch to the ErrBB, we create a new 
    // error block which jumps, at the end, to the normal continuation
    for (Instruction *I : errBranches) {
      ValueToValueMapTy VMap;
      BasicBlock *ErrBBCopy = CloneBasicBlock(ErrBB, VMap);
      ErrBBCopy->insertInto(ErrBB->getParent(), I->getParent());

      BasicBlock *NormalContinuation = nullptr;
      if (isa<BranchInst>(I)) {
        BranchInst *BI = cast<BranchInst>(I);
        if (BI->isConditional()) {
          NormalContinuation = BI->getSuccessor(0) == ErrBB ?
              BI->getSuccessor(1) : BI->getSuccessor(0);
        }
      } else if (isa<InvokeInst>(I)) {
        InvokeInst *II = cast<InvokeInst>(I);
        NormalContinuation = II->getNormalDest();
      }

      if (NormalContinuation) {
        IRBuilder<> ErrBCopy(ErrBBCopy);
        auto *BrInst = ErrBCopy.CreateBr(NormalContinuation);
        if (DebugEnabled) {
          BrInst->setDebugLoc(I->getDebugLoc());
        }
      } else {
        errs() << "Error: consistency check without a normal continuation! " << *I << "\n";
        IRBuilder<> ErrBCopy(ErrBBCopy);
        ErrBCopy.CreateUnreachable();
      }

      I->replaceSuccessorWith(ErrBB, ErrBBCopy);
    }
    ErrBB->eraseFromParent();

    if (DebugEnabled) {
      for (Instruction *I : errBranches) {
        auto *ErrBB = I->getSuccessor(1);
        // set the debug location to the instruction the ErrBB is related to
        for (Instruction &ErrI : *ErrBB) {
          if (!I->getDebugLoc()) {
            if(Fn.back().getTerminator()) {
              if(auto DL = findNearestDebugLoc(*Fn.back().getTerminator())) {
                ErrI.setDebugLoc(DL);
              }
            } else if(Fn.back().getPrevNode()->getTerminator()) {
              // In some cases, the last block of the function may not have a terminator (e.g., an incomplete ErrBB),
              // so we check the previous block's terminator as well
              if(auto DL = findNearestDebugLoc(*Fn.back().getPrevNode()->getTerminator())) {
                ErrI.setDebugLoc(DL);
              }
            }
          } else {
            ErrI.setDebugLoc(I->getDebugLoc());
          }
        }
      }
    }
  } else {
    // Leave just one error block for all consistency checks to minimize code size
    ErrB.CreateUnreachable();
    
    if (DebugEnabled) {
      for (Instruction &ErrI : *ErrBB) {
        if(auto retInst = getSingleReturnInst(Fn)) {
          auto DL = findNearestDebugLoc(*retInst);
          if (!DL && Fn.back().getTerminator()) {
            DL = findNearestDebugLoc(*Fn.back().getTerminator());
          }

          if(DL) {
            ErrI.setDebugLoc(DL);
          } else {
            errs() << "Warning: no debug location found for error block in function " << Fn.getName() << "\n";
          }
        }
      }
    }
  }
}

void EDDI::fixGlobalCtors(Module &M) {
  LLVMContext &Context = M.getContext();

  // Retrieve the existing @llvm.global_ctors.
  GlobalVariable *GlobalCtors = M.getGlobalVariable("llvm.global_ctors");
  if (!GlobalCtors) {
    errs() << "Error: @llvm.global_ctors not found in the module.\n";
    return;
  }

  // Get the constantness and the section name of the existing global variable.
  bool isConstant = GlobalCtors->isConstant();
  StringRef Section = GlobalCtors->getSection();

  // Get the type of the annotations array and struct.
  ArrayType *CtorsArrayType = cast<ArrayType>(GlobalCtors->getValueType());
  StructType *CtorStructType = cast<StructType>(CtorsArrayType->getElementType());

  // Create the new Ctor struct fields.
  PointerType *Int8PtrType = Type::getInt8Ty(Context)->getPointerTo();
  Constant *IntegerConstant = ConstantInt::get(Type::getInt32Ty(Context), 65535);
  Constant *NullPtr = ConstantPointerNull::get(Int8PtrType); // Null pointer for other fields.

  // Retrieve existing annotations and append the new one.
  std::vector<Constant *> Ctors;
  if (ConstantArray *ExistingArray = dyn_cast<ConstantArray>(GlobalCtors->getInitializer())) {
    for (unsigned i = 0; i < ExistingArray->getNumOperands(); ++i) {
      auto *ctorStr = ExistingArray->getOperand(i);

      auto *ctor = ctorStr->getOperand(1);
      if(isa<Function>(ctor)){
        Function *dupCtor = getFunctionDuplicate(cast<Function>(ctor));
        // If there isn't the duplicated constructor, use the original one
        if(dupCtor == NULL) {
          dupCtor = cast<Function>(ctor);
        }

        Constant *CtorAsConstant = ConstantExpr::getBitCast(dupCtor, Int8PtrType);;
        // Create the new Ctor struct.
        Constant *NewCtor = ConstantStruct::get(
            CtorStructType,
            {IntegerConstant, CtorAsConstant, NullPtr});
        Ctors.push_back(NewCtor);
      }
    }
  }

  // Create a new array with the correct type and size.
  ArrayType *NewCtorArrayType = ArrayType::get(CtorStructType, Ctors.size());
  Constant *NewCtorArray = ConstantArray::get(NewCtorArrayType, Ctors);

  // Remove the old global variable from the module's symbol table.
  GlobalCtors->removeFromParent();
  delete GlobalCtors;

  // Create a new global variable with the exact name "llvm.global_ctors".
  GlobalVariable *NewGlobalCtors = new GlobalVariable(
      M,
      NewCtorArray->getType(),
      isConstant,
      GlobalValue::AppendingLinkage, // Must use appending linkage for @llvm.global_ctors.
      NewCtorArray,
      "llvm.global_ctors");

  // Set the section to match the original.
  NewGlobalCtors->setSection(Section);
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
#ifdef DUPLICATE_ALL
                    FPM.addPass(EDDI(true, true));
#else
                    FPM.addPass(EDDI(false, true));
#endif
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
