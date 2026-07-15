#!/usr/bin/env python3
"""Oracle for the portable kanban helpers in hermes_cli/kanban_db.py that have
been ported into src/cli/kanban_util.c.

Reads the same fixture JSON the C harness reads, seeds a temp board DB with the
LIVE Python module, exercises the helpers, and prints the SAME JSON shape. The
runner diffs the two outputs byte-for-byte.

Mirrors t_port_kanban_util.c exactly (same ops, same placeholder substitution).
"""

import sys, os, json
import importlib.util

# Load the LIVE Python source of truth explicitly from the dev tree. Python may
# otherwise resolve `hermes_cli` to a stale installed clone elsewhere on the
# path; pin the file location like every other sta_oracle_*.py does.
ROOT = os.path.expanduser("~/hermes-agent-dev")
_spec = importlib.util.spec_from_file_location(
    "kanban_db_src", os.path.join(ROOT, "hermes_cli", "kanban_db.py"))
kdb = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(kdb)

# The C engine resolves its home via SLERMES_HOME; the Python profiles module
# via HERMES_HOME. The runner exports BOTH to the same temp dir for BOTH
# processes, so harness and oracle share one isolated home.
SLERMES_HOME = os.environ.get("SLERMES_HOME") or os.environ.get("HERMES_HOME") or os.environ.get("HOME")


def jprint_str(s):
    out = ['"']
    for ch in (s if s is not None else ""):
        if ch in ('"', '\\'):
            out.append('\\')
        out.append(ch)
    out.append('"')
    return ''.join(out)


def main():
    if len(sys.argv) < 2:
        print("usage: sta_oracle_kanban_util.py fixture.json", file=sys.stderr)
        return 2
    with open(sys.argv[1], "r", encoding="utf-8") as fh:
        data = json.load(fh)

    conn = kdb.connect(board="default")

    ids = {}
    parts = []
    for a in data.get("ops", []):
        name = a.get("op")
        if name == "create":
            tid = kdb.create_task(
                conn,
                title=a.get("title", "t"),
                assignee=a.get("assignee", "default"),
                created_by=a.get("created_by") or a.get("assignee", "default"),
            )
            ids[a.get("id")] = tid
            parts.append('{"op":"create","placeholder":%s,"real":%s}'
                         % (jprint_str(a.get("id")), jprint_str(tid)))
        elif name == "link":
            p = ids.get(a.get("parent"), a.get("parent"))
            c = ids.get(a.get("child"), a.get("child"))
            kdb.link_tasks(conn, p, c)
            parts.append('{"op":"link","ok":true}')
        elif name == "find_missing_parents":
            parents = [ids.get(x, x) for x in a.get("parents", [])]
            miss = kdb._find_missing_parents(conn, parents)
            parts.append('{"op":"find_missing_parents","missing":[%s]}'
                         % ",".join(jprint_str(m) for m in miss))
        elif name == "scan_prose":
            text = a.get("text", "")
            phantom = kdb._scan_prose_for_phantom_ids(conn, text)
            parts.append('{"op":"scan_prose","phantom":%s}' % phantom)
        elif name == "verify_created_cards":
            comp = ids.get(a.get("completing"), a.get("completing"))
            claimed = [ids.get(x, x) for x in a.get("claimed", [])]
            verified, phantom = kdb._verify_created_cards(conn, comp, claimed)
            # verified/phantom are lists of (id, ...) tuples in Python; reduce
            # to id strings to match the C JSON arrays.
            vids = [t[0] if isinstance(t, (list, tuple)) else t for t in verified]
            pids = [t[0] if isinstance(t, (list, tuple)) else t for t in phantom]
            parts.append('{"op":"verify_created_cards","verified":%s,"phantom":%s}'
                         % (json.dumps(vids), json.dumps(pids)))
        elif name == "is_managed_scratch":
            ph = a.get("path", "")
            full = ph.replace("KANBAN_HOME/", SLERMES_HOME + "/") if ph.startswith("KANBAN_HOME/") else ph
            r = kdb._is_managed_scratch_path(full)
            parts.append('{"op":"is_managed_scratch","path":%s,"managed":%s}'
                         % (jprint_str(full), "true" if r else "false"))
        elif name == "is_managed_scratch_bad":
            ph = a.get("path", "")
            full = ph.replace("KANBAN_HOME/", SLERMES_HOME + "/") if ph.startswith("KANBAN_HOME/") else ph
            r = kdb._is_managed_scratch_path(full)
            parts.append('{"op":"is_managed_scratch_bad","managed":%s}' % ("true" if r else "false"))
        elif name == "has_spawnable_ready":
            parts.append('{"op":"has_spawnable_ready","v":%s}'
                         % ("true" if kdb.has_spawnable_ready(conn) else "false"))
        elif name == "has_spawnable_review":
            parts.append('{"op":"has_spawnable_review","v":%s}'
                         % ("true" if kdb.has_spawnable_review(conn) else "false"))
        else:
            parts.append('{"op":%s,"ok":false}' % jprint_str(name))

    sys.stdout.write("[" + ",".join(parts) + "]\n")
    conn.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
