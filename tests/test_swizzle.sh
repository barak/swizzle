#!/bin/sh

set -eu

ROOT=${ROOT:-$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)}
PROBE=${PROBE:-"$ROOT/probe"}
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

mkdir -p "$TMPDIR/project/foo/sub" \
         "$TMPDIR/project/bar/baz" \
         "$TMPDIR/common/foo/sub" \
         "$TMPDIR/alt/baaz" \
         "$TMPDIR/home/work/tree/.claude" \
         "$TMPDIR/home/.config/claude/work/tree" \
         "$TMPDIR/bin"

printf 'project\n' >"$TMPDIR/project/foo/sub/value.txt"
printf 'redirected\n' >"$TMPDIR/common/foo/sub/value.txt"
printf 'local\n' >"$TMPDIR/project/bar/baz/value.txt"
printf 'alternate\n' >"$TMPDIR/alt/baaz/value.txt"
printf 'local claude\n' >"$TMPDIR/home/work/tree/.claude/config.txt"
printf 'redirected claude\n' >"$TMPDIR/home/.config/claude/work/tree/config.txt"
cat >"$TMPDIR/bin/claude" <<'EOF'
#!/bin/sh
cat .claude/config.txt
EOF
chmod +x "$TMPDIR/bin/claude"

cd "$TMPDIR/project"

test "$("$ROOT/swizzle" foo:../common/foo -- "$PROBE" read foo/sub/value.txt)" = "redirected"
test "$("$ROOT/swizzle" -- "$PROBE" read foo/sub/value.txt)" = "project"
test "$("$ROOT/swizzle" foo:../common/foo -- "$PROBE" access foo/sub/value.txt)" = "ok"
test "$("$ROOT/swizzle" foo:../common/foo -- "$PROBE" stat foo/sub/value.txt)" = "11"
"$ROOT/swizzle" foo:../common/foo -- mkdir foo/newdir
test -d ../common/foo/newdir
test ! -d foo/newdir
test "$("$ROOT/swizzle" bar/baz:"$TMPDIR/alt/baaz" -- "$PROBE" openat-read bar baz/value.txt)" = "alternate"
test "$("$ROOT/swizzle" foo:../common/foo bar/baz:"$TMPDIR/alt/baaz" -- "$PROBE" read bar/baz/value.txt)" = "alternate"

cd "$TMPDIR/home/work/tree"
test "$(
  HOME="$TMPDIR/home" PATH="$TMPDIR/bin:$PATH" "$ROOT/claude-lconf"
)" = "redirected claude"
