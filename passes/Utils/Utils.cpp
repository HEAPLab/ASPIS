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
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/Debug.h"

using namespace llvm;
using LinkageMap = std::unordered_map<std::string, std::vector<StringRef>>;

enum class ProfilingType {
  SynchronizationPoint,
  ConsistencyCheck
}; 

bool DebugEnabled;
static cl::opt<bool, true> DebugEnabledOpt("debug-enabled", cl::desc("Enable support for debug metadata"), cl::location(DebugEnabled), cl::init(false));

bool AlternateMemMapEnabled;
static cl::opt<bool, true> AlternateMemMap("alternate-memmap", cl::desc("Enable the alternate memory layout for alloca and global variables"), cl::location(AlternateMemMapEnabled), cl::init(false));

std::string DuplicateSecName;
static cl::opt<std::string, true> DuplicateSecNameOpt("duplicate-sec", cl::desc("Specify the name of the section where the duplicate data should be allocated"), cl::location(DuplicateSecName), cl::init(".dup_data"));

bool ProfilingEnabled;
static cl::opt<bool, true> ProfilingFuncCalls("enable-profiling", cl::desc("Enable the insertion of profiling function calls at synchonization points"), cl::location(ProfilingEnabled), cl::init(false));


bool IsNotAPHINode (Use &U){
  return !isa<PHINode>(U.getUser());
}

bool IsGlobalStructOfFunctions (Use &U) {
  bool res = false;
  Value *Val = U.get();
  Value *ValUser = U.getUser();
  if (isa<Function>(Val) && isa<Constant>(ValUser)) {
    Constant *ConstUser = cast<Constant>(U.getUser());
    errs() << "Found constant " << *ConstUser << "\n";

    for (auto NestedUser : ConstUser->users()) {
      errs() << "Used by " << NestedUser->getName() << "\n";
      if (NestedUser->getName().ends_with("_dup")) {
        res = true;
      }
    }
  }
  return false;
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
      &&
      !Fn.getName().contains("DataCorruption_Handler")
      &&
      !Fn.getName().contains("SigMismatch_Handler")
      && 
      !Fn.getName().contains("aspis.syncpt")
      // Moreover, it does not have to be marked as excluded or to_duplicate
      && (FuncAnnotations.find(&Fn) == FuncAnnotations.end() || 
      (!FuncAnnotations.find(&Fn)->second.startswith("exclude") /* && 
      !FuncAnnotations.find(&Fn)->second.startswith("to_duplicate") */))
      // nor it is one of the original functions
      && OriginalFunctions.find(&Fn) == OriginalFunctions.end();
}

