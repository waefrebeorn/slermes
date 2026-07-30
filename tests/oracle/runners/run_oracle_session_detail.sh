#!/usr/bin/env bash
# run_oracle_session_detail.sh — dedicated oracle runner for the
# web_server session-detail stack.
#
# Unlike the generic diff runner, this stack MUTATES the DB (set_title,
# set_archived, delete) and one side's writes must not bleed into the other's
# reads. So we hand each side its OWN copy of the seeded fixture. We also
# compare SEMANTICALLY (parse both JSON-line streams and compare values with
# numeric tolerance) because libjson emits 1000 for a REAL column that Python
# dumps as 1000.0 — the values are equal, only the textual form differs.
set -uo pipefail
cd "$(dirname "$0")/../../.."   # slermes root

FIX="tests/oracle/fixtures/session_detail"
SEED="$FIX/seed.db"
[ -f "$SEED" ] || { echo "MISSING $SEED (run make_seed.py first)"; exit 2; }

# Build the C harness once.
gcc -O2 -g -I include -I src -I lib -I lib/libdb \
  tests/t_port_session_detail.c src/cli/port_web_server_session_detail.o src/cli/port_web_server_session_endpoints.o \
  lib/libjson/json.o lib/libdb/sqlite3.o -o /tmp/tt_session_detail -lm -lz \
  || { echo "C compile failed"; exit 3; }

# Per-side DB copies.
C_DB=$(mktemp -p /tmp sess_c.XXXXXX.db)
P_DB=$(mktemp -p /tmp sess_py.XXXXXX.db)
cp "$SEED" "$C_DB"
cp "$SEED" "$P_DB"

python3 tests/oracle/fixtures/session_detail/make_seed.py >/dev/null 2>&1
/tmp/tt_session_detail "$C_DB" > /tmp/sess_c.json 2>/tmp/sess_c.err
if [ $? -ne 0 ]; then echo "C harness crashed:"; cat /tmp/sess_c.err; exit 4; fi
python3 tests/sta_oracle_session_detail.py "$P_DB" > /tmp/sess_py.json 2>/tmp/sess_py.err
if [ $? -ne 0 ]; then echo "PY oracle crashed:"; cat /tmp/sess_py.err; exit 5; fi

# Semantic compare.
python3 - <<'PY'
import json, sys

def load(path):
    rows = []
    with open(path) as fp:
        for line in fp:
            line = line.strip()
            if not line: continue
            rows.append(json.loads(line))
    return rows

c = load("/tmp/sess_c.json")
p = load("/tmp/sess_py.json")

if len(c) != len(p):
    print(f"ROW COUNT MISMATCH: C={len(c)} PY={len(p)}")
    sys.exit(1)

def norm(v):
    # normalize int/float so 1000 == 1000.0
    if isinstance(v, bool): return v
    if isinstance(v, float):
        if v.is_integer(): return int(v)
        return v
    if isinstance(v, list): return [norm(x) for x in v]
    if isinstance(v, dict): return {k: norm(x) for k,x in v.items()}
    return v

def equal(a, b, path=""):
    a, b = norm(a), norm(b)
    if isinstance(a, dict) and isinstance(b, dict):
        if a.keys() != b.keys():
            return False, f"keys {path}: {a.keys()} vs {b.keys()}"
        for k in a:
            ok, msg = equal(a[k], b[k], f"{path}.{k}")
            if not ok: return False, msg
        return True, ""
    if isinstance(a, list) and isinstance(b, list):
        if len(a) != len(b):
            return False, f"list len {path}: {len(a)} vs {len(b)}"
        for i,(x,y) in enumerate(zip(a,b)):
            ok, msg = equal(x, y, f"{path}[{i}]")
            if not ok: return False, msg
        return True, ""
    if a != b:
        return False, f"value {path}: {a!r} vs {b!r}"
    return True, ""

mismatch = 0
for i,(rc, rp) in enumerate(zip(c, p)):
    if rc.get("op") != rp.get("op"):
        print(f"op mismatch row {i}: {rc.get('op')} vs {rp.get('op')}")
        mismatch += 1
        continue
    ok, msg = equal(rc.get("out"), rp.get("out"), "out")
    if not ok:
        print(f"MISMATCH op={rc.get('op')}: {msg}")
        mismatch += 1
        continue
    # compare extra keys (err, title, after) loosely
    for k in ("err","title","after"):
        if k in rc or k in rp:
            ok, msg = equal(rc.get(k), rp.get(k), k)
            if not ok:
                print(f"MISMATCH op={rc.get('op')} key={k}: {msg}")
                mismatch += 1
                break

if mismatch == 0:
    print("MATCH: all", len(c), "operations semantically equal")
    sys.exit(0)
else:
    print(f"FAIL: {mismatch} mismatch(es)")
    sys.exit(1)
PY
code=$?
rm -f "$C_DB" "$P_DB" /tmp/tt_session_detail /tmp/sess_c.json /tmp/sess_py.json
exit $code
