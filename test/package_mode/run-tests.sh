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
ctest_args=(--test-dir "$build_dir" --output-on-failure)
if [ "${RUNNER_OS:-}" = "Windows" ]; then
    ctest_args+=(--build-config Debug)
fi
ctest "${ctest_args[@]}"

# The unit tests above prove the policy is correct; this grep-level contract
# proves it is actually called from production code. It is shell-only and
# portable, so it rides along with the unit tests on every runner.
bash "$test_dir/test-policy-wiring.sh"
