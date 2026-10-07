//===-- XAITargetInfo.cpp - XAI Target Implementation ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/XAITargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

Target &llvm::getTheXAITarget() {
  static Target TheXAITarget;
  return TheXAITarget;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeXAITargetInfo() {
  RegisterTarget<Triple::xai> X(getTheXAITarget(), "xai", "XAI", "XAI");
}
