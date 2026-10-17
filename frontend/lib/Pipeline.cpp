#include "XAI/Pipeline.h"
#include "XAI/Passes.h"
#include "XAI/XAIDialect.h"

#include "mlir/Conversion/ArithToLLVM/ArithToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h"
#include "mlir/Conversion/IndexToLLVM/IndexToLLVM.h"
#include "mlir/Conversion/LLVMCommon/ConversionTarget.h"
#include "mlir/Conversion/LLVMCommon/Pattern.h"
#include "mlir/Conversion/LLVMCommon/TypeConverter.h"
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"
#include "mlir/Conversion/ReconcileUnrealizedCasts/ReconcileUnrealizedCasts.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/GPU/Transforms/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Target/LLVMIR/Dialect/GPU/GPUToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Export.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;
using namespace mlir::xai;

namespace {

bool launchIsLowered(gpu::LaunchFuncOp op) {
  auto isHostScalar = [](Type type) {
    return type.isInteger(32) || type.isInteger(64);
  };
  if (!isHostScalar(op.getGridSizeX().getType()) ||
      !isHostScalar(op.getGridSizeY().getType()) ||
      !isHostScalar(op.getGridSizeZ().getType()) ||
      !isHostScalar(op.getBlockSizeX().getType()) ||
      !isHostScalar(op.getBlockSizeY().getType()) ||
      !isHostScalar(op.getBlockSizeZ().getType()))
    return false;
  for (Value operand : op.getKernelOperands()) {
    if (isa<MemRefType, IndexType>(operand.getType()))
      return false;
  }
  return true;
}

struct RewriteLaunchFunc : public ConvertOpToLLVMPattern<gpu::LaunchFuncOp> {
  using ConvertOpToLLVMPattern<gpu::LaunchFuncOp>::ConvertOpToLLVMPattern;

