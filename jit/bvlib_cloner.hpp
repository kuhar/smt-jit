#include "llvm/IR/Module.h"

#include <memory>

namespace smt_jit {

std::unique_ptr<llvm::Module> CloneDeclarationsAndPrepare(llvm::Module &M);

std::unique_ptr<llvm::Module> CloneBVLibTemplate(const llvm::Module &M);

} // namespace smt_jit