#!/usr/bin/env bash
# Stage checks for the XAI offload frontend.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
XAI_COMPILE="${XAI_COMPILE:-${ROOT}/build/bin/xai-compile}"
INPUT="${ROOT}/test/matmul_xai.mlir"
OUT="${OUT_DIR:-/tmp/xai-offload-check}"
mkdir -p "${OUT}"

die() { echo "error: $*" >&2; exit 1; }
[[ -x "${XAI_COMPILE}" ]] || die "missing ${XAI_COMPILE}"

echo "==> rewrite"
"${XAI_COMPILE}" --input="${INPUT}" --pipeline-stop-after=rewrite \
  --output="${OUT}/rewrite.mlir" >/dev/null
grep -q 'llvm.xai.mload.f16' "${OUT}/rewrite.mlir"
grep -q 'llvm.xai.mload.f32' "${OUT}/rewrite.mlir"
grep -q 'llvm.xai.mma' "${OUT}/rewrite.mlir"
grep -q 'llvm.xai.mstore.f32' "${OUT}/rewrite.mlir"
if grep -q 'linalg.matmul' "${OUT}/rewrite.mlir"; then
  die "linalg.matmul survived rewrite"
fi

echo "==> elf"
"${XAI_COMPILE}" --input="${INPUT}" --output="${OUT}/matmul.o" \
  --dump-device-elf="${OUT}/device.elf" >/dev/null
file "${OUT}/matmul.o" | grep -qi 'ELF' || die "host output is not an ELF"
file "${OUT}/device.elf" | grep -qi 'ELF' || die "device output is not an ELF"
llvm-readelf -h "${OUT}/matmul.o" | head -5
llvm-readelf -S "${OUT}/device.elf" | grep -q '.xai_isa' \
  || die "device ELF is missing .xai_isa"
strings "${OUT}/matmul.o" | grep -q 'xai.mma' \
  || die "host ELF does not embed xai.mma"
echo "OK: ${OUT}/matmul.o"
