#include "Utils.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <list>
#include <fstream>
#include <iostream>

using namespace llvm;
  
bool AlternateMemMapEnabled;
std::string DuplicateSecName;

static cl::opt<bool, true> AlternateMemMap("alternate-memmap", cl::desc("Enable the alternate memory layout for alloca and global variables"), cl::location(AlternateMemMapEnabled), cl::init(false));

static cl::opt<std::string, true> DuplicateSecNameOpt("duplicate-sec", cl::desc("Specify the name of the section where the duplicate data should be allocated"), cl::location(DuplicateSecName), cl::init(".dup_data"));

bool IsNotAPHINode (Use &U){
  return !isa<PHINode>(U.getUser());
}

void getFuncAnnotations(Module &Md, std::map<Value*, StringRef> &FuncAnnotations) {
  if(GlobalVariable* GA = Md.getGlobalVariable("llvm.global.annotations")) {
    // the first operand holds the metadata
    for (Value *AOp : GA->operands()) {
      // all metadata are stored in an array of struct of metadata
      if (ConstantArray *CA = dyn_cast<ConstantArray>(AOp)) {
        // so iterate over the operands
        for (Value *CAOp : CA->operands()) {
          // get the struct, which holds a pointer to the annotated function
          // as first field, and the annotation as second field
          if (ConstantStruct *CS = dyn_cast<ConstantStruct>(CAOp)) {
            if (CS->getNumOperands() >= 2) {
              if (isa<Function>(CS->getOperand(0)) || isa<GlobalValue>(CS->getOperand(0))) {
                Value* AnnotatedFunction = CS->getOperand(0)/*->getOperand(0)*/;
                // the second field is a pointer to a global constant Array that holds the string
                if (GlobalVariable *GAnn =
                        dyn_cast<GlobalVariable>(CS->getOperand(1)/*->getOperand(0)*/)) {
                  if (ConstantDataArray *A =
                          dyn_cast<ConstantDataArray>(GAnn->getOperand(0))) {
                    // we have the annotation!
                    StringRef AS = A->getAsString();
                    FuncAnnotations.insert(std::pair<Value*, StringRef>(AnnotatedFunction, AS)); // if the function is new, add it to the annotated functions
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

void persistCompiledFunctions(std::set<Function*> &CompiledFuncs, const char* filename) {
  std::ofstream file;
  file.open(filename);
  file << "fn_name\n";
  for (Function *Fn : CompiledFuncs) {
    file << Fn->getName().str() << "\n";
  }
  file.close();
}

bool shouldCompile(Function &Fn, 
    const std::map<Value*, StringRef> &FuncAnnotations,
    const std::set<Function*> &OriginalFunctions) {
  assert(&Fn != NULL && "Are you passing a null pointer?");
  return 
      // the function is neither null nor empty
      
      Fn.size() != 0 
      // Moreover, it does not have to be marked as excluded or to_duplicate
      && (FuncAnnotations.find(&Fn) == FuncAnnotations.end() || 
      (!FuncAnnotations.find(&Fn)->second.startswith("exclude") /* && 
      !FuncAnnotations.find(&Fn)->second.startswith("to_duplicate") */))
      // nor it is one of the original functions
      && OriginalFunctions.find(&Fn) == OriginalFunctions.end();
}

DebugLoc findNearestDebugLoc(Instruction &I) {
  std::list<BasicBlock*> candidates;

  auto *PrevI = I.getPrevNonDebugInstruction();

  while (PrevI = PrevI->getPrevNonDebugInstruction()) {
    if (auto DL = PrevI->getDebugLoc()) {
      return DL;
    }
  }

  for (auto *U : I.getParent()->users()) {
    candidates.push_back(cast<Instruction>(U)->getParent());
  }

  for (auto *BB : candidates) {
    PrevI = BB->getTerminator();
    while (PrevI = PrevI->getPrevNonDebugInstruction()) {
      if (auto DL = PrevI->getDebugLoc()) {
        return DL;
      }
    }
    for (auto *U : BB->users()) {
      if(std::find(candidates.begin(), candidates.end(), cast<Instruction>(U)->getParent()) == candidates.end()) {
        candidates.push_back(cast<Instruction>(U)->getParent());
      }
    }
  }
  errs() << "Could not find nearest debug location! Aborting compilation.\n";
  abort();
  return nullptr;
}