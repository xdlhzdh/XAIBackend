#!/usr/bin/env bash
# Configure and build llc plus the lit tools for the XAI backend only.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${ROOT}/third_party/llvm-project/llvm"
BUILD="${ROOT}/third_party/llvm-project/build"

"${ROOT}/scripts/setup_llvm.sh"

CMAKE_ID="Release assertions XAI tests"
CMAKE_STAMP="${BUILD}/.xai-cmake-id"
if [[ -f "${BUILD}/build.ninja" && -f "${CMAKE_STAMP}" && "$(cat "${CMAKE_STAMP}")" == "${CMAKE_ID}" ]]; then
  echo "cmake cache is current; skipped reconfigure"
else
  cmake -G Ninja -S "${SRC}" -B "${BUILD}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_ASSERTIONS=ON \
    -DLLVM_TARGETS_TO_BUILD=XAI \
    -DLLVM_ENABLE_PROJECTS="" \
    -DLLVM_OPTIMIZED_TABLEGEN=ON \
    -DLLVM_INCLUDE_BENCHMARKS=OFF \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_TESTS=ON \
    -DLLVM_PARALLEL_LINK_JOBS=8
  mkdir -p "${BUILD}"
  printf '%s\n' "${CMAKE_ID}" > "${CMAKE_STAMP}"
fi

# llvm-lit is written by CMake configure_file and is not a ninja target
# in this LLVM revision. FileCheck, not, and count are.
if command -v rtk >/dev/null 2>&1; then
  rtk ninja -C "${BUILD}" llc llvm-config FileCheck not count
else
  ninja -C "${BUILD}" llc llvm-config FileCheck not count
fi