  LogicalResult
  matchAndRewrite(gpu::LaunchFuncOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override {
    if (adaptor.getOperands().size() != op->getNumOperands())
      return op.emitError("host lowering changed the launch operand count");
    Operation *cloned = rewriter.clone(*op.getOperation());
    cloned->setOperands(adaptor.getOperands());
    rewriter.replaceOp(op, cloned->getResults());
    return success();
  }
};

LogicalResult lowerHostForOffload(ModuleOp module) {
  MLIRContext *ctx = module.getContext();
  LowerToLLVMOptions options(ctx);
  LLVMTypeConverter converter(ctx, options);
  RewritePatternSet patterns(ctx);
  populateFuncToLLVMConversionPatterns(converter, patterns);
  populateFinalizeMemRefToLLVMConversionPatterns(converter, patterns);
  arith::populateArithToLLVMConversionPatterns(converter, patterns);
  index::populateIndexToLLVMConversionPatterns(converter, patterns);
  patterns.add<RewriteLaunchFunc>(converter);

  LLVMConversionTarget target(*ctx);
  target.addLegalOp<gpu::BinaryOp>();
  target.addDynamicallyLegalOp<gpu::LaunchFuncOp>(launchIsLowered);
  if (failed(applyPartialConversion(module, target, std::move(patterns))))
    return module.emitError("failed to lower host code to the LLVM dialect");

  PassManager pm(ctx);
  pm.addPass(createReconcileUnrealizedCastsPass());
  if (failed(pm.run(module)))
    return module.emitError("unrealized casts remained after host lowering");
  return success();
}

LogicalResult runHostLlc(StringRef llc, StringRef llPath, StringRef objPath) {
  std::string err;
  SmallString<128> errFile;
  if (llvm::sys::fs::createTemporaryFile("xai-host-llc", "err", errFile))
    return failure();
  std::optional<StringRef> redirects[3] = {std::nullopt, std::nullopt,
                                           StringRef(errFile)};
  SmallVector<StringRef> args = {llc,
                                 "-mtriple=x86_64-unknown-linux-gnu",
                                 "-filetype=obj",
                                 "-relocation-model=pic",
                                 llPath,
                                 "-o",
                                 objPath};
  int rc = llvm::sys::ExecuteAndWait(llc, args, std::nullopt, redirects, 0, 0, &err);
  std::string llcErr;
  if (auto buf = llvm::MemoryBuffer::getFile(errFile))
    llcErr = buf.get()->getBuffer().str();
  llvm::sys::fs::remove(errFile);
  if (rc != 0) {
    llvm::errs() << "host llc failed: " << err << "\n" << llcErr << "\n";
    return failure();
  }
  return success();
}

std::string resolveHostLlc(StringRef requested) {
  if (!requested.empty())
    return requested.str();
  if (llvm::sys::fs::can_execute("/usr/local/bin/llc"))
    return "/usr/local/bin/llc";
  auto found = llvm::sys::findProgramByName("llc");
  if (found)
    return *found;
  return {};
}

LogicalResult writeText(StringRef path, StringRef text) {
  std::error_code ec;
  llvm::raw_fd_ostream os(path, ec);
  if (ec) {
    llvm::errs() << "cannot write " << path << ": " << ec.message() << "\n";
    return failure();
  }
  os << text;
  return success();
}

LogicalResult finishTextual(ModuleOp module, const PipelineOptions &options) {
  std::string text;
  llvm::raw_string_ostream os(text);
  module.print(os);
  os.flush();
  llvm::outs() << text << "\n";
  if (!options.outputPath.empty())
    return writeText(options.outputPath, text);
  return success();
}

void dumpModule(StringRef label, ModuleOp module) {
  llvm::errs() << "\n// -----// IR Dump After Stage: " << label
               << " //----- //\n";
  module.print(llvm::errs());
  llvm::errs() << "\n";
}

LogicalResult dumpDeviceElf(ModuleOp module, StringRef path) {
  for (gpu::BinaryOp binary : module.getOps<gpu::BinaryOp>()) {
    if (binary.getObjects().empty())
      continue;
    auto object = dyn_cast<gpu::ObjectAttr>(binary.getObjects()[0]);
    if (!object)
      continue;
    StringRef bytes = object.getObject().getValue();
    std::error_code ec;
    llvm::raw_fd_ostream os(path, ec, llvm::sys::fs::OF_None);
    if (ec)
      return module.emitError("cannot write device ELF");
    os.write(bytes.data(), bytes.size());
    return success();
  }
  return module.emitError("gpu.binary has no object to dump");
}

bool useXAIHandler(ModuleOp module) {
  bool changed = false;
  for (gpu::BinaryOp binary : module.getOps<gpu::BinaryOp>()) {
    if (binary.getObjects().empty())
      continue;
    auto object = dyn_cast<gpu::ObjectAttr>(binary.getObjects()[0]);
    if (!object || !isa<TargetAttr>(object.getTarget()))
      continue;
    binary.setOffloadingHandlerAttr(object.getTarget());
    changed = true;
  }
  return changed;
}

FailureOr<std::unique_ptr<llvm::Module>>
translateHost(ModuleOp module, llvm::LLVMContext &llvmCtx) {
  auto translated = translateModuleToLLVMIR(module, llvmCtx, "xai-host");
  if (translated)
    return translated;
  if (!useXAIHandler(module))
    return failure();
  llvm::errs() << "note: #gpu.select_object translation failed; "
                  "retrying with #xai.target offloading handler\n";
  translated = translateModuleToLLVMIR(module, llvmCtx, "xai-host");
  if (!translated)
    return failure();
  return translated;
}

LogicalResult emitHost(ModuleOp module, const PipelineOptions &options,
                       bool writeObject) {
  if (failed(lowerHostForOffload(module)))
    return failure();
  if (options.dumpIr)
    dumpModule("llvm-dialect", module);

  llvm::LLVMContext llvmCtx;
  auto translatedOr = translateHost(module, llvmCtx);
  if (failed(translatedOr))
    return module.emitError("offloading translate to LLVM IR failed");
  std::unique_ptr<llvm::Module> llvmModule = std::move(*translatedOr);

  std::string ir;
  llvm::raw_string_ostream irStream(ir);
  llvmModule->print(irStream, nullptr);
  irStream.flush();

  if (options.stopAfter == StopAfter::Llvm || options.dumpIr) {
    llvm::outs() << ir;
    if (!options.outputPath.empty() && options.stopAfter == StopAfter::Llvm)
      return writeText(options.outputPath, ir);
    if (options.stopAfter == StopAfter::Llvm)
      return success();
  }

  if (!writeObject)
    return success();
  if (options.outputPath.empty())
    return module.emitError("ELF output requires --output");

  std::string hostLlc = resolveHostLlc(options.hostLlc);
  if (hostLlc.empty() || !llvm::sys::fs::can_execute(hostLlc))
    return module.emitError(
        "host llc with the X86 backend was not found; pass --host-llc");

  SmallString<128> llPath;
  if (llvm::sys::fs::createTemporaryFile("xai-host", "ll", llPath))
    return module.emitError("cannot create a temporary host LLVM IR file");
  if (failed(writeText(llPath, ir))) {
    llvm::sys::fs::remove(llPath);
    return failure();
  }
  LogicalResult llcResult = runHostLlc(hostLlc, llPath, options.outputPath);
  llvm::sys::fs::remove(llPath);
  return llcResult;
}

} // namespace

StopAfter mlir::xai::parseStopAfter(StringRef name, bool &ok) {
  ok = true;
  if (name == "rewrite")
    return StopAfter::Rewrite;
  if (name == "outline")
    return StopAfter::Outline;
  if (name == "binary")
    return StopAfter::Binary;
  if (name == "llvm")
    return StopAfter::Llvm;
  if (name == "elf")
    return StopAfter::Elf;
  ok = false;
  return StopAfter::Elf;
}

LogicalResult mlir::xai::runXAIPipeline(ModuleOp module,
                                       const PipelineOptions &options) {
  setXAIDeviceLlcPath(options.xaiLlc);
  MLIRContext *ctx = module.getContext();

  auto runStage = [&](StringRef label, function_ref<void(OpPassManager &)> build) {
    PassManager pm(ctx);
    if (options.dumpIr) {
      module.getContext()->disableMultithreading();
      OpPrintingFlags flags;
      pm.enableIRPrinting([](Pass *, Operation *) { return false; },
                          [](Pass *, Operation *) { return true; }, true, false,
                          false, llvm::errs(), flags);
    }
    build(pm);
    if (failed(pm.run(module)))
      return failure();
    if (options.dumpIr)
      dumpModule(label, module);
    return success();
  };

  bool hasLinalgMatmul = false;
  module.walk([&](linalg::MatmulOp) {
    hasLinalgMatmul = true;
    return WalkResult::interrupt();
  });
  if (failed(runStage("rewrite", [&](OpPassManager &pm) {
        if (hasLinalgMatmul)
          pm.addPass(createMatmulToIntrinsicPass());
        else
          pm.addPass(createVectorToIntrinsicPass());
      })))
    return failure();
  if (options.stopAfter == StopAfter::Rewrite)
    return finishTextual(module, options);

  if (failed(runStage("outline", [](OpPassManager &pm) {
        pm.addPass(createOutlineAndAttachPass());
      })))
    return failure();
  if (options.stopAfter == StopAfter::Outline)
    return finishTextual(module, options);

  GpuModuleToBinaryPassOptions binaryOptions;
  binaryOptions.compilationTarget = "bin";
  if (failed(runStage("binary", [&](OpPassManager &pm) {
        pm.addPass(createGpuModuleToBinaryPass(binaryOptions));
      })))
    return failure();
  if (!options.deviceElfPath.empty() &&
      failed(dumpDeviceElf(module, options.deviceElfPath)))
    return failure();
  if (options.stopAfter == StopAfter::Binary)
    return finishTextual(module, options);

  return emitHost(module, options, options.stopAfter == StopAfter::Elf);
}
