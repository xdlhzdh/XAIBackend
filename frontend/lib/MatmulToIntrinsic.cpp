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

constexpr int64_t kTile = 16;

bool isIdentityTile(Type type, Type element) {
  auto memref = dyn_cast<MemRefType>(type);
  if (!memref || !memref.hasStaticShape() || memref.getRank() != 2)
    return false;
  if (memref.getDimSize(0) != kTile || memref.getDimSize(1) != kTile)
    return false;
  if (memref.getElementType() != element)
    return false;
  return memref.getLayout().isIdentity();
}

LLVM::LLVMFuncOp declareIntrinsic(ModuleOp module, StringRef name,
                                  LLVM::LLVMFunctionType type) {
  if (auto existing = module.lookupSymbol<LLVM::LLVMFuncOp>(name))
    return existing;
  OpBuilder builder(module.getContext());
  builder.setInsertionPointToStart(module.getBody());
  return builder.create<LLVM::LLVMFuncOp>(module.getLoc(), name, type);
}

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
  if (!isIdentityTile(inputs[0].getType(), f16) ||
      !isIdentityTile(inputs[1].getType(), f16) ||
      !isIdentityTile(outputs[0].getType(), f32)) {
    return matmul.emitOpError(
        "only memref<16x16xf16>, memref<16x16xf16>, memref<16x16xf32> "
        "with identity layout is supported");
  }

  auto ptrTy = LLVM::LLVMPointerType::get(ctx);
  auto tileF16 = VectorType::get({256}, f16);
  auto tileF32 = VectorType::get({256}, f32);
  auto voidTy = LLVM::LLVMVoidType::get(ctx);
  auto mloadF16 = LLVM::LLVMFunctionType::get(tileF16, ptrTy);
  auto mloadF32 = LLVM::LLVMFunctionType::get(tileF32, ptrTy);
  auto mma = LLVM::LLVMFunctionType::get(tileF32, {tileF16, tileF16, tileF32});
  auto mstoreF32 = LLVM::LLVMFunctionType::get(voidTy, {ptrTy, tileF32});
  declareIntrinsic(module, "llvm.xai.mload.f16", mloadF16);
  declareIntrinsic(module, "llvm.xai.mload.f32", mloadF32);
  declareIntrinsic(module, "llvm.xai.mma", mma);
  declareIntrinsic(module, "llvm.xai.mstore.f32", mstoreF32);

  OpBuilder builder(matmul);
  Location loc = matmul.getLoc();
  auto asPtr = [&](Value memref) -> Value {
    Value idx = builder.create<memref::ExtractAlignedPointerAsIndexOp>(loc, memref);
    Value i64 = builder.create<arith::IndexCastOp>(loc, builder.getI64Type(), idx);
    return builder.create<LLVM::IntToPtrOp>(loc, ptrTy, i64);
  };

  Value ptrA = asPtr(inputs[0]);
  Value ptrB = asPtr(inputs[1]);
  Value ptrC = asPtr(outputs[0]);
  Value tileA = builder
                    .create<LLVM::CallOp>(
                        loc, mloadF16, SymbolRefAttr::get(ctx, "llvm.xai.mload.f16"),
                        ValueRange{ptrA})
                    ->getResult(0);
  Value tileB = builder
                    .create<LLVM::CallOp>(
                        loc, mloadF16, SymbolRefAttr::get(ctx, "llvm.xai.mload.f16"),
                        ValueRange{ptrB})
                    ->getResult(0);
  Value tileC = builder
                    .create<LLVM::CallOp>(
                        loc, mloadF32, SymbolRefAttr::get(ctx, "llvm.xai.mload.f32"),
                        ValueRange{ptrC})
                    ->getResult(0);
  Value tileD = builder
                    .create<LLVM::CallOp>(loc, mma,
                                          SymbolRefAttr::get(ctx, "llvm.xai.mma"),
                                          ValueRange{tileA, tileB, tileC})
                    ->getResult(0);
  builder.create<LLVM::CallOp>(loc, mstoreF32,
                               SymbolRefAttr::get(ctx, "llvm.xai.mstore.f32"),
                               ValueRange{ptrC, tileD});
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
