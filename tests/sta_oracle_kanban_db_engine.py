#!/usr/bin/env python3
"""Oracle for the kanban_db engine port.

Reads the same fixture JSON the C harness reads, runs the operations against
the LIVE hermes_cli/kanban_db module against a temp board DB, and prints the
SAME JSON shape the C harness prints. The runner (run_oracle.sh) diffs the two.

To make parity byte-exact we replicate the C harness's exact output formatting:
  - create  -> {"op":"create","id":<id-or-null>}
  - list    -> {"op":"list","count":N,"ids":[...]}
  - claim   -> {"op":"claim","ok":true|false}
  - complete-> {"op":"complete","ok":true|false}
  - block   -> {"op":"block","ok":true|false}
  - unblock -> {"op":"unblock","ok":true|false}
  - recompute -> {"op":"recompute","promoted":N}
  - reassign-> {"op":"reassign","ok":true|false}
  - link    -> {"op":"link","ok":true|false}
  - comment -> {"op":"comment","ok":true|false}
  - notify_add -> {"op":"notify_add","ok":true|false}
  - stats   -> {"op":"stats","value":<json-string>}   (C emits a JSON *string*)
  - age     -> {"op":"age","value":<json-string>}
  - delete  -> {"op":"delete","ok":true|false}
"""

import sys, json, os, tempfile, importlib.util

fixture_path = sys.argv[1]
with open(fixture_path) as f:
    root = json.load(f)

# Stable id normalization: map each real task id -> "T<n>" so the oracle
# output is deterministic despite random id generation.
_real_ids = {}
_nid = [0]
def norm_id(rid):
    if rid is None:
        return None
    if rid not in _real_ids:
        _real_ids[rid] = "T%d" % _nid[0]
        _nid[0] += 1
    return _real_ids[rid]

# Oracle isolation: each side (C harness + Python oracle) must start from a
# FRESH, PRIVATE board so their outputs are comparable. The shared runner
# TMPH would otherwise let one side observe the other's writes. Use a
# per-process subdir under the oracle home (mirrors the C harness).
import tempfile
base = os.environ.get("SLERMES_HOME") or os.environ.get("HOME")
if base:
    priv = tempfile.mkdtemp(prefix="eng_", dir=base)
    os.environ["HERMES_KANBAN_HOME"] = priv

# Import the live module.
spec = importlib.util.spec_from_file_location(
    "kanban_db_oracle",
    os.path.join(os.path.dirname(__file__), "..", "..", "hermes_cli", "kanban_db.py"))
kb = importlib.util.module_from_spec(spec)
sys.modules["kanban_db_oracle"] = kb   # required so dataclasses/KW_ONLY resolve
spec.loader.exec_module(kb)

board = root.get("board")
# Freeze wall-clock so time-based stats compare byte-for-byte with the C engine.
_frozen = root.get("now")
if _frozen is not None:
    import time as _time
    _time.time = lambda: float(_frozen)
conn = kb.connect(board) if board else kb.connect()

named = {}
def subst(v):
    if isinstance(v, str) and v.startswith("<") and v.endswith(">"):
        return named.get(v[1:-1], v)
    return v

def jprint_str(s):
    if s is None:
        return "null"
    out = ['"']
    for c in s:
        if c in ('"', '\\'):
            out.append('\\')
        out.append(c)
    out.append('"')
    return ''.join(out)

