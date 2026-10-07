//===-- XAIMCAsmInfo.h - XAI asm properties ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_XAI_MCTARGETDESC_XAIMCASMINFO_H
#define LLVM_LIB_TARGET_XAI_MCTARGETDESC_XAIMCASMINFO_H

#include "llvm/MC/MCAsmInfo.h"

namespace llvm {

class Triple;

class XAIMCAsmInfo : public MCAsmInfo {
public:
  explicit XAIMCAsmInfo(const Triple &TT, const MCTargetOptions &Options);
};

} // namespace llvm

#endif
