#ifndef ERDITS_H
#define ERDITS_H

#include "llvm/IR/Value.h"
#include <map>

using namespace llvm;

// is a map containing the original value and its shadow
std::map<Value*, Value*> ShadowStorageMap;

#endif