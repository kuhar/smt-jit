#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/DataLayout.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Verifier.h"

using namespace llvm;

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

int main(int argc, char **argv) {
  llvm::llvm_shutdown_obj shutdown;
  llvm::cl::ParseCommandLineOptions(argc, argv, "SMT JIT");
                                    
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram PSTP(argc, argv);

  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();

  llvm::errs() << "Elo!\n";

  auto errJit = SmtJit::Create();
  if (!errJit) {
    llvm::errs() << "Could not create smt-jit: " << errJit.takeError() << "\n";
    std::abort();
  }

  std::unique_ptr<SmtJit> jit = std::move(errJit.get());

  SMDiagnostic error;
  std::unique_ptr<Module> m = parseIRFile("test.ll", error,
                                          jit.get()->getContext());

  auto errAddModule = jit->addModule(std::move(m));
  if (errAddModule) {
    llvm::errs() << "Could not load module: " << errAddModule << "\n";
    std::abort();
  }

  auto errLookup = jit->lookup("foo");
  if (!errLookup) {
    llvm::errs() << "Lookup failed: " << errLookup.takeError() << "\n";
    std::abort();
  }

  auto *foo = (void (*)(void)) errLookup->getAddress();
  foo();

  return 0;
}