#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "llvm/IRReader/IRReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/CallGraphSCCPass.h"

#include "llvm/Transforms/IPO/AlwaysInliner.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include "bvlib_cloner.hpp"

#include "bvlib/bvlib.h"
#include "sexpresso.hpp"
#include "smtlib_parser.hpp"
#include "smtlib_to_llvm.hpp"

#include <cstdio>
#include <chrono>

#define DEBUG_TYPE "smt-jit"

using namespace llvm;

static cl::list<std::string> InputFilenames(cl::Positional, cl::OneOrMore,
                                            cl::desc("<input smtlib2 files>"));

static llvm::cl::opt<bool>
    NoOpt("no-opt",
          llvm::cl::desc("[smt-jit] Don't run the optimization pipeline"),
          llvm::cl::init(false));

static llvm::cl::opt<bool>
    SaveTemps("save-temps", llvm::cl::desc("Save intermediate IR modules"),
              llvm::cl::init(false));

static llvm::cl::opt<std::string>
    TempDir("temp-dir", llvm::cl::desc("Temporary file directory"),
            llvm::cl::init("."), llvm::cl::value_desc("filename"));

static llvm::cl::opt<bool>
    BenchmarkMode("benchmark",
                  llvm::cl::desc("[smt-jit] Run in benchmarking mode"),
                  llvm::cl::init(false));

static llvm::cl::opt<unsigned> BenchmarkIterations(
    "iterations",
    llvm::cl::desc("[smt-jit] Number of iterations (for benchmarking)"),
    llvm::cl::init(10000));

static std::string LastTempModulePath;

class SmtJit {
private:
  orc::ExecutionSession ES;
  orc::RTDyldObjectLinkingLayer ObjectLayer;
  orc::IRCompileLayer CompileLayer;
  orc::IRTransformLayer OptimizeLayer;

  DataLayout DL;
  orc::MangleAndInterner Mangle;
  orc::ThreadSafeContext Ctx;

public:
  SmtJit(orc::JITTargetMachineBuilder JTMB, DataLayout DL)
      : ObjectLayer(ES,
                    []() { return llvm::make_unique<SectionMemoryManager>(); }),
        CompileLayer(ES, ObjectLayer,
                     orc::ConcurrentIRCompiler(std::move(JTMB))),
        OptimizeLayer(ES, CompileLayer, optimizeModule), DL(std::move(DL)),
        Mangle(ES, this->DL), Ctx(llvm::make_unique<LLVMContext>()) {
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
    return OptimizeLayer.add(ES.getMainJITDylib(),
                             orc::ThreadSafeModule(std::move(M), Ctx));
  }

  Expected<JITEvaluatedSymbol> lookup(StringRef Name) {
    return ES.lookup({&ES.getMainJITDylib()}, Mangle(Name.str()));
  }

private:
  static Expected<orc::ThreadSafeModule>
  optimizeModule(orc::ThreadSafeModule TSM,
                 const orc::MaterializationResponsibility &R) {
    if (NoOpt)
      return TSM;

    legacy::PassManager PM;
    PM.add(createAlwaysInlinerLegacyPass());
    PM.run(*TSM.getModule());

    if (SaveTemps)
      smt_jit::SaveIRToFile(*TSM.getModule(), {LastTempModulePath, ".inl.ll"});

    legacy::FunctionPassManager FPM(TSM.getModule());
    FPM.add(createInstructionCombiningPass());
    FPM.add(createGVNPass());
    FPM.add(createCFGSimplificationPass());
    FPM.doInitialization();

    for (auto &F : *TSM.getModule())
      if (F.getName().startswith("smt_"))
        FPM.run(F);

    if (SaveTemps)
      smt_jit::SaveIRToFile(*TSM.getModule(), {LastTempModulePath, ".opt.ll"});

    return TSM;
  }
};

void dummyFun() {}

static bool doBVLibSanityCheck(SmtJit &jit);

static int parseSmtAndEval(StringRef filename, SmtJit &jit,
                            const llvm::Module &bvLibTemplate);

static bool models(smt_jit::SmtLibParser &parser, unsigned assignmentIdx,
                   int (*smtFunctionPtr)(bv_array **), bool verbose = false);

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

  std::unique_ptr<Module> bvlibDeclsTemplate =
      smt_jit::CloneDeclarationsAndPrepare(*m);
  if (!bvlibDeclsTemplate) {
    llvm::errs() << "Failed to create module template for jitted formulas!\n";
    return 2;
  }

  auto errAddModule = jit->addModule(std::move(m));
  if (errAddModule) {
    llvm::errs() << "Could not load module: " << errAddModule << "\n";
    return 2;
  }

  for (const std::string &filename : InputFilenames) {
    if (!llvm::sys::fs::exists(filename)) {
      llvm::errs() << "File " << filename << " does not exits\n";
      continue;
    }

    const int res = parseSmtAndEval(filename, *jit, *bvlibDeclsTemplate);
    llvm::outs().flush();

    if (res != 0) {
      llvm::errs() << "Execution error, the jit will terminate\n";
      return res;
    }
  }

  return 0;
}

