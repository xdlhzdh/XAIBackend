#ifndef XAI_LIB_INTRINSICS_H
#define XAI_LIB_INTRINSICS_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"

namespace mlir::xai {

/// Edge length of one XAI matrix register tile.
constexpr int64_t kTileSize = 16;

/// True for a static 16x16 memref with identity layout and `element` type.
bool isXAITileMemref(Type type, Type element);

/// Declares every `llvm.xai.*` intrinsic at the start of `module`.
void declareXAIIntrinsics(ModuleOp module);

/// Returns the aligned base pointer of `memref` as `!llvm.ptr`, using the
/// extract_aligned_pointer_as_index -> index_cast -> inttoptr chain that the
/// XAI target maps back to a kernel argument.
Value createXAITilePointer(OpBuilder &builder, Location loc, Value memref);

/// `llvm.xai.mload.f16` / `llvm.xai.mload.f32`, selected by `element`.
Value createXAIMload(OpBuilder &builder, Location loc, Type element, Value ptr);

/// `llvm.xai.mstore.f16` / `llvm.xai.mstore.f32`, selected by the tile type.
void createXAIMstore(OpBuilder &builder, Location loc, Value ptr, Value tile);

/// `llvm.xai.mma`: 256xf16 x 256xf16 + 256xf32 -> 256xf32.
Value createXAIMma(OpBuilder &builder, Location loc, Value a, Value b, Value c);

} // namespace mlir::xai

#endif // XAI_LIB_INTRINSICS_H
