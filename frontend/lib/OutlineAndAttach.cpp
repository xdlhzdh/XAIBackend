#include "XAI/Passes.h"
#include "XAI/XAIDialect.h"

#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/GPU/Transforms/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/PassManager.h"

using namespace mlir;
using namespace mlir::xai;

#define GEN_PASS_DEF_XAIOUTLINEANDATTACH
#include "Passes.h.inc"

namespace {

void moveIntrinsicDecls(ModuleOp module, gpu::GPUModuleOp gpuModule) {
  OpBuilder builder(gpuModule.getContext());
  builder.setInsertionPointToStart(&gpuModule.getBodyRegion().front());
  SmallVector<LLVM::LLVMFuncOp> decls;
  for (auto func : module.getOps<LLVM::LLVMFuncOp>()) {
    if (func.getName().starts_with("llvm.xai."))
      decls.push_back(func);
  }
  for (LLVM::LLVMFuncOp func : decls) {
    if (!gpuModule.lookupSymbol(func.getName()))
      builder.clone(*func.getOperation());
    func.erase();
  }
}

struct OutlineAndAttachPass
    : public ::impl::XAIOutlineAndAttachBase<OutlineAndAttachPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    PassManager nested(&getContext());
    // Outlining moves llvm.call into gpu.module before the declarations move
    // with them. Verify after both steps, not between them.
    nested.enableVerifier(false);
    nested.addPass(createGpuKernelOutliningPass());
    if (failed(nested.run(module))) {
      signalPassFailure();
      return;
    }

    auto gpuModules = llvm::to_vector(module.getOps<gpu::GPUModuleOp>());
    if (gpuModules.size() != 1) {
      module.emitError("expected exactly one gpu.module after outlining, found ")
          << gpuModules.size();
      signalPassFailure();
      return;
    }
    gpu::GPUModuleOp gpuModule = gpuModules.front();
    moveIntrinsicDecls(module, gpuModule);

    MLIRContext *ctx = &getContext();
    auto target = TargetAttr::get(ctx, "generic");
    gpuModule.setTargetsAttr(ArrayAttr::get(ctx, {target}));
    gpuModule.setOffloadingHandlerAttr(
        gpu::SelectObjectAttr::get(ctx, Attribute()));

    if (!module->hasAttr(gpu::GPUDialect::getContainerModuleAttrName())) {
      module->setAttr(gpu::GPUDialect::getContainerModuleAttrName(),
                      UnitAttr::get(ctx));
    }
  }
};

} // namespace

std::unique_ptr<Pass> mlir::xai::createOutlineAndAttachPass() {
  return std::make_unique<OutlineAndAttachPass>();
}
