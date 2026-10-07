#!/usr/bin/env bash
# Materialize third_party/llvm-project from the pinned upstream commit.
#
# The git object store lives in third_party/cache and is never patched.
# The build tree is a pristine archive plus patches/series plus this
# repository's llvm/ overlay. It has no .git directory.
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/llvm_vendor.sh"
vendor_init "$0"
ensure_cache

if [[ -d "${TREE}/.git" ]]; then
  echo "refusing to patch a tree that still contains .git: ${TREE}" >&2
  exit 1
fi

if [[ "${XAI_LLVM_RESET:-}" == "1" ]]; then
  rematerialize_tree
  echo "reset llvm-project at ${REV}"
  exit 0
fi

if [[ -d "${TREE}/llvm" ]] && stamp_is_current; then
  if ! overlay_matches; then
    echo "llvm/ does not match patches/${ADD_PATCH}." >&2
    echo "Record the edits with scripts/update_llvm_patch.sh." >&2
    exit 1
  fi
  echo "patches already applied at ${REV}; skipped"
  exit 0
fi

if [[ ! -d "${TREE}/llvm" ]]; then
  rematerialize_tree
  echo "materialized llvm-project at ${REV}"
  exit 0
fi

if tree_is_pristine; then
  apply_series "${TREE}"
  write_stamp
  echo "applied patches/series onto ${REV}"
  exit 0
fi

if upstream_files_match_series; then
  write_stamp
  echo "tree already contains patches/series; recorded ${STAMP}"
  exit 0
fi

if [[ -f "${STAMP}" ]]; then
  rematerialize_tree
  echo "reapplied patches/series onto ${REV}"
  exit 0
fi

echo "third_party/llvm-project does not match patches/series, and it has no stamp." >&2
echo "Record intentional edits with scripts/update_llvm_patch.sh, or move the tree aside and rerun setup." >&2
exit 1
