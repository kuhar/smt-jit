#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"

#include "bvlib/bvlib.h"
#include "sexpresso.hpp"

#include <cstdio>

using namespace llvm;

static cl::list<std::string>
    InputFilenames(cl::Positional, cl::OneOrMore,
                   cl::desc("<input smtlib2 files>"));

class SmtJit {
private:
  orc::ExecutionSession ES;
  orc::RTDyldObjectLinkingLayer ObjectLayer;
  orc::IRCompileLayer CompileLayer;

  DataLayout DL;
  orc::MangleAndInterner Mangle;
  orc::ThreadSafeContext Ctx;

public:
  SmtJit(orc::JITTargetMachineBuilder JTMB, DataLayout DL)
      : ObjectLayer(ES,
                    []() { return llvm::make_unique<SectionMemoryManager>(); }),
        CompileLayer(ES, ObjectLayer,
                     orc::ConcurrentIRCompiler(std::move(JTMB))),
        DL(std::move(DL)), Mangle(ES, this->DL),
        Ctx(llvm::make_unique<LLVMContext>()) {
    ES.getMainJITDylib().setGenerator(
        cantFail(orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(DL)));
  }

  static Expected<std::unique_ptr<SmtJit>> Create() {
    auto JTMB = orc::JITTargetMachineBuilder::detectHost();

    if (!JTMB)
      return JTMB.takeError();

    auto errTM = JTMB->createTargetMachine();
    if (!errTM) {
      llvm::errs() << "Could not create a target machine\n";
      return errTM.takeError();
    }

    std::unique_ptr<TargetMachine> TM = std::move(errTM.get());

    auto DL = TM->createDataLayout();
    return llvm::make_unique<SmtJit>(std::move(*JTMB), DL);
  }

  const DataLayout &getDataLayout() const { return DL; }

  LLVMContext &getContext() { return *Ctx.getContext(); }

  Error addModule(std::unique_ptr<Module> M) {
    return CompileLayer.add(ES.getMainJITDylib(),
                            orc::ThreadSafeModule(std::move(M), Ctx));
  }

  Expected<JITEvaluatedSymbol> lookup(StringRef Name) {
    return ES.lookup({&ES.getMainJITDylib()}, Mangle(Name.str()));
  }
};

void dummyFun() {}

int main(int argc, char **argv) {
  llvm::llvm_shutdown_obj shutdown;
  llvm::cl::ParseCommandLineOptions(argc, argv, "SMT JIT");

  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram PSTP(argc, argv);

  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();

  const std::string exePath =
      llvm::sys::fs::getMainExecutable(argv[0], (void *)&dummyFun);
  const llvm::StringRef exePathRef = exePath;
  const std::string exeDir = exePathRef.substr(0, exePathRef.rfind('/')).str();

  llvm::errs() << "Running in directory: " << exeDir << "\n";

  auto errJit = SmtJit::Create();
  if (!errJit) {
    llvm::errs() << "Could not create smt-jit: " << errJit.takeError() << "\n";
    return 2;
  }

  std::unique_ptr<SmtJit> jit = std::move(errJit.get());

  SMDiagnostic error;
  const std::string bvlibBitcodePath = exeDir + "/bvlib/bvlib.ll";
  std::unique_ptr<Module> m =
      parseIRFile(bvlibBitcodePath, error, jit->getContext());
  if (!m) {
    llvm::errs() << "Could not load bitcode module: " << bvlibBitcodePath
                 << "\nError: \n";
    error.print(argv[0], llvm::errs());
    return 1;
  }

  auto errAddModule = jit->addModule(std::move(m));
  if (errAddModule) {
    llvm::errs() << "Could not load module: " << errAddModule << "\n";
    return 2;
  }

  auto errLookup = jit->lookup("bv_print");
  if (!errLookup) {
    llvm::errs() << "Lookup failed: " << errLookup.takeError() << "\n";
    return 2;
  }

  llvm::outs() << "[Startup-sanity-check] About to print bv_mk(8, 42):\n\t";
  auto *bv_printPtr = (void (*)(bitvector))errLookup->getAddress();
  llvm::outs() << "\n";
  llvm::outs().flush();

  const bitvector test_bv = bv_mk(8, 42);
  bv_printPtr(test_bv);

  llvm::outs() << "\nInput files: \n";
  for (const std::string &filename : InputFilenames) {
    llvm::outs() << "\t" << filename << "\n";

    if (!llvm::sys::fs::exists(filename)) {
      llvm::outs().flush();
      llvm::errs() << "\nFile " << filename << " does not exits!\n";
      return 1;
    }
  }

  return 0;
}
