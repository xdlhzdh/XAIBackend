//===-- XAIMCInstLower.h - Lower MachineInstr to MCInst ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_XAI_XAIMCINSTLOWER_H
#define LLVM_LIB_TARGET_XAI_XAIMCINSTLOWER_H

namespace llvm {

class MachineInstr;
class MCContext;
class MCInst;

void lowerXAIMachineInstrToMCInst(const MachineInstr &MI, MCInst &OutMI,
                                  MCContext &Ctx);

} // namespace llvm

#endif
