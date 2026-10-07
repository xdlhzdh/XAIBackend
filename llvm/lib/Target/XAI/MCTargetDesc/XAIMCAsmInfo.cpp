//===-- XAIMCAsmInfo.cpp - XAI asm properties -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAIMCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

XAIMCAsmInfo::XAIMCAsmInfo(const Triple &TT, const MCTargetOptions &Options)
    : MCAsmInfo(Options) {
  CodePointerSize = 8;
  CalleeSaveStackSlotSize = 8;
  CommentString = "//";
  ExceptionsType = TT.getDefaultExceptionHandling();
  SupportsDebugInformation = true;
}
