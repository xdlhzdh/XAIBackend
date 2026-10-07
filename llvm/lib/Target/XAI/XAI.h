//===-- XAI.h - Top-level interface for XAI --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_XAI_XAI_H
#define LLVM_LIB_TARGET_XAI_XAI_H

#include "llvm/Support/CodeGen.h"

namespace llvm {

class FunctionPass;
class PassRegistry;
class XAITargetMachine;

FunctionPass *createXAIISelDag(XAITargetMachine &TM, CodeGenOptLevel OptLevel);

void initializeXAIDAGToDAGISelLegacyPass(PassRegistry &);
void initializeXAIAsmPrinterPass(PassRegistry &);

} // namespace llvm

#endif
