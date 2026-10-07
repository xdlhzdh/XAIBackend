//===-- XAIAsmPrinter.cpp - XAI assembly writer ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "XAI.h"
#include "XAIMCInstLower.h"
#include "XAITargetMachine.h"
#include "TargetInfo/XAITargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

#define DEBUG_TYPE "xai-asm-printer"

namespace {

class XAIAsmPrinter : public AsmPrinter {
public:
  static char ID;

  XAIAsmPrinter(TargetMachine &TM, std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  StringRef getPassName() const override { return "XAI Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override;
};

} // namespace

char XAIAsmPrinter::ID = 0;

INITIALIZE_PASS(XAIAsmPrinter, DEBUG_TYPE, "XAI Assembly Printer", false, false)

void XAIAsmPrinter::emitInstruction(const MachineInstr *MI) {
  if (MI->isMetaInstruction())
    return;
  MCInst Lowered;
  lowerXAIMachineInstrToMCInst(*MI, Lowered, OutContext);
  EmitToStreamer(*OutStreamer, Lowered);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeXAIAsmPrinter() {
  RegisterAsmPrinter<XAIAsmPrinter> X(getTheXAITarget());
}
