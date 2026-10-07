#!/usr/bin/env bash
# Rewrite patches/series from the pinned commit.
# Edited upstream files come from the materialized tree.
# Added files come from this repository's llvm/ tree.
# Any other source difference is rejected.
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/llvm_vendor.sh"
vendor_init "$0"
ensure_cache

if [[ ! -d "${TREE}/llvm" ]]; then
  echo "missing materialized tree: ${TREE}" >&2
  echo "run scripts/setup_llvm.sh first" >&2
  exit 1
fi

for patch in "${HOOK_PATCH}" "${ADD_PATCH}"; do
  listed=0
  for item in "${SERIES_PATCHES[@]}"; do
    [[ "${item}" == "${patch}" ]] && listed=1
  done
  if [[ "${listed}" -ne 1 ]]; then
    echo "patches/series does not list ${patch}" >&2
    exit 1
  fi
done

stage="$(mktemp -d "${ROOT}/third_party/.llvm-stage.XXXXXX")"
changes="$(mktemp)"
trap 'rm -rf "${stage}" "${changes}"' EXIT
export_pristine "${stage}"

set +e
diff -rq -x build -x .xai-stamp "${stage}" "${TREE}" > "${changes}"
rc=$?
set -e
if [[ "${rc}" -eq 2 ]]; then
  echo "failed to compare the pristine tree with ${TREE}" >&2
  exit 1
fi

while IFS= read -r row; do
  [[ -z "${row}" ]] && continue
  rel=""
  if [[ "${row}" == Files\ "${stage}/"* ]]; then
    rel="${row#Files ${stage}/}"
    rel="${rel%% and *}"
  elif [[ "${row}" == "Only in ${TREE}/"* ]]; then
    rest="${row#Only in ${TREE}/}"
    dir="${rest%%:*}"
    base="${rest#*: }"
    rel="${dir}/${base}"
  elif [[ "${row}" == "Only in ${stage}/"* ]]; then
    rest="${row#Only in ${stage}/}"
    dir="${rest%%:*}"
    base="${rest#*: }"
    rel="${dir}/${base}"
  else
    echo "unrecognized diff line: ${row}" >&2
    exit 1
  fi
  if ! path_is_allowed "${rel}"; then
    echo "change is not listed in patches/upstream-files.txt: ${rel}" >&2
    exit 1
  fi
done < "${changes}"

hook_body="$(mktemp)"
trap 'rm -rf "${stage}" "${changes}" "${hook_body}"' EXIT
found=0
for path in "${HOOK_PATHS[@]}"; do
  set +e
  hunk="$(diff -u --label "a/${path}" --label "b/${path}" "${stage}/${path}" "${TREE}/${path}")"
  rc=$?
  set -e
  if [[ "${rc}" -eq 2 ]]; then
    echo "diff failed for ${path}" >&2
    exit 1
  fi
  if [[ "${rc}" -eq 1 ]]; then
    found=1
    {
      printf 'diff --git a/%s b/%s\n' "${path}" "${path}"
      printf '%s\n' "${hunk}"
    } >> "${hook_body}"
  fi
done
if [[ "${found}" -ne 1 ]]; then
  echo "no differences in the edited upstream files" >&2
  exit 1
fi

repo_added="$(added_file_list "${ROOT}")"
tree_added="$(added_file_list "${TREE}")"
if [[ "${repo_added}" != "${tree_added}" ]]; then
  echo "llvm/ and the materialized tree do not contain the same added files" >&2
  exit 1
fi
while IFS= read -r path; do
  [[ -z "${path}" ]] && continue
  if ! cmp -s "${ROOT}/${path}" "${TREE}/${path}"; then
    echo "added file differs between llvm/ and the materialized tree: ${path}" >&2
    exit 1
  fi
done <<< "${repo_added}"

{
  cat <<EOF
Subject: [PATCH] Register the XAI architecture with upstream LLVM

Selecting -mtriple=xai requires in-tree hooks: the architecture enum and
its names, the data layout, LLVM_ALL_TARGETS, and the intrinsic TableGen
include. Files that do not exist upstream are added by the next patch in
the series.

Upstream-revision: ${REV}

---
EOF
  cat "${hook_body}"
} > "${ROOT}/patches/${HOOK_PATCH}"

write_added_patch "${ROOT}/patches/${ADD_PATCH}"

probe="$(mktemp -d)"
trap 'rm -rf "${stage}" "${changes}" "${hook_body}" "${probe}"' EXIT
export_paths "${probe}" "${HOOK_PATHS[@]}"
apply_series "${probe}" >/dev/null
echo "updated patches/${HOOK_PATCH}"
echo "updated patches/${ADD_PATCH}"
