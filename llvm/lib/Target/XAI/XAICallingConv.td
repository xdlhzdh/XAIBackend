//===-- XAICallingConv.td - XAI Calling Conventions ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

def CC_XAI : CallingConv<[
  CCIfType<[i64], CCAssignToReg<[R0, R1, R2, R3, R4, R5, R6, R7]>>,
  CCIfType<[i64], CCAssignToStack<8, 8>>
]>;
