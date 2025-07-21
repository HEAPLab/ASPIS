#include "ASPISValues.h"
#include "../ASPIS.h"
#include "../Utils/Utils.h"

using namespace llvm;

/**
 * @brief Creates a comparison instruction between two LLVM IR values.
 *
 * @param V1        The first value to compare.
 * @param V2        The second value to compare.
 * @param insertPt  The instruction before which to insert the comparison.
 * @return          The resulting comparison Value, or nullptr if comparison is
 * not applicable.
 */
Value *createComparison(Value *V1, Value *V2, Instruction *insertPt) {
  // we don't compare two values if either of them is null or they are equal
  if (V1 == nullptr || V2 == nullptr || V1 == V2) {
    return nullptr;
  }

  IRBuilder<> B(insertPt);
  if (V1->getType()->isPointerTy()) {
    // TODO
  } else if (V1->getType()->isArrayTy()) {
    // TODO
  } else if (V1->getType()->isIntegerTy()) {
    // TODO
  } else if (V1->getType()->isFloatingPointTy()) {
    // TODO
  }
}

void ASPISInstr::CreateConsistencyCheck() {
  std::vector<Value *> cmpInstructions;

  for (Value *V : getOriginal()->operand_values()) {
    // The operand V is an ASPIS instruction
    if (eddiPass.getValuesToASPISValues()->find(V) !=
        eddiPass.getValuesToASPISValues()->end()) {
      auto *Operand = eddiPass.getValuesToASPISValues()->find(V)->second;
      Value *Cmp = createComparison(Operand->getOriginal(),
                                    Operand->getDuplicate(), getOriginal());
    }
    // The operand V is not an instruction, we just assume it is a global
    else {
      // TODO
    }
  }
}

void ASPISInstr::DuplicateInstruction() {
  // it may be that the current instruction has been duplicated on-demand by
  // other instructions
  if (getDuplicate() == nullptr) {
    return; // we don't need to duplicate
  }
}