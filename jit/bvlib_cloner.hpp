#include "llvm/ADT/Twine.h"
#include "llvm/IR/Module.h"

#include <memory>

namespace smt_jit {

std::unique_ptr<llvm::Module> CloneDeclarationsAndPrepare(llvm::Module &M);

std::unique_ptr<llvm::Module> CloneBVLibTemplate(const llvm::Module &M);

void SaveIRToFile(const llvm::Module &M, llvm::Twine path);

} // namespace smt_jit
