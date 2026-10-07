//===-- XAIRegisterInfo.cpp - XAI Register Information --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAIRegisterInfo.h"
#include "XAI.h"
#include "XAISubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "XAIGenRegisterInfo.inc"

XAIRegisterInfo::XAIRegisterInfo() : XAIGenRegisterInfo(XAI::R30) {}

const MCPhysReg *
XAIRegisterInfo::getCalleeSavedRegs(const MachineFunction *) const {
  static const MCPhysReg None[] = {0};
  return None;
}

BitVector XAIRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  Reserved.set(XAI::R31);
  return Reserved;
}

bool XAIRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                         int SPAdj, unsigned FIOperandNum,
                                         RegScavenger *) const {
  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  MachineFrameInfo &MFI = MF.getFrameInfo();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  int64_t Offset = MFI.getObjectOffset(FrameIndex);
  Offset += static_cast<int64_t>(MFI.getStackSize());
  Offset += MI.getOperand(FIOperandNum + 1).getImm();
  Offset += SPAdj;

  MI.getOperand(FIOperandNum).ChangeToRegister(XAI::R31, /*isDef=*/false);
  MI.getOperand(FIOperandNum + 1).setImm(Offset);
  return false;
}

Register XAIRegisterInfo::getFrameRegister(const MachineFunction &) const {
  return XAI::R31;
}
