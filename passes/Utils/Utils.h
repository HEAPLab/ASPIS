#ifndef UTILS_H
#define UTILS_H

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Support/CommandLine.h>
#include <map>

using namespace llvm;
using LinkageMap = std::unordered_map<std::string, std::vector<StringRef>>;


extern bool AlternateMemMapEnabled;
extern std::string DuplicateSecName;

// Given a Use U, it returns true if the instruction is a PHI instruction
bool IsNotAPHINode (Use &U);

/**
 * TODO This function supports only one annotation for each function, multiple annotations are discarded, perhaps I can fix this lol
 * @param Md The module where to look for the annotations
 * @param FuncAnnotations A map of Function and StringRef where to put the annotations for each Function
 */
void getFuncAnnotations(Module &Md, std::map<Value*, StringRef> &FuncAnnotations);

// Inserts the names of the compiled functions as a csv into the file passed as parameter
void persistCompiledFunctions(std::set<Function*> &CompiledFuncs, const char* filename);

bool shouldCompile(Function &Fn, 
    const std::map<Value*, StringRef> &FuncAnnotations,
    const std::set<Function*> &OriginalFunctions = std::set<Function*>());

DebugLoc findNearestDebugLoc(Instruction &I);

LinkageMap mapFunctionLinkageNames(const Module &M);
void printLinkageMap(const LinkageMap &linkageMap);
StringRef getLinkageName(const LinkageMap &linkageMap, const std::string &functionName);
bool isIntrinsicToDuplicate(CallBase *CInstr);




#endif