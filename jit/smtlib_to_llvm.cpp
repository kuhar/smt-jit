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

#define DEBUG_TYPE "smt2llvm"

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
  StructType *m_i64PairTy = nullptr;
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
  Function *m_bvEqFn = nullptr;

  Function *m_bvaSelectFn = nullptr;

public:
  Smt2LLVM(SmtLibParser &parser, llvm::Module &M);

  void emitFormula(Twine funName);

private:
  std::pair<Function *, StringMap<Argument *>>
      emitFunctionOverBVArrays(Twine name);

  Function *lowerAssert(unsigned idx, Twine name);
  Value *lowerIntegerConstant(long long val);
  std::pair<Value *, Value *> unpackI64Pair(Value *valPair);
  Value *lowerBVMk(Value *constant, Value *width, Twine name = "bv");
  Value *lowerCmp(Value *lhs, Value *rhs, Twine name = "cmp");
  Value *lowerEq(Value *lhs, Value *rhs, Twine name = "eq");
  Value *lowerSelect(Value *array, Value *index, Twine name = "select");
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

  m_i64PairTy = StructType::get(m_ctx, {m_i64Ty, m_i64Ty}, false);

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
  assert(m_bvMkFn->getReturnType() == m_i64PairTy);

  m_bvEqFn = m_module.getFunction("bv_eq");
  assert(m_bvEqFn);
  assert(m_bvEqFn->getReturnType() == m_i32Ty);
  assert(m_bvEqFn->arg_size() == 4);

  m_bvaSelectFn = m_module.getFunction("bva_select");
  assert(m_bvaSelectFn);
}

void Smt2LLVM::emitFormula(Twine funName) {
  auto *funcTy = FunctionType::get(m_i32Ty, m_bvaPtrTy->getPointerTo(0), false);

  Function *func =
      Function::Create(funcTy, GlobalValue::ExternalLinkage, funName, m_module);
  Argument *arrPack = &*func->arg_begin();
  arrPack->setName("arrays");

  BasicBlock::Create(m_ctx, "entry", func);
  IRBuilder<> formulaBuilder(&func->front());

  const size_t numArrays = m_parser.numArrays();
  SmallVector<Value *, 4> args;
  args.reserve(numArrays);
  {
    size_t i = 0;
    for (const ArrayInfo &ai : m_parser.arrays()) {
      Value *arr = formulaBuilder.CreateInBoundsGEP(
          arrPack, ConstantInt::get(m_i64Ty, i), {ai.name, ".ptr"});
      Value *arg = formulaBuilder.CreateLoad(arr, ai.name);
      args.push_back(arg);
      ++i;
    }
  }

  const size_t numAssertions = m_parser.numAssertions();
  for (size_t i = 0; i != numAssertions; ++i) {
    const std::string caseName = std::to_string(i + 1);
    Function *assertFn = lowerAssert(i, funName + "_assert");

    LLVM_DEBUG(assertFn->dump());

    Value *res =
        formulaBuilder.CreateCall(assertFn, args, Twine{"assert.", caseName});
    Value *failureRes = formulaBuilder.CreateICmpEQ(
        res, m_i32One, {res->getName(), ".success"});

    auto *blockSuccess = BasicBlock::Create(m_ctx, "cont", func);
    auto *blockFail =
        BasicBlock::Create(m_ctx, {"fail_", caseName}, func, blockSuccess);
    formulaBuilder.CreateCondBr(failureRes, blockSuccess, blockFail);

    formulaBuilder.SetInsertPoint(blockFail);
    formulaBuilder.CreateRet(ConstantInt::get(m_i32Ty, i + 1, false));

    formulaBuilder.SetInsertPoint(blockSuccess);
  }

  formulaBuilder.CreateRet(m_i32Zero);

  LLVM_DEBUG(func->dump());
}

bool IsIntegerConstant(StringRef val) {
  return !val.empty() && std::all_of(val.begin(), val.end(), ::isdigit);
}

std::pair<Function *, StringMap<Argument *>>
Smt2LLVM::emitFunctionOverBVArrays(Twine name) {
  const size_t numArrays = m_parser.numArrays();
  SmallVector<Type *, 4> arrayTyInputs(numArrays);
  for (auto &x : arrayTyInputs)
    x = m_bvaPtrTy;

  auto *funcTy = FunctionType::get(m_i32Ty, arrayTyInputs, false);
  Function *func =
      Function::Create(funcTy, GlobalValue::ExternalLinkage, name, m_module);

  BasicBlock::Create(m_ctx, "entry", func);

  assert(m_parser.numArrays() == func->arg_size());
  auto argIt = func->arg_begin();
  const auto argEnd = func->arg_end();

  StringMap<Argument *> arrayToArg;
  for (const ArrayInfo &ai : m_parser.arrays()) {
    assert(argIt != argEnd);
    argIt->setName(ai.name);
    arrayToArg[ai.name] = &*argIt;
    ++argIt;
  }

  return {func, std::move(arrayToArg)};
}

