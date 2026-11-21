#!/usr/bin/env bash
# Configure, build and test one preset on Linux.
# Usage: scripts/build.sh [preset] [configure|build|test|all]
#   preset  linux-debug (default) | linux-release | linux-asan
# Env overrides:
#   QT_PREFIX  Qt install to use (e.g. ~/Qt/6.12.1/gcc_64); default is the
#              distro's Qt 6 (qt6-base-dev etc.)
#   CXX / CC   compiler (default: the system c++)
#   BUILD_DIR  build tree (default: build/<preset>); point it at the Linux
#              filesystem when the checkout lives on a slow mount such as
#              /mnt/c under WSL
# GUI tests run with QT_QPA_PLATFORM=offscreen (set by CTest), so no display is
# needed.
set -euo pipefail

preset="${1:-linux-debug}"
step="${2:-all}"
cd "$(dirname "${BASH_SOURCE[0]}")/.."

extra=()
if [[ -n "${QT_PREFIX:-}" ]]; then
  extra+=("-DCMAKE_PREFIX_PATH=${QT_PREFIX}")
fi
if [[ -n "${CXX:-}" ]]; then
  extra+=("-DCMAKE_CXX_COMPILER=${CXX}")
fi
if [[ -n "${BUILD_DIR:-}" ]]; then
  extra+=("-B" "${BUILD_DIR}")
fi

case "${step}" in
  configure|build|test|all) ;;
  *) echo "error: step must be configure, build, test or all, got '${step}'" >&2; exit 1 ;;
esac

if [[ "${step}" == "configure" || "${step}" == "all" ]]; then
  cmake --preset "${preset}" "${extra[@]}"
fi
if [[ "${step}" == "build" || "${step}" == "all" ]]; then
  if [[ -n "${BUILD_DIR:-}" ]]; then cmake --build "${BUILD_DIR}"; else cmake --build --preset "${preset}"; fi
fi
if [[ "${step}" == "test" || "${step}" == "all" ]]; then
  if [[ -n "${BUILD_DIR:-}" ]]; then ctest --test-dir "${BUILD_DIR}" --output-on-failure; else ctest --preset "${preset}"; fi
fi
