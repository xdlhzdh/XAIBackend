#include "XAI/XAIDialect.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/DialectImplementation.h"
#include "llvm/ADT/TypeSwitch.h"

using namespace mlir;
using namespace mlir::xai;

#include "XAIDialect.cpp.inc"

#define GET_ATTRDEF_CLASSES
#include "XAIAttrs.cpp.inc"

void XAIDialect::initialize() {
  addAttributes<TargetAttr>();
  // gpu.module's target list accepts an attribute that promises or implements
  // TargetAttrInterface. The external model is attached by
  // registerXAITargetInterfaceExternalModels.
  declarePromisedInterface<gpu::TargetAttrInterface, TargetAttr>();
  declarePromisedInterface<gpu::OffloadingLLVMTranslationAttrInterface,
                           TargetAttr>();
}
