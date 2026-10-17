#include "XAI/Pipeline.h"
#include "XAI/XAIDialect.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/DLTI/DLTI.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/Index/IR/IndexDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Target/LLVMIR/Dialect/All.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

static llvm::cl::opt<std::string> inputFilename(
    "input", llvm::cl::desc("Input MLIR file"), llvm::cl::value_desc("filename"),
    llvm::cl::Required);

static llvm::cl::opt<std::string> outputFilename(
    "output", llvm::cl::desc("Output file: MLIR, LLVM IR, or host ELF object"),
    llvm::cl::value_desc("filename"));

static llvm::cl::opt<std::string> stopAfter(
    "pipeline-stop-after",
    llvm::cl::desc("Stop after rewrite, outline, binary, llvm, or elf"),
    llvm::cl::init("elf"));

static llvm::cl::opt<bool> dumpIr(
    "dump-ir", llvm::cl::desc("Print IR after each pipeline stage"));

static llvm::cl::opt<std::string> xaiLlc(
    "xai-llc", llvm::cl::desc("llc built with the XAI backend"),
    llvm::cl::init(XAI_LLC_DEFAULT));

static llvm::cl::opt<std::string> hostLlc(
    "host-llc",
    llvm::cl::desc("llc with an X86 backend, used for the host object"));

static llvm::cl::opt<std::string> deviceElf(
    "dump-device-elf",
    llvm::cl::desc("Write the device ELF embedded by gpu-module-to-binary"));

int main(int argc, char **argv) {
  llvm::InitLLVM init(argc, argv);
  llvm::cl::ParseCommandLineOptions(argc, argv, "XAI MLIR offload frontend\n");

  bool stopOk = false;
  xai::StopAfter stop = xai::parseStopAfter(stopAfter, stopOk);
  if (!stopOk) {
    llvm::errs() << "unknown --pipeline-stop-after value: " << stopAfter << "\n";
    return 1;
  }

  DialectRegistry registry;
  registry.insert<arith::ArithDialect, cf::ControlFlowDialect, DLTIDialect,
                  func::FuncDialect, gpu::GPUDialect, index::IndexDialect,
                  LLVM::LLVMDialect, linalg::LinalgDialect, memref::MemRefDialect,
                  xai::XAIDialect>();
  xai::registerXAITargetInterfaceExternalModels(registry);
  registerAllToLLVMIRTranslations(registry);
  gpu::registerOffloadingLLVMTranslationInterfaceExternalModels(registry);

  MLIRContext context(registry);
  context.loadAllAvailableDialects();

  llvm::SourceMgr sourceMgr;
  auto file = llvm::MemoryBuffer::getFile(inputFilename);
  if (!file) {
    llvm::errs() << "cannot read " << inputFilename << "\n";
    return 1;
  }
  sourceMgr.AddNewSourceBuffer(std::move(*file), llvm::SMLoc());
  OwningOpRef<ModuleOp> module = parseSourceFile<ModuleOp>(sourceMgr, &context);
  if (!module) {
    llvm::errs() << "failed to parse " << inputFilename << "\n";
    return 1;
  }

  xai::PipelineOptions options;
  options.stopAfter = stop;
  options.dumpIr = dumpIr;
  options.xaiLlc = xaiLlc;
  options.hostLlc = hostLlc;
  options.outputPath = outputFilename;
  options.deviceElfPath = deviceElf;
  if (failed(xai::runXAIPipeline(*module, options)))
    return 1;
  return 0;
}
