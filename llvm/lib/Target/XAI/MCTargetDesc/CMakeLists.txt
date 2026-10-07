include_directories(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)

add_llvm_component_library(LLVMXAIDesc
  XAIAsmBackend.cpp
  XAIInstPrinter.cpp
  XAIMCAsmInfo.cpp
  XAIMCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  Support
  TargetParser
  XAIInfo

  ADD_TO_COMPONENT
  XAI
  )
