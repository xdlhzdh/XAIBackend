//===-- XAIRegisterInfo.td - XAI Register defs --------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

class XAIReg<int enc, string n> : Register<n> {
  let Namespace = "XAI";
  let HWEncoding = enc;
}

foreach I = 0-31 in
  def R#I : XAIReg<I, "r"#I>, DwarfRegNum<[I]>;

foreach I = 0-7 in
  def M#I : XAIReg<!add(I, 32), "m"#I>, DwarfRegNum<[!add(I, 32)]>;

def GPR : RegisterClass<"XAI", [i64], 64, (sequence "R%u", 0, 31)>;

// One class holds both tile formats. Spill slots use the larger FP32 tile.
def MR : RegisterClass<"XAI", [v256f32, v256f16], 8192,
                       (sequence "M%u", 0, 7)> {
  let Size = 8192;
}
