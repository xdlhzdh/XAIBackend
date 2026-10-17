#include "Intrinsics.h"
#include "XAI/Passes.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Utils/StaticValueUtils.h"
#include "mlir/Dialect/Vector/IR/VectorOps.h"
#include "mlir/IR/AffineMap.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

using namespace mlir;
using namespace mlir::xai;

#define GEN_PASS_DEF_XAIVECTORTOINTRINSIC
#include "Passes.h.inc"

namespace {

VectorType flatTile(Type element) {
  return VectorType::get({kTileSize * kTileSize}, element);
}

bool isTileVector(Type type, Type element) {
  auto vector = dyn_cast<VectorType>(type);
  return vector && !vector.isScalable() &&
         vector.getShape() == ArrayRef<int64_t>{kTileSize, kTileSize} &&
         vector.getElementType() == element;
}

bool insideLaunch(Operation *op) {
  return op->getParentOfType<gpu::LaunchOp>() != nullptr;
}

/// Reports why `op` cannot become one XAI tile access, or succeeds.
/// When `emit` is false the reason is discarded, for use inside patterns.
LogicalResult checkTileTransfer(VectorTransferOpInterface op, bool emit) {
  auto fail = [&](const Twine &reason) {
    if (emit)
      op->emitOpError(reason);
    return failure();
  };
  auto memref = dyn_cast<MemRefType>(op.getShapedType());
  if (!memref)
    return fail("XAI tile access needs a memref base");
  Type element = memref.getElementType();
  if (!element.isF16() && !element.isF32())
    return fail("XAI tiles hold f16 or f32 only");
  if (!isXAITileMemref(memref, element))
    return fail("base must be a 16x16 memref with identity layout");
  if (!isTileVector(op.getVectorType(), element))
    return fail("vector must be vector<16x16x" + Twine(element.isF16() ? "f16" : "f32") + ">");
  if (op.getMask() || isa<vector::MaskOp>(op->getParentOp()))
    return fail("masked transfers have no XAI tile instruction");
  if (!op.getPermutationMap().isIdentity())
    return fail("XAI tile access needs an identity permutation map");
  for (Value index : op.getIndices()) {
    if (!isConstantIntValue(index, 0))
      return fail("XAI tile access must start at index 0");
  }
  return success();
}

/// Accepts only C(m,n) += A(m,k) * B(k,n) over 16x16 f16 tiles into f32.
LogicalResult checkMmaContract(vector::ContractionOp op, bool emit) {
  auto fail = [&](const Twine &reason) {
    if (emit)
      op->emitOpError(reason);
    return failure();
  };
  MLIRContext *ctx = op.getContext();
  Type f16 = Float16Type::get(ctx);
  Type f32 = Float32Type::get(ctx);
  if (!isTileVector(op.getLhsType(), f16) || !isTileVector(op.getRhsType(), f16) ||
      !isTileVector(op.getAccType(), f32) ||
      !isTileVector(op.getResultType(), f32)) {
    return fail("XAI mma needs vector<16x16xf16> x vector<16x16xf16> into "
                "vector<16x16xf32>");
  }
  if (op.getKind() != vector::CombiningKind::ADD)
    return fail("XAI mma only accumulates with kind<add>");
  if (isa<vector::MaskOp>(op->getParentOp()))
    return fail("masked contractions have no XAI mma");

  auto iterators = op.getIteratorTypesArray();
  if (iterators.size() != 3 || iterators[0] != vector::IteratorType::parallel ||
      iterators[1] != vector::IteratorType::parallel ||
      iterators[2] != vector::IteratorType::reduction)
    return fail("expected iterator_types [parallel, parallel, reduction]");

  AffineExpr m, n, k;
  bindDims(ctx, m, n, k);
  SmallVector<SmallVector<AffineExpr, 4>> exprs = {{m, k}, {k, n}, {m, n}};
  auto expected = AffineMap::inferFromExprList(exprs, ctx);
  if (op.getIndexingMapsArray() != ArrayRef<AffineMap>(expected))
    return fail("XAI mma needs row-major (m,k) x (k,n) -> (m,n); "
                "transposed operands are not supported");
  return success();
}

struct TransferReadToMload : OpRewritePattern<vector::TransferReadOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(vector::TransferReadOp op,
                                PatternRewriter &rewriter) const override {
    if (!insideLaunch(op) || failed(checkTileTransfer(op, /*emit=*/false)))
      return failure();
    Location loc = op.getLoc();
    Type element = op.getVectorType().getElementType();
    Value ptr = createXAITilePointer(rewriter, loc, op.getBase());
    Value tile = createXAIMload(rewriter, loc, element, ptr);
    rewriter.replaceOpWithNewOp<vector::ShapeCastOp>(op, op.getVectorType(), tile);
    return success();
  }
};

