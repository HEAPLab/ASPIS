/**
 * ************************************************************************************************
 * @brief  LLVM pass implementing globals duplication for EDDI (see EDDI.cpp).
 * 
 * @author Davide Baroffio, Politecnico di Milano, Italy (dav.baroffio@mail.polimi.it)
 * ************************************************************************************************
*/

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "Utils/Utils.h"
#include <map>
#include <list>
#include <unordered_set>
#include <queue>
#include <iostream>
#include <fstream>
using namespace llvm;

#define DEBUG_TYPE "eddi_verification"

namespace {
struct DuplicateGlobals : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  DuplicateGlobals() : ModulePass(ID) { }

  private:

    std::map<GlobalVariable*, GlobalVariable*> DuplicatedGlobals;

  /**
   * Reads the file `Filename` and populates the set with the entries of the file.
   * They represent the names of the functions for which we don't want to perform the duplication
  */
  void getFunctionsToNotModify(std::string Filename, std::set<std::string> &FunctionsToNotModify) {
    std::string line;
    std::ifstream file;
    file.open(Filename);
    if (file.is_open()) {
      while ( getline (file,line) ) {
        FunctionsToNotModify.insert(line);
      }
      file.close();
    }
  }

  /**
   * Returns the duplicate of GV in Md if it exists, NULL otherwise.
  */
  GlobalVariable* getDuplicatedGlobal(Module &Md, GlobalVariable &GV) {
    if (DuplicatedGlobals.find(&GV) != DuplicatedGlobals.end()) {
      return DuplicatedGlobals.find(&GV)->second;
    }
    else if (GV.getName().endswith("_dup")) {
      return NULL;
    }
    else {
      return Md.getGlobalVariable(GV.getName().str() + "_dup");
    }
  }

  /**
   * Modifies the call instruction `UCall` making it call the _dup version of the called function.
   * Moreover, it substitutes Original with Copy in the copied argument of the function called.
  */
  void duplicateCall(Module &Md, CallBase* UCall, Value* Original, Value* Copy) {
    if (UCall->getCalledFunction() == NULL || !UCall->getCalledFunction()->hasName()) {
      return;
    }
    // if the function is already a _dup function, we just duplicate the operand corresponding to our global
    if (UCall->getCalledFunction()->getName().endswith("_dup")) {
      int i = 0;
      for (auto &Op : UCall->args()) {
        if (Op == Original) {
          UCall->setOperand(i+1, Copy);
        }
        i++;
      }
    }
    else {
      // try to get the dup function
      StringRef name = UCall->getCalledFunction()->getName();
      auto *Fn = Md.getFunction(name.str() + "_dup");
      // if the function exists, we substitute the original with the duplicate
      if (Fn != NULL) {
        std::vector<Value*> args;
        for (Value *Old : UCall->args()) {
          args.push_back(Old);
          if (Old == Original) {
            args.push_back(Copy);
          }
          else {
            args.push_back(Old);
          }
        }
        // replace the original with the dup function
        IRBuilder<> B(UCall);
        auto *NewCall = B.CreateCall(Fn->getFunctionType(), Fn, args);
        UCall->replaceNonMetadataUsesWith(NewCall);
        UCall->eraseFromParent();
      }
    }
  }

  /**
   * Replaces the calls in the excluded functions with the 
  */
  void replaceCallsWithOriginalCalls(Module &Md, std::set<std::string> &FunctionsToNotModify) {
    for (Function &Fn : Md) {
      if (Fn.hasName() && FunctionsToNotModify.find(Fn.getName().str()) == FunctionsToNotModify.end()) {
        for (BasicBlock &BB : Fn) {
          for (Instruction &I : BB) {
            if (isa<CallBase>(I)) {
              CallBase *ICall = &(cast<CallBase>(I));
              if (ICall->getCalledFunction() != NULL && ICall->getCalledFunction()->hasName()) {
               Function *OriginalFn = Md.getFunction(ICall->getCalledFunction()->getName().str() + "_original");
                if (OriginalFn != NULL) {
                  ICall->setCalledFunction(OriginalFn);
                }
              }
            }
          }
        }
      }
    }
  }

