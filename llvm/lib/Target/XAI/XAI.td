//===-- XAI.td - Describe the XAI target machine ------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

include "XAIRegisterInfo.td"
include "XAICallingConv.td"
include "XAIInstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<GPR>;

def XAIInstrInfo : InstrInfo;

def XAIAsmWriter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
  bit isMCAsmWriter = 1;
}

def : Processor<"generic", NoItineraries, []>;

def XAI : Target {
  let InstructionSet = XAIInstrInfo;
  let AssemblyWriters = [XAIAsmWriter];
}
