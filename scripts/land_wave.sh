#!/bin/bash
# land_wave.sh <wave-number> -- close out one wave, and refuse to commit a red tree.
#
#   bash scripts/land_wave.sh 24
#
# Order matters and each step gates the next: a wave is only committed after the
# full binary links (rc 0) and the build-health census reports zero failing
# files. An earlier version of this ran git anyway and pushed a wave whose build
# was red; the numbers in the commit message were true of the gates and false of
# the tree, which is exactly the kind of "progress" this repo does not record.
set -o pipefail
W="$1"
[ -z "$W" ] && { echo "usage: land_wave.sh <wave-number>"; exit 2; }

export PATH=/mingw64/bin:$PATH
export USERPROFILE='C:/Users/eman5'
export LOCALAPPDATA='C:/Users/eman5/AppData/Local'
cd /c/Users/eman5/slermes || exit 2
# winlibs-env.sh sets LIBRARY_PATH/CPATH and the ccache/home exports the link
# needs; a bare `export PATH` build links with rc 2 and no diagnostic at all, so
# this is sourced before any make.
# shellcheck disable=SC1091
source build/winlibs-env.sh
LOG=".testbin/w${W}_land.log"
: > "$LOG"

plan="docs/wave${W}-plan.json"
[ -f "$plan" ] || { echo "no $plan"; exit 2; }

echo "=== PATHS" | tee -a "$LOG"
C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe - "$W" <<'PY' | tee -a "$LOG"
import json, os, sys
w = sys.argv[1]
root = os.getcwd()
assert os.path.isdir(os.path.join(root, "docs")), "run land_wave.sh from the repo root"
plan = json.load(open(os.path.join(root, "docs", "wave%s-plan.json" % w), encoding="utf-8"))
paths = ["docs/wave%s-plan.json" % w, "build/objects.mk", "tests/run_all_oracles.sh"]
missing = []
for gate, mod in plan.items():
    for rel in ("include/noc_%s.h" % gate, "src/%s.c" % mod, "tests/t_port_%s.c" % gate,
                "tests/oracle_%s.py" % gate, "tests/fixtures/%s.jsonl" % gate):
        (paths.append(rel) if os.path.exists(os.path.join(root, rel)) else missing.append(rel))
if missing:
    print("MISSING %d file(s): %s" % (len(missing), missing[:5]))
    sys.exit(1)
open(os.path.join(root, "docs", ".w%s-paths.txt" % w), "w", newline="\n").write("\n".join(paths) + "\n")
print("paths %d (expected %d)" % (len(paths), 3 + 5 * len(plan)))
PY
[ $? -ne 0 ] && { echo "ABORT: wave files incomplete"; exit 3; }

echo "=== BUILD $(date +%H:%M:%S)" | tee -a "$LOG"
make -j10 slermes USERPROFILE='C:\Users\eman5' LOCALAPPDATA='C:\Users\eman5\AppData\Local' >> "$LOG" 2>&1
BRC=$?
echo "BUILD_RC=$BRC" | tee -a "$LOG"
if [ $BRC -ne 0 ]; then
    echo "ABORT: build is red -- not committing. Tail:"; grep -E "error:|undefined reference" "$LOG" | head -20
    exit 4
fi

echo "=== HEALTH $(date +%H:%M:%S)" | tee -a "$LOG"
C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe tests/verify_build_health.py --no-link >> "$LOG" 2>&1
HRC=$?
echo "HEALTH_RC=$HRC" | tee -a "$LOG"
if [ $HRC -ne 0 ] || ! grep -q "port object gate: PASS" "$LOG"; then
    echo "ABORT: build health is red -- not committing."; grep -E "failing|port object gate" "$LOG" | tail -5
    exit 5
fi

export PATH=/c/Users/eman5/AppData/Local/hermes/tools/git-2.53.0-win32-x64/mingw64/bin:$PATH
export GIT_EXEC_PATH='C:/Users/eman5/AppData/Local/hermes/tools/git-2.53.0-win32-x64/mingw64/libexec/git-core'
unset GIT_CONFIG_GLOBAL GIT_CONFIG_SYSTEM
export GIT_TERMINAL_PROMPT=0
export GIT_ASKPASS='C:/Users/eman5/AppData/Local/hermes/tools/askpass.exe'

git add --pathspec-from-file="docs/.w${W}-paths.txt" || { echo "ABORT: git add"; exit 6; }
echo "STAGED=$(git diff --cached --name-only | wc -l)" | tee -a "$LOG"
git commit -q -F - <<MSG
port: wave ${W} - no-C-home modules with differential gates

Each gate: header, real C11 port replacing the stub, JSON-lines harness,
differential oracle against the live upstream Python module, fixture.
Gates green + full build rc 0 + build health clean before this commit.
MSG
echo "FILES=$(git show --name-only --pretty=format: HEAD | grep -c .)" | tee -a "$LOG"
git log --oneline -1 | tee -a "$LOG"
# Push must be self-verifying: a commit that never reaches origin is invisible
# from the local tree, and a backgrounded landing can be cut off between
# commit and push. Retry until remote head matches local head, and record it.
LOCAL_SHA=$(git rev-parse HEAD)
SYNCED=no
for attempt in 1 2 3; do
    git push origin main >> "$LOG" 2>&1
    echo "PUSH_RC=$?" | tee -a "$LOG"
    REMOTE_SHA=$(git ls-remote origin refs/heads/main | cut -f1)
    echo "attempt $attempt local=$LOCAL_SHA remote=$REMOTE_SHA" | tee -a "$LOG"
    if [ "$REMOTE_SHA" = "$LOCAL_SHA" ]; then SYNCED=yes; break; fi
    sleep 5
done
echo "SYNCED=$SYNCED" | tee -a "$LOG"
[ "$SYNCED" = yes ] || { echo "ABORT: committed but not pushed -- run git push origin main"; exit 7; }
git ls-remote origin refs/heads/main | tee -a "$LOG"
echo "=== DONE $(date +%H:%M:%S)" | tee -a "$LOG"
