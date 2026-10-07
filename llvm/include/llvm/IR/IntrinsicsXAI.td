//===- IntrinsicsXAI.td - XAI intrinsics --------------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "xai" in {
  def int_xai_mload_f16
      : Intrinsic<[LLVMType<v256f16>], [llvm_ptr_ty], [IntrReadMem]>;
  def int_xai_mload_f32
      : Intrinsic<[LLVMType<v256f32>], [llvm_ptr_ty], [IntrReadMem]>;
  def int_xai_mstore_f16
      : Intrinsic<[], [llvm_ptr_ty, LLVMType<v256f16>], [IntrWriteMem]>;
  def int_xai_mstore_f32
      : Intrinsic<[], [llvm_ptr_ty, LLVMType<v256f32>], [IntrWriteMem]>;
  def int_xai_mma
      : Intrinsic<[LLVMType<v256f32>],
                  [LLVMType<v256f16>, LLVMType<v256f16>, LLVMType<v256f32>],
                  [IntrNoMem]>;
}
