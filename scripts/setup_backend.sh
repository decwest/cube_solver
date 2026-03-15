#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
THIRD_PARTY_ROOT="${1:-${REPO_ROOT}/third_party}"

SOLVER_REPO="https://github.com/dwalton76/rubiks-cube-NxNxN-solver.git"
SOLVER_COMMIT="c776db79314db3d98cc3dd99685ca85766656937"
SOLVER_DIR="${THIRD_PARTY_ROOT}/rubiks-cube-NxNxN-solver"

KOCIEMBA_REPO="https://github.com/dwalton76/kociemba.git"
KOCIEMBA_COMMIT="e1959d275e59c845ab63a8d29a3f9b3835d54eea"
KOCIEMBA_DIR="${THIRD_PARTY_ROOT}/kociemba"
KOCIEMBA_BIN="${KOCIEMBA_DIR}/kociemba/ckociemba/bin/kociemba"

clone_at_commit() {
  local repo_url="$1"
  local target_dir="$2"
  local commit="$3"

  if [[ ! -d "${target_dir}/.git" ]]; then
    git clone "${repo_url}" "${target_dir}"
  fi

  git -C "${target_dir}" fetch --depth 1 origin "${commit}"
  git -C "${target_dir}" checkout --detach "${commit}"
}

mkdir -p "${THIRD_PARTY_ROOT}"

clone_at_commit "${SOLVER_REPO}" "${SOLVER_DIR}" "${SOLVER_COMMIT}"
clone_at_commit "${KOCIEMBA_REPO}" "${KOCIEMBA_DIR}" "${KOCIEMBA_COMMIT}"

if [[ ! -x "${SOLVER_DIR}/ida_search_via_graph" ]]; then
  gcc -O3 \
    -o "${SOLVER_DIR}/ida_search_via_graph" \
    "${SOLVER_DIR}/rubikscubennnsolver/ida_search_core.c" \
    "${SOLVER_DIR}/rubikscubennnsolver/rotate_xxx.c" \
    "${SOLVER_DIR}/rubikscubennnsolver/ida_search_666.c" \
    "${SOLVER_DIR}/rubikscubennnsolver/ida_search_777.c" \
    "${SOLVER_DIR}/rubikscubennnsolver/ida_search_via_graph.c" \
    -lm
fi

if [[ ! -x "${KOCIEMBA_BIN}" ]]; then
  make -C "${KOCIEMBA_DIR}/kociemba/ckociemba"
fi

echo "backend ready"
