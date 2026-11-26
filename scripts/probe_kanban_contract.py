#!/usr/bin/env python3
"""Assert the kanban tools answer the way upstream does, against a scratch board.

Run the server with a throwaway database first:

    HERMES_KANBAN_DB=.testbin\\kanban_probe.db  (see .testbin/run_mcp_kanban.sh)
    python scripts/probe_kanban_contract.py

Every assertion is upstream behaviour from tools/kanban_tools.py + hermes_cli/kanban_db.py, not a
preference of ours:

  * list reports the caller's limit (KANBAN_LIST_DEFAULT_LIMIT = 50) and returns next_limit
  * limit outside 1..200 is refused by name, not clamped
  * initial_status is validated against {running, blocked}; "running" means claimed, never completed
  * a created id is the one the store holds, so show/list/complete find it again

Exit code is the number of failed assertions.
"""
from __future__ import annotations

import json
import sys
import urllib.request

URL = "http://127.0.0.1:9100/mcp"
DEFAULT_LIMIT = 50          # tools/kanban_tools.py: KANBAN_LIST_DEFAULT_LIMIT
MAX_LIMIT = 200             # tools/kanban_tools.py: KANBAN_LIST_MAX_LIMIT


def rpc(method: str, params=None, timeout: float = 90):
    body = {"jsonrpc": "2.0", "id": 1, "method": method}
    if params:
        body["params"] = params
    req = urllib.request.Request(URL, data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def call(tool: str, arguments: dict) -> dict:
    res = rpc("tools/call", {"name": tool, "arguments": arguments})["result"]
    # MCP wraps the tool's JSON in a text content block.
    if isinstance(res, dict) and isinstance(res.get("content"), list):
        for block in res["content"]:
            if isinstance(block, dict) and block.get("type") == "text":
                try:
                    return json.loads(block["text"])
                except Exception:                                   # noqa: BLE001
                    return {"_raw": block.get("text", "")[:200]}
    return res if isinstance(res, dict) else {"_raw": str(res)[:200]}


def main() -> int:
    fails = 0

    def check(name: str, ok: bool, detail: str = "") -> None:
        nonlocal fails
        print(f"{'PASS' if ok else 'FAIL'}  {name}{'' if ok else '  <- ' + detail}")
        if not ok:
            fails += 1

    listing = call("kanban_list", {})
    check("list reports the default limit", listing.get("limit") == DEFAULT_LIMIT,
          f"limit={listing.get('limit')!r}")
    check("list carries next_limit", "next_limit" in listing, f"keys={sorted(listing)}")

    two = call("kanban_list", {"limit": 2})
    check("list echoes the requested limit, not the row count", two.get("limit") == 2,
          f"limit={two.get('limit')!r} count={two.get('count')!r}")

    zero = call("kanban_list", {"limit": 0})
    check("limit 0 is refused", "error" in zero, json.dumps(zero)[:120])

    big = call("kanban_list", {"limit": MAX_LIMIT + 1})
    check(f"limit {MAX_LIMIT + 1} is refused", "error" in big, json.dumps(big)[:120])

    bad = call("kanban_create", {"title": "probe bad status", "assignee": "probe",
                                 "initial_status": "banana"})
    check("unknown initial_status is refused", "error" in bad, json.dumps(bad)[:120])

    # assignee is required upstream too (_handle_create: "name the profile that should execute").
    run = call("kanban_create", {"title": "probe running", "assignee": "probe",
                                 "initial_status": "running"})
    tid = str(run.get("task_id", ""))
    check("running create returns a store id", tid.startswith("t_"), json.dumps(run)[:120])
    check("running create does not complete the task",
          str(run.get("status")) not in {"done", "completed", "auto-accepted"},
          f"status={run.get('status')!r}")

    if tid.startswith("t_"):
        shown = call("kanban_show", {"task_id": tid})
        # Upstream nests the row: {"task": {...}, "parents": [...], "unsatisfied_parents": [...],
        # "children": [...], "comments": [...], "events": [-50:], "runs": [...], "worker_context": str}
        task = shown.get("task") if isinstance(shown.get("task"), dict) else {}
        check("show nests the row under task", task.get("id") == tid,
              f"task.id={task.get('id')!r} keys={sorted(shown)[:8]}")
        for key in ("parents", "unsatisfied_parents", "children", "comments", "events", "runs"):
            check(f"show carries {key}", isinstance(shown.get(key), list), f"{key}={shown.get(key)!r:.40}")
        check("show carries worker_context", isinstance(shown.get("worker_context"), str),
              f"worker_context={shown.get('worker_context')!r:.40}")
        done = call("kanban_complete", {"task_id": tid, "result": "probe"})
        check("complete accepts the id", done.get("ok") is True, json.dumps(done)[:120])
        again = call("kanban_show", {"task_id": tid})
        again_task = again.get("task") if isinstance(again.get("task"), dict) else {}
        check("show reports the completed status", again_task.get("status") == "done",
              f"status={again_task.get('status')!r}")

    # unsatisfied_parents is the field that tells a worker why it is held; prove it is populated, not
    # just present. Upstream: a parent counts until it is done or archived.
    parent = call("kanban_create", {"title": "probe parent", "assignee": "probe"})
    pid = str(parent.get("task_id", ""))
    child = call("kanban_create", {"title": "probe child", "assignee": "probe", "parents": [pid]})
    cid = str(child.get("task_id", ""))
    if pid.startswith("t_") and cid.startswith("t_"):
        held = call("kanban_show", {"task_id": cid})
        blockers = [b.get("id") for b in held.get("unsatisfied_parents", []) if isinstance(b, dict)]
        check("an open parent is reported as a blocker", pid in blockers, f"blockers={blockers}")
        call("kanban_complete", {"task_id": pid, "result": "probe"})
        free_now = call("kanban_show", {"task_id": cid})
        blockers2 = [b.get("id") for b in free_now.get("unsatisfied_parents", []) if isinstance(b, dict)]
        check("a done parent releases the child", pid not in blockers2, f"blockers={blockers2}")
    else:
        check("parent/child probe could create its tasks", False, f"parent={pid!r} child={cid!r}")

    # block: `kind` is validated upstream against VALID_BLOCK_KINDS, and the legacy `sticky` flag is
    # not a kind at all -- it used to be written into the kind column verbatim.
    bt = call("kanban_create", {"title": "probe block", "assignee": "probe"})
    bid = str(bt.get("task_id", ""))
    if bid.startswith("t_"):
        bad_kind = call("kanban_block", {"task_id": bid, "reason": "probe", "kind": "banana"})
        check("an unknown block kind is refused", "error" in bad_kind, json.dumps(bad_kind)[:120])
        stuck = call("kanban_block", {"task_id": bid, "reason": "probe needs input",
                                      "kind": "needs_input"})
        check("a valid block kind is accepted", stuck.get("ok") is True, json.dumps(stuck)[:120])
        after = call("kanban_show", {"task_id": bid})
        after_task = after.get("task") if isinstance(after.get("task"), dict) else {}
        check("show reports the blocked status", after_task.get("status") == "blocked",
              f"status={after_task.get('status')!r}")
        st = call("kanban_create", {"title": "probe sticky", "assignee": "probe"})
        sid = str(st.get("task_id", ""))
        sticky_ok = call("kanban_block", {"task_id": sid, "reason": "sticky probe", "sticky": True})
        check("the legacy sticky flag still blocks", sticky_ok.get("ok") is True,
              json.dumps(sticky_ok)[:120])
    else:
        check("block probe could create its task", False, json.dumps(bt)[:120])

    # schedule: upstream registers kanban_schedule; ours did not, so a client calling it got
    # "unknown tool" while the engine (kdb_schedule_task) sat unwired.
    sc = call("kanban_create", {"title": "probe schedule", "assignee": "probe"})
    scid = str(sc.get("task_id", ""))
    if scid.startswith("t_"):
        parked = call("kanban_schedule", {"task_id": scid, "reason": "later"})
        check("schedule parks the task", parked.get("ok") is True, json.dumps(parked)[:120])
        sched = call("kanban_show", {"task_id": scid})
        sched_task = sched.get("task") if isinstance(sched.get("task"), dict) else {}
        check("show reports the scheduled status", sched_task.get("status") == "scheduled",
              f"status={sched_task.get('status')!r}")
    else:
        check("schedule probe could create its task", False, json.dumps(sc)[:120])

    print(f"\n{fails} failed assertion(s)")
    return fails


if __name__ == "__main__":
    sys.exit(main())
