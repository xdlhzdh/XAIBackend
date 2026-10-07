//===-- XAIFrameLowering.cpp - XAI frame lowering -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAIFrameLowering.h"
#include "XAIInstrInfo.h"
#include "XAISubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

static void adjustSP(MachineBasicBlock &MBB, MachineBasicBlock::iterator Insert,
                     const DebugLoc &DL, int64_t Delta,
                     MachineInstr::MIFlag Flag) {
  if (!Delta)
    return;
  const XAIInstrInfo &TII =
      *MBB.getParent()->getSubtarget<XAISubtarget>().getInstrInfo();
  BuildMI(MBB, Insert, DL, TII.get(XAI::XAI_ADDI), XAI::R31)
      .addReg(XAI::R31)
      .addImm(Delta)
      .setMIFlag(Flag);
}

void XAIFrameLowering::emitPrologue(MachineFunction &MF,
                                   MachineBasicBlock &MBB) const {
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  adjustSP(MBB, MBB.begin(), DebugLoc(), -static_cast<int64_t>(StackSize),
           MachineInstr::FrameSetup);
}

void XAIFrameLowering::emitEpilogue(MachineFunction &MF,
                                   MachineBasicBlock &MBB) const {
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  adjustSP(MBB, MBB.getFirstTerminator(), DebugLoc(),
           static_cast<int64_t>(StackSize), MachineInstr::FrameDestroy);
}
