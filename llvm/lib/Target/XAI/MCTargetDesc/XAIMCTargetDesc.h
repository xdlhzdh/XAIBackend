//===-- XAIMCTargetDesc.h - XAI Target Descriptions -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_XAI_MCTARGETDESC_XAIMCTARGETDESC_H
#define LLVM_LIB_TARGET_XAI_MCTARGETDESC_XAIMCTARGETDESC_H

#define GET_REGINFO_ENUM
#include "XAIGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "XAIGenInstrInfo.inc"

namespace llvm {

class MCAsmBackend;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class Target;

MCAsmBackend *createXAIMCAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                   const MCRegisterInfo &MRI,
                                   const MCTargetOptions &Options);

} // namespace llvm

#endif