  public:
    /**
     * @param Md
     * @return
     */
    bool runOnModule(Module &Md) override {

      std::map<Function*, StringRef> FuncAnnotations;
      getFuncAnnotations(Md, FuncAnnotations);

      // we find the functions not to modify between the ones we already compiled with EDDI
      std::set<std::string> FunctionsToNotModify;
      getFunctionsToNotModify("compiled_eddi_functions.csv", FunctionsToNotModify);

      std::list<GlobalVariable*> Globals;
      for (GlobalVariable &GV : Md.globals()) {
        Globals.push_back(&GV);
      }
      
      for (GlobalVariable *GV : Globals) {
        // we don't care if the global is constant as it should not change at runtime
        // if the global is a struct or an array we cannot just duplicate the stores
        bool toDuplicate = !isa<Function>(GV) && 
            FuncAnnotations.find(cast<Function>(GV)) != FuncAnnotations.end() && 
            (FuncAnnotations.find(cast<Function>(GV)))->second.startswith("to_duplicate");
        if (! (GV->getType()->isFunctionTy() || GV->isConstant() || GV->getValueType()->isStructTy() || GV->getValueType()->isArrayTy() || GV->getValueType()->isOpaquePointerTy())
            || toDuplicate/* && ! GV.getName().endswith("_dup") */) {
          // see if the global variable has already been cloned
          GlobalVariable *GVCopy = Md.getGlobalVariable((GV->getName() + "_dup").str(), true);
          Constant *Initializer = NULL;
          if (GV->hasInitializer()) {
            Initializer = GV->getInitializer();
            // we may want to check whether our copy exists and is externally initialized
            if (GVCopy != NULL && !GVCopy->isExternallyInitialized()) {
              // in case it exists and is not externally initialized, we set the initializer
              GVCopy->setInitializer(Initializer);
              GVCopy->setExternallyInitialized(GV->isExternallyInitialized());
            }
          }
          if (GVCopy == NULL && !GV->getName().endswith_insensitive("_dup")) {
            // get a copy of the global variable
            GVCopy = new GlobalVariable(
                                          Md,
                                          GV->getValueType(), 
                                          false,
                                          GV->getLinkage(),
                                          Initializer,
                                          GV->getName()+"_dup",
                                          GV,
                                          GV->getThreadLocalMode(),
                                          GV->getAddressSpace(),
                                          GV->isExternallyInitialized()
                                          );
            GVCopy->setAlignment(GV->getAlign());
            DuplicatedGlobals.insert(std::pair<GlobalVariable*, GlobalVariable*>(GV, GVCopy));
          }
        }

        GlobalVariable *GVCopy = getDuplicatedGlobal(Md, *GV);
        if (GVCopy != NULL) {
          // clone all the stores performed on GV

          std::list<User*> Users;
          for (User *U : GV->users()) {
            Users.push_back(U);
          }
          for (User *U : Users) {
            // the user has to be a store of a excluded function writing the global 
            if (isa<StoreInst>(U) && 
                cast<StoreInst>(U)->getPointerOperand() == GV && 
                FunctionsToNotModify.find(cast<Instruction>(U)->getParent()->getParent()->getName().str()) == FunctionsToNotModify.end()) {
              // duplicate the store!
              StoreInst *I = cast<StoreInst>(U);
              StoreInst *IClone = cast<StoreInst>(I->clone());
              IClone->insertAfter(I);

              // change the operand
              IClone->setOperand(IClone->getPointerOperandIndex(), GVCopy);
            }
            else if (isa<LoadInst>(U)) {
              for (User *URec : U->users()) {
                // we have a load used by a call
                if (isa<CallBase>(URec)) {
                  LoadInst *ULoad = cast<LoadInst>(U);
                  Instruction *ULoadClone = ULoad->clone();
                  ULoadClone->insertAfter(ULoad);
                  int id = ULoad->getPointerOperandIndex();
                  ULoadClone->setOperand(id, GVCopy);
                  
                  duplicateCall(Md, cast<CallBase>(URec), ULoad, ULoadClone);
                  break;
                }
              }
            }
            else if (isa<CallBase>(U)) {
              duplicateCall(Md, cast<CallBase>(U), GV, GVCopy);
            }
          }
        }
      }

      std::set<Function*> Excluded;
      for (Function &Fn : Md) {
        if (FunctionsToNotModify.find(Fn.getName().str()) == FunctionsToNotModify.end()){
          for (BasicBlock &BB : Fn) {
            for (Instruction &I : BB) {
              if (isa<CallBase>(I)) {
                Function *CalledFn = cast<CallBase>(I).getCalledFunction();
                if (CalledFn != NULL && CalledFn->hasName() && FunctionsToNotModify.find(CalledFn->getName().str()) != FunctionsToNotModify.end()) {
                  Excluded.insert(CalledFn);
                }
              }
            }
          }
        }
      }
      return true;
    }
};
} // namespace

char DuplicateGlobals::ID = 0;
static RegisterPass<DuplicateGlobals> X("duplicate_globals", "Duplicate Global variables");