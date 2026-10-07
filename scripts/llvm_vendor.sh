# Shared helpers for fetching the pinned llvm-project commit and applying
# patches/series onto a tree that is not itself a git repository.
# Sourced by setup_llvm.sh and update_llvm_patch.sh.

vendor_init() {
  local caller="$1"
  export LC_ALL=C
  ROOT="$(cd "$(dirname "$caller")/.." && pwd)"
  REV="$(tr -d '[:space:]' < "${ROOT}/patches/llvm-project.rev")"
  SERIES_FILE="${ROOT}/patches/series"
  ALLOW_FILE="${ROOT}/patches/upstream-files.txt"
  GIT_DIR="${ROOT}/third_party/cache/llvm-project.git"
  TREE="${ROOT}/third_party/llvm-project"
  STAMP="${TREE}/.xai-stamp"
  HOOK_PATCH="0001-register-xai-architecture.patch"
  ADD_PATCH="0002-add-xai-backend.patch"

  if [[ ! -s "${ROOT}/patches/llvm-project.rev" ]]; then
    echo "missing patches/llvm-project.rev" >&2
    exit 1
  fi

  SERIES_PATCHES=()
  local line
  while IFS= read -r line || [[ -n "${line}" ]]; do
    line="${line%%#*}"
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    [[ -z "${line}" ]] && continue
    SERIES_PATCHES+=("${line}")
  done < "${SERIES_FILE}"
  if [[ ${#SERIES_PATCHES[@]} -eq 0 ]]; then
    echo "patches/series is empty" >&2
    exit 1
  fi

  HOOK_PATHS=()
  ADDED_PATHS=()
  while IFS= read -r line || [[ -n "${line}" ]]; do
    line="${line%%#*}"
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    [[ -z "${line}" ]] && continue
    if [[ "${line}" == +* ]]; then
      ADDED_PATHS+=("${line#+}")
    else
      HOOK_PATHS+=("${line}")
    fi
  done < "${ALLOW_FILE}"
  if [[ ${#HOOK_PATHS[@]} -eq 0 || ${#ADDED_PATHS[@]} -eq 0 ]]; then
    echo "patches/upstream-files.txt must list both edited files and +added paths" >&2
    exit 1
  fi
}

path_is_allowed() {
  local path="$1" item
  for item in "${HOOK_PATHS[@]}"; do
    [[ "${item}" == "${path}" ]] && return 0
  done
  for item in "${ADDED_PATHS[@]}"; do
    if [[ "${item}" == */ ]]; then
      [[ "${path}" == "${item%/}" || "${path}" == "${item}"* ]] && return 0
    else
      [[ "${item}" == "${path}" ]] && return 0
    fi
  done
  return 1
}

added_file_list() {
  local root="$1" spec path
  for spec in "${ADDED_PATHS[@]}"; do
    path="${root}/${spec%/}"
    if [[ -d "${path}" ]]; then
      find "${path}" -type f | sed "s|^${root}/||" | sort
    elif [[ -f "${path}" ]]; then
      printf '%s\n' "${spec}"
    fi
  done
}

install_cache_hook() {
  local hook="${GIT_DIR}/hooks/pre-commit"
  mkdir -p "${GIT_DIR}/hooks"
  cat > "${hook}" <<'EOF'
#!/bin/sh
echo "third_party/cache stores the pristine llvm-project commit. Commit XAI changes in the XAIBackend repository." >&2
exit 1
EOF
  chmod +x "${hook}"
}

ensure_cache() {
  if [[ -d "${TREE}/.git" && ! -d "${GIT_DIR}" ]]; then
    mkdir -p "$(dirname "${GIT_DIR}")"
    mv "${TREE}/.git" "${GIT_DIR}"
    echo "moved the llvm-project git directory to third_party/cache/llvm-project.git"
  fi

  if [[ ! -d "${GIT_DIR}" ]]; then
    local work="${ROOT}/third_party/cache/llvm-fetch"
    rm -rf "${work}"
    mkdir -p "${work}"
    git -C "${work}" init
    git -C "${work}" remote add origin https://github.com/llvm/llvm-project.git
    git -C "${work}" sparse-checkout init --cone
    git -C "${work}" sparse-checkout set llvm cmake third-party
    git -C "${work}" fetch --depth 1 origin "${REV}"
    git -C "${work}" checkout --detach FETCH_HEAD
    if [[ ! -f "${work}/third-party/siphash/include/siphash/SipHash.h" ]]; then
      echo "sparse checkout is missing third-party/siphash" >&2
      exit 1
    fi
    mkdir -p "$(dirname "${GIT_DIR}")"
    mv "${work}/.git" "${GIT_DIR}"
    rm -rf "${work}"
  fi

  if ! git --git-dir="${GIT_DIR}" rev-parse --verify HEAD >/dev/null 2>&1; then
    echo "unusable llvm-project cache: ${GIT_DIR}" >&2
    exit 1
  fi

  local head
  head="$(git --git-dir="${GIT_DIR}" rev-parse HEAD)"
  if [[ "${head}" != "${REV}" ]]; then
    echo "cache HEAD is ${head}; fetching ${REV}" >&2
    if ! git --git-dir="${GIT_DIR}" remote get-url origin >/dev/null 2>&1; then
      git --git-dir="${GIT_DIR}" remote add origin https://github.com/llvm/llvm-project.git
    fi
    git --git-dir="${GIT_DIR}" fetch --depth 1 origin "${REV}"
    git --git-dir="${GIT_DIR}" update-ref HEAD FETCH_HEAD
    head="$(git --git-dir="${GIT_DIR}" rev-parse HEAD)"
    if [[ "${head}" != "${REV}" ]]; then
      echo "fetch did not produce ${REV}" >&2
      exit 1
    fi
  fi
  install_cache_hook
}

export_paths() {
  local dest="$1"
  shift
  mkdir -p "${dest}"
  git --git-dir="${GIT_DIR}" archive --format=tar "${REV}" "$@" | tar -C "${dest}" -xf -
}

export_pristine() {
  local dest="$1"
  export_paths "${dest}" llvm cmake third-party
}

apply_one() {
  local dest="$1" patch="$2" mode="$3" log
  # The build tree lives inside this repository but is not part of it.
  # Without a ceiling, git apply finds the parent repo, skips every path,
  # and still exits 0.
  log="$(mktemp)"
  if ! GIT_CEILING_DIRECTORIES="$(dirname "${dest}")" \
      git -C "${dest}" apply --verbose ${mode} "${ROOT}/patches/${patch}" \
      >"${log}" 2>&1; then
    cat "${log}" >&2
    rm -f "${log}"
    return 1
  fi
  if grep -q '^Skipped patch' "${log}" || ! grep -q -e '^Checking patch ' -e '^Applied patch ' "${log}"; then
    echo "git apply did not apply patches/${patch}" >&2
    cat "${log}" >&2
    rm -f "${log}"
    return 1
  fi
  rm -f "${log}"
}

apply_series() {
  local dest="$1" patch
  for patch in "${SERIES_PATCHES[@]}"; do
    if [[ ! -s "${ROOT}/patches/${patch}" ]]; then
      echo "missing patches/${patch}" >&2
      return 1
    fi
    apply_one "${dest}" "${patch}" --check
    apply_one "${dest}" "${patch}" ""
    echo "applied patches/${patch}"
  done
}

stamp_text() {
  local patch sum
  echo "${REV}"
  for patch in "${SERIES_PATCHES[@]}"; do
    sum="$(sha256sum "${ROOT}/patches/${patch}" | awk '{print $1}')"
    echo "${patch} ${sum}"
  done
}

write_stamp() {
  mkdir -p "${TREE}"
  stamp_text > "${STAMP}"
}

stamp_is_current() {
  [[ -f "${STAMP}" ]] || return 1
  local expected actual
  expected="$(stamp_text)"
  actual="$(cat "${STAMP}")"
  [[ "${expected}" == "${actual}" ]]
}

upstream_files_match_series() {
  local probe path rel
  probe="$(mktemp -d)"
  export_paths "${probe}" "${HOOK_PATHS[@]}"
  if ! apply_series "${probe}" >/dev/null; then
    rm -rf "${probe}"
    return 1
  fi
  for path in "${HOOK_PATHS[@]}"; do
    if ! cmp -s "${probe}/${path}" "${TREE}/${path}"; then
      rm -rf "${probe}"
      return 1
    fi
  done
  while IFS= read -r rel; do
    [[ -z "${rel}" ]] && continue
    if ! cmp -s "${probe}/${rel}" "${TREE}/${rel}" || ! cmp -s "${probe}/${rel}" "${ROOT}/${rel}"; then
      rm -rf "${probe}"
      return 1
    fi
  done < <(added_file_list "${probe}")
  local repo_list probe_list
  repo_list="$(added_file_list "${ROOT}")"
  probe_list="$(added_file_list "${probe}")"
  rm -rf "${probe}"
  [[ "${repo_list}" == "${probe_list}" ]]
}

write_added_patch() {
  local out="$1" path mode rc
  {
    cat <<EOF
Subject: [PATCH] Add the XAI backend sources

These paths do not exist in the pinned llvm-project commit: the XAI
target, IntrinsicsXAI.td, and the CodeGen tests. The paths are listed
with a '+' prefix in patches/upstream-files.txt.

Upstream-revision: ${REV}

---
EOF
    while IFS= read -r path; do
      [[ -z "${path}" ]] && continue
      mode=100644
      [[ -x "${ROOT}/${path}" ]] && mode=100755
      printf 'diff --git a/%s b/%s\n' "${path}" "${path}"
      printf 'new file mode %s\n' "${mode}"
      set +e
      diff -u --label /dev/null --label "b/${path}" /dev/null "${ROOT}/${path}"
      rc=$?
      set -e
      if [[ "${rc}" -eq 2 ]]; then
        echo "failed to diff ${path}" >&2
        return 1
      fi
    done < <(added_file_list "${ROOT}")
  } > "${out}"
}

overlay_matches() {
  local repo_list tree_list path
  repo_list="$(added_file_list "${ROOT}")"
  tree_list="$(added_file_list "${TREE}")"
  [[ "${repo_list}" == "${tree_list}" ]] || return 1
  while IFS= read -r path; do
    [[ -z "${path}" ]] && continue
    cmp -s "${ROOT}/${path}" "${TREE}/${path}" || return 1
  done <<< "${repo_list}"
}

tree_is_pristine() {
  local probe path added
  probe="$(mktemp -d)"
  export_paths "${probe}" "${HOOK_PATHS[@]}"
  for path in "${HOOK_PATHS[@]}"; do
    if ! cmp -s "${probe}/${path}" "${TREE}/${path}"; then
      rm -rf "${probe}"
      return 1
    fi
  done
  rm -rf "${probe}"
  added="$(added_file_list "${TREE}")"
  [[ -z "${added}" ]]
}

rematerialize_tree() {
  local stage
  stage="$(mktemp -d "${ROOT}/third_party/.llvm-stage.XXXXXX")"
  export_pristine "${stage}"
  apply_series "${stage}"
  if [[ -d "${TREE}/llvm" ]]; then
    rsync -a --checksum --delete \
      --exclude build \
      --exclude .xai-stamp \
      "${stage}/" "${TREE}/"
    rm -rf "${stage}"
  else
    mkdir -p "$(dirname "${TREE}")"
    rm -rf "${TREE}"
    mv "${stage}" "${TREE}"
  fi
  write_stamp
}
