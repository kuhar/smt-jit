#include "smtlib_to_llvm.hpp"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cassert>

using namespace llvm;

namespace smt_jit {
namespace {
class Smt2LLVM {
  LLVMContext &m_ctx;
  Module &m_module;
  SmtLibParser &m_parser;

  StructType *m_bitvectorTy = nullptr;
  StructType *m_bvaTy = nullptr;
  PointerType *m_bvaPtrTy = nullptr;

  IntegerType *m_i32Ty = nullptr;
  IntegerType *m_i64Ty = nullptr;
  PointerType *m_i64PtrTy = nullptr;

  Function *m_bvPrintFn = nullptr;
  Function *m_bvaPrintFn = nullptr;

  Function *m_bvMkFn = nullptr;
  Function *m_bvaSelectFn = nullptr;

public:
  Smt2LLVM(SmtLibParser &parser, llvm::Module &M);

  void emitFormula(Twine funName);
};
} // namespace

std::string emitSmtFormula(smt_jit::SmtLibParser &parser, llvm::Module &M) {
  static unsigned cnt = 0;
  Twine name("smt_", std::to_string(cnt++));

  Smt2LLVM smt2llvm(parser, M);
  smt2llvm.emitFormula(name);

  return name.str();
}

namespace {
Smt2LLVM::Smt2LLVM(SmtLibParser &parser, llvm::Module &M)
    : m_ctx(M.getContext()), m_module(M), m_parser(parser) {
  m_bitvectorTy = m_module.getTypeByName("struct.bitvector_t");
  assert(m_bitvectorTy);

  m_bvaTy = m_module.getTypeByName("struct.bv_array_t");
  assert(m_bvaTy);
  m_bvaPtrTy = m_bvaTy->getPointerTo(0);

  m_i32Ty = Type::getInt32Ty(m_ctx);
  assert(m_i32Ty);
  m_i64Ty = Type::getInt64Ty(m_ctx);
  assert(m_i64Ty);
  m_i64PtrTy = m_i64Ty->getPointerTo(0);

  m_bvPrintFn = m_module.getFunction("bv_print");
  assert(m_bvPrintFn);
  m_bvaPrintFn = m_module.getFunction("bva_print");
  assert(m_bvaPrintFn);

  m_bvMkFn = m_module.getFunction("bv_mk");
  assert(m_bvMkFn);

  m_bvaSelectFn = m_module.getFunction("bva_select");
  assert(m_bvaSelectFn);
}

void Smt2LLVM::emitFormula(Twine funName) {
  Type *returnTy = Type::getInt32Ty(m_ctx);
  Type *inputArrayTy = Type::getInt64PtrTy(m_ctx);
  Type *inputWidthTy = Type::getInt32Ty(m_ctx);
  auto *funcTy =
      FunctionType::get(returnTy, {inputWidthTy, inputArrayTy}, false);
  Function *func =
      Function::Create(funcTy, GlobalValue::ExternalLinkage, funName, m_module);
  BasicBlock *entry = BasicBlock::Create(m_ctx, "entry", func);

  IRBuilder<> builder(entry);
  Value *i64One = ConstantInt::get(Type::getInt64Ty(m_ctx), 1, false);
  Instruction *callInst = builder.CreateCall(m_bvPrintFn, {i64One, i64One});
  Instruction *ret = builder.CreateRet(ConstantInt::get(returnTy, 34, false));

  func->dump();
}

} // namespace
} // namespace smt_jit
