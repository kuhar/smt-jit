#include "bvlib_cloner.hpp"

#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"

#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>

using namespace llvm;

namespace smt_jit {

std::unique_ptr<llvm::Module> CloneDeclarationsAndPrepare(llvm::Module &M) {
  for (auto &func : M) {
    if (func.isDeclaration())
      continue;

    if ((func.getInstructionCount() <= 28 &&
         !func.getName().contains("context")) ||
        func.getName() == "bv_mk")
      func.addFnAttr(Attribute::AlwaysInline);

    func.setLinkage(GlobalValue::LinkageTypes::ExternalLinkage);
    if (func.getUnnamedAddr() == GlobalValue::UnnamedAddr::Local)
      func.setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  }

  std::unique_ptr<Module> origCloned = llvm::CloneModule(M);
  assert(origCloned);

  Module &cloned = *origCloned;

  for (auto &func : cloned) {
    if (func.isDeclaration())
      continue;
		
    func.setLinkage(GlobalValue::LinkageTypes::WeakAnyLinkage);
    if (!func.hasFnAttribute(Attribute::AlwaysInline))
      func.deleteBody();
  }

  for (auto it = cloned.global_begin(), e = cloned.global_end(); it != e;) {
    GlobalVariable &global = *it;
    ++it;

    if (!global.hasExternalLinkage())
      global.eraseFromParent();
  }

  return std::move(origCloned);
}

std::unique_ptr<Module> CloneBVLibTemplate(const Module &M) {
  return llvm::CloneModule(M);
}

void SaveIRToFile(const llvm::Module &M, Twine path) {
  // FIXME: That's most likely not the best way to save .ll to a file...
  llvm::errs() << "Saving temp: " << path << "\n";
  std::ofstream file(path.str());

  std::string buff;
  llvm::raw_string_ostream os(buff);
  M.print(os, nullptr, false, false);
  os.flush();

  file << buff;
}

} // namespace smt_jit
