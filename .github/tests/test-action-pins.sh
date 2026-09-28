#!/bin/bash
# Guard for the third-party action pins in .github/workflows.
#
# Every remote `uses:` must name a 40-hex commit SHA followed by a comment
# carrying the version it resolves to, and each action may appear under one
# SHA only. A floating tag can be moved under us; a missing comment hides
# which release a SHA is; two SHAs for one action means one call site was
# left behind on an upgrade, which is how a deprecated runtime survives.
#
# The checker first runs against synthetic bad workflows to prove it rejects
# each case, so a regex bug cannot turn it into a silent pass.
set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Prints one line per violation found under the given directory.
check_dir() {
  local dir="$1" file lineno line ref action rest key prev
  local -A seen=()
  for file in "$dir"/*.yml "$dir"/*.yaml; do
    [ -f "$file" ] || continue
    lineno=0
    while IFS= read -r line || [ -n "$line" ]; do
      lineno=$((lineno + 1))
      line=${line%$'\r'}
      [[ "$line" =~ ^[[:space:]]*(-[[:space:]]+)?uses:[[:space:]]*([^[:space:]#]+)(.*)$ ]] || continue
      ref=${BASH_REMATCH[2]}
      rest=${BASH_REMATCH[3]}
      ref=${ref//\"/}
      ref=${ref//\'/}
      # Local actions and docker images are not tag-addressed GitHub refs.
      case "$ref" in ./* | docker://*) continue ;; esac
      key="$(basename "$file"):$lineno"
      if [[ "$ref" != *@* ]]; then
        echo "$key: $ref has no ref at all"
        continue
      fi
      action=${ref%@*}
      if ! [[ "${ref##*@}" =~ ^[0-9a-f]{40}$ ]]; then
        echo "$key: $ref is not pinned to a 40-hex commit SHA"
        continue
      fi
      if ! [[ "$rest" =~ ^[[:space:]]+#[[:space:]]*v[0-9]+(\.[0-9]+)*[[:space:]]*$ ]]; then
        echo "$key: $ref lacks a '# vX[.Y.Z]' version comment"
      fi
      prev=${seen[$action]:-}
      if [ -z "$prev" ]; then
        seen[$action]="${ref##*@} $key"
      elif [ "${prev%% *}" != "${ref##*@}" ]; then
        echo "$key: $action pinned to ${ref##*@}, but ${prev#* } pins ${prev%% *}"
      fi
    done < "$file"
  done
}

FAILURES=0
fail() {
  echo "FAIL: $1" >&2
  FAILURES=$((FAILURES + 1))
}

TMPDIR_T=$(mktemp -d)
trap 'rm -rf "$TMPDIR_T"' EXIT

SHA_A=0123456789abcdef0123456789abcdef01234567
SHA_B=89abcdef0123456789abcdef0123456789abcdef

# Each fixture is one bad workflow; the checker must report something for it.
expect_rejected() {
  local name="$1" body="$2" out
  mkdir -p "$TMPDIR_T/$name"
  printf '%s\n' "$body" > "$TMPDIR_T/$name/wf.yml"
  out=$(check_dir "$TMPDIR_T/$name")
  if [ -z "$out" ]; then
    fail "fixture '$name' was accepted but must be rejected"
  fi
}

expect_rejected floating-tag "      - uses: actions/cache@v4"
expect_rejected floating-branch "        uses: owner/action@main # v1"
expect_rejected no-comment "      - uses: actions/cache@$SHA_A"
expect_rejected non-version-comment "      - uses: actions/cache@$SHA_A # latest"
expect_rejected short-sha "      - uses: actions/cache@${SHA_A:0:12} # v4"
expect_rejected two-shas "      - uses: actions/cache@$SHA_A # v4
      - uses: actions/cache@$SHA_B # v4"

mkdir -p "$TMPDIR_T/good"
printf '%s\n' \
  "      - uses: actions/cache@$SHA_A # v6.1.0" \
  "        uses: actions/cache@$SHA_A # v6.1.0" \
  "      - uses: ./.github/actions/local" \
  > "$TMPDIR_T/good/wf.yml"
GOOD_OUT=$(check_dir "$TMPDIR_T/good")
if [ -n "$GOOD_OUT" ]; then
  fail "a correctly pinned fixture was rejected: $GOOD_OUT"
fi

WORKFLOWS="$REPO_ROOT/.github/workflows"
USES_COUNT=$(cat "$WORKFLOWS"/*.yml | grep -cE '^[[:space:]]*(-[[:space:]]+)?uses:' || true)
if [ "$USES_COUNT" -eq 0 ]; then
  fail "found no uses: lines under $WORKFLOWS; the scan itself is broken"
fi

REAL_OUT=$(check_dir "$WORKFLOWS")
if [ -n "$REAL_OUT" ]; then
  while IFS= read -r violation; do
    fail "$violation"
  done <<< "$REAL_OUT"
fi

if [ "$FAILURES" -ne 0 ]; then
  echo "$FAILURES action pin check(s) failed" >&2
  exit 1
fi
echo "OK: $USES_COUNT uses: lines pinned by SHA with a version comment, one SHA per action"
