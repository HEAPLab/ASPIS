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
#include <map>

using namespace llvm;

// Given a Use U, it returns true if the instruction is a PHI instruction
bool IsNotAPHINode (Use &U);

/**
 * TODO This function supports only one annotation for each function, multiple annotations are discarded, perhaps I can fix this lol
 * @param Md The module where to look for the annotations
 * @param FuncAnnotations A map of Function and StringRef where to put the annotations for each Function
 */
void getFuncAnnotations(Module &Md, std::map<Function*, StringRef> &FuncAnnotations);

// Inserts the names of the compiled functions as a csv into the file passed as parameter
void persistCompiledFunctions(std::set<Function*> &CompiledFuncs, const char* filename);

bool shouldCompile(Function &Fn, 
    const std::map<Function*, StringRef> &FuncAnnotations,
    const std::set<Function*> &OriginalFunctions = std::set<Function*>());

#endif