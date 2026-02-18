#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="build-debug"
LLVM21_BIN="${LLVM21_BIN:-/opt/local/libexec/llvm-21/bin}"
CMAKE_C_COMPILER_BIN="${CMAKE_C_COMPILER:-${LLVM21_BIN}/clang}"
CMAKE_CXX_COMPILER_BIN="${CMAKE_CXX_COMPILER:-${LLVM21_BIN}/clang++}"
CMAKE_AR_BIN="${CMAKE_AR:-${LLVM21_BIN}/llvm-ar}"
CMAKE_RANLIB_BIN="${CMAKE_RANLIB:-${LLVM21_BIN}/llvm-ranlib}"

# Optional first positional arg overrides the build directory.
if [[ $# -gt 0 && "${1}" != -* ]]; then
  BUILD_DIR="$1"
  shift
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
  -DXEN_BUILD_TESTS=ON \
  -DXEN_BUILD_AUDIO_PLUGIN_HOST=ON \
  -DXEN_BUILD_TOOLS=ON \
  "$@"

echo "Configured Debug build in ${BUILD_DIR}"
echo "Using LLVM toolchain from ${LLVM21_BIN}"
echo "Build with: cmake --build ${ROOT_DIR}/${BUILD_DIR}"
