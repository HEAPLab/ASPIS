/**
 * ************************************************************************************************
 * @brief  LLVM pass implementing a Profiler for checking the number of checked variable in the module.
 *         Original algorithm by Oh et Al. (DOI: 10.1109/24.994926)
 * 
 * @author Davide Baroffio, Politecnico di Milano, Italy (davide.baroffio@polimi.it)
 * ************************************************************************************************
*/
#include "../ASPIS.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "../Utils/Utils.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <list>
#include <llvm/IR/DebugLoc.h>
#include <map>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

using namespace llvm;

#define DEBUG_TYPE "aspis_profiler"

bool isCheckBarrierBegin(Instruction *I) {
  return isa<CallBase>(I) && cast<CallBase>(I)->getCalledFunction()->getName().equals("aspis.datacheck.begin");
}
bool isCheckBarrierEnd(Instruction *I) {
  return isa<CallBase>(I) && cast<CallBase>(I)->getCalledFunction()->getName().equals("aspis.datacheck.end");
}

// returns true if the instruction is used within a consistency check
bool isInstructionChecked(Instruction *Inst) {
  for (auto U : Inst->users()) {
    if (isa<Instruction>(U)) {
      auto I = cast<Instruction>(U);
      Instruction *PrevI = I->getPrevNode();
      Instruction *NextI = I->getNextNode();

      while (PrevI != nullptr && !isCheckBarrierBegin(PrevI)) {
        PrevI = PrevI->getPrevNode();
      }
      while (NextI != nullptr && !isCheckBarrierEnd(NextI)) {
        NextI = NextI->getNextNode();
      }
      if (NextI != nullptr && PrevI != nullptr) {
        return true;
      }
    }
  }
  return false;
}

// returns the list of operands of I that are in a consistency check
void findCheckedOps(Instruction &I, std::set<Instruction*> *checkedOps, std::set<Instruction*> *uncheckedOps) {
  std::list<Instruction*> res;
  for (auto *Op : I.operand_values()) {
    if (isa<Instruction>(Op)){
      auto OpInstr = cast<Instruction>(Op);
      if (isInstructionChecked(OpInstr)) {
        checkedOps->insert(OpInstr);
      } else {
        uncheckedOps->insert(OpInstr);
      }
    }
  }
}

/*
Find the instruction associated with the metadata having name `name` and for which the operand in position `op_pos`
is equal to the string `op_val`. If no instruction can be found, nullptr is returned.
*/
Instruction *FindInstructionWithMetadata(Module &Md, std::string name, int op_pos=-1, std::string op_val="") {
  for (auto &Fn : Md) {
    for (auto &BB : Fn) {
      for (auto &I : BB) {
        if (I.hasMetadata(name)) {
          if (op_pos < 0 || op_val.empty() || dyn_cast<MDString>(I.getMetadata(name)->getOperand(op_pos).get())->getString() == op_val)
            return &I;
        }
      }
    }
  }
  return nullptr;
}

void initializeInstructionWeights(Module &Md, std::map<Value*, StringRef> *FuncAnnotations, std::map<Instruction*, double> *InstWeights) {
  std::ifstream file(Md.getName().str() + ".aspis.bb.txt"); 
  std::string ln;
  while (std::getline(file, ln)) {
    std::istringstream iss(ln);
    std::string key;
    int value;

    // find the key and value stored in the line
    if (iss >> key >> value) {
      auto id = key.substr(key.rfind('.')+1);
      // get the instruction associated with the metadata
      Instruction *IWithMD = FindInstructionWithMetadata(Md, "aspis.bb", 0, id);
      // if the instruction exists
      if (IWithMD) {
        // assign the weight to each instruction in the bb containing the instruction with metadata
        for (auto &I : *(IWithMD->getParent())) {
          InstWeights->insert(std::pair(&I, value));
        }
      }
      else {
        errs() << "aspis.bb" << " " << id << "\n";
        assert(false && "Cannot find instructions associated with the metadata above");
      }
    } else {
      std::cerr << "Malformed line: " << ln << std::endl;
      abort();
    }
  }
}

