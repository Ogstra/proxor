#!/usr/bin/env bash
set -euo pipefail

test_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
build_dir="${PACKAGE_MODE_BUILD_DIR:-$test_dir/.build}"

rm -rf "$build_dir"
cmake_args=(-S "$test_dir" -B "$build_dir")
if [ -n "${QT_ROOT_DIR:-}" ]; then
    cmake_args+=(-DCMAKE_PREFIX_PATH="$QT_ROOT_DIR")
elif [ -n "${CMAKE_PREFIX_PATH:-}" ]; then
    cmake_args+=(-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH")
fi

cmake "${cmake_args[@]}"
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
