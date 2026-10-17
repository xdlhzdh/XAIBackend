#!/usr/bin/env bash
# Stage checks for the XAI offload frontend.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
XAI_COMPILE="${XAI_COMPILE:-${ROOT}/build/bin/xai-compile}"
OUT="${OUT_DIR:-/tmp/xai-offload-check}"
mkdir -p "${OUT}"

die() { echo "error: $*" >&2; exit 1; }
[[ -x "${XAI_COMPILE}" ]] || die "missing ${XAI_COMPILE}"

# check_input NAME INPUT LEFTOVER_PATTERN
check_input() {
  local name="$1" input="$2" leftover="$3"

  echo "==> ${name}: rewrite"
  "${XAI_COMPILE}" --input="${input}" --pipeline-stop-after=rewrite \
    --output="${OUT}/${name}.rewrite.mlir" >/dev/null
  grep -q 'llvm.call @llvm.xai.mload.f16' "${OUT}/${name}.rewrite.mlir"
  grep -q 'llvm.call @llvm.xai.mload.f32' "${OUT}/${name}.rewrite.mlir"
  grep -q 'llvm.call @llvm.xai.mma' "${OUT}/${name}.rewrite.mlir"
  grep -q 'llvm.call @llvm.xai.mstore.f32' "${OUT}/${name}.rewrite.mlir"
  if grep -Eq "${leftover}" "${OUT}/${name}.rewrite.mlir"; then
    die "${name}: '${leftover}' survived rewrite"
  fi

  echo "==> ${name}: elf"
  "${XAI_COMPILE}" --input="${input}" --output="${OUT}/${name}.o" \
    --dump-device-elf="${OUT}/${name}.device.elf" >/dev/null
  file "${OUT}/${name}.o" | grep -qi 'ELF' || die "${name}: host output is not an ELF"
  file "${OUT}/${name}.device.elf" | grep -qi 'ELF' \
    || die "${name}: device output is not an ELF"
  llvm-readelf -S "${OUT}/${name}.device.elf" | grep -q '.xai_isa' \
    || die "${name}: device ELF is missing .xai_isa"
  strings "${OUT}/${name}.o" | grep -q 'xai.mma' \
    || die "${name}: host ELF does not embed xai.mma"
  echo "OK: ${OUT}/${name}.o"
}

check_input linalg "${ROOT}/test/matmul_xai.mlir" 'linalg\.matmul'
check_input vector "${ROOT}/test/matmul_vector_xai.mlir" 'vector\.'

echo "==> vector: transposed B is rejected"
if "${XAI_COMPILE}" --input="${ROOT}/test/matmul_vector_transposed_b.mlir" \
    --pipeline-stop-after=rewrite >/dev/null 2>"${OUT}/transposed_b.err"; then
  die "transposed B contraction was accepted"
fi
grep -q 'transposed operands are not supported' "${OUT}/transposed_b.err" \
  || die "unexpected diagnostic: $(cat "${OUT}/transposed_b.err")"
echo "OK: transposed B rejected"
