add_llvm_component_library(LLVMXAIInfo
  XAITargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support
  TargetParser

  ADD_TO_COMPONENT
  XAI
  )
