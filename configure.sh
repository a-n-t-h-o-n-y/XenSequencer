#!/usr/bin/env bash
set -euo pipefail

preset="dev"

case "${1:-}" in
  dev)
    preset="dev"
    shift
    ;;
  release)
    preset="release"
    shift
    ;;
  -h|--help)
    cat <<'USAGE'
usage: ./configure.sh [dev|release] [cmake args...]

Defaults to the dev preset:
  ./configure.sh

Common overrides:
  CC=clang CXX=clang++ ./configure.sh
  ./configure.sh -DXEN_WEB_UI_DEV_URL=http://127.0.0.1:5173
  ./configure.sh -DXEN_WEBVIEW_HARDWARE_ACCELERATION_POLICY=never
  ./configure.sh release -DXEN_WEB_UI_DIST_DIR=/path/to/xen-frontend/dist
USAGE
    exit 0
    ;;
esac

cmake --preset "${preset}" "$@"