PreservedAnalyses ASPISCheckProfiler::run(Module &Md, ModuleAnalysisManager &AM) {
  std::list<Instruction*> SyncPts;

  std::set<Instruction*> CheckedData;
  std::set<Instruction*> UncheckedData;

  /* 
  This data structure contains the weights to apply to each instruction encountered.
  */
  std::map<Instruction*, double> InstWeights;

  int tot_instructions = 0;

  getFuncAnnotations(Md, FuncAnnotations);

  initializeInstructionWeights(Md, &FuncAnnotations, &InstWeights);

  for (auto &Fn : Md) {
    if (shouldCompile(Fn, FuncAnnotations)) {
      for (auto &BB : Fn) {
        for (auto &I : BB) {
          tot_instructions++;
          if (I.hasMetadata("aspis.syncpt")) {            
            SyncPts.push_back(&I);
          }
        }
      }
    }
  }
  
  // errs() << "Found " << SyncPts.size() << " Syncronization Points\n";

  std::set<Instruction*> done;
  for (auto I : SyncPts) {
    std::list<Instruction*> worklist;
    findCheckedOps(*I, &CheckedData, &UncheckedData);

    // fill the worklists
    for (auto item : CheckedData) {
      if (done.find(item) == done.end()) {
        worklist.push_back(item);
        done.insert(item);
      }
    }
    for (auto item : UncheckedData) {
      if (done.find(item) == done.end()) {
        worklist.push_back(item);
        done.insert(item);
      }
    }

    // iterate on the worklist
    while (!worklist.empty()) {
      // take the current instruction
      Instruction *CurrI = worklist.front();
      // iterate over the users
      for (auto U : CurrI->operand_values()) {
        // the user is an instruction that we did not find before
        if (isa<Instruction>(U) && done.find(cast<Instruction>(U)) == done.end() ) {
          // add it to the corresponding set
          if (CheckedData.find(CurrI) != CheckedData.end())
            CheckedData.insert(cast<Instruction>(U));
          if (UncheckedData.find(CurrI) != UncheckedData.end())
            UncheckedData.insert(cast<Instruction>(U));

          // add the new instruction to the worklist and mark it as done
          worklist.push_back(cast<Instruction>(U));
          done.insert(cast<Instruction>(U));
        }
      }
      worklist.pop_front();
    }
  }  

  double CheckedNumExecutedI = 0;
  double UncheckedNumExecutedI = 0;
  for (auto I : CheckedData) {
    auto data = InstWeights.find(I);
    assert(data != InstWeights.end() && "Found instruction without weight!");
    CheckedNumExecutedI+=data->second;
  }
  for (auto I : UncheckedData) {
    auto data = InstWeights.find(I);
    assert(data != InstWeights.end() && "Found instruction without weight!");
    UncheckedNumExecutedI+=data->second;
  }

  errs() << "checked/tot: " << (long)CheckedNumExecutedI << "/" << (long)(UncheckedNumExecutedI+CheckedNumExecutedI) << " (" << (int)((CheckedNumExecutedI /(UncheckedNumExecutedI+CheckedNumExecutedI))*100) << ")\n";

  return PreservedAnalyses::all();
}

void createPrintProfileData(Module &M, std::string GVPrefix) {
  // find the return of main
  std::list<Instruction*> RetInstructions;
  auto MainFn = M.getFunction("main");
  for (BasicBlock &BB : *MainFn) {
    if (auto *R = dyn_cast<ReturnInst>(BB.getTerminator())) {
      RetInstructions.push_back(R);
    }
  }
  assert(RetInstructions.size() == 1 && "Main has multiple return instructions!");

  // get the functions for file handling
  LLVMContext &Ctx = M.getContext();

  FunctionCallee FOpen = M.getOrInsertFunction("fopen",
    PointerType::getUnqual(Type::getInt8Ty(Ctx)),
    PointerType::getUnqual(Type::getInt8Ty(Ctx)),
    PointerType::getUnqual(Type::getInt8Ty(Ctx))
  );

  FunctionCallee FPrintf = M.getOrInsertFunction("fprintf",
    Type::getInt32Ty(Ctx),
    PointerType::getUnqual(Type::getInt8Ty(Ctx)),
    PointerType::getUnqual(Type::getInt8Ty(Ctx)),
    Type::getInt32Ty(Ctx)
  );

  FunctionCallee FClose = M.getOrInsertFunction("fclose",
    Type::getInt32Ty(Ctx),
    PointerType::getUnqual(Type::getInt8Ty(Ctx))
  );

  IRBuilder<> Builder(RetInstructions.front());

  // create fopen
  Value *FileName = Builder.CreateGlobalStringPtr(M.getName().str() + "." + GVPrefix + ".txt");
  Value *Mode = Builder.CreateGlobalStringPtr("w");
  Value *FileHandle = Builder.CreateCall(FOpen, {FileName, Mode});

  // print the counters
  for (GlobalVariable &GV : M.globals()) {
    if (GV.getName().startswith(GVPrefix + ".counter.")) {
      // Load counter
      Value *Count = Builder.CreateLoad(Type::getInt32Ty(Ctx), &GV);

      // Prepare fprintf format string
      std::string FmtStr = GV.getName().str() + " %d\n";
      Value *Fmt = Builder.CreateGlobalStringPtr(FmtStr);

      // Call fprintf
      Builder.CreateCall(FPrintf, {FileHandle, Fmt, Count});
    }
  }

  // 6. fclose(f)
  Builder.CreateCall(FClose, {FileHandle});
}

