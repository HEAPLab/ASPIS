/**
 * ************************************************************************************************
 * @brief  LLVM ModulePass that transforms the functions with a return value to void functions
 *         where a pointer is passed as parameter to store the return value.
 * 
 * @author Davide Baroffio, Politecnico di Milano, Italy (davide.baroffio@polimi.it)
 * ************************************************************************************************
*/
#include "ASPIS.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ModRef.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "Utils/Utils.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <llvm/IR/Value.h>

using namespace llvm;

#define DEBUG_TYPE "func-ret-to-ref"



/**
 * @param Fn the function for which we want to add the return value as a parameter
 * @param Md the module
 * 
 * @returns 
 * NULL if the function has VoidTy as return type. 
 * Else it returns a clone of the function with: 
 *  - void return type 
 *  - a ptr to the old return type as a function argument
*/
Function* FuncRetToRef::updateFnSignature(Function &Fn, Module &Md) {
    Type *RetType = Fn.getReturnType();
    if (RetType->isVoidTy()) {
        return NULL;
    }
    FunctionType *FnType = Fn.getFunctionType();
    
    // create the param type list adding the return param type
    std::vector<Type*> paramTypeList;
    for (Type *ParamType : FnType->params()) {
        paramTypeList.push_back(ParamType);
    }

    paramTypeList.push_back(RetType->getPointerTo()); // we "return" a pointer to the return type

    // set the returntype to void
    FunctionType *NewFnType = FnType->get(Type::getVoidTy(Md.getContext()), // returntype
                                            paramTypeList,                    // params
                                            FnType->isVarArg());              // vararg
    
    // create the function and clone the old one
    Function *ClonedFunc = Fn.Create(NewFnType, Fn.getLinkage(), Fn.getName() + "_ret", Md);
    ValueToValueMapTy Params;
    for (int i=0; i < Fn.arg_size(); i++) {
        Params[Fn.getArg(i)] = ClonedFunc->getArg(i);
    }
    SmallVector<ReturnInst*, 8> returns;
    CloneFunctionInto(ClonedFunc, &Fn, Params, CloneFunctionChangeType::LocalChangesOnly, returns);

    std::list<Attribute> attrs_to_remove;
    for (auto AttrSet : ClonedFunc->getAttributes()) {
        for (auto Attr : AttrSet) {
            if (Attr.isEnumAttribute() || Attr.isIntAttribute() || Attr.isTypeAttribute())
            attrs_to_remove.push_back(Attr);
        }
    }
    for (auto elem : attrs_to_remove) {
        try {
            ClonedFunc->removeRetAttr(elem.getKindAsEnum());
        }
        catch (int e) {
            continue;
        }
    }
    

    ClonedFunc->setMemoryEffects(MemoryEffects::unknown());
    for (int i=0; i < ClonedFunc->arg_size(); i++) {
        if(ClonedFunc->hasParamAttribute(i, Attribute::Returned)){
            ClonedFunc->removeParamAttr(i, Attribute::Returned);
        }
        if(ClonedFunc->hasParamAttribute(i, Attribute::StructRet)){
            ClonedFunc->removeParamAttr(i, Attribute::StructRet);
        }
    }

    updateRetInstructions(*ClonedFunc);

    return ClonedFunc;
}

/**
 * Substitutes all the return instructions of a function with void return instructions and
 * add store instructions of the return param
*/
void FuncRetToRef::updateRetInstructions(Function &Fn) {
    for (BasicBlock &BB : Fn) {
        Instruction *I = BB.getTerminator();
        if ( I != NULL && isa<ReturnInst>(I)) {
            // get the return value from the instruction
            Value *ReturnValue = cast<ReturnInst>(*I).getReturnValue();

            IRBuilder<> B(I);

            // Get the return pointer from the function signature (the last arg)
            Value *ReturnPtr = Fn.getArg(Fn.arg_size()-1); // the last argument is the return ptr

            // Store the returnvalue in the returnptr
            B.CreateStore(ReturnValue, ReturnPtr, true);

            // create a ret instruction
            B.CreateRetVoid();

            // delete the return instruction
            I->eraseFromParent();
        }
    }
}

