#ifndef XAI_PASSES_TD
#define XAI_PASSES_TD

include "mlir/Pass/PassBase.td"

def XAIMatmulToIntrinsic : Pass<"xai-matmul-to-intrinsic", "mlir::ModuleOp"> {
  let summary = "Rewrite one 16x16 linalg.matmul into llvm.xai calls";
  let description = [{
    Matches a single `linalg.matmul` inside `gpu.launch` with shapes
    `memref<16x16xf16>`, `memref<16x16xf16>`, `memref<16x16xf32>` and replaces
    it with `llvm.xai.mload.f16`, `llvm.xai.mload.f32`, `llvm.xai.mma`, and
    `llvm.xai.mstore.f32`.
  }];
  let constructor = "mlir::xai::createMatmulToIntrinsicPass()";
  let dependentDialects = [
    "mlir::LLVM::LLVMDialect",
    "mlir::arith::ArithDialect",
    "mlir::memref::MemRefDialect",
  ];
}

def XAIOutlineAndAttach : Pass<"xai-outline-and-attach", "mlir::ModuleOp"> {
  let summary = "Outline gpu.launch and attach #xai.target";
  let description = [{
    Runs `gpu-kernel-outlining`, moves the `llvm.xai.*` declarations into the
    `gpu.module`, and sets `#xai.target` plus `#gpu.select_object`.
  }];
  let constructor = "mlir::xai::createOutlineAndAttachPass()";
  let dependentDialects = [
    "mlir::gpu::GPUDialect",
    "mlir::LLVM::LLVMDialect",
    "mlir::xai::XAIDialect",
  ];
}

#endif // XAI_PASSES_TD
