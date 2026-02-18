#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="build-debug-embedded"
LLVM21_BIN="${LLVM21_BIN:-/opt/local/libexec/llvm-21/bin}"
CMAKE_C_COMPILER_BIN="${CMAKE_C_COMPILER:-${LLVM21_BIN}/clang}"
CMAKE_CXX_COMPILER_BIN="${CMAKE_CXX_COMPILER:-${LLVM21_BIN}/clang++}"
CMAKE_AR_BIN="${CMAKE_AR:-${LLVM21_BIN}/llvm-ar}"
CMAKE_RANLIB_BIN="${CMAKE_RANLIB:-${LLVM21_BIN}/llvm-ranlib}"
DEFAULT_WEB_UI_DIST_DIR="${ROOT_DIR}/../xen-frontend/dist"
WEB_UI_DIST_DIR="${XEN_WEB_UI_DIST_DIR:-${DEFAULT_WEB_UI_DIST_DIR}}"

# Optional first positional arg overrides the build directory.
if [[ $# -gt 0 && "${1}" != -* ]]; then
  BUILD_DIR="$1"
  shift
fi

# Optional second positional arg overrides XEN_WEB_UI_DIST_DIR.
if [[ $# -gt 0 && "${1}" != -* ]]; then
  WEB_UI_DIST_DIR="$1"
  shift
fi

if [[ ! -d "${WEB_UI_DIST_DIR}" ]]; then
  echo "error: web UI dist directory not found: ${WEB_UI_DIST_DIR}" >&2
  echo "default: ${DEFAULT_WEB_UI_DIST_DIR}" >&2
  echo "usage: $0 [build-dir] [web-ui-dist-dir] [cmake args...]" >&2
  exit 1
fi

if [[ ! -f "${WEB_UI_DIST_DIR}/index.html" ]]; then
  echo "error: ${WEB_UI_DIST_DIR} must contain index.html" >&2
  exit 1
fi

for tool in \
  "${CMAKE_C_COMPILER_BIN}" \
  "${CMAKE_CXX_COMPILER_BIN}" \
  "${CMAKE_AR_BIN}" \
  "${CMAKE_RANLIB_BIN}"; do
  if [[ ! -x "${tool}" ]]; then
    echo "error: required LLVM tool not found or not executable: ${tool}" >&2
    echo "set LLVM21_BIN or CMAKE_* env vars to override" >&2
    exit 1
  fi
done

cmake \
  -S "${ROOT_DIR}" \
  -B "${ROOT_DIR}/${BUILD_DIR}" \
  -G Ninja \
  -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER_BIN}" \
  -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER_BIN}" \
  -DCMAKE_AR="${CMAKE_AR_BIN}" \
  -DCMAKE_RANLIB="${CMAKE_RANLIB_BIN}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DXEN_WEB_UI_MODE=EMBEDDED \
  -DXEN_WEB_UI_DIST_DIR="${WEB_UI_DIST_DIR}" \
  -DXEN_BUILD_TESTS=ON \
  -DXEN_BUILD_AUDIO_PLUGIN_HOST=ON \
  -DXEN_BUILD_TOOLS=ON \
  "$@"

echo "Configured Debug build in ${BUILD_DIR}"
echo "Configured Web UI mode: EMBEDDED"
echo "Using LLVM toolchain from ${LLVM21_BIN}"
echo "Build with: cmake --build ${ROOT_DIR}/${BUILD_DIR}"
