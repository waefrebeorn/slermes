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
        check("show finds the id that was just created", shown.get("id") == tid,
              f"id={shown.get('id')!r} keys={sorted(shown)[:8]}")
        done = call("kanban_complete", {"task_id": tid, "result": "probe"})
        check("complete accepts the id", done.get("ok") is True, json.dumps(done)[:120])
        again = call("kanban_show", {"task_id": tid})
        check("show reports the completed status", again.get("status") == "done",
              f"status={again.get('status')!r}")

    print(f"\n{fails} failed assertion(s)")
    return fails


if __name__ == "__main__":
    sys.exit(main())
