// Negative case: B is read as transposed (n, k). XAI has no transposed MMA,
// so xai-vector-to-intrinsic must reject this contraction.
#mapA = affine_map<(m, n, k) -> (m, k)>
#mapBt = affine_map<(m, n, k) -> (n, k)>
#mapC = affine_map<(m, n, k) -> (m, n)>
module {
  func.func @matmul_kernel(%A: memref<16x16xf16>,
                           %B: memref<16x16xf16>,
                           %C: memref<16x16xf32>) {
    %c1 = arith.constant 1 : index
    gpu.launch blocks(%bx, %by, %bz) in (%gx = %c1, %gy = %c1, %gz = %c1)
               threads(%tx, %ty, %tz) in (%bdx = %c1, %bdy = %c1, %bdz = %c1) {
      %c0 = arith.constant 0 : index
      %padh = arith.constant 0.0 : f16
      %padf = arith.constant 0.0 : f32
      %a = vector.transfer_read %A[%c0, %c0], %padh {in_bounds = [true, true]}
          : memref<16x16xf16>, vector<16x16xf16>
      %b = vector.transfer_read %B[%c0, %c0], %padh {in_bounds = [true, true]}
          : memref<16x16xf16>, vector<16x16xf16>
      %c = vector.transfer_read %C[%c0, %c0], %padf {in_bounds = [true, true]}
          : memref<16x16xf32>, vector<16x16xf32>
      %d = vector.contract {indexing_maps = [#mapA, #mapBt, #mapC],
                            iterator_types = ["parallel", "parallel", "reduction"],
                            kind = #vector.kind<add>} %a, %b, %c
          : vector<16x16xf16>, vector<16x16xf16> into vector<16x16xf32>
      vector.transfer_write %d, %C[%c0, %c0] {in_bounds = [true, true]}
          : vector<16x16xf32>, memref<16x16xf32>
      gpu.terminator
    }
    return
  }
}
