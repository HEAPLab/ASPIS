#include "llvm/IR/Instructions.h"

using namespace llvm;

class ASPISInstr {
  Instruction *original;
  Instruction *duplicate;

public:
  virtual void harden() = 0;
};

class ASPISInstrDefault : ASPISInstr {

};

class ASPISInstrAlloca : ASPISInstr {

};

class ASPISInstrBranch : ASPISInstr {

};


class ASPISInstrStore : ASPISInstr {

};

class ASPISInstrCall : ASPISInstr {

};

