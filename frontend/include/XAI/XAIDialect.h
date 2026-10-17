#ifndef XAI_DIALECT_H
#define XAI_DIALECT_H

#include "mlir/Dialect/GPU/IR/CompilationInterfaces.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Dialect.h"
#include "llvm/ADT/StringRef.h"

#include "XAIDialect.h.inc"

#define GET_ATTRDEF_CLASSES
#include "XAIAttrs.h.inc"

namespace mlir {
class DialectRegistry;
class MLIRContext;
} // namespace mlir

namespace mlir::xai {

void registerXAITargetInterfaceExternalModels(DialectRegistry &registry);
void registerXAITargetInterfaceExternalModels(MLIRContext &context);

/// Path of `llc` built with `-DLLVM_TARGETS_TO_BUILD=XAI`.
void setXAIDeviceLlcPath(llvm::StringRef path);

} // namespace mlir::xai

#endif // XAI_DIALECT_H