struct TransferWriteToMstore : OpRewritePattern<vector::TransferWriteOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(vector::TransferWriteOp op,
                                PatternRewriter &rewriter) const override {
    if (!insideLaunch(op) || failed(checkTileTransfer(op, /*emit=*/false)))
      return failure();
    Location loc = op.getLoc();
    Type element = op.getVectorType().getElementType();
    Value flat = vector::ShapeCastOp::create(rewriter, loc, flatTile(element),
                                             op.getValueToStore());
    Value ptr = createXAITilePointer(rewriter, loc, op.getBase());
    createXAIMstore(rewriter, loc, ptr, flat);
    rewriter.eraseOp(op);
    return success();
  }
};

struct ContractToMma : OpRewritePattern<vector::ContractionOp> {
  using OpRewritePattern::OpRewritePattern;

  LogicalResult matchAndRewrite(vector::ContractionOp op,
                                PatternRewriter &rewriter) const override {
    if (!insideLaunch(op) || failed(checkMmaContract(op, /*emit=*/false)))
      return failure();
    Location loc = op.getLoc();
    Type f16 = rewriter.getF16Type();
    Type f32 = rewriter.getF32Type();
    Value a = vector::ShapeCastOp::create(rewriter, loc, flatTile(f16), op.getLhs());
    Value b = vector::ShapeCastOp::create(rewriter, loc, flatTile(f16), op.getRhs());
    Value c = vector::ShapeCastOp::create(rewriter, loc, flatTile(f32), op.getAcc());
    Value d = createXAIMma(rewriter, loc, a, b, c);
    rewriter.replaceOpWithNewOp<vector::ShapeCastOp>(op, op.getResultType(), d);
    return success();
  }
};

struct VectorToIntrinsicPass
    : public ::impl::XAIVectorToIntrinsicBase<VectorToIntrinsicPass> {
  void runOnOperation() override {
    ModuleOp module = getOperation();
    MLIRContext *ctx = &getContext();

    // Diagnose unsupported forms up front so the IR is not left half rewritten.
    bool invalid = false;
    int contracts = 0;
    module.walk([&](Operation *op) {
      if (!insideLaunch(op))
        return;
      if (auto contract = dyn_cast<vector::ContractionOp>(op)) {
        ++contracts;
        invalid |= failed(checkMmaContract(contract, /*emit=*/true));
      } else if (auto transfer = dyn_cast<VectorTransferOpInterface>(op)) {
        invalid |= failed(checkTileTransfer(transfer, /*emit=*/true));
      }
    });
    if (invalid)
      return signalPassFailure();
    if (contracts == 0) {
      module.emitError("expected a vector.contract inside gpu.launch");
      return signalPassFailure();
    }

    declareXAIIntrinsics(module);

    RewritePatternSet patterns(ctx);
    patterns.add<TransferReadToMload, TransferWriteToMstore, ContractToMma>(ctx);
    vector::ShapeCastOp::getCanonicalizationPatterns(patterns, ctx);
    if (failed(applyPatternsGreedily(module, std::move(patterns)))) {
      module.emitError("vector to llvm.xai rewrite did not converge");
      return signalPassFailure();
    }

    // The XAI target only understands the pointer chain and llvm.xai calls,
    // so every vector op inside the launch has to be gone by now.
    module.walk([&](Operation *op) {
      if (insideLaunch(op) && isa<vector::VectorDialect>(op->getDialect())) {
        op->emitOpError("has no llvm.xai lowering; tile values may only flow "
                        "between transfer_read, contract, and transfer_write");
        invalid = true;
      }
    });
    if (invalid)
      signalPassFailure();
  }
};

} // namespace

std::unique_ptr<Pass> mlir::xai::createVectorToIntrinsicPass() {
  return std::make_unique<VectorToIntrinsicPass>();
}
