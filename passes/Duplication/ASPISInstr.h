#include "../ASPIS.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

class ASPISInstr {

protected:
  const EDDI &EDDIPass;
  
  Instruction *original;
  Instruction *duplicate;

  BasicBlock *ErrBB = nullptr;

  void CreateConsistencyCheck();
  void DuplicateInstruction();

public:
  virtual void harden() = 0;

  void setErrBB(BasicBlock *BB) {
    ErrBB = BB;
  }
  BasicBlock *getErrBB() {
    return ErrBB;
  }
};

class ASPISInstrDefault : ASPISInstr {
  void harden() {
    DuplicateInstruction();
  }
};

class ASPISInstrAlloca : ASPISInstr {

};

class ASPISInstrBranch : ASPISInstr {

};

class ASPISInstrStore : ASPISInstr {

};

class ASPISInstrCall : ASPISInstr {

};

