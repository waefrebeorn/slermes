#!/usr/bin/env python3
"""Oracle for the portable kanban helpers in hermes_cli/kanban_db.py that have
been ported into src/cli/kanban_util.c.

Reads the same fixture JSON the C harness reads, seeds a temp board DB with the
LIVE Python module, exercises the helpers, and prints the SAME JSON shape. The
runner diffs the two outputs byte-for-byte.

Mirrors t_port_kanban_util.c exactly (same ops, same placeholder substitution).
Random task ids are masked back to their fixture placeholders so the diff is
deterministic.
"""

import sys, os, json
from pathlib import Path
import importlib.util

# Load the LIVE Python source of truth from the dev tree. Pin the dev tree's
# hermes_cli package on sys.path[0] so `import hermes_cli.kanban_db` resolves
# to THIS tree (not a stale installed clone). Absolute path -- independent of
# the temp HOME the runner sets for the isolated kanban home. The module must
# be imported as `hermes_cli.kanban_db` (not a one-off spec name) so its
# dataclasses' __module__ resolves correctly in sys.modules.
ROOT = os.environ.get("SLERMES_SRC") or "/home/wubu/hermes-agent-dev"
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)
import hermes_cli.kanban_db as kdb

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
    rid_to_ph = {}
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
            rid_to_ph[tid] = a.get("id")
            # emit the fixture placeholder (deterministic) instead of the random real id
            parts.append('{"op":"create","placeholder":%s,"real":%s}'
                         % (jprint_str(a.get("id")), jprint_str(a.get("id"))))
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
            parts.append('{"op":"scan_prose","phantom":%s}' % json.dumps(phantom, separators=(",", ":")))
        elif name == "verify_created_cards":
            comp = ids.get(a.get("completing"), a.get("completing"))
            claimed = [ids.get(x, x) for x in a.get("claimed", [])]
            verified, phantom = kdb._verify_created_cards(conn, comp, claimed)
            # verified/phantom elements are either plain id strings or
            # (id, status) tuples in Python; normalize to id, then map the
            # (random) real id back to the fixture placeholder so output is
            # deterministic.
            def norm(t):
                rid = t[0] if isinstance(t, (list, tuple)) else t
                return rid_to_ph.get(rid, rid)
            vids = [norm(t) for t in verified]
            pids = [norm(t) for t in phantom]
            parts.append('{"op":"verify_created_cards","verified":%s,"phantom":%s}'
                         % (json.dumps(vids, separators=(",", ":")), json.dumps(pids, separators=(",", ":"))))
        elif name == "is_managed_scratch":
            ph = a.get("path", "")
            full = ph.replace("KANBAN_HOME/", SLERMES_HOME + "/") if ph.startswith("KANBAN_HOME/") else ph
            r = kdb._is_managed_scratch_path(Path(full))
            masked = full.replace(SLERMES_HOME, "<HOME>")
            parts.append('{"op":"is_managed_scratch","path":%s,"managed":%s}'
                         % (jprint_str(masked), "true" if r else "false"))
        elif name == "is_managed_scratch_bad":
            ph = a.get("path", "")
            full = ph.replace("KANBAN_HOME/", SLERMES_HOME + "/") if ph.startswith("KANBAN_HOME/") else ph
            r = kdb._is_managed_scratch_path(Path(full))
            parts.append('{"op":"is_managed_scratch_bad","managed":%s}' % ("true" if r else "false"))
        elif name == "has_spawnable_ready":
            parts.append('{"op":"has_spawnable_ready","v":%s}'
                         % ("true" if kdb.has_spawnable_ready(conn) else "false"))
        elif name == "has_spawnable_review":
            parts.append('{"op":"has_spawnable_review","v":%s}'
                         % ("true" if kdb.has_spawnable_review(conn) else "false"))
        elif name == "is_busy_error":
            msg = a.get("msg", "")
            lo = msg.lower()
            v = ("database is locked" in lo) or ("database is busy" in lo)
            parts.append('{"op":"is_busy_error","v":%s}' % ("true" if v else "false"))
        elif name == "absolute_hermes_path":
            ph = a.get("path", "")
            out = kdb._absolute_hermes_path(ph)
            masked = (out or "").replace(SLERMES_HOME, "<HOME>")
            parts.append('{"op":"absolute_hermes_path","path":%s}' % jprint_str(masked))
        elif name == "path_search_names":
            cmd = a.get("cmd", "")
            nv = kdb._path_search_names(cmd)
            parts.append('{"op":"path_search_names","names":[%s]}'
                         % ",".join(jprint_str(x) for x in nv))
        elif name == "safe_which_no_cwd":
            cmd = a.get("cmd", "")
            out = kdb._safe_which_no_cwd(cmd)
            parts.append('{"op":"safe_which_no_cwd","path":%s}' % jprint_str(out))
        else:
            parts.append('{"op":%s,"ok":false}' % jprint_str(name))

    sys.stdout.write("[" + ",".join(parts) + "]\n")
    conn.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
