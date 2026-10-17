#ifndef XAI_PASSES_H
#define XAI_PASSES_H

#include "mlir/Pass/Pass.h"

namespace mlir::xai {

#define GEN_PASS_DECL
#include "Passes.h.inc"

std::unique_ptr<Pass> createMatmulToIntrinsicPass();
std::unique_ptr<Pass> createOutlineAndAttachPass();

} // namespace mlir::xai

#endif // XAI_PASSES_H
