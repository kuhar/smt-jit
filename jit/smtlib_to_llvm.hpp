#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"

#include <string>

namespace smt_jit {
class SmtLibParser;

std::string emitSmtFormula(SmtLibParser &parser, llvm::Module &M);
} // namespace smt_jit
