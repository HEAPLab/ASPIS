#pragma once

#include "../ASPIS.h"
#include "llvm/IR/Instructions.h"

class EDDI;

/**
 *
 */
class ASPISValue {
public:
  /**
   * @brief ASPISValue constructor
   */
  ASPISValue(llvm::Value *original, const EDDI &eddiPass)
      : original(original), eddiPass(eddiPass) {}

  llvm::Value *getOriginal() { return original; }
  llvm::Value *getDuplicate() { return duplicate; }

  virtual void harden() = 0;

protected:
  const EDDI &eddiPass;

private:
  llvm::Value *original;
  llvm::Value *duplicate;
};

/**
 *
 */
class ASPISGlobalVariable : public ASPISValue {
public:
  void harden() override {
    // TODO
  }

  llvm::GlobalVariable *getOriginal() {
    return llvm::cast<llvm::GlobalVariable>(ASPISValue::getOriginal());
  }

  llvm::GlobalVariable *getDuplicate() {
    return llvm::cast<llvm::GlobalVariable>(ASPISValue::getDuplicate());
  }
};

/**
 *
 */
class ASPISInstr : public ASPISValue {
public:
  void setErrBB(llvm::BasicBlock *BB) { ErrBB = BB; }
  llvm::BasicBlock *getErrBB() { return ErrBB; }

  llvm::Instruction *getOriginal() {
    return llvm::cast<llvm::Instruction>(ASPISValue::getOriginal());
  }

  llvm::Instruction *getDuplicate() {
    return llvm::cast<llvm::Instruction>(ASPISValue::getDuplicate());
  }

protected:
  void CreateConsistencyCheck();
  void DuplicateInstruction();

  llvm::BasicBlock *ErrBB = nullptr;
};

/**
 *
 */
class ASPISInstrDefault : ASPISInstr {
  void harden() { DuplicateInstruction(); }
};

class ASPISInstrAlloca : ASPISInstr {};

class ASPISInstrBranch : ASPISInstr {};

class ASPISInstrStore : ASPISInstr {};

class ASPISInstrCall : ASPISInstr {};
