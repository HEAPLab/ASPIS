/**
 * ************************************************************************************************
 * @brief  LLVM ModulePass that lets Rust front-end use ASPIS annotations.
 *
 * Converts any global symbol marked with `#[unsafe(link_section =
 * "aspis_<annotation>")]` to the corresponding ASPIS
 * `__attribute__((annotate("<annotation>")))`. The section may be a
 * comma-separated list that also contains a regular section (e.g.
 * `".data,aspis_to_harden"`), which is kept on the symbol. ASPIS annotations
 * are mutually exclusive, so a symbol with more than one is rejected.
 *
 * ************************************************************************************************
 */
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalObject.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/ErrorHandling.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "aspis-rust-annotation-bridge"

class RustAnnotationBridge : public PassInfoMixin<RustAnnotationBridge> {
public:
  PreservedAnalyses run(Module &Md, ModuleAnalysisManager &) {
    std::vector<std::pair<GlobalObject *, std::string>> ToAnnotate;

    for (GlobalVariable &GV : Md.globals()) {
      if (std::optional<StringRef> Annotation = extractASPISAnnotation(GV)) {
        ToAnnotate.emplace_back(&GV, Annotation->str());
      }
    }

    for (Function &Fn : Md) {
      if (std::optional<StringRef> Annotation = extractASPISAnnotation(Fn)) {
        ToAnnotate.emplace_back(&Fn, Annotation->str());
      }
    }

    // If no ASPIS annotations were found, do not touch anything else
    if (ToAnnotate.empty()) {
      return PreservedAnalyses::all();
    }

    SetVector<GlobalObject *> Annotated;
    for (auto &[GO, Annotation] : ToAnnotate) {
      Annotated.insert(GO);
    }

    for (GlobalObject *GO : findRustStartupGlue(Md)) {
      LLVM_DEBUG(dbgs() << "Found Rust startup glue: " << GO->getName()
                        << "\n");
      if (auto *Fn = dyn_cast<Function>(GO); Fn && Annotated.insert(Fn)) {
        ToAnnotate.emplace_back(Fn, "exclude");
      }
    }

    // Generic code from Rust's standard library (e.g.
    // `core::fmt::Arguments::new`) is monomorphized into the crate. Exclude it,
    // as if it were an external library like the C++ standard library.
    for (Function &Fn : Md) {
      if (!Fn.isDeclaration() && isRustStdLibFunction(Fn) &&
          Annotated.insert(&Fn)) {
        LLVM_DEBUG(dbgs() << "Found Rust standard library function: "
                          << Fn.getName() << "\n");
        ToAnnotate.emplace_back(&Fn, "exclude");
      }
    }

    addAnnotations(Md, ToAnnotate);

    return PreservedAnalyses::none();
  }

  static bool isRequired() { return true; }

private:
  static constexpr StringRef SectionPrefix = "aspis_";

  std::optional<StringRef> getASPISAnnotation(StringRef Entry) {
    if (Entry == "aspis_to_harden") {
      return {"to_harden"};
    } else if (Entry == "aspis_to_duplicate") {
      return {"to_duplicate"};
    } else if (Entry == "aspis_exclude") {
      return {"exclude"};
    } else {
      return std::nullopt;
    }
  }

  // The section may hold a comma-separated list containing both ASPIS markers
  // and regular sections, e.g. ".data, aspis_to_harden". The ASPIS marker is
  // removed and returned as the annotation; the remaining entries are kept as
  // the symbol's section. At most one ASPIS marker is allowed per symbol.
  std::optional<StringRef> extractASPISAnnotation(GlobalObject &GO) {
    if (!GO.hasSection()) {
      return std::nullopt;
    }

    SmallVector<StringRef, 4> Entries;
    GO.getSection().split(Entries, ',');

    std::optional<StringRef> Found;
    SmallVector<StringRef, 4> Remaining;
    for (StringRef Entry : Entries) {
      Entry = Entry.trim();
      if (std::optional<StringRef> Annotation = getASPISAnnotation(Entry)) {
        if (Found) {
          report_fatal_error("Symbol '" + GO.getName() +
                             "' has multiple ASPIS annotations in section '" +
                             GO.getSection() + "', only one is allowed");
        }
        Found = Annotation;
      } else {
        Remaining.push_back(Entry);
      }
    }

    if (Found) {
      GO.setSection(join(Remaining, ","));
    }
    return Found;
  }

