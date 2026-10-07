//===-- XAIFrameLowering.h - XAI frame lowering -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_XAI_XAIFRAMELOWERING_H
#define LLVM_LIB_TARGET_XAI_XAIFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class XAIFrameLowering : public TargetFrameLowering {
public:
  XAIFrameLowering()
      : TargetFrameLowering(StackGrowsDown, Align(1024), /*LocalAreaOffset=*/0,
                            Align(1024), /*StackRealignable=*/false) {}

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

protected:
  bool hasFPImpl(const MachineFunction &MF) const override { return false; }
};

} // namespace llvm

#endif
