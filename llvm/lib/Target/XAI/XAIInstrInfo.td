//===-- XAIInstrInfo.td - XAI instructions ------------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "XAIInstrFormats.td"

def XAISimm32 : PatLeaf<(imm), [{ return isInt<32>(N->getSExtValue()); }]>;

def brtarget : Operand<OtherVT> {
  let PrintMethod = "printOperand";
}

def XAI_retflag
    : SDNode<"XAIISD::RET_FLAG", SDTNone,
             [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

def XAI_RET : XAIInst<(outs), (ins), "xai.ret", [(XAI_retflag)]> {
  let isTerminator = true;
  let isReturn = true;
  let isBarrier = true;
  let hasCtrlDep = true;
}

let mayLoad = true in {
  def XAI_MLOAD_F16
      : XAIInst<(outs MR:$dst), (ins GPR:$base, i32imm:$imm),
                "xai.mload.f16 $dst, $imm($base)">;
  def XAI_MLOAD_F32
      : XAIInst<(outs MR:$dst), (ins GPR:$base, i32imm:$imm),
                "xai.mload.f32 $dst, $imm($base)">;
  def XAI_LD
      : XAIInst<(outs GPR:$dst), (ins GPR:$base, i32imm:$imm),
                "xai.ld $dst, $imm($base)">;
}

let mayStore = true in {
  def XAI_MSTORE_F16
      : XAIInst<(outs), (ins MR:$src, GPR:$base, i32imm:$imm),
                "xai.mstore.f16 $imm($base), $src">;
  def XAI_MSTORE_F32
      : XAIInst<(outs), (ins MR:$src, GPR:$base, i32imm:$imm),
                "xai.mstore.f32 $imm($base), $src">;
  def XAI_ST
      : XAIInst<(outs), (ins GPR:$src, GPR:$base, i32imm:$imm),
                "xai.st $imm($base), $src">;
}

let hasSideEffects = false in {
  def XAI_MMA
      : XAIInst<(outs MR:$dst), (ins MR:$a, MR:$b, MR:$acc),
                "xai.mma $dst, $a, $b, $acc">;
  def XAI_MOV : XAIInst<(outs GPR:$rd), (ins GPR:$rs), "xai.mov $rd, $rs"> {
    let isMoveReg = true;
    let isAsCheapAsAMove = true;
  }
  def XAI_MMOV : XAIInst<(outs MR:$rd), (ins MR:$rs), "xai.mmov $rd, $rs"> {
    let isMoveReg = true;
    let isAsCheapAsAMove = true;
  }
  def XAI_ADDI
      : XAIInst<(outs GPR:$rd), (ins GPR:$rs, i64imm:$imm),
                "xai.addi $rd, $rs, $imm"> {
    let isAsCheapAsAMove = true;
  }
  def XAI_ADD
      : XAIInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2),
                "xai.add $rd, $rs1, $rs2">;
  def XAI_LI : XAIInst<(outs GPR:$rd), (ins i64imm:$imm), "xai.li $rd, $imm"> {
    let isReMaterializable = true;
    let isAsCheapAsAMove = true;
  }
}

let isBranch = true, isTerminator = true, hasSideEffects = false in {
  def XAI_BR : XAIInst<(outs), (ins brtarget:$dst), "xai.br $dst",
                       [(br bb:$dst)]> {
    let isBarrier = true;
  }
  def XAI_BEQ
      : XAIInst<(outs), (ins GPR:$rs1, GPR:$rs2, brtarget:$dst),
                "xai.beq $rs1, $rs2, $dst">;
  def XAI_BNE
      : XAIInst<(outs), (ins GPR:$rs1, GPR:$rs2, brtarget:$dst),
                "xai.bne $rs1, $rs2, $dst">;
  def XAI_BLTU
      : XAIInst<(outs), (ins GPR:$rs1, GPR:$rs2, brtarget:$dst),
                "xai.bltu $rs1, $rs2, $dst">;
  def XAI_BGEU
      : XAIInst<(outs), (ins GPR:$rs1, GPR:$rs2, brtarget:$dst),
                "xai.bgeu $rs1, $rs2, $dst">;
}

def : Pat<(int_xai_mma MR:$a, MR:$b, MR:$c), (XAI_MMA MR:$a, MR:$b, MR:$c)>;
def : Pat<(add GPR:$rs1, GPR:$rs2), (XAI_ADD GPR:$rs1, GPR:$rs2)>;
def : Pat<(add GPR:$rs, XAISimm32:$imm), (XAI_ADDI GPR:$rs, XAISimm32:$imm)>;
def : Pat<(i64 XAISimm32:$imm), (XAI_LI XAISimm32:$imm)>;