Function *Smt2LLVM::lowerAssert(unsigned idx, Twine name) {
  assert(idx < m_parser.numAssertions());
  Sexp &assertion = m_parser.assertions()[idx];

  auto funcArraysPair =
      emitFunctionOverBVArrays(name + "_" + std::to_string(idx + 1));
  Function *func = funcArraysPair.first;
  StringMap<Argument *> &arrayToArg = funcArraysPair.second;

  m_builder = llvm::make_unique<IRBuilder<>>(&func->front());

  SmallVector<Value *, 4> valueStack;
  auto stackPush = [&valueStack](Value *val) { valueStack.push_back(val); };
  auto stackPop = [&valueStack] {
    assert(!valueStack.empty());
    return valueStack.pop_back_val();
  };

  LLVM_DEBUG(for (SexpPostOrderView view
                  : SexpPostOrderRange(assertion)) llvm::errs()
             << view << "\n");

  StringMap<Value *> letToVal;

  for (SexpPostOrderView view : SexpPostOrderRange(assertion)) {
    LLVM_DEBUG(llvm::errs() << view << "\n");

    assert(view.sexp);
    assert(view.sexp->isString());
    StringRef str = view.sexp->getString();

    LLVM_DEBUG(errs() << "str: " << str << "\n");

    assert(view.parent);
    Sexp &parent = *view.parent;

    if (!view.isHead()) {
      if (view.sexp->isNumber()) {
        const auto num = view.sexp->toNumber();
        stackPush(lowerIntegerConstant(num));
        continue;
      }

      if (str == "false") {
        stackPush(m_i32Zero);
        continue;
      }

      if (str.startswith("bv")) {
        StringRef remainder = str.substr(2);
        assert(IsIntegerConstant(remainder));
        const long long num = std::stoll(remainder.str());
        stackPush(lowerIntegerConstant(num));
        continue;
      }

      if (letToVal.count(str)) {
        stackPush(letToVal[str]);
        continue;
      }

      if (arrayToArg.count(str) > 0) {
        stackPush(arrayToArg[str]);
        continue;
      }

      if (letToVal.count(str) > 0) {
        stackPush(letToVal[str]);
        continue;
      }

      llvm::errs() << "Operand \"" << str << "\" not handled!\n";
      llvm_unreachable("Unknown symbol");
    } else {
      if (str == "_") {
        Value *width = stackPop();
        Value *constant = stackPop();
        stackPush(lowerBVMk(constant, width, parent.getChild(1).getString()));
        continue;
      }

      // Let definition.
      if (str.startswith(R"(\?)")) {
        letToVal[str] = stackPop();
        continue;
      }

      if (str == "let") {
        // The value of let is it's second operand. Leave it on the stack.
        // Same as: stackPush(stackPop());
        continue;
      }

      if (str == "select") {
        Value *index = stackPop();
        Value *arr = stackPop();
        stackPush(lowerSelect(arr, index));
        continue;
      }

      if (str == "=") {
        Value *rhs = stackPop();
        Value *lhs = stackPop();
        stackPush(lowerCmp(lhs, rhs));
        continue;
      }

      if (str == "assert") {
        Value *res = stackPop();
        m_builder->CreateRet(res);
        break;
      }

      llvm::errs() << "Head \"" << str << "\" not handled!\n";
      llvm_unreachable("Symbol not handled");
    }
  }

  m_builder = nullptr;

  return func;
}

Value *Smt2LLVM::lowerIntegerConstant(long long val) {
  return ConstantInt::get(m_i64Ty, val, val < 0);
}

std::pair<Value *, Value *> Smt2LLVM::unpackI64Pair(Value *valPair) {
  assert(valPair->getType() == m_i64PairTy);
  Twine prefix = valPair->hasName() ? valPair->getName() : "";
  Value *first = m_builder->CreateExtractValue(valPair, {0}, prefix + ".fi");
  Value *second = m_builder->CreateExtractValue(valPair, {1}, prefix + ".se");

  return {first, second};
}

Value *Smt2LLVM::lowerBVMk(Value *constant, Value *width, Twine name) {
  Value *c = m_builder->CreateSExtOrTrunc(constant, m_i64Ty);
  Value *w = m_builder->CreateZExtOrTrunc(width, m_i32Ty);
  return m_builder->CreateCall(m_bvMkFn, {w, c}, name);
}

Value *Smt2LLVM::lowerCmp(Value *lhs, Value *rhs, Twine name) {
  assert(lhs->getType() == rhs->getType());

  if (!lhs->getType()->isIntegerTy())
    return lowerEq(lhs, rhs);

  Value *cmp = m_builder->CreateICmpEQ(lhs, rhs, name);
  return m_builder->CreateZExt(cmp, m_i32Ty);
}

Value *Smt2LLVM::lowerEq(Value *lhs, Value *rhs, Twine name) {
  assert(lhs->getType() == m_i64PairTy);
  assert(rhs->getType() == m_i64PairTy);
  auto lhsUnpacked = unpackI64Pair(lhs);
  auto rhsUnpacked = unpackI64Pair(rhs);
  return m_builder->CreateCall(m_bvEqFn,
                               {lhsUnpacked.first, lhsUnpacked.second,
                                rhsUnpacked.first, rhsUnpacked.second},
                               name);
}

Value *Smt2LLVM::lowerSelect(Value *array, Value *index, Twine name) {
  assert(array->getType() == m_bvaPtrTy);
  assert(index->getType() == m_i64PairTy);
  auto firstSecond = unpackI64Pair(index);
  return m_builder->CreateCall(
      m_bvaSelectFn, {array, firstSecond.first, firstSecond.second}, name);
}

} // namespace
} // namespace smt_jit
