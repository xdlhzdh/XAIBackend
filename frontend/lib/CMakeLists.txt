set(LLVM_TARGET_DEFINITIONS ${PROJECT_SOURCE_DIR}/include/XAI/XAIOps.td)
mlir_tablegen(XAIDialect.h.inc -gen-dialect-decls -dialect=xai)
mlir_tablegen(XAIDialect.cpp.inc -gen-dialect-defs -dialect=xai)
mlir_tablegen(XAIAttrs.h.inc -gen-attrdef-decls -attrdefs-dialect=xai)
mlir_tablegen(XAIAttrs.cpp.inc -gen-attrdef-defs -attrdefs-dialect=xai)
add_public_tablegen_target(XAIDialectIncGen)

set(LLVM_TARGET_DEFINITIONS ${PROJECT_SOURCE_DIR}/include/XAI/Passes.td)
mlir_tablegen(Passes.h.inc -gen-pass-decls -name XAI)
add_public_tablegen_target(XAIPassIncGen)

add_library(XAIFrontend STATIC
  XAIDialect.cpp
  Intrinsics.cpp
  MatmulToIntrinsic.cpp
  VectorToIntrinsic.cpp
  OutlineAndAttach.cpp
  Target.cpp
  Pipeline.cpp
)
add_dependencies(XAIFrontend XAIDialectIncGen XAIPassIncGen)

target_include_directories(XAIFrontend PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(XAIFrontend PUBLIC
  MLIRArithDialect
  MLIRArithToLLVM
  MLIRControlFlowDialect
  MLIRDLTIDialect
  MLIRFuncDialect
  MLIRFuncToLLVM
  MLIRGPUDialect
  MLIRGPUToLLVMIRTranslation
  MLIRGPUTransforms
  MLIRIR
  MLIRIndexDialect
  MLIRIndexToLLVM
  MLIRLLVMCommonConversion
  MLIRLLVMDialect
  MLIRLinalgDialect
  MLIRMemRefDialect
  MLIRMemRefToLLVM
  MLIRParser
  MLIRPass
  MLIRReconcileUnrealizedCasts
  MLIRSupport
  MLIRTargetLLVMIRExport
  MLIRToLLVMIRTranslationRegistration
  MLIRTransformUtils
  MLIRTransforms
  MLIRVectorDialect
)
