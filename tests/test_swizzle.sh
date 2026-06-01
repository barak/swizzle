#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

mkdir -p "$TMPDIR/project/foo/sub" \
         "$TMPDIR/project/bar/baz" \
         "$TMPDIR/common/foo/sub" \
         "$TMPDIR/alt/baaz"

printf 'project\n' >"$TMPDIR/project/foo/sub/value.txt"
printf 'redirected\n' >"$TMPDIR/common/foo/sub/value.txt"
printf 'local\n' >"$TMPDIR/project/bar/baz/value.txt"
printf 'alternate\n' >"$TMPDIR/alt/baaz/value.txt"

cd "$TMPDIR/project"

test "$("$ROOT/swizzle" foo:../common/foo -- "$ROOT/tests/probe" read foo/sub/value.txt)" = "redirected"
test "$("$ROOT/swizzle" foo:../common/foo -- "$ROOT/tests/probe" access foo/sub/value.txt)" = "ok"
test "$("$ROOT/swizzle" foo:../common/foo -- "$ROOT/tests/probe" stat foo/sub/value.txt)" = "11"
test "$("$ROOT/swizzle" bar/baz:"$TMPDIR/alt/baaz" -- "$ROOT/tests/probe" openat-read bar baz/value.txt)" = "alternate"
test "$("$ROOT/swizzle" foo:../common/foo bar/baz:"$TMPDIR/alt/baaz" -- "$ROOT/tests/probe" read bar/baz/value.txt)" = "alternate"
