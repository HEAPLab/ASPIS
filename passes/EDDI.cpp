#include "ASPIS.h"
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

#include "Utils/Utils.h"


PreservedAnalyses EDDI::run(Module &Md, ModuleAnalysisManager &AM) {

  PreprocessModule(Md);

  FuncRetToRef(Md);

  DuplicateGlobals(Md);

  CreateDupFunctions(Md);

  CreateErrBB(Md);

  for (Value *V : SphereOfReplication) {
    if (isa<Instruction>(V)) {
      ASPISInstr *AI = ValuesToASPISInstr.find(V)->second;
      
      // AI must always exist
      assert(AI); 

      AI->harden();
    }
  }

  return PreservedAnalyses::none();
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
                  if (Name == "eddi-verify") {
                    FPM.addPass(EDDI());
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