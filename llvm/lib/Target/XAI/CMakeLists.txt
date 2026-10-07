add_llvm_component_group(XAI)

set(LLVM_TARGET_DEFINITIONS XAI.td)

tablegen(LLVM XAIGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM XAIGenCallingConv.inc -gen-callingconv)
tablegen(LLVM XAIGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM XAIGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM XAIGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM XAIGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(XAICommonTableGen)

add_llvm_target(XAICodeGen
  XAIISelDAGToDAG.cpp
  XAIISelLowering.cpp
  XAIInstrInfo.cpp
  XAIFrameLowering.cpp
  XAIRegisterInfo.cpp
  XAISubtarget.cpp
  XAITargetMachine.cpp
  XAIAsmPrinter.cpp
  XAIMCInstLower.cpp

  LINK_COMPONENTS
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  MC
  SelectionDAG
  Support
  Target
  TargetParser
  XAIDesc
  XAIInfo

  ADD_TO_COMPONENT
  XAI
  )

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