/*  
Create a counter to verify how many times instruction `I` is hit at runtime.
MDName is the name of the metadata containing the ID of the instruction,
the ID must be stored as a string in the operand 0 of the mdnode.
 */
void createCounter(Instruction &I, std::string MDName) {
  Module *Md = I.getParent()->getParent()->getParent();
  std::string VarName = "";
  if (MDNode *MD = I.getMetadata(MDName)) {
    if (MD->getNumOperands() > 0) {
      if (auto *MDS = dyn_cast<MDString>(MD->getOperand(0))) {
          VarName = MDName + ".counter." + MDS->getString().str();
      }
    }
  }
  // create a global variable to store the counter
  GlobalVariable *GV = Md->getGlobalVariable(VarName);
  if (!GV) {
    GV = new GlobalVariable(
      *Md,
      Type::getInt32Ty(Md->getContext()), // int32
      false,                 // isConstant
      GlobalValue::ExternalLinkage,
      ConstantInt::get(Type::getInt32Ty(Md->getContext()), 0), // initializer
      VarName
    );
  }
  
  // update the counter before I
  IRBuilder<> B(&I);
  auto GVInstr = B.CreateLoad(Type::getInt32Ty(Md->getContext()), GV);
  auto NewVal = B.CreateAdd(GVInstr, ConstantInt::get(Type::getInt32Ty(Md->getContext()), 1));
  B.CreateStore(NewVal, GV);
}

int bb_id = 0;
PreservedAnalyses ASPISInsertCheckProfiler::run(Module &Md, ModuleAnalysisManager &AM) {
  std::list<Instruction*> SyncPts;
  std::list<Instruction*> BBs;

  std::set<Instruction*> CheckedData;

  int tot_instructions = 0;

  getFuncAnnotations(Md, FuncAnnotations);
  for (auto &Fn : Md) {
    if (shouldCompile(Fn, FuncAnnotations)) {
      for (auto &BB : Fn) {
        (*BB.getFirstInsertionPt()).setMetadata("aspis.bb", MDNode::get(BB.getContext(), MDString::get(BB.getContext(), std::to_string(bb_id))));
        BBs.push_back(&*BB.getFirstInsertionPt());
        bb_id++;
        for (auto &I : BB) {
          tot_instructions++;
          if (I.hasMetadata("aspis.syncpt")) {            
            SyncPts.push_back(&I);
          }
        }
      }
    }
  }
  for (auto SyncPt : SyncPts) {
    createCounter(*SyncPt, "aspis.syncpt");
  }
  for (auto BB : BBs) {
    createCounter(*BB, "aspis.bb");
  }
  
  createPrintProfileData(Md, "aspis.syncpt");
  createPrintProfileData(Md, "aspis.bb");

  return PreservedAnalyses::none();
}


//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getASPISCheckProfilerPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "aspis-check-profiler", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "aspis-check-profile") {
                    FPM.addPass(ASPISCheckProfiler());
                    return true;
                  }
                  return false;
                });
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "aspis-insert-check-profile") {
                    FPM.addPass(ASPISInsertCheckProfiler());
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
  return getASPISCheckProfilerPluginInfo();
}