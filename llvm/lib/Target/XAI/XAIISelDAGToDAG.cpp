//===-- XAIISelDAGToDAG.cpp - XAI dag selector ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAI.h"
#include "XAIISelLowering.h"
#include "XAITargetMachine.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsXAI.h"

using namespace llvm;

#define DEBUG_TYPE "xai-isel"
#define PASS_NAME "XAI DAG->DAG Pattern Instruction Selection"

namespace {

class XAIDAGToDAGISel : public SelectionDAGISel {
public:
  XAIDAGToDAGISel() = delete;
  XAIDAGToDAGISel(XAITargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISel(TM, OptLevel) {}

private:
#include "XAIGenDAGISel.inc"

  void Select(SDNode *N) override;

  bool selectMemIntrinsic(SDNode *N);
};

class XAIDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;
  XAIDAGToDAGISelLegacy(XAITargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISelLegacy(ID, std::make_unique<XAIDAGToDAGISel>(TM, OptLevel)) {}
};

} // namespace

char XAIDAGToDAGISelLegacy::ID = 0;

INITIALIZE_PASS(XAIDAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)

FunctionPass *llvm::createXAIISelDag(XAITargetMachine &TM,
                                     CodeGenOptLevel OptLevel) {
  return new XAIDAGToDAGISelLegacy(TM, OptLevel);
}

bool XAIDAGToDAGISel::selectMemIntrinsic(SDNode *N) {
  unsigned Opcode = N->getOpcode();
  if (Opcode != ISD::INTRINSIC_W_CHAIN && Opcode != ISD::INTRINSIC_VOID)
    return false;

  unsigned IntID = N->getConstantOperandVal(1);
  unsigned Opc = 0;
  bool IsLoad = false;
  if (IntID == Intrinsic::xai_mload_f16) {
    Opc = XAI::XAI_MLOAD_F16;
    IsLoad = true;
  } else if (IntID == Intrinsic::xai_mload_f32) {
    Opc = XAI::XAI_MLOAD_F32;
    IsLoad = true;
  } else if (IntID == Intrinsic::xai_mstore_f16) {
    Opc = XAI::XAI_MSTORE_F16;
  } else if (IntID == Intrinsic::xai_mstore_f32) {
    Opc = XAI::XAI_MSTORE_F32;
  } else {
    return false;
  }

  SDLoc DL(N);
  SDValue Chain = N->getOperand(0);
  SDValue Ptr = N->getOperand(2);
  SDValue Imm = CurDAG->getTargetConstant(0, DL, MVT::i32);
  SmallVector<SDValue, 4> Ops;
  if (IsLoad) {
    Ops.push_back(Ptr);
    Ops.push_back(Imm);
    Ops.push_back(Chain);
    CurDAG->SelectNodeTo(N, Opc, N->getValueType(0), MVT::Other, Ops);
    return true;
  }

  Ops.push_back(N->getOperand(3));
  Ops.push_back(Ptr);
  Ops.push_back(Imm);
  Ops.push_back(Chain);
  CurDAG->SelectNodeTo(N, Opc, MVT::Other, Ops);
  return true;
}

void XAIDAGToDAGISel::Select(SDNode *N) {
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }
  if (selectMemIntrinsic(N))
    return;
  SelectCode(N);
}
