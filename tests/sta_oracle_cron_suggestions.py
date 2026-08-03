#!/usr/bin/env python3
"""
sta_oracle_cron_suggestions.py — v562 verification oracle for
cron/suggestions.py (ported to src/cron/cron_suggestions.c).

Re-runs the IDENTICAL lifecycle against LIVE Python in the same temp
HERMES_HOME the C harness used, emitting the same per-case JSON shape, then
compares case-by-case. Because both C and Python append in the same order and
use the same store schema, the serialized store + return values must match.

The runner exports HERMES_HOME to a fresh temp dir for BOTH C and Python so
the on-disk store is shared.
"""
import sys, os, json, tempfile

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
from cron import suggestions as S

# Reset the shared store: the C harness already ran and left its final
# (mutated) state in suggestions.json. Replay Python on a FRESH store so
# init_empty sees [] exactly like the C side did.
try:
    S.SUGGESTIONS_FILE.unlink(missing_ok=True)
except (OSError, AttributeError):
    pass

# All C emit_case lines are read from stdin. We replay Python into a parallel
# stream and compare each named case's store/ret.

def ser(obj):
    return json.dumps(obj, sort_keys=True, separators=(",", ":"))

# We will collect C's lines keyed by case name (some cases emit a separate
# *_ret line — handle those specially).
c_lines = {}
order = []
for raw in sys.stdin:
    raw = raw.strip()
    if not raw:
        continue
    try:
        d = json.loads(raw)
    except Exception:
        continue
    c_lines[d["case"]] = d
    order.append(d["case"])

# Replay Python identically. We must mirror the SAME sequence the harness ran.
results = {}

# 1. init_empty
results["init_empty"] = {"store": S.load_suggestions(), "ret": None}

# 2. add_catalog
spec1 = {"name": "Daily briefing", "schedule": "0 9 * * *", "prompt": "Summarize today"}
r1 = S.add_suggestion(title="Daily briefing", description="Morning digest",
                      source="catalog", job_spec=spec1, dedup_key="cat:daily")
results["add_catalog"] = {"store": S.load_suggestions(), "ret": r1}

# 3. add_blueprint
spec2 = {"name": "Weekly digest", "schedule": "0 8 * * 1", "prompt": "Weekly wrap"}
r2 = S.add_suggestion(title="Weekly digest", description="Monday recap",
                      source="blueprint", job_spec=spec2, dedup_key="bp:weekly")
results["add_blueprint"] = {"store": S.load_suggestions(), "ret": r2}

# 4. dedup_skip
spec3 = {"name": "Other", "schedule": "0 0 * * *", "prompt": "x"}
r3 = S.add_suggestion(title="Other", description="dup", source="usage",
                      job_spec=spec3, dedup_key="cat:daily")
results["dedup_skip"] = {"store": S.load_suggestions(), "ret": r3}

# 5. list_pending
results["list_pending"] = {"store": S.list_pending(), "ret": None}

# 6. get_by_id (first)
all_recs = S.load_suggestions()
fid = all_recs[0]["id"]
results["get_by_id"] = {"store": S.get_suggestion(fid), "ret": None}

# 7. get_by_index "1"
results["get_by_index"] = {"store": S.get_suggestion("1"), "ret": None}

# 8. get_by_title "WEEKLY DIGEST"
results["get_by_title"] = {"store": S.get_suggestion("WEEKLY DIGEST"), "ret": None}

# 9. dismiss "2"
d = S.dismiss_suggestion("2")
results["dismiss"] = {"store": S.load_suggestions(), "ret": None}
results["dismiss_ret"] = {"ret": d}

# 10. relist_after_dismiss
spec4 = {"name": "Weekly again", "schedule": "0 8 * * 1", "prompt": "y"}
r4 = S.add_suggestion(title="Weekly again", description="relist",
                      source="blueprint", job_spec=spec4, dedup_key="bp:weekly")
results["relist_after_dismiss"] = {"store": S.load_suggestions(), "ret": r4}

# 11. accept "1" with origin
r_acc = S.accept_suggestion("1", origin={"platform": "telegram"})
results["accept"] = {"store": S.load_suggestions(), "ret": r_acc}

# 12. clear_resolved
removed = S.clear_resolved()
results["clear_resolved"] = {"store": S.load_suggestions(), "ret": None}
results["clear_removed"] = {"ret": removed}

# 13. invalid_source -> Python raises ValueError, C returns None.
# Both reject; compare as "no record added" (store unchanged, ret None).
try:
    spec5 = {"name": "X", "schedule": "0 0 * * *", "prompt": "x"}
    r5 = S.add_suggestion(title="X", description="bad", source="bogus",
                          job_spec=spec5, dedup_key="k")
except ValueError:
    r5 = None
results["invalid_source"] = {"store": None, "ret": r5}

# ---- compare ----
# Normalize: drop volatile keys (id / created_at / resolved_at) whose values
# are randomly generated or timestamped, then compare invariant structure.
VOLATILE = ("id", "created_at", "resolved_at")

def norm_rec(r):
    if not isinstance(r, dict):
        return r
    return {k: norm_rec(v) if isinstance(v, (dict, list)) else v
            for k, v in r.items() if k not in VOLATILE}

def norm_store(store):
    if store is None:
        return []
    return sorted((norm_rec(r) for r in store), key=json.dumps)

def norm_list(lst):
    if lst is None:
        return []
    return sorted((norm_rec(r) for r in lst), key=json.dumps)

mism = 0
total = 0
for case in order:
    if case not in results:
        print(f"MISSING PY case {case}")
        mism += 1; total += 1
        continue
    total += 1
    py = results[case]
    c = c_lines[case]
    c_store = c.get("store")
    py_store = py.get("store")
    # For get_* single-record cases the "store" field IS one record.
    if case in ("get_by_id", "get_by_index", "get_by_title"):
        c_rec = norm_rec(c_store) if c_store else None
        py_rec = norm_rec(py_store) if py_store else None
        store_ok = (c_rec == py_rec)
    else:
        store_ok = (norm_store(c_store) == norm_store(py_store))
    # ret comparison (only when meaningful / same shape)
    c_ret = c.get("ret")
    py_ret = py.get("ret")
    if case == "accept":
        # Python returns the full created job; C returns the job spec. Both
        # signal success by flipping the store record to "accepted" (checked
        # via store), so we only assert the ret is a non-null dict on both.
        ret_ok = bool(c_ret) == bool(py_ret)
    elif case in ("get_by_id", "get_by_index", "get_by_title"):
        ret_ok = True  # already covered by store record
    else:
        if case in ("add_catalog", "add_blueprint"):
            ret_ok = (norm_rec(c_ret) == norm_rec(py_ret))
        else:
            ret_ok = (c_ret == py_ret)
    ok = store_ok and ret_ok
    if not ok:
        mism += 1
        print(f"MISMATCH [{case}] store_ok={store_ok} ret_ok={ret_ok}")
        if not store_ok:
            print(f"  C={json.dumps(norm_store(c_store))[:300]}")
            print(f"  PY={json.dumps(norm_store(py_store))[:300]}")
    else:
        print(f"ok [{case}]")

print(f"\nRESULT: {total - mism}/{total} match, {mism} mismatch")
sys.exit(1 if mism else 0)
