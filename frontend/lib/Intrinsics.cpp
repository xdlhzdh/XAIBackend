#include "Intrinsics.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinTypes.h"

using namespace mlir;

namespace {

VectorType tileVector(Type element) {
  return VectorType::get({xai::kTileSize * xai::kTileSize}, element);
}

StringRef mloadName(Type element) {
  return element.isF16() ? "llvm.xai.mload.f16" : "llvm.xai.mload.f32";
}

StringRef mstoreName(Type element) {
  return element.isF16() ? "llvm.xai.mstore.f16" : "llvm.xai.mstore.f32";
}

Value callIntrinsic(OpBuilder &builder, Location loc, StringRef name,
                    Type result, ValueRange args) {
  SmallVector<Type> argTypes(args.getTypes());
  auto fnType = LLVM::LLVMFunctionType::get(result, argTypes);
  auto call = LLVM::CallOp::create(
      builder, loc, fnType, SymbolRefAttr::get(builder.getContext(), name),
      args);
  return call.getNumResults() == 1 ? call->getResult(0) : Value();
}

} // namespace

bool xai::isXAITileMemref(Type type, Type element) {
  auto memref = dyn_cast<MemRefType>(type);
  if (!memref || !memref.hasStaticShape() || memref.getRank() != 2)
    return false;
  if (memref.getDimSize(0) != kTileSize || memref.getDimSize(1) != kTileSize)
    return false;
  if (memref.getElementType() != element)
    return false;
  return memref.getLayout().isIdentity();
}

void xai::declareXAIIntrinsics(ModuleOp module) {
  MLIRContext *ctx = module.getContext();
  OpBuilder builder(ctx);
  builder.setInsertionPointToStart(module.getBody());
  Type f16 = Float16Type::get(ctx);
  Type f32 = Float32Type::get(ctx);
  Type ptr = LLVM::LLVMPointerType::get(ctx);
  Type voidTy = LLVM::LLVMVoidType::get(ctx);
  auto declare = [&](StringRef name, Type result, ArrayRef<Type> args) {
    if (module.lookupSymbol(name))
      return;
    LLVM::LLVMFuncOp::create(builder, module.getLoc(), name,
                             LLVM::LLVMFunctionType::get(result, args));
  };
  declare(mloadName(f16), tileVector(f16), {ptr});
  declare(mloadName(f32), tileVector(f32), {ptr});
  declare(mstoreName(f16), voidTy, {ptr, tileVector(f16)});
  declare(mstoreName(f32), voidTy, {ptr, tileVector(f32)});
  declare("llvm.xai.mma", tileVector(f32),
          {tileVector(f16), tileVector(f16), tileVector(f32)});
}

Value xai::createXAITilePointer(OpBuilder &builder, Location loc, Value memref) {
  Value index = memref::ExtractAlignedPointerAsIndexOp::create(builder, loc, memref);
  Value i64 = arith::IndexCastOp::create(builder, loc, builder.getI64Type(), index);
  return LLVM::IntToPtrOp::create(builder, loc,
                                  LLVM::LLVMPointerType::get(builder.getContext()),
                                  i64);
}

Value xai::createXAIMload(OpBuilder &builder, Location loc, Type element,
                          Value ptr) {
  return callIntrinsic(builder, loc, mloadName(element), tileVector(element),
                       ptr);
}

void xai::createXAIMstore(OpBuilder &builder, Location loc, Value ptr,
                          Value tile) {
  Type element = cast<VectorType>(tile.getType()).getElementType();
  callIntrinsic(builder, loc, mstoreName(element),
                LLVM::LLVMVoidType::get(builder.getContext()),
                ValueRange{ptr, tile});
}

Value xai::createXAIMma(OpBuilder &builder, Location loc, Value a, Value b,
                        Value c) {
  return callIntrinsic(builder, loc, "llvm.xai.mma", c.getType(),
                       ValueRange{a, b, c});
}
