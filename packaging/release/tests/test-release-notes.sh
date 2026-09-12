#!/usr/bin/env bash
set -euo pipefail

# The release body is rendered by the hand-rolled parser in the version the user is
# already running (docs/Release_Notes_Format.md), so the notes ship in the repository
# and are checked against the failure modes that parser actually has.
root="$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)"
version="$(tr -d '\n\r' < "$root/VERSION.txt")"
notes="$root/packaging/release/notes/v$version.md"

fail() { printf '%s: %s\n' "$notes" "$1" >&2; exit 1; }
reject() { if grep -Eq "$1" "$notes"; then fail "$2"; fi; }

test -s "$notes" || fail 'release notes are required'

# House style: a single themed title, then the Added/Changed/Fixed groups.
if ! head -n1 "$notes" | grep -Eq '^# [^[:space:]#]'; then
  fail 'must open with a "# Theme" title'
fi
if tail -n +2 "$notes" | grep -Eq '^# '; then
  fail 'only the title may be a level-one heading'
fi
if ! grep -Eq '^## (Added|Changed|Fixed)$' "$notes"; then
  fail 'needs an Added, Changed or Fixed group'
fi
if grep -E '^## ' "$notes" | grep -Evq '^## (Added|Changed|Fixed)$'; then
  fail 'unexpected group heading'
fi

# Parser limits: an indented or nested marker breaks out of the list, tables and
# blockquotes render as literal punctuation, and a lone asterisk opens emphasis.
reject '^[[:space:]]+[-*+] ' 'indented list markers render as text'
reject '\|' 'tables are not supported'
reject '^[[:space:]]*> ' 'blockquotes are not supported'
reject '(^|[[:space:]])\*([[:space:]]|$)' 'a lone asterisk opens emphasis'
