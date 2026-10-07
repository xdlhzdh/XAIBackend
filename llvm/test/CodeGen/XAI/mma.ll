; RUN: llc -mtriple=xai -verify-machineinstrs -o - %s | FileCheck %s

declare <256 x half> @llvm.xai.mload.f16(ptr)
declare <256 x float> @llvm.xai.mload.f32(ptr)
declare void @llvm.xai.mstore.f32(ptr, <256 x float>)
declare <256 x float> @llvm.xai.mma(<256 x half>, <256 x half>, <256 x float>)

define void @kernel(ptr %a, ptr %b, ptr %c) {
; CHECK-LABEL: kernel:
; CHECK: xai.mload.f16
; CHECK: xai.mload.f16
; CHECK: xai.mload.f32
; CHECK: xai.mma
; CHECK: xai.mstore.f32
; CHECK: xai.ret
  %va = call <256 x half> @llvm.xai.mload.f16(ptr %a)
  %vb = call <256 x half> @llvm.xai.mload.f16(ptr %b)
  %vc = call <256 x float> @llvm.xai.mload.f32(ptr %c)
  %vr = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %vc)
  call void @llvm.xai.mstore.f32(ptr %c, <256 x float> %vr)
  ret void
}
