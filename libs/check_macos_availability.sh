#!/bin/bash
# Check the macOS sources against the macOS 12 availability floor.
#
# Compiles every object of the GUI target and its in-tree libraries (no link,
# nothing is run) at CMAKE_OSX_DEPLOYMENT_TARGET (default 12.0, the Intel
# build's floor) with -Werror=unguarded-availability and
# -Werror=unguarded-availability-new for C++ and Objective-C++, so an API newer
# than the floor used without @available fails the build. The diagnostics depend
# on the deployment target, not on the CPU, so Homebrew Qt on an Apple Silicon
# Mac is enough.
#
# Env knobs: BUILD_DIR (default /tmp/proxor-availability-check, must be outside
# the repo, wiped on every run), MACOS_FLOOR (default 12.0), CMAKE_PREFIX_PATH
# (default the Homebrew prefix), PROXOR_QT_DIR + NKR_LIBS (use these instead of
# Homebrew, as the CI Intel job does).
set -euo pipefail

#### guards ####
if [ "$(uname -s)" != "Darwin" ]; then
  echo "ERROR: libs/check_macos_availability.sh only runs on macOS." >&2
  exit 1
fi
if [ ! -f VERSION.txt ]; then
  echo "ERROR: run this from the repository root." >&2
  exit 1
fi
for tool in cmake ninja; do
  command -v "$tool" >/dev/null 2>&1 || { echo "ERROR: $tool not found." >&2; exit 1; }
done

BUILD_DIR="${BUILD_DIR:-/tmp/proxor-availability-check}"
MACOS_FLOOR="${MACOS_FLOOR:-12.0}"
REPO_ROOT="$PWD"

case "$BUILD_DIR/" in
  "$REPO_ROOT"/*)
    echo "ERROR: BUILD_DIR must be outside the repository ($BUILD_DIR)." >&2
    exit 1
    ;;
esac

WERR="-Werror=unguarded-availability -Werror=unguarded-availability-new"

if [ -n "${PROXOR_QT_DIR:-}" ] && [ -n "${NKR_LIBS:-}" ]; then
  PREFIX_ARGS=(-DCMAKE_PREFIX_PATH="$PROXOR_QT_DIR" -DNKR_LIBS="$NKR_LIBS")
else
  PREFIX="${CMAKE_PREFIX_PATH:-$(brew --prefix)}"
  PREFIX_ARGS=(-DNKR_DISABLE_LIBS=ON -DCMAKE_PREFIX_PATH="$PREFIX")
fi

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
LOG="$BUILD_DIR/availability.log"

#### configure ####
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DQT_VERSION_MAJOR=6 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS_FLOOR" \
  -DCMAKE_CXX_FLAGS="$WERR" \
  -DCMAKE_OBJCXX_FLAGS="$WERR" \
  "${PREFIX_ARGS[@]}" > "$BUILD_DIR/configure.log" 2>&1 || {
    echo "ERROR: cmake configure failed, see $BUILD_DIR/configure.log" >&2
    tail -20 "$BUILD_DIR/configure.log" >&2
    exit 1
  }

#### objects ####
ninja -C "$BUILD_DIR" -t targets all \
  | sed -n 's/^\(CMakeFiles\/[^:]*\.o\): .*/\1/p' > "$BUILD_DIR/objects.txt"
COUNT="$(wc -l < "$BUILD_DIR/objects.txt" | tr -d ' ')"
if [ "$COUNT" = 0 ]; then
  echo "ERROR: no object targets found." >&2
  exit 1
fi
for must in MacPlatform.mm.o MacLoginItem.mm.o; do
  grep -q "$must" "$BUILD_DIR/objects.txt" || { echo "ERROR: $must is not in the object list." >&2; exit 1; }
done

#### build (no link) ####
OBJS=()
while IFS= read -r line; do OBJS+=("$line"); done < "$BUILD_DIR/objects.txt"
if ! ninja -C "$BUILD_DIR" -k 0 "${OBJS[@]}" > "$LOG" 2>&1; then
  echo "ERROR: availability check failed at floor $MACOS_FLOOR:" >&2
  grep -E 'only available on macOS|unguarded-availability' "$LOG" | sort -u >&2 || true
  echo "(full log: $LOG)" >&2
  exit 1
fi

echo "AVAILABILITY-OK ($COUNT objects, floor $MACOS_FLOOR)"
