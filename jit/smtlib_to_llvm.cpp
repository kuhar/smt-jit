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

#include "smtlib_parser.hpp"

#include <algorithm>
#include <cassert>
#include <memory>

using namespace llvm;
using namespace sexpresso;

namespace smt_jit {
namespace {
class Smt2LLVM {
  LLVMContext &m_ctx;
  Module &m_module;
  SmtLibParser &m_parser;

  std::unique_ptr<IRBuilder<>> m_builder = nullptr;

  StructType *m_bitvectorTy = nullptr;
  StructType *m_bvaTy = nullptr;
  PointerType *m_bvaPtrTy = nullptr;

  IntegerType *m_i32Ty = nullptr;
  IntegerType *m_i64Ty = nullptr;
  PointerType *m_i64PtrTy = nullptr;

  ConstantInt *m_i32Zero = nullptr;
  ConstantInt *m_i32One = nullptr;

  Function *m_bvPrintFn = nullptr;
  Function *m_bvaPrintFn = nullptr;

  Function *m_bvMkFn = nullptr;
  Function *m_bvaSelectFn = nullptr;

public:
  Smt2LLVM(SmtLibParser &parser, llvm::Module &M);

  void emitFormula(Twine funName);

private:
  Function *lowerAssert(unsigned idx);
  Value *lowerCmp(Value *lhs, Value *rhs);
  Value *lowerSelect(Value *array, Value *index);
};
} // namespace

std::string emitSmtFormula(smt_jit::SmtLibParser &parser, llvm::Module &M) {
  static unsigned cnt = 0;
  std::string num = std::to_string(cnt++);
  Twine name("smt_", num);

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

  m_i32Zero = ConstantInt::get(m_i32Ty, 0, false);
  assert(m_i32Zero);
  m_i32One = ConstantInt::get(m_i32Ty, 1, false);
  assert(m_i32One);

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
  Function *assertion = lowerAssert(0);
  assertion->setName(funName);
  assertion->dump();
}

Function *Smt2LLVM::lowerAssert(unsigned idx) {
  assert(idx < m_parser.numAssertions());
  Sexp &assertion = m_parser.assertions()[idx];
  std::string name = "assert_" + std::to_string(idx);

  auto *funcTy = FunctionType::get(m_i32Ty, {m_bvaPtrTy}, false);
  Function *func =
      Function::Create(funcTy, GlobalValue::ExternalLinkage, name, m_module);

  BasicBlock *entry = BasicBlock::Create(m_ctx, "entry", func);
  m_builder = llvm::make_unique<IRBuilder<>>(entry);

  m_builder->CreateRet(m_i32Zero);
  m_builder.release();

  return func;
}

Value *Smt2LLVM::lowerCmp(Value *lhs, Value *rhs) {
  assert(lhs);
  assert(rhs);
  assert(m_builder);

  Value *cmp = m_builder->CreateICmpEQ(lhs, rhs);
  Value *zext = m_builder->CreateZExt(cmp, m_i32Ty);
  return zext;
}

Value *Smt2LLVM::lowerSelect(Value *array, Value *index) { return nullptr; }

} // namespace
} // namespace smt_jit
