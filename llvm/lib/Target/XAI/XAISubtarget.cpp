//===-- XAISubtarget.cpp - XAI Subtarget Information ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAISubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "xai-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "XAIGenSubtargetInfo.inc"

XAISubtarget::XAISubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                           const TargetMachine &TM)
    : XAIGenSubtargetInfo(TT, CPU, CPU, FS), InstrInfo(*this), FrameLowering(),
      TLInfo(TM, *this) {
  ParseSubtargetFeatures(CPU, CPU, FS);
}

const char *XAISelectionDAGInfo::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case XAIISD::RET_FLAG:
    return "XAIISD::RET_FLAG";
  default:
    return nullptr;
  }
}