DebugLoc findNearestDebugLoc(Instruction *I) {
  std::list<BasicBlock*> candidates;
  if (I == nullptr) {
    return nullptr;
  }

  if (I->getDebugLoc()) return I->getDebugLoc();

  auto *PrevI = I->getPrevNonDebugInstruction();

  while (PrevI && (PrevI = PrevI->getPrevNonDebugInstruction())) {
    if (auto DL = PrevI->getDebugLoc()) {
      return DL;
    }
  }

  for (auto *U : I->getParent()->users()) {
    candidates.push_back(cast<Instruction>(U)->getParent());
  }

  for (auto *BB : candidates) {
    PrevI = BB->getTerminator();
    while ((PrevI = PrevI->getPrevNonDebugInstruction())) {
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

  errs() << "Could not find nearest debug location!\n";
  errs() << "Instruction: " << *I << "\n";
  errs() << "In function: \n";
  errs() << *I->getParent()->getParent() << "\n";
  return nullptr;
}

LinkageMap mapFunctionLinkageNames(const Module &M) {
    LinkageMap linkageMap;
    for (const Function &F : M) {
        if (DISubprogram *SP = F.getSubprogram()) {
            StringRef linkageName = F.getName();
            
            if (!linkageName.empty()) {
                linkageMap[std::string(SP->getName())].push_back(linkageName);
            }
        }
    }
    return linkageMap;
}

#include "llvm/Support/raw_ostream.h"

void printLinkageMap(const LinkageMap &linkageMap) {
    for (const auto &entry : linkageMap) {
        errs() << "Function Name: " << entry.first << "\n";
        for (const StringRef &linkageName : entry.second) {
            errs() << "  Linkage Name: " << linkageName << "\n";
        }
    }
}


// This function retrieves the first linkage name for the given function name
StringRef getLinkageName(const LinkageMap &linkageMap, const std::string &functionName) {
    // Find the function name in the linkageMap
    auto it = linkageMap.find(functionName);

    // Check if the function name exists in the map
    if (it != linkageMap.end() && !it->second.empty()) {
        // Return the first linkage name from the vector
        DEBUG_WITH_TYPE("linkage_verification",dbgs() << "Linkage name given to "<<functionName <<": " << it->second.front() << "\n");
        return it->second.front();
    } else {
        // Return an empty StringRef if the function name or linkage name is not found
        DEBUG_WITH_TYPE("linkage_verification",dbgs()<< "No linkage name found for "<< functionName << "\n");
        return StringRef(functionName);
    }
}

bool isIntrinsicToDuplicate(CallBase *CInstr) {
        Intrinsic::ID intrinsicID = CInstr->getIntrinsicID();
        if (intrinsicID != Intrinsic::not_intrinsic /* intrinsicID == Intrinsic::memcpy || intrinsicID == Intrinsic::memset */) {
            return true; 
        }    

    return false; 
}

void createFtFunc(Module &Md, StringRef name) {
  Value *FnValue = Md.getFunction(name);
  Function *Fn;

  if (FnValue == nullptr) { // the function does not exist and has not even been declared
    FnValue = Md.getOrInsertFunction(name, FunctionType::getVoidTy(Md.getContext())).getCallee();
    assert(isa<Function>(FnValue) && "The function name must correspond to a function.");
  
    Fn = cast<Function>(FnValue);
  
    if (Fn->isDeclaration()) {
      BasicBlock *StartBB = BasicBlock::Create(Md.getContext(), "start", Fn);
      BasicBlock *LoopBB = BasicBlock::Create(Md.getContext(), "loop", Fn);
  
      IRBuilder<> B(StartBB);
      B.CreateBr(LoopBB);
  
      B.SetInsertPoint(LoopBB);
      B.CreateBr(LoopBB);
    }
  } else {
    Fn = cast<Function>(FnValue);
  }
  
  Fn->addFnAttr(Attribute::NoInline);
}

void createProfilingFunc(Module &Md, StringRef name, ProfilingType PT) {
  auto &Ctx = Md.getContext();
  Type *RetType;
  std::vector<Type*> ArgTypes;
  AttributeList AL;

  // common attributes
  AL = AL.addFnAttribute(Ctx, Attribute::NoInline);

  // create the function
  switch (PT)
  {
  case ProfilingType::ConsistencyCheck :
    RetType = Type::getVoidTy(Ctx);
    //ArgTypes.push_back(Type::getVoidTy(Ctx));
    AL = AL.addFnAttribute(Ctx, Attribute::NoUnwind);
    AL = AL.addFnAttribute(Ctx, Attribute::OptimizeNone);
    break;
  case ProfilingType::SynchronizationPoint :
    RetType = Type::getInt1Ty(Ctx);
    ArgTypes.push_back(Type::getInt1Ty(Ctx));  // condition bit (1 if the check was successful)
    ArgTypes.push_back(Type::getInt1Ty(Ctx));  // condition bit (1 if the check was successful)
    break;
  default:
    assert(false && "No valid profiling type for profiling function.");
    break;
  }

  Value *FnValue = Md.getOrInsertFunction(name, RetType, ArrayRef(ArgTypes)).getCallee();
  assert(isa<Function>(FnValue) && "The function name must correspond to a function.");

  Function *Fn = cast<Function>(FnValue);

  Fn->setAttributes(AL);
  Fn->setOnlyReadsMemory();

  // create the body
  if (Fn->isDeclaration()) {
    BasicBlock *StartBB = BasicBlock::Create(Md.getContext(), "start", Fn);
    IRBuilder<> B(StartBB);
    Value *RetVal;
    switch (PT)
    {
    case ProfilingType::ConsistencyCheck:
      B.CreateRetVoid();
      break;
    case ProfilingType::SynchronizationPoint :
      RetVal = B.CreateCmp(CmpInst::ICMP_EQ, Fn->getArg(0), Fn->getArg(1));
      B.CreateRet(RetVal);
      break;
    default:
      break;
    }
  }

}

void createFtFuncs(Module &Md) {
  createFtFunc(Md, "DataCorruption_Handler");
  createFtFunc(Md, "SigMismatch_Handler");
  if (ProfilingEnabled) {
    createProfilingFunc(Md, "aspis.syncpt", ProfilingType::SynchronizationPoint);
    createProfilingFunc(Md, "aspis.cfcpt", ProfilingType::SynchronizationPoint);
    createProfilingFunc(Md, "aspis.datacheck.begin", ProfilingType::ConsistencyCheck);
    createProfilingFunc(Md, "aspis.datacheck.end", ProfilingType::ConsistencyCheck);
  }
}
