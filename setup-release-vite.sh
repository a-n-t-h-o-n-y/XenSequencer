#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="build-release-vite"
LLVM22_BIN="${LLVM22_BIN:-/opt/local/libexec/llvm-22/bin}"
CMAKE_C_COMPILER_BIN="${CMAKE_C_COMPILER:-${LLVM22_BIN}/clang}"
CMAKE_CXX_COMPILER_BIN="${CMAKE_CXX_COMPILER:-${LLVM22_BIN}/clang++}"
CMAKE_AR_BIN="${CMAKE_AR:-${LLVM22_BIN}/llvm-ar}"
CMAKE_RANLIB_BIN="${CMAKE_RANLIB:-${LLVM22_BIN}/llvm-ranlib}"
WEB_UI_DEV_URL="${XEN_WEB_UI_DEV_URL:-http://127.0.0.1:5173,http://localhost:5173}"

# Optional first positional arg overrides the build directory.
if [[ $# -gt 0 && "${1}" != -* ]]; then
  BUILD_DIR="$1"
  shift
fi

# Optional second positional arg overrides the ordered XEN_WEB_UI_DEV_URL list.
if [[ $# -gt 0 && "${1}" != -* ]]; then
  WEB_UI_DEV_URL="$1"
  shift
fi

for tool in \
  "${CMAKE_C_COMPILER_BIN}" \
  "${CMAKE_CXX_COMPILER_BIN}" \
  "${CMAKE_AR_BIN}" \
  "${CMAKE_RANLIB_BIN}"; do
  if [[ ! -x "${tool}" ]]; then
    echo "error: required LLVM tool not found or not executable: ${tool}" >&2
    echo "set LLVM22_BIN or CMAKE_* env vars to override" >&2
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
  -DCMAKE_BUILD_TYPE=Release \
  -DXEN_WEB_UI_MODE=DEV_SERVER \
  -DXEN_WEB_UI_DEV_URL="${WEB_UI_DEV_URL}" \
  -DXEN_BUILD_TESTS=OFF \
  -DXEN_BUILD_AUDIO_PLUGIN_HOST=OFF \
  -DXEN_BUILD_TOOLS=OFF \
  "$@"

echo "Configured Release build in ${BUILD_DIR}"
echo "Configured Web UI mode: DEV_SERVER"
echo "Using LLVM toolchain from ${LLVM22_BIN}"
echo "Configured dev-server URLs: ${WEB_UI_DEV_URL}"
echo "Build with: cmake --build ${ROOT_DIR}/${BUILD_DIR}"
