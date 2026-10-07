//===-- XAIMCTargetDesc.cpp - XAI Target Descriptions ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAIMCTargetDesc.h"
#include "XAIMCAsmInfo.h"
#include "XAIInstPrinter.h"
#include "TargetInfo/XAITargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#include "XAIGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "XAIGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "XAIGenRegisterInfo.inc"

static MCInstrInfo *createXAIMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitXAIMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createXAIMCRegisterInfo(const Triple &) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitXAIMCRegisterInfo(X, XAI::R30);
  return X;
}

static MCAsmInfo *createXAIMCAsmInfo(const MCRegisterInfo &, const Triple &TT,
                                    const MCTargetOptions &Options) {
  return new XAIMCAsmInfo(TT, Options);
}

static MCSubtargetInfo *createXAIMCSubtargetInfo(const Triple &TT, StringRef CPU,
                                                StringRef FS) {
  if (CPU.empty())
    CPU = "generic";
  return createXAIMCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

static MCInstPrinter *createXAIMCInstPrinter(const Triple &, unsigned SyntaxVariant,
                                            const MCAsmInfo &MAI,
                                            const MCInstrInfo &MII,
                                            const MCRegisterInfo &MRI) {
  if (SyntaxVariant == 0)
    return new XAIInstPrinter(MAI, MII, MRI);
  return nullptr;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeXAITargetMC() {
  Target &T = getTheXAITarget();
  TargetRegistry::RegisterMCAsmInfo(T, createXAIMCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createXAIMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createXAIMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createXAIMCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createXAIMCInstPrinter);
  TargetRegistry::RegisterMCAsmBackend(T, createXAIMCAsmBackend);
}