results = []
for op in root.get("ops", []):
    name = op.get("op", "")
    a = op.get("args", {}) or {}
    nm = a.get("name")
    if name == "create":
        parents = [subst(p) for p in a.get("parents", [])]
        tid = kb.create_task(
            conn,
            title=a.get("title"), body=a.get("body"),
            assignee=a.get("assignee"), created_by=a.get("created_by"),
            workspace_kind=a.get("workspace_kind", "scratch"),
            workspace_path=a.get("workspace_path"),
            branch_name=a.get("branch_name"), tenant=a.get("tenant"),
            priority=int(a.get("priority", 0)),
            parents=parents, triage=bool(a.get("triage", False)),
            idempotency_key=a.get("idempotency_key"),
            session_id=a.get("session_id"), project_id=a.get("project_id"),
            goal_mode=bool(a.get("goal_mode", False)),
        )
        if nm and tid:
            named[nm] = tid
        results.append({"op": "create", "id": norm_id(tid)})
    elif name == "list":
        tasks = kb.list_tasks(
            conn,
            status=a.get("status"),
            assignee=a.get("assignee"),
            include_archived=bool(a.get("include_archived", False)),
        )
        results.append({"op": "list", "count": len(tasks),
                        "ids": [norm_id(t.id) for t in tasks]})
    elif name == "claim":
        t = kb.claim_task(conn, subst(a["task_id"]))
        results.append({"op": "claim", "ok": t is not None})
    elif name == "complete":
        ok = kb.complete_task(conn, subst(a["task_id"]),
                              result=a.get("result"),
                              summary=a.get("summary"))
        results.append({"op": "complete", "ok": bool(ok)})
    elif name == "block":
        ok = kb.block_task(conn, subst(a["task_id"]), kind=a.get("kind"))
        results.append({"op": "block", "ok": bool(ok)})
    elif name == "unblock":
        ok = kb.unblock_task(conn, subst(a["task_id"]))
        results.append({"op": "unblock", "ok": bool(ok)})
    elif name == "recompute":
        n = kb.recompute_ready(conn)
        results.append({"op": "recompute", "promoted": int(n)})
    elif name == "reassign":
        ok = kb.assign_task(conn, subst(a["task_id"]), a.get("profile"))
        results.append({"op": "reassign", "ok": bool(ok)})
    elif name == "link":
        try:
            kb.link_tasks(conn, subst(a["parent_id"]), subst(a["child_id"]))
            ok = True
        except Exception:
            ok = False
        results.append({"op": "link", "ok": bool(ok)})
    elif name == "comment":
        ok = kb.add_comment(conn, subst(a["task_id"]), a.get("author"), a.get("body"))
        results.append({"op": "comment", "ok": bool(ok)})
    elif name == "notify_add":
        kb.add_notify_sub(conn, task_id=subst(a["task_id"]),
                          platform=a["platform"], chat_id=a["chat_id"])
        results.append({"op": "notify_add", "ok": True})
    elif name == "stats":
        s = kb.board_stats(conn)
        results.append({"op": "stats", "value": json.dumps(s, sort_keys=True, separators=(",", ":"))})
    elif name == "age":
        t = kb.get_task(conn, subst(a["task_id"]))
        s = kb.task_age(t) if t else {}
        results.append({"op": "age", "value": json.dumps(s, sort_keys=True, separators=(",", ":"))})
    elif name == "delete":
        ok = kb.delete_task(conn, subst(a["task_id"]))
        results.append({"op": "delete", "ok": bool(ok)})
    else:
        results.append({"op": name, "ok": False})

# Emit with the exact compact formatting the C harness uses.
parts = []
for r in results:
    if r["op"] == "create":
        parts.append('{"op":"create","id":' + jprint_str(r["id"]) + '}')
    elif r["op"] == "list":
        ids = '[' + ','.join(jprint_str(x) for x in r["ids"]) + ']'
        parts.append('{"op":"list","count":%d,"ids":%s}' % (r["count"], ids))
    elif r["op"] == "stats" or r["op"] == "age":
        parts.append('{"op":"%s","value":%s}' % (r["op"], jprint_str(r["value"])))
    elif r["op"] == "recompute":
        parts.append('{"op":"recompute","promoted":%d}' % r["promoted"])
    else:
        parts.append('{"op":"%s","ok":%s}' % (r["op"], "true" if r.get("ok") else "false"))
print('{"results":[' + ','.join(parts) + ']}')
sys.exit(0)
