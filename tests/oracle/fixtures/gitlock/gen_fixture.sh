#!/usr/bin/env bash
# gen_fixture.sh — materialize the gitlock oracle fixture repos.
# Idempotent: recreates the lock files (with fixed old mtimes) and the real
# git repo on every run so each oracle invocation starts from clean state.
set -euo pipefail
FIX="$(cd "$(dirname "$0")" && pwd)"

LOCK_MTIME=1700000000
# now used by fixtures: LOCK_MTIME + 1200 => cutoff = LOCK_MTIME + 600
mkdir -p "$FIX/r1/.git" "$FIX/r2/.git"

touch "$FIX/r1/.git/shallow.lock" "$FIX/r1/.git/index.lock"
touch -d @${LOCK_MTIME} "$FIX/r1/.git/shallow.lock" "$FIX/r1/.git/index.lock"
# boundary-fresh lock: mtime == cutoff - 1 ... actually NOW-660 (stale) and one at NOW-599 (fresh)
touch "$FIX/r2/.git/HEAD.lock" "$FIX/r2/.git/MERGE_HEAD.lock"
touch -d @$((LOCK_MTIME + 1200 - 599)) "$FIX/r2/.git/HEAD.lock"
touch -d @${LOCK_MTIME} "$FIX/r2/.git/MERGE_HEAD.lock"

# real git repo with two commits on main
if [ ! -d "$FIX/rgit/.git" ]; then
  export GIT_AUTHOR_NAME=t GIT_AUTHOR_EMAIL=t@t GIT_COMMITTER_NAME=t GIT_COMMITTER_EMAIL=t@t
  mkdir -p "$FIX/rgit"
  git -C "$FIX/rgit" init -q -b main
  echo one > "$FIX/rgit/a.txt"; git -C "$FIX/rgit" add a.txt
  git -C "$FIX/rgit" commit -qm "first"
  echo two > "$FIX/rgit/b.txt"; git -C "$FIX/rgit" add b.txt
  git -C "$FIX/rgit" commit -qm "second"
fi
exit 0
