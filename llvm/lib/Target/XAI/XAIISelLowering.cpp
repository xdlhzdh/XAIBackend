//===-- XAIISelLowering.cpp - XAI DAG lowering ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAIISelLowering.h"
#include "XAI.h"
#include "XAIInstrInfo.h"
#include "XAISubtarget.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#include "XAIGenCallingConv.inc"

XAITargetLowering::XAITargetLowering(const TargetMachine &TM,
                                     const XAISubtarget &STI)
    : TargetLowering(TM, STI) {
  addRegisterClass(MVT::i64, &XAI::GPRRegClass);
  addRegisterClass(MVT::v256f16, &XAI::MRRegClass);
  addRegisterClass(MVT::v256f32, &XAI::MRRegClass);

  setBooleanContents(ZeroOrOneBooleanContent);
  setBooleanVectorContents(ZeroOrOneBooleanContent);
  setOperationAction(ISD::BRCOND, MVT::Other, Expand);
  setOperationAction(ISD::BR_CC, MVT::i64, Custom);
  setStackPointerRegisterToSaveRestore(XAI::R31);
  setMinFunctionAlignment(Align(4));
  setSchedulingPreference(Sched::RegPressure);

  computeRegisterProperties(STI.getRegisterInfo());
}

const char *XAITargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case XAIISD::RET_FLAG:
    return "XAIISD::RET_FLAG";
  default:
    return nullptr;
  }
}

SDValue XAITargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  if (IsVarArg)
    report_fatal_error("XAI does not support varargs");

  MachineFunction &MF = DAG.getMachineFunction();
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_XAI);

  for (const CCValAssign &VA : ArgLocs) {
    if (!VA.isRegLoc())
      report_fatal_error("XAI demo only passes arguments in GPR");
    Register VReg = MF.addLiveIn(VA.getLocReg(), &XAI::GPRRegClass);
    InVals.push_back(DAG.getCopyFromReg(Chain, DL, VReg, VA.getLocVT()));
  }
  return Chain;
}

SDValue XAITargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID, bool, const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &, const SDLoc &DL,
    SelectionDAG &DAG) const {
  if (!Outs.empty())
    report_fatal_error("XAI demo only supports void return");
  return DAG.getNode(XAIISD::RET_FLAG, DL, MVT::Other, Chain);
}

SDValue XAITargetLowering::LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::BR_CC:
    return LowerBR_CC(Op, DAG);
  }
  report_fatal_error("XAI cannot lower this operation");
}

SDValue XAITargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);

  unsigned Opc;
  switch (CC) {
  case ISD::SETEQ:
    Opc = XAI::XAI_BEQ;
    break;
  case ISD::SETNE:
    Opc = XAI::XAI_BNE;
    break;
  case ISD::SETULT:
    Opc = XAI::XAI_BLTU;
    break;
  case ISD::SETUGE:
    Opc = XAI::XAI_BGEU;
    break;
  default:
    report_fatal_error("XAI does not support this branch condition");
  }

  // Chain is last. A basic block is also MVT::Other, so the emitter treats
  // only the final operand as the chain.
  return SDValue(DAG.getMachineNode(Opc, SDLoc(Op), MVT::Other,
                                    {LHS, RHS, Dest, Chain}),
                 0);
}