/** 
 * Gets all the uses of the function and replaces them with the clone function
*/
void FuncRetToRef::updateFunctionCalls(Function &Fn, Function &NewFn) {
    std::list<Instruction*> ListInstrToRemove;
    
    std::list<CallBase*> FnUsers;

    // for each place in which the function has been called, we replace it with the _ret version
    for (User *U : Fn.users()) {
        if (isa<CallBase>(U)) {
            CallBase *CInstr = cast<CallBase>(U); // this is the instruction that called the function
            if (CInstr->getCalledFunction() == &Fn)
                FnUsers.push_back(CInstr);
        }
    }
    for (CallBase *CInstr : FnUsers) {
        bool createdNewCall = false;
        // duplicate all the args of the original instruction and add a new arg for the ret value
        std::vector<Value*> args;
        for (Value *V : CInstr->args()) {
            args.push_back(V);
        }

        IRBuilder<> B(CInstr);
        
        // The function return value can be used immediately by a store instruction, 
        // so we try to get the pointer operand of the store instruction and use it as a return value
        int found = 0;
        for (User *UC : CInstr->users()) {
            // if the user is a store instruction and the stored value is the output of the call instr
            if (isa<StoreInst>(UC) && cast<StoreInst>(UC)->getValueOperand() == CInstr) {
                StoreInst *SI = cast<StoreInst>(UC);

                // get the pointer
                Value *CandidateAlloca = SI->getPointerOperand();
                if (isa<AllocaInst>(CandidateAlloca)) {
                    found = 1;
                    args.push_back(SI->getPointerOperand());
                        // remove the store from the basic block as the operation is performed in the called fun
                    SI->eraseFromParent();
                    
                    // do the call
                    if (isa<CallInst>(CInstr)) {
                        B.CreateCall(NewFn.getFunctionType(), &NewFn, args);
                    } else if (isa<InvokeInst>(CInstr)) {
                        auto IInstr = cast<InvokeInst>(CInstr);
                        B.CreateInvoke(NewFn.getFunctionType(), &NewFn, IInstr->getNormalDest(), IInstr->getUnwindDest(), args);
                        B.SetInsertPoint(&*(IInstr->getNormalDest()->getFirstInsertionPt()));
                    } else {
                        errs() << "ERROR - Unsupported call instruction:\n" << *CInstr << "\n";
                        abort();
                    } 
                    Instruction *TmpLoad = B.CreateLoad(CInstr->getType(), CandidateAlloca);
                    createdNewCall = true; 
                    CInstr->replaceNonMetadataUsesWith(TmpLoad);
                    break;
                }
            }
        }

        // if the return is not used by a store but is used by other instructions,
        // we allocate the memory for return value in the caller function
        if (!found) {
            IRBuilder<> BInit(&(CInstr->getParent()->getParent()->front().front()));
            Instruction *TmpAlloca = BInit.CreateAlloca(CInstr->getType());
            args.push_back(TmpAlloca);
            // do the call
            if (isa<CallInst>(CInstr)) {
                B.CreateCall(NewFn.getFunctionType(), &NewFn, args);
            } else if (isa<InvokeInst>(CInstr)) {
                auto IInstr = cast<InvokeInst>(CInstr);
                B.CreateInvoke(NewFn.getFunctionType(), &NewFn, IInstr->getNormalDest(), IInstr->getUnwindDest(), args);
                B.SetInsertPoint(&*(IInstr->getNormalDest()->getFirstInsertionPt()));
            } else {
                errs() << "ERROR - Unsupported call instruction:\n" << *CInstr << "\n";
                abort();
            } 
            // use the load on the return value instead of the previous function output
            Instruction *TmpLoad = B.CreateLoad(CInstr->getType(), TmpAlloca, true);
            createdNewCall = true;
            CInstr->replaceNonMetadataUsesWith(TmpLoad);
        }
        if (createdNewCall)
            ListInstrToRemove.push_back(CInstr);
    }

    // remove all the older call instructions
    for (Instruction *InstToRemove : ListInstrToRemove) {
        InstToRemove->eraseFromParent();
    }
}

PreservedAnalyses FuncRetToRef::run(Module &Md, ModuleAnalysisManager &AM) {
    LinkageMap linkageMap=mapFunctionLinkageNames(Md);
    std::map<Value*, StringRef> FuncAnnotations;
    getFuncAnnotations(Md, FuncAnnotations);

    std::set<Function*> OriginalFunctions;

    // store the functions that are currently in the module
    std::list<Function*> FnList;

    for (Function &Fn : Md) {
        if (shouldCompile(Fn, FuncAnnotations, OriginalFunctions)) {
            FnList.push_back(&Fn);
        }
    }

    for (Function *Fn : FnList) {
        Function *newFn = updateFnSignature(*Fn, Md);
        if (newFn != NULL) {
            updateFunctionCalls(*Fn, *newFn); 
        }
    }
    return PreservedAnalyses::none();
}

