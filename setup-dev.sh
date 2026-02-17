#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="build"

# Optional first positional arg overrides the build directory.
if [[ $# -gt 0 && "${1}" != -* ]]; then
  BUILD_DIR="$1"
  shift
fi

cmake \
  -S "${ROOT_DIR}" \
  -B "${ROOT_DIR}/${BUILD_DIR}" \
  -G Ninja \
  -DXEN_BUILD_TESTS=ON \
  -DXEN_BUILD_AUDIO_PLUGIN_HOST=ON \
  -DXEN_BUILD_TOOLS=ON \
  "$@"

echo "Configured ${BUILD_DIR}"
echo "Build with: cmake --build ${ROOT_DIR}/${BUILD_DIR}"
