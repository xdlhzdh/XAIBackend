// One XAI MMA tile: 16x16xf16 * 16x16xf16 + 16x16xf32, one thread.
module {
  func.func @matmul_kernel(%A: memref<16x16xf16>,
                           %B: memref<16x16xf16>,
                           %C: memref<16x16xf32>) {
    %c1 = arith.constant 1 : index
    gpu.launch blocks(%bx, %by, %bz) in (%gx = %c1, %gy = %c1, %gz = %c1)
               threads(%tx, %ty, %tz) in (%bdx = %c1, %bdy = %c1, %bdz = %c1) {
      linalg.matmul
          ins(%A, %B : memref<16x16xf16>, memref<16x16xf16>)
          outs(%C : memref<16x16xf32>)
      gpu.terminator
    }
    return
  }
}