int parseSmtAndEval(StringRef filename, SmtJit &jit,
                    const llvm::Module &bvLibTemplate) {
  llvm::outs() << "Evaluating: " << filename << "\n";
  const StringRef tempBasename = llvm::sys::path::filename(filename);
  const Twine tempDest = TempDir + "/" + tempBasename;

  smt_jit::SmtLibParser parser(filename);

  std::unique_ptr<llvm::Module> freshModule =
      smt_jit::CloneBVLibTemplate(bvLibTemplate);
  assert(freshModule);

  std::string smtFunctionName = emitSmtFormula(parser, *freshModule);
  if (SaveTemps) {
    LastTempModulePath = tempDest.str();
    smt_jit::SaveIRToFile(*freshModule, tempDest + ".ll");
  }

  auto addSmtModule = jit.addModule(std::move(freshModule));
  if (addSmtModule) {
    llvm::errs() << "Could not add a new smt module: " << addSmtModule << "\n";
    return 2;
  }

  LLVM_DEBUG(if (!doBVLibSanityCheck(jit)) {
    llvm::errs() << "Sanity check failed, aborting.\n";
    return 2;
  });

  auto lookupFunctionOrNone =
      [&jit](StringRef name) -> llvm::Optional<JITTargetAddress> {
    auto errLookup = jit.lookup(name);
    if (!errLookup) {
      llvm::errs() << "Lookup of the function " << name
                   << " failed: " << errLookup.takeError() << "\n";
      return llvm::None;
    }

    return errLookup->getAddress();
  };

  auto smtFnAddr = lookupFunctionOrNone(smtFunctionName);
  if (!smtFnAddr.hasValue())
    return 2;

  auto *smtFunctionPtr = (int (*)(bv_array **))smtFnAddr.getValue();
  llvm::outs().flush();

  bv_init_context();
  if (!BenchmarkMode) {
    llvm::outs() << "Formula modeled by assignments: ";
    for (size_t assignmentIdx = 0, e = parser.numAssignments();
         assignmentIdx != e; ++assignmentIdx) {
      const bool res = models(parser, assignmentIdx, smtFunctionPtr, false);
      if (res)
        llvm::outs() << assignmentIdx << ", ";
    }
    llvm::outs() << "\n";
  } else {
    using namespace std::chrono;
    const auto startTime = steady_clock::now();

    size_t totalModels = 0;
    for (unsigned iter = 0, e = BenchmarkIterations; iter != e; ++iter) {
      for (size_t assignmentIdx = 0, e = parser.numAssignments();
           assignmentIdx != e; ++assignmentIdx)
        totalModels += models(parser, assignmentIdx, smtFunctionPtr, false);

      bv_reset_context();
    }

    const auto endTime = steady_clock::now();
    const auto ms = duration_cast<milliseconds>(endTime - startTime);

    llvm::outs() << "Total models: " << totalModels << " / "
                 << BenchmarkIterations << " iterations\n";
    llvm::outs() << "[BENCHMARK] Time " << ms.count() << " ms\n";
  }
  bv_reset_context();

  return 0;
}

bool models(smt_jit::SmtLibParser &parser, unsigned assignmentIdx,
            int (*smtFunctionPtr)(bv_array **), bool verbose /* = false */) {
  smt_jit::Assignment &assignment = parser.assignments()[assignmentIdx];
  const size_t numArrays = parser.numArrays();

  if (verbose)
    llvm::outs() << "Assignment " << assignmentIdx << ": ";

  if (assignment.numVariables() != numArrays) {
    if (verbose)
      llvm::outs() << "wrong number of variables (" << assignment.numVariables()
                   << " vs. " << numArrays << ")\n";
    return false;
  }

  SmallVector<bv_array *, 2> varToArray;

  for (const smt_jit::ArrayInfo &ai : parser.arrays()) {
    using AssignmentVector = smt_jit::Assignment::AssignmentVector;

    if (!assignment.hasVariable(ai.name)) {
      if (verbose)
        llvm::outs() << "partial assignment, " << ai.name << " missing\n";
      return false;
    }

    AssignmentVector &arr = assignment.getValue(ai.name);
    bv_array *bv_arr = bva_mk_init(ai.element_width, arr.size(), arr.data());
    varToArray.push_back(bv_arr);
  }

  assert(varToArray.size() == numArrays);

  int res = smtFunctionPtr(varToArray.data());
  if (res == 0) {
    if (verbose)
      llvm::outs() << "models\n";
    return true;
  }

  if (verbose)
    llvm::outs() << "assertion " << res << " failed\n";

  return false;
}

bool doBVLibSanityCheck(SmtJit &jit) {
  auto errLookup = jit.lookup("bv_print");
  if (!errLookup) {
    llvm::errs() << "Lookup failed: " << errLookup.takeError() << "\n";
    return false;
  }

  static bv_word cnt = 0;
  ++cnt;

  llvm::outs() << "[Startup-sanity-check] About to print bv_mk(8, " << cnt
               << "):\n\t";
  auto *bv_printPtr = (void (*)(bitvector))errLookup->getAddress();
  llvm::outs().flush();

  const bitvector test_bv = bv_mk(8, cnt);
  bv_printPtr(test_bv);
  puts("");
  return true;
}