  // Whether Fn's demangled path starts with one of the standard library
  // crates.
  //
  // A trait impl (`<Type as Trait>::method`) is matched when the trait is from
  // the standard library and the type is either a standard library type or a
  // primitive one, e.g. `<alloc::boxed::Box<T> as core::ops::drop::Drop>`.
  // Impls for user types (`<mycrate::MyType as core::fmt::Display>`) are not.
  bool isRustStdLibFunction(const Function &Fn) {
    std::string Name = demangleRust(Fn.getName());
    if (isStdLibPath(Name)) {
      return true;
    }

    std::optional<std::pair<StringRef, StringRef>> Impl = splitTraitImpl(Name);
    if (!Impl) {
      return false;
    }
    auto [Type, Trait] = *Impl;
    if (isPrimitiveType(Type) && isOperatorTrait(Trait)) {
      return false;
    }
    return isStdLibPath(Trait) && (isStdLibPath(Type) || isPrimitiveType(Type));
  }

  // Whether Trait is one of the arithmetic or bitwise operator traits, e.g.
  // `core::ops::arith::AddAssign`. Their impls for primitive types are the
  // built-in operators, which in C/C++ are plain instructions rather than
  // library calls, so they must be hardened like user code: excluding them
  // would make e.g. `x += 1` update only the original copy of `x`.
  bool isOperatorTrait(StringRef Trait) {
    return Trait.starts_with("core::ops::arith::") ||
           Trait.starts_with("core::ops::bit::");
  }

  bool isStdLibPath(StringRef Path) {
    for (StringRef Crate : {"core::", "alloc::", "std::"}) {
      if (Path.starts_with(Crate)) {
        return true;
      }
    }
    return false;
  }

  // Whether Type is a primitive type, a reference or pointer to one, or a
  // slice, array or tuple: everything that does not start with a path.
  bool isPrimitiveType(StringRef Type) {
    for (StringRef Prefix : {"&mut ", "&", "*const ", "*mut "}) {
      if (Type.consume_front(Prefix)) {
        break;
      }
    }
    return !Type.take_until([](char C) { return C == '<'; }).contains("::");
  }

  // Demangles a Rust symbol, also decoding the escapes left by the legacy
  // mangling scheme (e.g. `_$LT$alloc..boxed..Box$LT$T$GT$` becomes
  // `<alloc::boxed::Box<T>`).
  //
  // The alternative to decoding the escapes "by hand" is compiling with
  // `-C symbol-mangling-version=v0`, which uses a different simpler scheme.
  std::string demangleRust(StringRef mangledName) {
    std::string name = demangle(mangledName.str());
    if (name.find('$') == std::string::npos) {
      return name;
    }

    if (StringRef(name).starts_with("_$")) {
      name.erase(0, 1);
    }
    static const std::pair<StringRef, StringRef> Escapes[] = {
        {"$LT$", "<"},  {"$GT$", ">"},  {"$C$", ","},   {"$RF$", "&"},
        {"$BP$", "*"},  {"$LP$", "("},  {"$RP$", ")"},  {"$SP$", "@"},
        {"$u20$", " "}, {"$u27$", "'"}, {"$u3b$", ";"}, {"$u5b$", "["},
        {"$u5d$", "]"}, {"$u7b$", "{"}, {"$u7d$", "}"}, {"$u7e$", "~"},
        {"..", "::"}};
    for (auto [From, To] : Escapes) {
      for (size_t Pos = name.find(From); Pos != std::string::npos;
           Pos = name.find(From, Pos + To.size())) {
        name.replace(Pos, From.size(), To.str());
      }
    }
    return name;
  }

  // Splits a demangled `<Type as Trait>::method` name into Type and Trait.
  std::optional<std::pair<StringRef, StringRef>>
  splitTraitImpl(StringRef name) {
    if (!name.starts_with("<")) {
      return std::nullopt;
    }

    int depth = 0;
    std::optional<size_t> As;
    for (size_t i = 0; i < name.size(); i++) {
      if (name[i] == '<') {
        depth++;
      } else if (name[i] == '>' && (i == 0 || name[i - 1] != '-')) {
        depth--;
        if (depth == 0) {
          if (!As) {
            return std::nullopt;
          }
          return std::make_pair(name.slice(1, *As), name.slice(*As + 4, i));
        }
      } else if (depth == 1 && !As && name.substr(i).starts_with(" as ")) {
        As = i;
      }
    }
    return std::nullopt;
  }

