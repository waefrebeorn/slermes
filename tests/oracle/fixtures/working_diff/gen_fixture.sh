#!/usr/bin/env bash
# gen_fixture.sh — (re)generate the committed git repo fixture + cases.in.
#
# The fixture directory IS the git repo: tests/oracle/fixtures/working_diff/
# contains a checked-in worktree (.git + files) so the C harness and Python
# oracle both operate on identical, deterministic state at run time.
# cases.in uses the literal token REPO which each side substitutes with its
# own fixture dir path.
set -euo pipefail
cd "$(dirname "$0")"

rm -rf .git repo files 2>/dev/null || true
mkdir -p repo
git -C repo init -q
git -C repo config user.email oracle@test
git -C repo config user.name oracle
git -C repo config commit.gpgsign false

echo "hello" > repo/base.txt
echo "tracked" > repo/tracked.txt
git -C repo add base.txt tracked.txt
git -C repo commit -qm init

# unstaged modification + staged modification + untracked file
echo "world" >> repo/base.txt                       # unstaged
echo "staged-change" > repo/new.txt
git -C repo add new.txt                             # staged
echo "tracked-modified" >> repo/tracked.txt         # unstaged
echo "brand new" > repo/untracked.txt               # untracked

# cases: <REPO-path> <mode> [--stat] [path...]
printf '%s\n' \
  "REPO working" \
  "REPO staged" \
  "REPO all" \
  "REPO working --stat" \
  "REPO working base.txt" \
  "REPO all new.txt" \
  "REPO bogus" \
  "/nonexistent working" \
  > cases.in

echo "fixture regenerated: $(pwd)"
