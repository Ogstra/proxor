# Shared by libs/build_deps_all.sh and libs/tests/test-dependency-pins.sh.
# Sourced, never executed: keep this file free of side effects.
#
# Everything this repository links into a shipped binary is verified before it is
# used. shasum -a 256 is the tool because it exists in Git Bash on the Windows
# runner as well as on Linux and macOS; sha256sum does not.
fetch_verified() {
  fv_url=$1
  fv_expected=$2
  fv_out=$3

  # -f so an HTTP error page is an error here instead of a confusing tar failure
  # three lines later; --retry for the flaky-mirror case, not for a bad hash.
  if ! curl -fL --retry 3 --retry-delay 2 -o "$fv_out" "$fv_url"; then
    echo "download failed: $fv_url" >&2
    rm -f "$fv_out"
    return 1
  fi

  fv_actual=$(shasum -a 256 "$fv_out" | cut -d' ' -f1)
  if [ "$fv_actual" != "$fv_expected" ]; then
    echo "checksum mismatch, refusing to build $fv_url" >&2
    echo "  expected: $fv_expected" >&2
    echo "  actual:   $fv_actual" >&2
    rm -f "$fv_out"
    return 1
  fi

  echo "verified $fv_out sha256 $fv_actual"
}
