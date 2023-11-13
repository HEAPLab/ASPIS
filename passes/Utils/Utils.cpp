#include "Utils.hpp"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <map>

using namespace llvm;

bool IsNotAPHINode (Use &U){
  return !isa<PHINode>(U.getUser());
}

void getFuncAnnotations(Module &Md, std::map<Function*, StringRef> &FuncAnnotations) {
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
            Function* AnnotatedFunction = cast<Function>(CS->getOperand(0)/*->getOperand(0)*/);
            // the second field is a pointer to a global constant Array that holds the string
            if (GlobalVariable *GAnn =
                    dyn_cast<GlobalVariable>(CS->getOperand(1)/*->getOperand(0)*/)) {
              if (ConstantDataArray *A =
                      dyn_cast<ConstantDataArray>(GAnn->getOperand(0))) {
                // we have the annotation!
                StringRef AS = A->getAsString();
                FuncAnnotations.insert(std::pair<Function*, StringRef>(AnnotatedFunction, AS));                      // if the function is new, add it to the annotated functions
              }
            }
          }
        }
      }
    }
  }
}
}
