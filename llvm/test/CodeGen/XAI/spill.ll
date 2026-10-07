; RUN: llc -mtriple=xai -verify-machineinstrs -o - %s | FileCheck %s

declare <256 x half> @llvm.xai.mload.f16(ptr)
declare <256 x float> @llvm.xai.mload.f32(ptr)
declare <256 x float> @llvm.xai.mma(<256 x half>, <256 x half>, <256 x float>)
declare void @llvm.xai.mstore.f32(ptr, <256 x float>)

; Nine tile results are carried across the loop edge. No single instruction
; reads all of them, but m0-m7 cannot keep every value live, so register
; allocation spills at least one matrix register.
define void @spill(ptr %a, ptr %b, ptr %c, i64 %n) {
; CHECK-LABEL: spill:
; CHECK: xai.addi r31, r31, -
; CHECK: xai.mstore.f32 {{[-0-9]+}}(r31)
; CHECK: xai.mload.f32 {{m[0-7]}}, {{[-0-9]+}}(r31)
; CHECK: xai.addi r31, r31, {{[0-9]}}
; CHECK: xai.ret
entry:
  %va = call <256 x half> @llvm.xai.mload.f16(ptr %a)
  %vb = call <256 x half> @llvm.xai.mload.f16(ptr %b)
  %vc = call <256 x float> @llvm.xai.mload.f32(ptr %c)
  %r0 = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %vc)
  %r1 = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %r0)
  %r2 = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %r1)
  %r3 = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %r2)
  %r4 = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %r3)
  %r5 = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %r4)
  %r6 = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %r5)
  %r7 = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %r6)
  %r8 = call <256 x float> @llvm.xai.mma(<256 x half> %va, <256 x half> %vb, <256 x float> %r7)
  br label %header

header:
  %i = phi i64 [ 0, %entry ], [ %inext, %body ]
  %cp = phi ptr [ %c, %entry ], [ %cnext, %body ]
  %s0 = phi <256 x float> [ %r0, %entry ], [ %s0, %body ]
  %s1 = phi <256 x float> [ %r1, %entry ], [ %s1, %body ]
  %s2 = phi <256 x float> [ %r2, %entry ], [ %s2, %body ]
  %s3 = phi <256 x float> [ %r3, %entry ], [ %s3, %body ]
  %s4 = phi <256 x float> [ %r4, %entry ], [ %s4, %body ]
  %s5 = phi <256 x float> [ %r5, %entry ], [ %s5, %body ]
  %s6 = phi <256 x float> [ %r6, %entry ], [ %s6, %body ]
  %s7 = phi <256 x float> [ %r7, %entry ], [ %s7, %body ]
  %s8 = phi <256 x float> [ %r8, %entry ], [ %s8, %body ]
  %cmp = icmp ult i64 %i, %n
  br i1 %cmp, label %body, label %exit

body:
  call void @llvm.xai.mstore.f32(ptr %cp, <256 x float> %s0)
  call void @llvm.xai.mstore.f32(ptr %cp, <256 x float> %s1)
  call void @llvm.xai.mstore.f32(ptr %cp, <256 x float> %s2)
  call void @llvm.xai.mstore.f32(ptr %cp, <256 x float> %s3)
  call void @llvm.xai.mstore.f32(ptr %cp, <256 x float> %s4)
  call void @llvm.xai.mstore.f32(ptr %cp, <256 x float> %s5)
  call void @llvm.xai.mstore.f32(ptr %cp, <256 x float> %s6)
  call void @llvm.xai.mstore.f32(ptr %cp, <256 x float> %s7)
  call void @llvm.xai.mstore.f32(ptr %cp, <256 x float> %s8)
  %cnext = getelementptr i8, ptr %cp, i64 1024
  %inext = add i64 %i, 1
  br label %header

exit:
  ret void
}
