#include "Intrinsics.h"
#include "XAI/Passes.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinTypes.h"

using namespace mlir;
using namespace mlir::xai;

#define GEN_PASS_DEF_XAIMATMULTOINTRINSIC
#include "Passes.h.inc"

namespace {

LogicalResult rewriteMatmul(linalg::MatmulOp matmul) {
  auto launch = matmul->getParentOfType<gpu::LaunchOp>();
  if (!launch)
    return matmul.emitOpError("expected to sit inside gpu.launch");

  auto module = matmul->getParentOfType<ModuleOp>();
  if (!module)
    return matmul.emitOpError("expected a parent builtin.module");

  MLIRContext *ctx = matmul.getContext();
  Type f16 = Float16Type::get(ctx);
  Type f32 = Float32Type::get(ctx);
  auto inputs = matmul.getDpsInputs();
  auto outputs = matmul.getDpsInits();
  if (inputs.size() != 2 || outputs.size() != 1)
    return matmul.emitOpError("expected two inputs and one output");
  if (!isXAITileMemref(inputs[0].getType(), f16) ||
      !isXAITileMemref(inputs[1].getType(), f16) ||
      !isXAITileMemref(outputs[0].getType(), f32)) {
    return matmul.emitOpError(
        "only memref<16x16xf16>, memref<16x16xf16>, memref<16x16xf32> "
        "with identity layout is supported");
  }

  declareXAIIntrinsics(module);

  OpBuilder builder(matmul);
  Location loc = matmul.getLoc();
  Value ptrA = createXAITilePointer(builder, loc, inputs[0]);
  Value ptrB = createXAITilePointer(builder, loc, inputs[1]);
  Value ptrC = createXAITilePointer(builder, loc, outputs[0]);
  Value tileA = createXAIMload(builder, loc, f16, ptrA);
  Value tileB = createXAIMload(builder, loc, f16, ptrB);
  Value tileC = createXAIMload(builder, loc, f32, ptrC);
  Value tileD = createXAIMma(builder, loc, tileA, tileB, tileC);
  createXAIMstore(builder, loc, ptrC, tileD);
  matmul.erase();
  return success();
}

struct MatmulToIntrinsicPass
    : public ::impl::XAIMatmulToIntrinsicBase<MatmulToIntrinsicPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    SmallVector<linalg::MatmulOp> matmuls;
    module.walk([&](linalg::MatmulOp op) { matmuls.push_back(op); });
    if (matmuls.size() != 1) {
      module.emitError("expected exactly one linalg.matmul, found ")
          << matmuls.size();
      signalPassFailure();
      return;
    }
    if (failed(rewriteMatmul(matmuls.front())))
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::xai::createMatmulToIntrinsicPass() {
  return std::make_unique<MatmulToIntrinsicPass>();
}
