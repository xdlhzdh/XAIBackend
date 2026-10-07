//===-- XAISubtarget.h - Define Subtarget for the XAI -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_XAI_XAISUBTARGET_H
#define LLVM_LIB_TARGET_XAI_XAISUBTARGET_H

#include "XAIFrameLowering.h"
#include "XAIISelLowering.h"
#include "XAIInstrInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Target/TargetMachine.h"

#define GET_SUBTARGETINFO_HEADER
#include "XAIGenSubtargetInfo.inc"

namespace llvm {

class XAISelectionDAGInfo : public SelectionDAGTargetInfo {
public:
  const char *getTargetNodeName(unsigned Opcode) const override;
};

class XAISubtarget : public XAIGenSubtargetInfo {
  XAIInstrInfo InstrInfo;
  XAIFrameLowering FrameLowering;
  XAITargetLowering TLInfo;
  XAISelectionDAGInfo TSInfo;

public:
  XAISubtarget(const Triple &TT, StringRef CPU, StringRef FS,
               const TargetMachine &TM);

  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);

  const XAIInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const XAITargetLowering *getTargetLowering() const override { return &TLInfo; }
  const XAIFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const XAIRegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
  const XAISelectionDAGInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }
};

} // namespace llvm

#endif
