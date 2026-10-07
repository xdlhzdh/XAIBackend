; RUN: llc -mtriple=xai -verify-machineinstrs -o - %s | FileCheck %s

declare <256 x half> @llvm.xai.mload.f16(ptr)
declare <256 x float> @llvm.xai.mload.f32(ptr)
declare void @llvm.xai.mstore.f32(ptr, <256 x float>)
declare <256 x float> @llvm.xai.mma(<256 x half>, <256 x half>, <256 x float>)

define void @loop(ptr %a, ptr %b, ptr %c, i64 %n, i64 %step) {
; CHECK-LABEL: loop:
; CHECK-DAG: xai.li
; CHECK-DAG: xai.add r
; CHECK-DAG: xai.addi
; CHECK-DAG: xai.bgeu
; CHECK-DAG: xai.bltu
; CHECK-DAG: xai.mma
; CHECK: xai.ret
entry:
  br label %header

header:
  %i = phi i64 [ 0, %entry ], [ %inext, %body ]
  %ap = phi ptr [ %a, %entry ], [ %anext, %body ]
  %bp = phi ptr [ %b, %entry ], [ %bnext, %body ]
  %cp = phi ptr [ %c, %entry ], [ %cnext, %body ]
  %cmp = icmp ult i64 %i, %n
  br i1 %cmp, label %body, label %exit

body:
  %va = call <256 x half> @llvm.xai.mload.f16(ptr %ap)
  %vb = call <256 x half> @llvm.xai.mload.f16(ptr %bp)
  %vc = call <256 x float> @llvm.xai.mload.f32(ptr %cp)
  %vr = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %vc)
  call void @llvm.xai.mstore.f32(ptr %cp, <256 x float> %vr)
  %anext = getelementptr i8, ptr %ap, i64 512
  %bnext = getelementptr i8, ptr %bp, i64 512
  %cnext = getelementptr i8, ptr %cp, i64 1024
  %inext = add i64 %i, %step
  br label %header

exit:
  ret void
}