  // Whether every use of V, looking through constant expressions and
  // initializers, lies in Glue: in an instruction of one of its functions, or
  // in the initializer of one of its globals.
  bool isOnlyUsedBy(const Value *V, const SetVector<GlobalObject *> &Glue) {
    for (const User *U : V->users()) {
      if (const auto *I = dyn_cast<Instruction>(U)) {
        if (!Glue.contains(const_cast<Function *>(I->getFunction()))) {
          return false;
        }
      } else if (const auto *GO = dyn_cast<GlobalObject>(U)) {
        if (!Glue.contains(const_cast<GlobalObject *>(GO))) {
          return false;
        }
      } else if (!isa<Constant>(U) || !isOnlyUsedBy(U, Glue)) {
        return false;
      }
    }
    return true;
  }

  // Collects the startup code rustc emits for a `fn main()` using std: a C
  // `main` that calls `std::rt::lang_start(...)`, plus every non-exported
  // function or global that is only reachable from that code. The user's main
  // is never included.
  SetVector<GlobalObject *> findRustStartupGlue(Module &Md) {
    SetVector<GlobalObject *> Glue;

    Function *CMain = Md.getFunction("main");
    if (CMain == nullptr || CMain->isDeclaration()) {
      return Glue;
    }

    Function *UserMain = nullptr;
    for (Instruction &I : instructions(*CMain)) {
      auto *Call = dyn_cast<CallBase>(&I);
      Function *Callee = Call ? Call->getCalledFunction() : nullptr;
      if (Callee == nullptr || Callee->isDeclaration() ||
          !Callee->getName().contains("lang_start") || Call->arg_empty()) {
        continue;
      }
      UserMain =
          dyn_cast<Function>(Call->getArgOperand(0)->stripPointerCasts());
      if (UserMain != nullptr) {
        Glue.insert(CMain);
        Glue.insert(Callee);
        break;
      }
    }

    if (UserMain == nullptr) {
      return Glue;
    }

    bool Changed = true;
    while (Changed) {
      Changed = false;
      for (GlobalObject &GO : Md.global_objects()) {
        if (&GO == UserMain || Glue.contains(&GO) || GO.isDeclaration() ||
            !(GO.hasLocalLinkage() || GO.hasHiddenVisibility()) ||
            GO.use_empty()) {
          continue;
        }
        if (isOnlyUsedBy(&GO, Glue)) {
          Glue.insert(&GO);
          Changed = true;
        }
      }
    }

    return Glue;
  }

  void addAnnotations(
      Module &Md,
      const std::vector<std::pair<GlobalObject *, std::string>> &ToAnnotate) {
    LLVMContext &Ctx = Md.getContext();
    auto *PtrTy = PointerType::getUnqual(Ctx);
    auto *EntryTy = StructType::get(Ctx, {PtrTy, PtrTy});
    std::vector<Constant *> Entries;

    // Preserve any annotations clang already emitted
    if (GlobalVariable *Existing =
            Md.getGlobalVariable("llvm.global.annotations")) {
      if (auto *CA = dyn_cast<ConstantArray>(Existing->getInitializer())) {
        for (Value *Op : CA->operands()) {
          Entries.push_back(cast<Constant>(Op));
        }
      }
      Existing->eraseFromParent();
    }

    for (auto &[GV, Annotation] : ToAnnotate) {
      Constant *Str =
          ConstantDataArray::getString(Ctx, Annotation, /*AddNull=*/true);
      auto *StrGV = new GlobalVariable(Md, Str->getType(), /*isConstant=*/true,
                                       GlobalValue::PrivateLinkage, Str,
                                       GV->getName() + ".aspis_annotation");
      StrGV->setSection("llvm.metadata");
      StrGV->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);

      Entries.push_back(ConstantStruct::get(EntryTy, {GV, StrGV}));
    }

    auto *ArrTy = ArrayType::get(EntryTy, Entries.size());
    auto *NewGlobal = new GlobalVariable(
        Md, ArrTy, /*isConstant=*/false, GlobalValue::AppendingLinkage,
        ConstantArray::get(ArrTy, Entries), "llvm.global.annotations");
    NewGlobal->setSection("llvm.metadata");
  }
};

llvm::PassPluginLibraryInfo getRustAnnotationBridgePluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "aspis-rust-annotation-bridge",
          LLVM_VERSION_STRING, [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "aspis-rust-annotation-bridge") {
                    MPM.addPass(RustAnnotationBridge());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getRustAnnotationBridgePluginInfo();
}
