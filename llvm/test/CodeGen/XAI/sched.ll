; The address add does not depend on the matrix multiply. Scheduling moves it
; into the gap between the matrix loads and xai.mma. -O0 keeps source order.
; RUN: llc -mtriple=xai -verify-machineinstrs -o - %s | FileCheck %s
; RUN: llc -mtriple=xai -O0 -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=O0

declare <256 x half> @llvm.xai.mload.f16(ptr)
declare <256 x float> @llvm.xai.mload.f32(ptr)
declare void @llvm.xai.mstore.f32(ptr, <256 x float>)
declare <256 x float> @llvm.xai.mma(<256 x half>, <256 x half>, <256 x float>)

define void @hide_load_latency(ptr %a, ptr %b, ptr %c, ptr %d) {
; CHECK-LABEL: hide_load_latency:
; CHECK: xai.mload.f16
; CHECK: xai.mload.f16
; CHECK: xai.mload.f32
; CHECK-NEXT: xai.addi r0, r3, 64
; CHECK-NEXT: xai.mma
; CHECK-NEXT: xai.mstore.f32
; CHECK-NEXT: xai.mload.f32
; CHECK-NEXT: xai.mstore.f32
; CHECK-NEXT: xai.ret
; O0-LABEL: hide_load_latency:
; O0: xai.mload.f32
; O0-NEXT: xai.mma
; O0-NEXT: xai.mstore.f32
; O0-NEXT: xai.addi r0, r3, 64
  %va = call <256 x half> @llvm.xai.mload.f16(ptr %a)
  %vb = call <256 x half> @llvm.xai.mload.f16(ptr %b)
  %vc = call <256 x float> @llvm.xai.mload.f32(ptr %c)
  %vr = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %vc)
  call void @llvm.xai.mstore.f32(ptr %c, <256 x float> %vr)
  %d2 = getelementptr i8, ptr %d, i64 64
  %vd = call <256 x float> @llvm.xai.mload.f32(ptr %d2)
  call void @llvm.xai.mstore.f32(ptr %d, <256 x float> %vd)
  ret void
}
