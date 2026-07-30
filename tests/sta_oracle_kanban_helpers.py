#!/usr/bin/env python3
"""
sta_oracle_kanban_helpers.py — Python oracle for the PURE kanban helpers port
(src/hermes_cli/kanban_format.c). Re-imports the REAL hermes_cli/kanban.py and
evaluates every fixture line through the canonical Python functions, reading the
SAME env vars the C harness sets, so the runner can diff C vs PY output.

Fields are '|'-separated (matching the C harness) so titles/bodies may contain
spaces. Emits the SAME flat JSON shapes the C harness emits (task_dict and
run_state as flat top-level fields), so the runner can diff them line-by-line.

Run by tests/oracle/runners/run_oracle.sh as the "kanban_helpers" group.
"""
import os
import sys
import json

_HERE = os.path.dirname(os.path.abspath(__file__))
for _c in (os.path.join(_HERE, "..", ".."), os.environ.get("HERMES_AGENT_DIR", "")):
    if _c and os.path.isdir(os.path.join(_c, "hermes_cli")):
        sys.path.insert(0, os.path.abspath(_c))
        break

import hermes_cli.kanban as K   # noqa: E402


def split_pipe(rest, n):
    out = ["" for _ in range(n)]
    if not rest:
        return out
    parts = rest.split("|")
    for i in range(min(len(parts), n)):
        out[i] = parts[i]
    return out


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: %s <cases.txt>\n" % sys.argv[0])
        return 2

    with open(sys.argv[1], "r") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if not line.strip():
                continue
            op, _, rest = line.partition(" ")
            rest = rest.strip()
            a = split_pipe(rest, 32)

            if op == "fmt_ts":
                ts = int(rest) if rest else 0
                out = K._fmt_ts(ts) if ts else ""
                print(json.dumps({"op": "fmt_ts", "ts": ts, "out": out},
                                 separators=(",", ":"), ensure_ascii=False))
            elif op == "fmt_task_line":
                parts = split_pipe(rest, 5)

                class T:
                    pass
                t = T()
                t.id = parts[0]; t.status = parts[1]; t.assignee = parts[2]
                t.tenant = parts[3]; t.title = parts[4]
                out = K._fmt_task_line(t)
                print(json.dumps({"op": "fmt_task_line", "out": out},
                                 separators=(",", ":"), ensure_ascii=False))
            elif op == "task_dict":
                parts = split_pipe(rest, 19)

                class T:
                    pass
                t = T()
                t.id = parts[0]; t.title = parts[1]; t.body = parts[2]
                t.assignee = parts[3]; t.status = parts[4]
                t.priority = parts[5] if parts[5] else "0"
                t.tenant = parts[6]; t.workspace_kind = ""
                t.workspace_path = parts[7]; t.branch_name = parts[8]
                t.project_id = parts[9]; t.created_by = parts[10]
                t.created_at = int(parts[11]) if parts[11] else 0
                t.started_at = int(parts[12]) if parts[12] else 0
                t.completed_at = int(parts[13]) if parts[13] else 0
                t.result = parts[14]
                t.skills = (json.loads(parts[15]) if parts[15] and parts[15] != "[]" else [])
                t.max_retries = int(parts[16]) if parts[16] else 0
                t.session_id = parts[17]; t.workflow_template_id = parts[18]
                t.current_step_key = parts[18]
                t.model_override = ""
                t.provider_override = ""
                d = K._task_to_dict(t)
                print(json.dumps({
                    "op": "task_dict",
                    "id": d.get("id", ""),
                    "title": d.get("title", ""),
                    "body": d.get("body", ""),
                    "assignee": d.get("assignee", ""),
                    "status": d.get("status", ""),
                    "priority": d.get("priority", ""),
                    "tenant": d.get("tenant", ""),
                    "workspace_path": d.get("workspace_path", ""),
                    "branch_name": d.get("branch_name", ""),
                    "project_id": d.get("project_id", ""),
                    "created_by": d.get("created_by", ""),
                    "created_at": d.get("created_at", 0),
                    "started_at": d.get("started_at", 0),
                    "completed_at": d.get("completed_at", 0),
                    "result": d.get("result", ""),
                    "max_retries": d.get("max_retries", 0),
                    "session_id": d.get("session_id", ""),
                    "workflow_template_id": d.get("workflow_template_id", ""),
                    "current_step_key": d.get("current_step_key", ""),
                    "skills": d.get("skills", []),
                }, separators=(",", ":"), ensure_ascii=False))
            elif op == "run_state":
                parts = split_pipe(rest, 2)
                st = parts[0] if parts[0] else None
                sn = parts[1] if parts[1] else None

                class A:
                    pass
                args = A()
                args.state_type = st
                args.state_name = sn
                try:
                    r = K._run_state_kwargs(args)
                except Exception:
                    r = "MISMATCH"
                if r == "MISMATCH" or r is None:
                    print(json.dumps({"op": "run_state", "state": None},
                                     separators=(",", ":"), ensure_ascii=False))
                elif r == {}:
                    print(json.dumps({"op": "run_state", "state": {}},
                                     separators=(",", ":"), ensure_ascii=False))
                else:
                    print(json.dumps({"op": "run_state",
                                      "state_name": r.get("state_name", ""),
                                      "state_type": r.get("state_type", "")},
                                     separators=(",", ":"), ensure_ascii=False))
            elif op == "ws_flag":
                try:
                    kind, path = K._parse_workspace_flag(rest if rest else "")
                    print(json.dumps({"op": "ws_flag", "ok": True, "kind": kind,
                                     "path": path or "", "err": ""},
                                     separators=(",", ":"), ensure_ascii=False))
                except Exception as e:
                    print(json.dumps({"op": "ws_flag", "ok": False, "kind": "",
                                     "path": "", "err": str(e)},
                                     separators=(",", ":"), ensure_ascii=False))
            elif op == "branch_flag":
                try:
                    b = K._parse_branch_flag(rest if rest else None)
                    print(json.dumps({"op": "branch_flag", "ok": bool(b), "branch": b or "",
                                     "err": ""}, separators=(",", ":"), ensure_ascii=False))
                except Exception as e:
                    print(json.dumps({"op": "branch_flag", "ok": False, "branch": "",
                                     "err": str(e)}, separators=(",", ":"), ensure_ascii=False))
            elif op == "profile_author":
                print(json.dumps({"op": "profile_author", "author": K._profile_author()},
                                 separators=(",", ":"), ensure_ascii=False))
            elif op == "duration":
                try:
                    r = K._parse_duration(rest if rest else "")
                    print(json.dumps({"op": "duration", "seconds": -2 if r is None else r},
                                     separators=(",", ":"), ensure_ascii=False))
                except Exception:
                    print(json.dumps({"op": "duration", "seconds": -1},
                                     separators=(",", ":"), ensure_ascii=False))
            elif op == "worker_run":
                rid = K._worker_run_id_for(rest)
                print(json.dumps({"op": "worker_run", "task_id": rest,
                                 "run_id": -1 if rid is None else rid},
                                 separators=(",", ":"), ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
